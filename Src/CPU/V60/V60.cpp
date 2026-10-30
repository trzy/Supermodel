// license:BSD-3-Clause
// copyright-holders:Farfetch'd, R. Belmont
// Ported to Supermodel from MAME (src/devices/cpu/v60/v60.cpp).
// V60.C
// Undiscover the beast!
// Main hacking and coding by Farfetch'd
// Portability fixes by R. Belmont
//
// Emulation for the NEC V60 (uPD70615) and V70 (uPD70632) CPUs
//

/*
Taken from the NEC Semiconductor Selection Guide Guide Book (Oct. 1995):

uPD70615 (V60)
Features:
- Virtual memory (paging method)
- Level protection architecture - 4-level hierarchical protection function
    for system multi-programming.
- Abundant general registers - Thirty two 32-bit general registers for
    optimizing compiler
- Refined instruction set - 2-address method: Arbitrary addressing mode
    can be used independently for source operand and destination operand.
- Abundant address modes and data types - Auto increment/decrement mode
    for string process, and memory indirect addressing for pointer operation
- High cost-to performance chip
- No multiprocessor system - no FRM function for increasing system
    reliability using two or more processors.
- No V20/V30 simulation mode
Address bus: 24 bits
Data bus: 16 bits
Memory space: 4G bytes
Operating frequency: 16 MHz
Package: 120-pin QFP

uPD70616 (V60)
Features:
- Virtual memory (paging method)
- Level protection architecture - 4-level hierarchical protection function
    for system multi-programming.
- Abundant general registers - Thirty two 32-bit general registers for
    optimizing compiler
- Refined instruction set - 2-address method: Arbitrary addressing mode
    can be used independently for source operand and destination operand.
- Abundant address modes and data types - Auto increment/decrement mode
    for string process, and memory indirect addressing for pointer operation
- Multiprocessor system - FRM function for increasing system reliability
    using two or more processors.
- V20/V30 simulation mode
Address bus: 24 bits
Data bus: 16 bits
Memory space: 4G bytes
Operating frequency: 16 MHz
Package: 68-pin PGA

uPD70632 (V70)
Features:
- Virtual memory (paging method)
- Level protection architecture - 4-level hierarchical protection function
    for system multi-programming.
- Abundant general registers - Thirty two 32-bit general registers for
    optimizing compiler
- Refined instruction set - 2-address method: Arbitrary addressing mode
    can be used independently for source operand and destination operand.
- Abundant address modes and data types - Auto increment/decrement mode
    for string process, and memory indirect addressing for pointer operation
- Multiprocessor system - FRM function for increasing system reliability
    using two or more processors.
- V20/V30 simulation mode
Address bus: 32 bits
Data bus: 32 bits
Memory space: 4G bytes
Operating frequency: 20 MHz
Package: 132-pin PGA, 200-pin QFP
*/


#include "V60.h"
#include "V60Disasm.h"
#include "BlockFile.h"
#include "OSD/Logger.h"

#include <cassert>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <stdexcept>


// ---------------------------------------------------------------------------
// Supermodel port: replacements for MAME framework helpers
// ---------------------------------------------------------------------------

namespace
{
	// lib/util/corefloat.h
	inline float u2f(uint32_t v)
	{
		float f;
		std::memcpy(&f, &v, sizeof(f));
		return f;
	}

	inline uint32_t f2u(float f)
	{
		uint32_t v;
		std::memcpy(&v, &f, sizeof(v));
		return v;
	}

	// osd/eminline.h (generic versions)
	inline int64_t mul_32x32(int32_t a, int32_t b)
	{
		return int64_t(a) * int64_t(b);
	}

	inline uint64_t mulu_32x32(uint32_t a, uint32_t b)
	{
		return uint64_t(a) * uint64_t(b);
	}

	inline int32_t div_64x32(int64_t a, int32_t b)
	{
		return int32_t(a / int64_t(b));
	}

	inline uint32_t divu_64x32(uint64_t a, uint32_t b)
	{
		return uint32_t(a / uint64_t(b));
	}

	inline int32_t div_64x32_rem(int64_t a, int32_t b, int32_t &remainder)
	{
		int32_t const res(div_64x32(a, b));
		remainder = int32_t(a - (int64_t(b) * res));
		return res;
	}

	inline uint32_t divu_64x32_rem(uint64_t a, uint32_t b, uint32_t &remainder)
	{
		uint32_t const res(divu_64x32(a, b));
		remainder = uint32_t(a - (uint64_t(b) * res));
		return res;
	}

	// lib/util/coretmpl.h bitswap<B>(val, b(B-1), ..., b0): result bit (B-1-i) = val bit b[i]
	template <unsigned B, typename T, typename... U>
	inline T bitswap(T val, U... b)
	{
		static_assert(sizeof...(b) == B, "bitswap: wrong number of bits");
		const int bits[] = { int(b)... };
		T r = 0;
		for (unsigned i = 0; i < B; i++)
			r |= T(((val >> bits[i]) & 1) << (B - 1 - i));
		return r;
	}

	// osd/osdcomm.h
	inline uint32_t swapendian_int32(uint32_t val)
	{
		return ((val << 24) & 0xff000000) | ((val << 8) & 0x00ff0000) | ((val >> 8) & 0x0000ff00) | ((val >> 24) & 0x000000ff);
	}

	struct V60FatalError : public std::runtime_error
	{
		explicit V60FatalError(const char *msg) : std::runtime_error(msg) {}
	};
}

// MAME's fatalerror() throws; here it logs and throws an exception caught by Run().
#define fatalerror(...)     FatalError(__VA_ARGS__)
#define logerror(...)       DebugLog(__VA_ARGS__)

static const uint32_t V60_ADDR_MASK = 0xffffff; // 24-bit address bus


// ---------------------------------------------------------------------------
// Memory space stand-in (see V60.h for the access-splitting rules)
// ---------------------------------------------------------------------------

uint64_t CV60::Space::read_n(uint32_t a, unsigned nbytes)
{
	uint64_t r = 0;
	unsigned shift = 0;
	if (bus == nullptr)
		return nbytes >= 8 ? ~uint64_t(0) : ((uint64_t(1) << (nbytes * 8)) - 1);
	if (a & 1)
	{
		r = bus->Read8(a & V60_ADDR_MASK);
		shift = 8;
		a++;
		nbytes--;
	}
	while (nbytes >= 2)
	{
		r |= uint64_t(bus->Read16(a & V60_ADDR_MASK)) << shift;
		shift += 16;
		a += 2;
		nbytes -= 2;
	}
	if (nbytes)
		r |= uint64_t(bus->Read8(a & V60_ADDR_MASK)) << shift;
	return r;
}

void CV60::Space::write_n(uint32_t a, unsigned nbytes, uint64_t d)
{
	if (bus == nullptr)
		return;
	if (a & 1)
	{
		bus->Write8(a & V60_ADDR_MASK, uint8_t(d));
		d >>= 8;
		a++;
		nbytes--;
	}
	while (nbytes >= 2)
	{
		bus->Write16(a & V60_ADDR_MASK, uint16_t(d));
		d >>= 16;
		a += 2;
		nbytes -= 2;
	}
	if (nbytes)
		bus->Write8(a & V60_ADDR_MASK, uint8_t(d));
}

uint8_t CV60::Space::read_byte(uint32_t a)
{
	return bus ? bus->Read8(a & V60_ADDR_MASK) : 0xff;
}

uint16_t CV60::Space::read_word_unaligned(uint32_t a)
{
	if (bus && !(a & 1))
		return bus->Read16(a & V60_ADDR_MASK);
	return uint16_t(read_n(a, 2));
}

uint32_t CV60::Space::read_dword(uint32_t a)
{
	// MAME's aligned read_dword ignores the low address bits
	return uint32_t(read_n(a & ~3u, 4));
}

uint32_t CV60::Space::read_dword_unaligned(uint32_t a)
{
	return uint32_t(read_n(a, 4));
}

uint64_t CV60::Space::read_qword_unaligned(uint32_t a)
{
	return read_n(a, 8);
}

void CV60::Space::write_byte(uint32_t a, uint8_t d)
{
	if (bus)
		bus->Write8(a & V60_ADDR_MASK, d);
}

void CV60::Space::write_word_unaligned(uint32_t a, uint16_t d)
{
	write_n(a, 2, d);
}

void CV60::Space::write_dword_unaligned(uint32_t a, uint32_t d)
{
	write_n(a, 4, d);
}

void CV60::Space::write_qword_unaligned(uint32_t a, uint64_t d)
{
	write_n(a, 8, d);
}


// ---------------------------------------------------------------------------
// Opcode fetch (MAME: m_pr8/m_pr16/m_pr32 = m_cache16.read_*_unaligned)
// ---------------------------------------------------------------------------

inline const UINT8 *CV60::FetchPtr(uint32_t a, unsigned n) const
{
	a &= V60_ADDR_MASK;
	for (int i = 0; i < m_numFetch; i++)
	{
		uint32_t off = a - m_fetch[i].base;
		if (off < m_fetch[i].size && m_fetch[i].size - off >= n)
			return m_fetch[i].ptr + off;
	}
	return nullptr;
}

uint8_t CV60::OpRead8(uint32_t a)
{
	if (const UINT8 *p = FetchPtr(a, 1))
		return p[0];
	return m_program->read_byte(a);
}

uint16_t CV60::OpRead16(uint32_t a)
{
	if (const UINT8 *p = FetchPtr(a, 2))
		return uint16_t(p[0] | (p[1] << 8));
	return m_program->read_word_unaligned(a);
}

uint32_t CV60::OpRead32(uint32_t a)
{
	if (const UINT8 *p = FetchPtr(a, 4))
		return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
	return m_program->read_dword_unaligned(a);
}

void CV60::FatalError(const char *fmt, ...)
{
	char buf[512];
	va_list vl;
	va_start(vl, fmt);
	vsnprintf(buf, sizeof(buf), fmt, vl);
	va_end(vl);
	ErrorLog("V60: %s", buf);
	throw V60FatalError(buf);
}


// macros stolen from MAME for flags calc
// note that these types are in x86 naming:
// byte = 8 bit, word = 16 bit, long = 32 bit

// parameter x = result, y = source 1, z = source 2

#define SetOFL_Add(x, y,z)  (_OV = (((x) ^ (y)) & ((x) ^ (z)) & 0x80000000) ? 1: 0)
#define SetOFW_Add(x, y,z)  (_OV = (((x) ^ (y)) & ((x) ^ (z)) & 0x8000) ? 1 : 0)
#define SetOFB_Add(x, y,z)  (_OV = (((x) ^ (y)) & ((x) ^ (z)) & 0x80) ? 1 : 0)

#define SetOFL_Sub(x, y,z)  (_OV = (((z) ^ (y)) & ((z) ^ (x)) & 0x80000000) ? 1 : 0)
#define SetOFW_Sub(x, y,z)  (_OV = (((z) ^ (y)) & ((z) ^ (x)) & 0x8000) ? 1 : 0)
#define SetOFB_Sub(x, y,z)  (_OV = (((z) ^ (y)) & ((z) ^ (x)) & 0x80) ? 1 : 0)

#define SetCFB(x)           {_CY = ((x) & 0x100) ? 1 : 0; }
#define SetCFW(x)           {_CY = ((x) & 0x10000) ? 1 : 0; }
#define SetCFL(x)           {_CY = ((x) & (((uint64_t)1) << 32)) ? 1 : 0; }

#define SetSF(x)            (_S = (x))
#define SetZF(x)            (_Z = (x))

#define SetSZPF_Byte(x)     {_Z = ((uint8_t)(x) == 0);  _S = ((x)&0x80) ? 1 : 0; }
#define SetSZPF_Word(x)     {_Z = ((uint16_t)(x) == 0);  _S = ((x)&0x8000) ? 1 : 0; }
#define SetSZPF_Long(x)     {_Z = ((uint32_t)(x) == 0);  _S = ((x)&0x80000000) ? 1 : 0; }

#define ORB(dst, src)       { (dst) |= (src); _OV = 0; SetSZPF_Byte(dst); }
#define ORW(dst, src)       { (dst) |= (src); _OV = 0; SetSZPF_Word(dst); }
#define ORL(dst, src)       { (dst) |= (src); _OV = 0; SetSZPF_Long(dst); }

#define ANDB(dst, src)      { (dst) &= (src); _OV = 0; SetSZPF_Byte(dst); }
#define ANDW(dst, src)      { (dst) &= (src); _OV = 0; SetSZPF_Word(dst); }
#define ANDL(dst, src)      { (dst) &= (src); _OV = 0; SetSZPF_Long(dst); }

#define XORB(dst, src)      { (dst) ^= (src); _OV = 0; SetSZPF_Byte(dst); }
#define XORW(dst, src)      { (dst) ^= (src); _OV = 0; SetSZPF_Word(dst); }
#define XORL(dst, src)      { (dst) ^= (src); _OV = 0; SetSZPF_Long(dst); }

#define SUBB(dst, src, c)   { unsigned res = (dst) - (src) - (c); SetCFB(res); SetOFB_Sub(res, src, dst); SetSZPF_Byte(res); dst = (uint8_t)res; }
#define SUBW(dst, src, c)   { unsigned res = (dst) - (src) - (c); SetCFW(res); SetOFW_Sub(res, src, dst); SetSZPF_Word(res); dst = (uint16_t)res; }
#define SUBL(dst, src, c)   { uint64_t res = (uint64_t)(dst) - (int64_t)(src) - (c); SetCFL(res); SetOFL_Sub(res, src, dst); SetSZPF_Long(res); dst = (uint32_t)res; }

#define ADDB(dst, src, c)   { unsigned res = (dst) + (src) + (c); SetCFB(res); SetOFB_Add(res, src, dst); SetSZPF_Byte(res); dst = (uint8_t)res; }
#define ADDW(dst, src, c)   { unsigned res = (dst) + (src) + (c); SetCFW(res); SetOFW_Add(res, src, dst); SetSZPF_Word(res); dst = (uint16_t)res; }
#define ADDL(dst, src, c)   { uint64_t res = (uint64_t)(dst) + (uint64_t)(src) + (c); SetCFL(res); SetOFL_Add(res, src, dst); SetSZPF_Long(res); dst = (uint32_t)res; }

#define SETREG8(a, b)       (a) = ((a) & ~0xff) | ((b) & 0xff)
#define SETREG16(a, b)      (a) = ((a) & ~0xffff) | ((b) & 0xffff)


/*
 * Prevent warnings on NetBSD.  All identifiers beginning with an underscore
 * followed by an uppercase letter are reserved by the C standard (ISO / IEC
 * 9899:1999, 7.1.3) to be used by the implementation.  It'd be best to rename
 * all such instances, but this is less intrusive and error-prone.
 */
#undef _S

#define _CY     m_flags.CY
#define _OV     m_flags.OV
#define _S      m_flags.S
#define _Z      m_flags.Z


// Defines of all v60 register...
#define R0      m_reg[0]
#define R1      m_reg[1]
#define R2      m_reg[2]
#define R3      m_reg[3]
#define R4      m_reg[4]
#define R5      m_reg[5]
#define R6      m_reg[6]
#define R7      m_reg[7]
#define R8      m_reg[8]
#define R9      m_reg[9]
#define R10     m_reg[10]
#define R11     m_reg[11]
#define R12     m_reg[12]
#define R13     m_reg[13]
#define R14     m_reg[14]
#define R15     m_reg[15]
#define R16     m_reg[16]
#define R17     m_reg[17]
#define R18     m_reg[18]
#define R19     m_reg[19]
#define R20     m_reg[20]
#define R21     m_reg[21]
#define R22     m_reg[22]
#define R23     m_reg[23]
#define R24     m_reg[24]
#define R25     m_reg[25]
#define R26     m_reg[26]
#define R27     m_reg[27]
#define R28     m_reg[28]
#define AP      m_reg[29]
#define FP      m_reg[30]
#define SP      m_reg[31]

#define PC      m_reg[32]
#define PSW     m_reg[33]

// Privileged registers
#define ISP     m_reg[36]
#define L0SP    m_reg[37]
#define L1SP    m_reg[38]
#define L2SP    m_reg[39]
#define L3SP    m_reg[40]
#define SBR     m_reg[41]
#define TR      m_reg[42]
#define SYCW    m_reg[43]
#define TKCW    m_reg[44]
#define PIR     m_reg[45]
//10-14 reserved
#define PSW2    m_reg[51]
#define ATBR0   m_reg[52]
#define ATLR0   m_reg[53]
#define ATBR1   m_reg[54]
#define ATLR1   m_reg[55]
#define ATBR2   m_reg[56]
#define ATLR2   m_reg[57]
#define ATBR3   m_reg[58]
#define ATLR3   m_reg[59]
#define TRMODE  m_reg[60]
#define ADTR0   m_reg[61]
#define ADTR1   m_reg[62]
#define ADTMR0  m_reg[63]
#define ADTMR1  m_reg[64]
//29-31 reserved

// Defines...
#define NORMALIZEFLAGS() \
{ \
	_S    = _S  ? 1 : 0; \
	_OV   = _OV ? 1 : 0; \
	_Z    = _Z  ? 1 : 0; \
	_CY   = _CY ? 1 : 0; \
}


void CV60::v60SaveStack()
{
	if (PSW & 0x10000000)
		ISP = SP;
	else
		m_reg[37 + ((PSW >> 24) & 3)] = SP;
}

void CV60::v60ReloadStack()
{
	if (PSW & 0x10000000)
		SP = ISP;
	else
		SP = m_reg[37 + ((PSW >> 24) & 3)];
}

uint32_t CV60::v60ReadPSW()
{
	PSW &= 0xfffffff0;
	PSW |= (_Z?1:0) | (_S?2:0) | (_OV?4:0) | (_CY?8:0);
	return PSW;
}

void CV60::v60WritePSW(uint32_t newval)
{
	/* determine if we need to save / restore the stacks */
	int updateStack = 0;

	/* if the interrupt state is changing, we definitely need to update */
	if ((newval ^ PSW) & 0x10000000)
		updateStack = 1;

	/* if we are not in interrupt mode and the level is changing, we also must update */
	else if (!(PSW & 0x10000000) && ((newval ^ PSW) & 0x03000000))
		updateStack = 1;

	/* save the previous stack value */
	if (updateStack)
		v60SaveStack();

	/* set the new value and update the flags */
	PSW = newval;
	_Z =  (uint8_t)(PSW & 1);
	_S =  (uint8_t)(PSW & 2);
	_OV = (uint8_t)(PSW & 4);
	_CY = (uint8_t)(PSW & 8);

	/* fetch the new stack value */
	if (updateStack)
		v60ReloadStack();
}


uint32_t CV60::v60_update_psw_for_exception(int is_interrupt, int target_level)
{
	uint32_t oldPSW = v60ReadPSW();
	uint32_t newPSW = oldPSW;

	// Change to interrupt context
	newPSW &= ~(3 << 24);  // PSW.EL = 0
	newPSW |= target_level << 24; // set target level
	newPSW &= ~(1 << 18);  // PSW.IE = 0
	newPSW &= ~(1 << 16);  // PSW.TE = 0
	newPSW &= ~(1 << 27);  // PSW.TP = 0
	newPSW &= ~(1 << 17);  // PSW.AE = 0
	newPSW &= ~(1 << 29);  // PSW.EM = 0
	if (is_interrupt)
		newPSW |=  (1 << 28);// PSW.IS = 1
	newPSW |=  (1 << 31);  // PSW.ASA = 1
	v60WritePSW(newPSW);

	return oldPSW;
}


#define GETINTVECT(nint)                    m_program->read_dword((SBR & ~0xfff) + (nint) * 4)
#define EXCEPTION_CODE_AND_SIZE(code, size) (((code) << 16) | (size))


// Supermodel port: the MAME handler code below has implicit 64->32 bit and 32->8/16 bit
// narrowing assignments that MSVC reports as C4244; keep the MAME code unchanged and
// silence that warning for it only.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4244)
#endif

// Addressing mode decoding functions
#include "am.hxx"

// Opcode functions
#include "op12.hxx"
#include "op2.hxx"
#include "op3.hxx"
#include "op4.hxx"
#include "op5.hxx"
#include "op6.hxx"
#include "op7a.hxx"

uint32_t CV60::opUNHANDLED()
{
	fatalerror("Unhandled OpCode found : %02x at %08x\n", OpRead16(PC), PC);
}

// Opcode jump table
#include "optable.hxx"

#ifdef _MSC_VER
#pragma warning(pop)
#endif


// ---------------------------------------------------------------------------
// Construction (MAME constructor + device_start)
// ---------------------------------------------------------------------------

CV60::CV60()
	: m_program(&m_programSpace)
	, m_io(&m_ioSpace)
	, m_numFetch(0)
	, m_irq_cycle(nullptr)
	, m_inRun(false)
	, m_fatal(false)
	, m_cyclesStolen(0)
	, m_start_pc(0xfffffff0)
{
	std::memset(m_fetch, 0, sizeof(m_fetch));
	std::memset(m_reg, 0, sizeof(m_reg));  // MAME leaves SP/PC/PSW/SBR/... to device_reset
	m_PPC = 0;
	m_icount = 0;

	// Set m_PIR (Processor ID) for NEC m_ LSB is reserved to NEC,
	// so I don't know what it contains.
	m_reg[45] = 0x00006000;

	// device_start()
	m_stall_io = 0;
	m_irq_line = CLEAR_LINE;
	m_nmi_line = CLEAR_LINE;

	for ( int i = 0; i < 68; i++ )
	{
		// Don't set SP (31), PCi (32), PSW (33), SBR (41), SYCW (43), TKCW (44), PIR (45), PSW2 (51)
		if ( i != 31 && i != 32 && i != 33 && i != 41 && i != 43 && i != 44 && i != 45 && i != 51 )
		{
			m_reg[i] = 0;
		}
	}

	m_flags.CY = 0;
	m_flags.OV = 0;
	m_flags.S = 0;
	m_flags.Z = 0;

	m_op1 = 0;
	m_op2 = 0;
	m_flag1 = 0;
	m_flag2 = 0;
	m_instflags = 0;
	m_lenop1 = 0;
	m_lenop2 = 0;
	m_subop = 0;
	m_bamoffset1 = 0;
	m_bamoffset2 = 0;
	m_amflag = 0;
	m_amout = 0;
	m_bamoffset = 0;
	m_amlength1 = 0;
	m_amlength2 = 0;
	m_modadd = 0;
	m_modm = 0;
	m_modval = 0;
	m_modval2 = 0;
	m_modwritevalb = 0;
	m_modwritevalh = 0;
	m_modwritevalw = 0;
	m_moddim = 0;
}

void CV60::AttachBus(IBus *mem, IBus *io)
{
	m_programSpace.bus = mem;
	m_ioSpace.bus = io;
}

void CV60::SetFetchRegion(UINT32 base, UINT32 size, const UINT8 *ptr)
{
	base &= V60_ADDR_MASK;
	for (int i = 0; i < m_numFetch; i++)
	{
		if (m_fetch[i].base == base)
		{
			if (size == 0 || ptr == nullptr)
			{
				for (int j = i; j < m_numFetch - 1; j++)
					m_fetch[j] = m_fetch[j + 1];
				m_numFetch--;
			}
			else
			{
				m_fetch[i].size = size;
				m_fetch[i].ptr = ptr;
			}
			return;
		}
	}
	if (size == 0 || ptr == nullptr)
		return;
	if (m_numFetch >= MAX_FETCH_REGIONS)
	{
		ErrorLog("V60: too many fetch regions (max %d); region at %06X ignored.", MAX_FETCH_REGIONS, base);
		return;
	}
	m_fetch[m_numFetch].base = base;
	m_fetch[m_numFetch].size = size;
	m_fetch[m_numFetch].ptr = ptr;
	m_numFetch++;
}

void CV60::ClearFetchRegions()
{
	m_numFetch = 0;
}

void CV60::SetIRQAckCallback(std::function<int(void)> cb)
{
	m_irq_cycle = cb;
}

void CV60::SetTraceCallback(std::function<void(UINT32 pc)> cb)
{
	m_trace = cb;
}


// ---------------------------------------------------------------------------
// Register access (MAME state_import/state_export)
// ---------------------------------------------------------------------------

UINT32 CV60::GetPC() const
{
	return m_reg[32];
}

UINT32 CV60::GetPPC() const
{
	return m_PPC;
}

void CV60::SetPC(UINT32 pc)
{
	PC = pc;
}

UINT32 CV60::GetReg(unsigned n) const
{
	return n < 68 ? m_reg[n] : 0;
}

void CV60::SetReg(unsigned n, UINT32 v)
{
	if (n < 68)
		m_reg[n] = v;
}

UINT32 CV60::GetPSW()
{
	return v60ReadPSW();
}

void CV60::SetPSW(UINT32 v)
{
	v60WritePSW(v);
}

const char *CV60::GetRegName(unsigned n)
{
	return v60_disassembler::RegName(n);
}


// ---------------------------------------------------------------------------
// Reset / run / interrupts
// ---------------------------------------------------------------------------

void CV60::Reset()
{
	// device_reset()
	PSW   = 0x10000000;
	PC    = m_start_pc;
	SBR   = 0x00000000;
	SYCW  = 0x00000070;
	TKCW  = 0x0000e000;
	PSW2  = 0x0000f002;

	_CY   = 0;
	_OV   = 0;
	_S    = 0;
	_Z    = 0;

	m_fatal = false;
}


void CV60::Stall()
{
	m_stall_io = 1;
	AbortTimeslice();
}

void CV60::AbortTimeslice()
{
	// cpu_device::abort_timeslice()
	if (!m_inRun)
		return;
	int delta = m_icount;
	m_cyclesStolen += delta;
	m_icount -= delta;
}


void CV60::v60_do_irq(int vector)
{
	// standard_irq_callback(vector, PC);  -- MAME debugger/acknowledge hook, not ported
	uint32_t oldPSW = v60_update_psw_for_exception(1, 0);

	// Push PC and PSW onto the stack
	SP-=4;
	m_program->write_dword_unaligned(SP, oldPSW);
	SP-=4;
	m_program->write_dword_unaligned(SP, PC);

	// Jump to vector for user interrupt
	PC = GETINTVECT(vector);
}

void CV60::v60_try_irq()
{
	if(m_irq_line == CLEAR_LINE)
		return;
	if((PSW & (1 << 18)) != 0) {
		int vector;
		if(m_irq_line != ASSERT_LINE)
			m_irq_line = CLEAR_LINE;

		// m_irq_cycle is a devcb_read8 in MAME (default 0)
		vector = m_irq_cycle ? uint8_t(m_irq_cycle()) : 0;

		v60_do_irq(vector + 0x40);
	}
}

void CV60::execute_set_input(int irqline, int state)
{
	if(irqline == INPUT_LINE_NMI) {
		switch(state) {
		case ASSERT_LINE:
			if(m_nmi_line == CLEAR_LINE) {
				m_nmi_line = ASSERT_LINE;
				v60_do_irq(2);
			}
			break;
		case CLEAR_LINE:
			m_nmi_line = CLEAR_LINE;
			break;
		}
	} else {
		m_irq_line = state;
		v60_try_irq();
	}
}

void CV60::ApplyPendingInputs()
{
	// Delivered in order, like MAME's per-line input event queue after the sync
	for (size_t i = 0; i < m_pendingInputs.size(); i++)
		execute_set_input(m_pendingInputs[i].first, m_pendingInputs[i].second);
	m_pendingInputs.clear();
}

void CV60::SetIRQLine(bool asserted)
{
	int state = asserted ? ASSERT_LINE : CLEAR_LINE;
	if (m_inRun)
	{
		m_pendingInputs.emplace_back(INPUT_LINE_IRQ0, state);
		AbortTimeslice();
	}
	else
	{
		try { execute_set_input(INPUT_LINE_IRQ0, state); }
		catch (const V60FatalError &) { m_fatal = true; }
	}
}

void CV60::SetNMILine(bool asserted)
{
	int state = asserted ? ASSERT_LINE : CLEAR_LINE;
	if (m_inRun)
	{
		m_pendingInputs.emplace_back(INPUT_LINE_NMI, state);
		AbortTimeslice();
	}
	else
	{
		try { execute_set_input(INPUT_LINE_NMI, state); }
		catch (const V60FatalError &) { m_fatal = true; }
	}
}

// Actual cycles / instruction is unknown

void CV60::execute_run()
{
	if (m_irq_line != CLEAR_LINE)
		v60_try_irq();

	while (m_icount > 0)
	{
		uint32_t inc;
		m_PPC = PC;
		if (m_trace)    // debugger_instruction_hook(PC);
			m_trace(PC);
		m_icount -= 8;  /* fix me -- this is just an average */
		inc = (this->*s_OpCodeTable[OpRead8(PC)])();
		PC += inc;
		if (m_irq_line != CLEAR_LINE)
			v60_try_irq();
	}
}

int CV60::Run(int cycles)
{
	if (m_fatal)
		return cycles;

	m_icount = cycles;
	m_cyclesStolen = 0;
	m_inRun = true;
	try
	{
		execute_run();
	}
	catch (const V60FatalError &)
	{
		m_fatal = true;
	}
	m_inRun = false;

	int executed = (cycles - m_cyclesStolen) - m_icount;
	if (m_fatal)
		executed = cycles;

	// Input line changes made while executing take effect at the end of the timeslice
	if (!m_pendingInputs.empty() && !m_fatal)
	{
		try { ApplyPendingInputs(); }
		catch (const V60FatalError &) { m_fatal = true; }
	}
	m_pendingInputs.clear();

	return executed;
}


// ---------------------------------------------------------------------------
// Disassembler
// ---------------------------------------------------------------------------

void CV60::SetDisasmReadCallback(std::function<UINT8(UINT32)> cb)
{
	m_disasmRead = cb;
}

std::string CV60::Disassemble(UINT32 pc, unsigned *length)
{
	v60_disassembler::data_buffer buf([this](uint32_t a) -> uint8_t {
		if (m_disasmRead)
			return m_disasmRead(a & V60_ADDR_MASK);
		if (const UINT8 *p = FetchPtr(a, 1))
			return p[0];
		return m_program->read_byte(a);
	});
	v60_disassembler dasm;
	std::ostringstream ss;
	uint32_t res = dasm.disassemble(ss, pc, buf, buf);
	if (length)
		*length = res & v60_disassembler::LENGTHMASK;
	return ss.str();
}


// ---------------------------------------------------------------------------
// Save states (MAME save_item list: m_reg, m_irq_line, m_nmi_line, m_PPC, CY, OV, S, Z)
// ---------------------------------------------------------------------------

void CV60::SaveState(CBlockFile *StateFile, const char *name)
{
	StateFile->NewBlock(name, __FILE__);
	StateFile->Write(m_reg, sizeof(m_reg));
	StateFile->Write(&m_irq_line, sizeof(m_irq_line));
	StateFile->Write(&m_nmi_line, sizeof(m_nmi_line));
	StateFile->Write(&m_PPC, sizeof(m_PPC));
	StateFile->Write(&m_flags.CY, sizeof(m_flags.CY));
	StateFile->Write(&m_flags.OV, sizeof(m_flags.OV));
	StateFile->Write(&m_flags.S, sizeof(m_flags.S));
	StateFile->Write(&m_flags.Z, sizeof(m_flags.Z));
	int32_t stall = m_stall_io;
	StateFile->Write(&stall, sizeof(stall));
}

void CV60::LoadState(CBlockFile *StateFile, const char *name)
{
	if (Result::OKAY != StateFile->FindBlock(name))
	{
		ErrorLog("Unable to load V60 state. Save state file is corrupt.");
		return;
	}
	StateFile->Read(m_reg, sizeof(m_reg));
	StateFile->Read(&m_irq_line, sizeof(m_irq_line));
	StateFile->Read(&m_nmi_line, sizeof(m_nmi_line));
	StateFile->Read(&m_PPC, sizeof(m_PPC));
	StateFile->Read(&m_flags.CY, sizeof(m_flags.CY));
	StateFile->Read(&m_flags.OV, sizeof(m_flags.OV));
	StateFile->Read(&m_flags.S, sizeof(m_flags.S));
	StateFile->Read(&m_flags.Z, sizeof(m_flags.Z));
	int32_t stall = 0;
	StateFile->Read(&stall, sizeof(stall));
	m_stall_io = stall;
	m_fatal = false;
}
