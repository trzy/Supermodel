/**
 ** Supermodel
 ** A Sega Model Series Arcade Emulator.
 **
 ** This file is part of Supermodel.
 **
 ** Supermodel is free software: you can redistribute it and/or modify it under
 ** the terms of the GNU General Public License as published by the Free
 ** Software Foundation, either version 3 of the License, or (at your option)
 ** any later version.
 **
 ** Supermodel is distributed in the hope that it will be useful, but WITHOUT
 ** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 ** FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 ** more details.
 **
 ** You should have received a copy of the GNU General Public License along
 ** with Supermodel.  If not, see <http://www.gnu.org/licenses/>.
 **/

/*
 * MB86233.cpp
 *
 * Based on original work by Olivier Galibert in MAME. Portions Copyright (c)
 * Olivier Galibert, used under the BSD 3-Clause License: see Docs/README.txt.
 *
 * Ported to Supermodel from MAME (src/devices/cpu/mb86233/mb86233.cpp).
 */

#include "MB86233.h"
#include "MB86233Disasm.h"
#include "BlockFile.h"
#include "OSD/Logger.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iterator>

/*
  Driver based on the initial reverse-engineering of Elsemi, extended,
  generalized and made to look more like a cpu since then thanks in
  part to a "manual" that barely deserves the name.

  The 86232 has 512 32-bits dwords of triple-port memory (1 write, 2
  read).  The 86233/86234 have instead two normal (1 read, 1 write,
  non-simultaneous) independent ram banks, one of 256 dwords and one
  of 512.

  The ram banks are mapped at 0x000-0x0ff and 0x200-0x3ff (proven by
  geometrizer code that clears the ram at startup).  Move and load
  instructions kind of target a specific ram, but do it by adding
  0x200 to the address on one side of the other, which can then end up
  anywhere.  In particular model1 coprocessor has the output fifo at
  0x400, which is sometimes hit by having x1 at 0x200 and using the
  automatic 0x200 adder.  Theorically external accesses to 100-1ff and
  400+ seem to be routed externally, since they're used for the fifo
  in model 1.

  The cpu can theorically work in either floating point (32-bits ieee
  flots) or fixed point (32/36/48 bits registers) modes.  All sega
  programs start by activating floating point and staying there, so
  fixed point is not implemented.

  An interrupt is used to update the rf0 (status? leds?) registers in
  the coprocessor programs.  It's on bit 1 of the mask (irq3?) and
  vector 0x004.  It's probably periodic, maybe on vblank.  Note that
  the copro programs never initialize the stack pointer.  Interrupts
  are not implemented at this point.

  The 86233 and 86234 dies are slightly different in the die shots,
  but there's no known programming-level difference at this point.
  It's unclear whether some register-file linked functionality is
  internal or external though (fifos, banking in model2/86234), so
  there may lie the actual differences.

  Note: in the event of two writes occurring at the same register in a
  single instruction, priority appears to vary depending on the ALU
  operation. Transfers appear to take precedence over integer ops, while
  floating point ops take precedence over transfers. This may be a result
  of the ALU unit taking more than one cycle to execute floating point ops
  (86232 datasheet), hence the register is updated after the transfer.
*/

// Replacements for MAME's corefloat.h f2u/u2f and util::sext
// Supermodel: the TGP's floating point has no NaN or infinity. An exponent of
// 255 is an ordinary (huge) number, taken here as the largest finite value,
// and a result that overflows saturates. With IEEE floats, 0xFFFFFFFF (NaN)
// gives NaN results: Desert Tank's attract mode checks the cannon shell
// against an object read through a null pointer (the program ROM's FFFFFFFF
// words), and the NaN distance (FFFFFFFF, negative) made the shell hit at the
// muzzle. On the board the distance is huge and the shell flies on to the
// bunker. (A NaN result, as 0/0, is taken as 0.)
static inline float u2f(UINT32 v)
{
	if (((v >> 23) & 0xff) == 0xff)
		v = (v & 0x80000000) | 0x7f7fffff;
	float f; memcpy(&f, &v, sizeof(f)); return f;
}
static inline UINT32 f2u(float f)
{
	UINT32 v; memcpy(&v, &f, sizeof(v));
	if (((v >> 23) & 0xff) == 0xff)
		v = (v & 0x007fffff) ? 0 : ((v & 0x80000000) | 0x7f7fffff);
	return v;
}
static inline INT32 sext(UINT32 value, int width) { return INT32(value << (32 - width)) >> (32 - width); }

// Supermodel: the small helpers Run() calls for nearly every instruction are
// forced inline (they otherwise stay calls, which costs a lot here)
#if defined(_MSC_VER)
#define MB86233_INLINE __forceinline
#elif defined(__GNUC__)
#define MB86233_INLINE inline __attribute__((always_inline))
#else
#define MB86233_INLINE inline
#endif


CMB86233::CMB86233()
  : m_bus(&m_nullBus), m_rom(nullptr), m_romWords(0), m_internalRam(true),
    m_icount(0), m_stolen(0), m_halt(false)
{
  memset(m_ram, 0, sizeof(m_ram));
  // device_start
  m_gpio0 = m_gpio1 = m_gpio2 = m_gpio3 = false;
  Reset();
}

// Data space: on-chip RAM banks at 0x000-0x0ff and 0x200-0x3ff (in MAME these
// are .ram() entries of model1_state::copro_data_map), everything else external.
MB86233_INLINE UINT32 CMB86233::data_r(UINT16 a)
{
  if (m_internalRam && (a < 0x100 || (a >= 0x200 && a < 0x400)))
    return m_ram[a];
  return m_bus->ReadData(a);
}

MB86233_INLINE void CMB86233::data_w(UINT16 a, UINT32 v)
{
  if (m_internalRam && (a < 0x100 || (a >= 0x200 && a < 0x400)))
    m_ram[a] = v;
  else
    m_bus->WriteData(a, v);
}

void CMB86233::Reset()
{
	m_pc = 0;
	m_ppc = 0;
	m_st = F_ZRC|F_ZRD|F_ZX0|F_ZX1|F_ZX2|F_ZC0|F_ZC1;
	m_sp = 0;

	m_a = 0;
	m_b = 0;
	m_d = 0;
	m_p = 0;
	m_r = 1;
	m_rpc = 1;
	m_c0 = 1;
	m_c1 = 1;
	m_b0 = 0;
	m_b1 = 0;
	m_x0 = 0;
	m_x1 = 0;
	m_i0 = 0;
	m_i1 = 0;
	m_sft = 0;
	m_vsm = 0;
	m_vsmr = 7;
	m_mask = 0;
	m_m = 1;

	m_alu_stmask = 0;
	m_alu_stset = 0;
	m_alu_r1 = 0;
	m_alu_r2 = 0;

	std::fill(std::begin(m_pcs), std::end(m_pcs), 0);

	m_stall = false;
}

// Same accounting as MAME's device_execute_interface::abort_timeslice():
// the remaining cycles are "stolen" so Run() reports only what was used.
void CMB86233::AbortTimeslice()
{
  m_stolen += m_icount;
  m_icount = 0;
}

UINT32 CMB86233::set_exp(UINT32 val, UINT32 exp)
{
	return (val & 0x807fffff) | ((exp & 0xff) << 23);
}

UINT32 CMB86233::set_mant(UINT32 val, UINT32 mant)
{
	return (val & 0x07f800000) | ((mant & 0x00800000) << 8) | (mant & 0x007fffff);
}

UINT32 CMB86233::get_exp(UINT32 val)
{
	return (val >> 23) & 0xff;
}

UINT32 CMB86233::get_mant(UINT32 val)
{
	return val & 0x80000000 ? val | 0x7f800000 : val & 0x807fffff;
}

void CMB86233::pcs_push()
{
	for(unsigned int i=3; i; i--)
		m_pcs[i] = m_pcs[i-1];
	m_pcs[0] = m_pc;
}

void CMB86233::pcs_pop()
{
	m_pc = m_pcs[0];
	for(unsigned int i=0; i != 3; i++)
		m_pcs[i] = m_pcs[i+1];
}

MB86233_INLINE void CMB86233::stset_set_sz_int(UINT32 val)
{
	m_alu_stset = val ? (val & 0x80000000 ? F_SGD : 0) : F_ZRD;
}

MB86233_INLINE void CMB86233::stset_set_sz_fp(UINT32 val)
{
	m_alu_stset = (val & 0x7fffffff) ? (val & 0x80000000 ? F_SGD : 0) : F_ZRD;
}

MB86233_INLINE void CMB86233::alu_pre(UINT32 alu)
{
	switch(alu) {
	case 0x00: break; // no alu

	case 0x01: {
		// andd
		m_alu_stmask = F_ZRD|F_SGD|F_CPD|F_OVD|F_DVZD;
		m_alu_r1 = m_d & m_a;
		stset_set_sz_int(m_alu_r1);
		break;
	}

	case 0x02: {
		// orad
		m_alu_stmask = F_ZRD|F_SGD|F_CPD|F_OVD|F_DVZD;
		m_alu_r1 = m_d | m_a;
		stset_set_sz_int(m_alu_r1);
		break;
	}

	case 0x03: {
		// eord
		m_alu_stmask = F_ZRD|F_SGD|F_CPD|F_OVD|F_DVZD;
		m_alu_r1 = m_d ^ m_a;
		stset_set_sz_int(m_alu_r1);
		break;
	}

	case 0x04: {
		// notd
		m_alu_stmask = F_ZRD|F_SGD|F_CPD|F_OVD|F_DVZD;
		m_alu_r1 = ~m_d;
		stset_set_sz_int(m_alu_r1);
		break;
	}

	case 0x05: {
		// fcpd
		m_alu_stmask = F_ZRD|F_SGD|F_CPD|F_OVD|F_DVZD;
		UINT32 r = f2u(u2f(m_d) - u2f(m_a));
		stset_set_sz_fp(r);
		break;
	}

	case 0x06: {
		// fadd
		m_alu_stmask = F_ZRD|F_SGD|F_CPD|F_OVD|F_DVZD;
		m_alu_r1 = f2u(u2f(m_d) + u2f(m_a));
		stset_set_sz_fp(m_alu_r1);
		break;
	}

	case 0x07: {
		// fsbd
		m_alu_stmask = F_ZRD|F_SGD|F_CPD|F_OVD|F_DVZD;
		m_alu_r1 = f2u(u2f(m_d) - u2f(m_a));
		stset_set_sz_fp(m_alu_r1);
		break;
	}

	case 0x08: {
		// fml
		m_alu_stmask = 0;
		m_alu_r1 = f2u(u2f(m_a) * u2f(m_b));
		m_alu_stset = 0;
		break;
	}

	case 0x09: {
		// fmsd
		m_alu_stmask = F_ZRD|F_SGD|F_CPD|F_OVD|F_DVZD;
		m_alu_r1 = f2u(u2f(m_d) + u2f(m_p));
		m_alu_r2 = f2u(u2f(m_a) * u2f(m_b));
		stset_set_sz_fp(m_alu_r1);
		break;
	}

	case 0x0a: {
		// fmrd
		m_alu_stmask = F_ZRD|F_SGD|F_CPD|F_OVD|F_DVZD;
		m_alu_r1 = f2u(u2f(m_d) - u2f(m_p));
		m_alu_r2 = f2u(u2f(m_a) * u2f(m_b));
		stset_set_sz_fp(m_alu_r1);
		break;
	}

	case 0x0b: {
		// fabd
		m_alu_stmask = F_ZRD|F_SGD|F_CPD|F_OVD|F_DVZD;
		m_alu_r1 = m_d & 0x7fffffff;
		stset_set_sz_fp(m_alu_r1);
		break;
	}

	case 0x0c: {
		// fsmd
		m_alu_stmask = F_ZRD|F_SGD|F_CPD|F_OVD|F_DVZD;
		m_alu_r1 = f2u(u2f(m_d) + u2f(m_p));
		stset_set_sz_fp(m_alu_r1);
		break;
	}

	case 0x0d: {
		// fspd
		m_alu_stmask = F_ZRD|F_SGD|F_CPD|F_OVD|F_DVZD;
		m_alu_r1 = m_p;
		m_alu_r2 = f2u(u2f(m_a) * u2f(m_b));
		stset_set_sz_fp(m_alu_r1);
		break;
	}

	case 0x0e: {
		// cxfd
		m_alu_stmask = F_ZRD|F_SGD|F_CPD|F_OVD|F_DVZD;
		m_alu_r1 = f2u(float(INT32(m_d)));
		stset_set_sz_int(m_alu_r1);
		break;
	}

	case 0x0f: {
		// cfxd
		m_alu_stmask = F_ZRD|F_SGD|F_CPD|F_OVD|F_DVZD;
		switch((m_m >> 1) & 3) {
		case 0: m_alu_r1 = INT32(roundf(u2f(m_d))); break;
		case 1: m_alu_r1 = INT32(ceilf(u2f(m_d))); break;
		case 2: m_alu_r1 = INT32(floorf(u2f(m_d))); break;
		case 3: m_alu_r1 = INT32(u2f(m_d)); break;
		}
		stset_set_sz_int(m_alu_r1);
		break;
	}

	case 0x10: {
		// fdvd
		m_alu_stmask = F_ZRD|F_SGD|F_CPD|F_OVD|F_DVZD;
		m_alu_r1 = f2u(u2f(m_d) / u2f(m_a));
		stset_set_sz_fp(m_alu_r1);
		break;
	}

	case 0x11: {
		// fned
		m_alu_stmask = F_ZRD|F_SGD|F_CPD|F_OVD|F_DVZD;
		m_alu_r1 = m_d ? m_d ^ 0x80000000 : 0;
		stset_set_sz_fp(m_alu_r1);
		break;
	}

	case 0x13: {
		// d = b + a
		m_alu_stmask = F_ZRD|F_SGD|F_CPD|F_OVD|F_DVZD;
		m_alu_r1 = f2u(u2f(m_b) + u2f(m_a));
		stset_set_sz_fp(m_alu_r1);
		break;
	}

	case 0x14: {
		// d = b - a
		m_alu_stmask = F_ZRD|F_SGD|F_CPD|F_OVD|F_DVZD;
		m_alu_r1 = f2u(u2f(m_b) - u2f(m_a));
		stset_set_sz_fp(m_alu_r1);
		break;
	}

	case 0x16: {
		// lsrd
		m_alu_stmask = F_ZRD|F_SGD|F_CPD|F_OVD|F_DVZD;
		m_alu_r1 = m_d >> m_sft;
		stset_set_sz_int(m_alu_r1);
		break;
	}

	case 0x17: {
		// lsld
		m_alu_stmask = F_ZRD|F_SGD|F_CPD|F_OVD|F_DVZD;
		m_alu_r1 = m_d << m_sft;
		stset_set_sz_int(m_alu_r1);
		break;
	}

	case 0x18: {
		// asrd
		m_alu_stmask = F_ZRD|F_SGD|F_CPD|F_OVD|F_DVZD;
		m_alu_r1 = INT32(m_d) >> m_sft;
		stset_set_sz_int(m_alu_r1);
		break;
	}

	case 0x19: {
		// asld
		m_alu_stmask = F_ZRD|F_SGD|F_CPD|F_OVD|F_DVZD;
		m_alu_r1 = INT32(m_d) << m_sft;
		stset_set_sz_int(m_alu_r1);
		break;
	}

	case 0x1a: {
		// addd
		m_alu_stmask = F_ZRD|F_SGD|F_CPD|F_OVD|F_DVZD;
		m_alu_r1 = m_d + m_a;
		stset_set_sz_int(m_alu_r1);
		break;
	}

	case 0x1b: {
		// subd
		m_alu_stmask = F_ZRD|F_SGD|F_CPD|F_OVD|F_DVZD;
		m_alu_r1 = m_d - m_a;
		stset_set_sz_int(m_alu_r1);
		break;
	}

	default:
		DebugLog("unhandled alu pre %02x\n", alu);
		break;
	}
}

MB86233_INLINE void CMB86233::alu_update_st()
{
	m_st = (m_st & ~m_alu_stmask) | m_alu_stset;
}

// Supermodel: the post ops are picked with bit masks of the alu field rather
// than switches (fewer indirect jumps in the interpreter loop)
#define ALU_BIT(n) (1u << (n))
static const UINT32 ALU_INT_D = ALU_BIT(0x01)|ALU_BIT(0x02)|ALU_BIT(0x03)|ALU_BIT(0x04)|ALU_BIT(0x0e)|ALU_BIT(0x0f)|
                                ALU_BIT(0x16)|ALU_BIT(0x17)|ALU_BIT(0x18)|ALU_BIT(0x19)|ALU_BIT(0x1a)|ALU_BIT(0x1b);
static const UINT32 ALU_FP_ST = ALU_BIT(0x05);
static const UINT32 ALU_FP_D  = ALU_BIT(0x06)|ALU_BIT(0x07)|ALU_BIT(0x0b)|ALU_BIT(0x0c)|
                                ALU_BIT(0x10)|ALU_BIT(0x11)|ALU_BIT(0x13)|ALU_BIT(0x14);
static const UINT32 ALU_FP_P  = ALU_BIT(0x08);
static const UINT32 ALU_FP_DP = ALU_BIT(0x09)|ALU_BIT(0x0a)|ALU_BIT(0x0d);
#undef ALU_BIT

MB86233_INLINE void CMB86233::alu_post_1(UINT32 alu)
{
	// integer alu post ops: d update
	if(ALU_INT_D & (1u << alu)) {
		m_d = m_alu_r1;
		alu_update_st();
	}
}

MB86233_INLINE void CMB86233::alu_post_2(UINT32 alu)
{
	// floating point alu post ops
	// assume each one takes 2 cycles
	UINT32 bit = 1u << alu;
	if(!(bit & (ALU_FP_ST|ALU_FP_D|ALU_FP_P|ALU_FP_DP)))
		return;
	if(bit & ALU_FP_P) {
		// p update
		m_p = m_alu_r1;
	} else {
		if(bit & (ALU_FP_D|ALU_FP_DP))
			m_d = m_alu_r1;     // d update
		if(bit & ALU_FP_DP)
			m_p = m_alu_r2;     // and p
		alu_update_st();
	}
	m_icount--;
}

MB86233_INLINE UINT16 CMB86233::ea_pre_0(UINT32 r)
{
	switch(r & 0x180) {
	case 0x000: return r & 0x7f;
	case 0x080: case 0x100: return (r & 0x7f) + m_b0 + m_x0;
	case 0x180: {
		switch(r & 0x60) {
		case 0x00: return m_b0 + m_x0;
		case 0x20: return m_x0;
		case 0x40: return m_b0 + (m_x0 & m_vsmr);
		case 0x60: return m_x0 & m_vsmr;
		}
	}
	}
	return 0;
}

MB86233_INLINE void CMB86233::ea_post_0(UINT32 r)
{
	if(!(r & 0x100))
		return;
	if(!(r & 0x080))
		m_x0 += m_i0;
	else
		m_x0 += sext(r, 5);
}

MB86233_INLINE UINT16 CMB86233::ea_pre_1(UINT32 r)
{
	switch(r & 0x180) {
	case 0x000: return r & 0x7f;
	case 0x080: case 0x100: return (r & 0x7f) + m_b1 + m_x1;
	case 0x180: {
		switch(r & 0x60) {
		case 0x00: return m_b1 + m_x1;
		case 0x20: return m_x1;
		case 0x40: return m_b1 + (m_x1 & m_vsmr);
		case 0x60: return m_x1 & m_vsmr;
		}
	}
	}
	return 0;
}

MB86233_INLINE void CMB86233::ea_post_1(UINT32 r)
{
	if(!(r & 0x100))
		return;
	if(!(r & 0x080))
		m_x1 += m_i1;
	else
		m_x1 += sext(r, 5);
}

// Supermodel: a, b, d and p are nearly all the register moves; they are
// handled inline and the rest in read_reg_other()/write_reg_other()
MB86233_INLINE UINT32 CMB86233::read_reg(UINT32 r)
{
	switch(r & 0x3f) {
	case 0x10: return m_a;
	case 0x13: return m_b;
	case 0x19: return m_d;
	case 0x1c: return m_p;
	default:   return read_reg_other(r);
	}
}

UINT32 CMB86233::read_reg_other(UINT32 r)
{
	r &= 0x3f;
	if(r >= 0x20 && r < 0x30)
		return m_bus->ReadRF(UINT8(r & 0x1f));
	switch(r) {
	case 0x00: return m_b0;
	case 0x01: return m_b1;
	case 0x02: return m_x0;
	case 0x03: return m_x1;

	case 0x0c: return m_c0;
	case 0x0d: return m_c1;

	case 0x10: return m_a;
	case 0x11: return get_exp(m_a);
	case 0x12: return get_mant(m_a);
	case 0x13: return m_b;
	case 0x14: return get_exp(m_b);
	case 0x15: return get_mant(m_b);
	case 0x19: return m_d;
		/* c */
	case 0x1a: return get_exp(m_d);
	case 0x1b: return get_mant(m_d);
	case 0x1c: return m_p;
	case 0x1d: return get_exp(m_p);
	case 0x1e: return get_mant(m_p);
	case 0x1f: return m_sft;

	case 0x34: return m_rpc;

	default:
		DebugLog("unimplemented read_reg(%02x) (%x)\n", r, m_ppc);
		return 0;
	}
}

MB86233_INLINE void CMB86233::write_reg(UINT32 r, UINT32 v)
{
	switch(r & 0x3f) {
	case 0x10: m_a = v; break;
	case 0x13: m_b = v; break;
	case 0x19: m_d = v; break;
	case 0x1c: m_p = v; break;
	default:   write_reg_other(r, v); break;
	}
}

void CMB86233::write_reg_other(UINT32 r, UINT32 v)
{
	r &= 0x3f;
	if(r >= 0x20 && r < 0x30) {
		m_bus->WriteRF(UINT8(r & 0x1f), v);
		return;
	}
	switch(r) {
	case 0x00: m_b0 = v; break;
	case 0x01: m_b1 = v; break;
	case 0x02: m_x0 = v; break;
	case 0x03: m_x1 = v; break;

	case 0x05: m_i0 = v; break;
	case 0x06: m_i1 = v; break;

	case 0x08: m_sp = v; break;

	case 0x0a: m_vsm = v & 7; m_vsmr = (8 << m_vsm) - 1; break;

	case 0x0c:
		m_c0 = v;
		if(m_c0 == 1)
			m_st |= F_ZC0;
		else
			m_st &= ~F_ZC0;
		break;

	case 0x0d:
		m_c1 = v;
		if(m_c1 == 1)
			m_st |= F_ZC1;
		else
			m_st &= ~F_ZC1;
		break;

	case 0x0f: break;

	case 0x10: m_a = v; break;
	case 0x11: m_a = set_exp(m_a, v); break;
	case 0x12: m_a = set_mant(m_a, v); break;
	case 0x13: m_b = v; break;
	case 0x14: m_b = set_exp(m_b, v); break;
	case 0x15: m_b = set_mant(m_b, v); break;
		/* c */
	case 0x19: m_d = v; break;
	case 0x1a: m_d = set_exp(m_d, v); break;
	case 0x1b: m_d = set_mant(m_d, v); break;
	case 0x1c: m_p = v; break;
	case 0x1d: m_p = set_exp(m_p, v); break;
	case 0x1e: m_p = set_mant(m_p, v); break;
	case 0x1f: m_sft = v; break;

	case 0x34: m_rpc = v; break;
	case 0x3c: m_mask = v; break;

	default:
		DebugLog("unimplemented write_reg(%02x, %08x) (%x)\n", r, v, m_ppc);
		break;
	}
}

MB86233_INLINE void CMB86233::write_mem_internal_1(UINT32 r, UINT32 v, bool bank)
{
	UINT16 ea = ea_pre_1(r);
	if(bank)
		ea += 0x200;
	data_w(ea, v);
	ea_post_1(r);
}

MB86233_INLINE void CMB86233::write_mem_io_1(UINT32 r, UINT32 v)
{
	UINT16 ea = ea_pre_1(r);
	io_w(ea, v);
	ea_post_1(r);
}

int CMB86233::Run(int cycles)
{
	// Supermodel: replaces MAME's execute_run() plus the scheduler's icount
	// bookkeeping. Returns cycles requested - cycles left - cycles stolen.
	m_icount = cycles;
	m_cyclesRequested = cycles;
	m_stolen = 0;
	if (m_halt) {
		// halted (INPUT_LINE_HALT): no execution, the whole slice elapses
		m_icount = 0;
		return cycles;
	}

	// (the program ROM doesn't change during a slice)
	const UINT32 *rom = m_rom;
	const unsigned romWords = m_romWords;

	while(m_icount > 0) {
		m_ppc = m_pc;
		if (m_trace)
			m_trace(m_ppc);   // debugger_instruction_hook
		UINT32 opcode = m_pc < romWords ? rom[m_pc] : m_bus->ReadProgram(m_pc);
		m_pc++;

		switch((opcode >> 26) & 0x3f) {
		case 0x00: {
			// lab
			UINT32 r1 = opcode & 0x1ff;
			UINT32 r2 = (opcode >> 9) & 0x1ff;
			UINT32 alu = (opcode >> 21) & 0x1f;
			UINT32 op = (opcode >> 18) & 0x7;

			if(alu)
				alu_pre(alu);

			switch(op) {
			case 0: case 1: {
				// lab mem, mem (e)

				UINT32 ea1 = ea_pre_0(r1);
				UINT32 v1 = data_r(ea1);
				if(m_stall) goto do_stall;

				UINT32 ea2 = ea_pre_1(r2);
				UINT32 v2 = io_r(ea2);
				if(m_stall) goto do_stall;

				ea_post_0(r1);
				ea_post_1(r2);

				m_a = v1;
				m_b = v2;
				break;
			}

			case 3: {
				// lab mem, mem + 0x200

				UINT32 ea1 = ea_pre_0(r1);
				UINT32 v1 = data_r(ea1);
				if(m_stall) goto do_stall;

				UINT32 ea2 = ea_pre_1(r2) + 0x200;
				UINT32 v2 = data_r(ea2);
				if(m_stall) goto do_stall;

				ea_post_0(r1);
				ea_post_1(r2);

				m_a = v1;
				m_b = v2;
				break;
			}

			case 4: {
				// lab mem + 0x200, mem

				UINT32 ea1 = ea_pre_0(r1) + 0x200;
				UINT32 v1 = data_r(ea1);
				if(m_stall) goto do_stall;

				UINT32 ea2 = ea_pre_1(r2);
				UINT32 v2 = data_r(ea2);
				if(m_stall) goto do_stall;

				ea_post_0(r1);
				ea_post_1(r2);

				m_a = v1;
				m_b = v2;
				break;
			}

			default:
				DebugLog("unhandled lab subop %x\n", op);
				DebugLog("%x\n", m_ppc);
				break;

			}

			alu_post_1(alu);
			alu_post_2(alu);
			break;
		}


		case 0x07: {
			// ld / mov
			UINT32 r1 = opcode & 0x1ff;
			UINT32 r2 = (opcode >> 9) & 0x1ff;
			UINT32 alu = (opcode >> 21) & 0x1f;
			UINT32 op = (opcode >> 18) & 0x7;

			if(alu)
				alu_pre(alu);

			// Supermodel: subop 7 is split by r2 >> 6 in the same switch (one
			// dispatch instead of two for the most common instructions)
			switch(op == 7 ? 8 + (r2 >> 6) : op) {
			case 0: {
				// mov mem, mem (e)
				UINT32 ea = ea_pre_0(r1);
				UINT32 v = data_r(ea);
				if(m_stall) goto do_stall;
				ea_post_0(r1);
				alu_post_1(alu);
				write_mem_io_1(r2, v);
				break;
			}

			case 1: {
				// mov mem, mem (e)
				UINT32 ea = ea_pre_0(r1);
				UINT32 v = data_r(ea);
				if(m_stall) goto do_stall;
				ea_post_0(r1);
				alu_post_1(alu);
				write_mem_io_1(r2, v);
				break;
			}

			case 2: {
				// mov mem (e), mem
				UINT32 ea = ea_pre_0(r1);
				UINT32 v = io_r(ea);
				if(m_stall) goto do_stall;
				ea_post_0(r1);
				alu_post_1(alu);
				write_mem_internal_1(r2, v, false);
				break;
			}

			case 3: {
				// mov mem, mem + 0x200
				UINT32 ea = ea_pre_0(r1);
				UINT32 v = data_r(ea);
				if(m_stall) goto do_stall;
				ea_post_0(r1);
				alu_post_1(alu);
				write_mem_internal_1(r2, v, true);
				break;
			}

			case 4: {
				// mov mem + 0x200, mem
				UINT32 ea = ea_pre_0(r1) + 0x200;
				UINT32 v = data_r(ea);
				if(m_stall) goto do_stall;
				ea_post_0(r1);
				alu_post_1(alu);
				write_mem_internal_1(r2, v, false);
				break;
			}

			case 5: {
				// mov mem (o), mem
				UINT32 ea = ea_pre_0(r1);
				UINT32 v = prog_r(ea);
				if(m_stall) goto do_stall;
				ea_post_0(r1);
				alu_post_1(alu);
				write_mem_internal_1(r2, v, false);
				break;
			}

			case 8: {
				// mov reg, mem
				UINT32 v = read_reg(r2);
				if(m_stall) goto do_stall;
				alu_post_1(alu);
				write_mem_internal_1(r1, v, false);
				break;
			}

			case 9: {
				// mov reg, mem (e)
				UINT32 v = read_reg(r2);
				if(m_stall) goto do_stall;
				alu_post_1(alu);
				write_mem_io_1(r1, v);
				break;
			}

			case 10: {
				// mov mem + 0x200, reg
				UINT32 ea = ea_pre_1(r1) + 0x200;
				UINT32 v = data_r(ea);
				if(m_stall) goto do_stall;
				ea_post_1(r1);
				alu_post_1(alu);
				write_reg(r2, v);
				break;
			}

			case 11: {
				// mov mem, reg
				UINT32 ea = ea_pre_1(r1);
				UINT32 v = data_r(ea);
				if(m_stall) goto do_stall;
				ea_post_1(r1);
				alu_post_1(alu);
				write_reg(r2, v);
				break;
			}

			case 12: {
				// mov mem (e), reg
				UINT32 ea = ea_pre_1(r1);
				UINT32 v = io_r(ea);
				if(m_stall) goto do_stall;
				ea_post_1(r1);
				alu_post_1(alu);
				write_reg(r2, v);
				break;
			}

			case 13: {
				// mov mem (o), reg
				UINT32 ea = ea_pre_0(r1);
				UINT32 v = prog_r(ea);
				if(m_stall) goto do_stall;
				ea_post_0(r1);
				alu_post_1(alu);
				write_reg(r2, v);
				break;
			}

			case 14: {
				// mov reg, reg
				UINT32 v = read_reg(r1);
				if(m_stall) goto do_stall;
				alu_post_1(alu);
				write_reg(r2, v);
				break;
			}

			case 15:
				alu_post_1(alu);
				DebugLog("unhandled ld/mov subop 7/%x (%x)\n", r2 >> 6, m_ppc);
				break;

			default:
				alu_post_1(alu);
				DebugLog("unhandled ld/mov subop %x (%x)\n", op, m_ppc);
				break;
			}

			// For floating point ops, registers are updated after transfer
			alu_post_2(alu);
			break;
		}

		case 0x0d: {
			// stm/clm
			UINT32 sub2 = (opcode >> 17) & 7;

			// Theorically has restricted alu too

			switch(sub2) {
			case 5:
				// stmh
				// bit 0 = floating point
				// bit 1-2 = rounding mode
				m_m = opcode;
				break;

			default:
				DebugLog("unimplemented opcode 0d/%x (%x)\n", sub2, m_ppc);
				break;
			}
			break;
		}

		case 0x0e: {
			// lipl / lia / lib / lid
			switch((opcode >> 24) & 0x3) {
			case 0:
				m_p = UINT32((UINT64(m_p) & 0xffffff000000ULL) | (opcode & 0xffffff));   // as MAME: m_p is 32 bits, so this keeps bits 24-31
				break;
			case 1:
				m_a = sext(opcode, 24);
				break;
			case 2:
				m_b = sext(opcode, 24);
				break;
			case 3:
				m_d = sext(opcode, 24);
				break;
			}
			break;
		}

		case 0x0f: {
			// rep/clr0/clr1/set
			UINT32 alu = (opcode >> 20) & 0x1f;
			UINT32 sub2 = (opcode >> 17) & 7;

			if(alu)
				alu_pre(alu);

			switch(sub2) {
			case 0:
				// clr0
				if(opcode & 0x0004) m_a = 0;
				if(opcode & 0x0008) m_b = 0;
				if(opcode & 0x0010) m_d = 0;
				break;

			case 1:
				// clr1 - flags mapping unknown
				break;

			case 2: {
				// rep
				UINT8 r = opcode & 0x8000 ? read_reg(opcode) : opcode;
				if(m_stall) goto do_stall;
				m_r = r;
				goto rep_start;
			}

			case 3:
				// set - flags mapping unknown
				// 0800 = enable interrupt flag
				break;

			default:
				DebugLog("unimplemented opcode 0f/%x (%x)\n", sub2, m_ppc);
				break;
			}

			alu_post_1(alu);
			break;
		}

		case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17:
		case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e: case 0x1f: {
			// ldi
			write_reg(opcode >> 24, sext(opcode, 24));
			break;
		}

		case 0x2f: case 0x3f: {
			// Conditional branch of every kind
			UINT32 cond = ( opcode >> 20 ) & 0x1f;
			UINT32 subtype = ( opcode >> 17 ) & 7;
			UINT32 data = opcode & 0xffff;
			bool invert = opcode & 0x40000000;

			bool cond_passed = false;

			switch(cond) {
			case 0x00: // zrd - d zero
				cond_passed = m_st & F_ZRD;
				break;

			case 0x01: // ged - d >= 0
				cond_passed = !(m_st & F_SGD);
				break;

			case 0x02: // led - d <= 0
				cond_passed = m_st & (F_ZRD | F_SGD);
				break;

			case 0x0a: // gpio0
				cond_passed = m_gpio0;
				break;

			case 0x0b: // gpio1
				cond_passed = m_gpio1;
				break;

			case 0x0c: // gpio2
				cond_passed = m_gpio2;
				break;

			case 0x10: // zc0 - c0 == 1
				cond_passed = !(m_st & F_ZC0);
				break;

			case 0x11: // zc1 - c1 == 1
				cond_passed = !(m_st & F_ZC1);
				break;

			case 0x12: // gpio3
				cond_passed = m_gpio3;
				break;

			case 0x16: // alw - always
				cond_passed = true;
				break;

			default:
				DebugLog("unimplemented condition %x (%x)\n", cond, m_ppc);
				break;
			}
			if(invert)
				cond_passed = !cond_passed;

			if(cond_passed) {
				switch(subtype) {
				case 0: // brif #adr
					m_pc = data;
					break;

				case 1: // brul
					if(opcode & 0x4000) {
						// brul reg
						UINT32 v = read_reg(opcode);
						if(m_stall) goto do_stall;
						m_pc = v;
					} else {
						// brul adr
						UINT32 ea = ea_pre_0(opcode);
						UINT32 v = data_r(ea);
						if(m_stall) goto do_stall;
						ea_post_0(opcode);
						m_pc = v;
					}
					break;

				case 2: // bsif #adr
					pcs_push();
					m_pc = data;
					break;

				case 3: // bsul
					if(opcode & 0x4000) {
						// bsul reg
						UINT32 v = read_reg(opcode);
						if(m_stall) goto do_stall;
						pcs_push();
						m_pc = v;
					} else {
						// bsul adr
						UINT32 ea = ea_pre_0(opcode);
						UINT32 v = data_r(ea);
						if(m_stall) goto do_stall;
						ea_post_0(opcode);
						pcs_push();
						m_pc = v;
					}
					break;

				case 5: // rtif #adr
					pcs_pop();
					break;

				case 6: { // ldif adr, rn
					UINT32 ea = ea_pre_0(opcode);
					UINT32 v = data_r(ea);
					if(m_stall) goto do_stall;
					ea_post_0(opcode);
					write_reg(opcode >> 9, v);
					break;
				}

				default:
					DebugLog("unimplemented branch subtype %x (%x)\n", subtype, m_ppc);
					break;
				}
			}

			// Supermodel fix: the loop counter only counts down when the branch
			// is taken (MAME also did it for an untaken !zc0/!zc1, which sends
			// the 315-5571 geometrizer's triangle dispatch into the wrong
			// handler). The TGP programs only use plain zc0/zc1, where taken
			// and c != 1 are the same thing.
			if(subtype < 2 && cond_passed)
				switch(cond) {
				case 0x10:
					if(m_c0 != 1) {
						m_c0 --;
						if(m_c0 == 1)
							m_st |= F_ZC0;
					}
					break;

				case 0x11:
					if(m_c1 != 1) {
						m_c1 --;
						if(m_c1 == 1)
							m_st |= F_ZC1;
					}
				break;
				}

			break;
		}

		default:
			DebugLog("unimplemented opcode type %02x (%x)\n", (opcode >> 26) & 0x3f, m_ppc);
			break;
		}

		if(m_r != 1) {
			m_pc = m_ppc;
			m_r --;
		}

	rep_start:
		if(0) {
		do_stall:
			m_pc = m_ppc;
			m_stall = false;
			// Supermodel: in MAME the stalling fifo also requests a
			// machine-wide sync, which aborts this timeslice (from within the
			// bus access, i.e. before the decrement below). Do it here.
			AbortTimeslice();
		}
		m_icount--;
	}

	int executed = cycles - m_icount - m_stolen;
	m_icount = 0;
	m_stolen = 0;
	return executed;
}


/******************************************************************************
 Supermodel: debugging and save state interface
******************************************************************************/

static const char *const s_regNames[CMB86233::REG_COUNT] = {
  "A", "B", "D", "P",
  "R", "C0", "C1",
  "B0", "B1", "X0", "X1", "I0", "I1",
  "SFT", "VSM", "MASK", "M",
  "PCS0", "PCS1", "PCS2", "PCS3",
  "SP",
  "PC", "PPC", "ST", "RPC", "VSMR"
};

const char *CMB86233::GetRegName(int id)
{
  return (id >= 0 && id < REG_COUNT) ? s_regNames[id] : "?";
}

UINT32 CMB86233::GetReg(int id) const
{
  switch (id)
  {
  case REG_A:    return m_a;
  case REG_B:    return m_b;
  case REG_D:    return m_d;
  case REG_P:    return m_p;
  case REG_R:    return m_r;
  case REG_C0:   return m_c0;
  case REG_C1:   return m_c1;
  case REG_B0:   return m_b0;
  case REG_B1:   return m_b1;
  case REG_X0:   return m_x0;
  case REG_X1:   return m_x1;
  case REG_I0:   return m_i0;
  case REG_I1:   return m_i1;
  case REG_SFT:  return m_sft;
  case REG_VSM:  return m_vsm;
  case REG_MASK: return m_mask;
  case REG_M:    return m_m;
  case REG_PCS0: return m_pcs[0];
  case REG_PCS1: return m_pcs[1];
  case REG_PCS2: return m_pcs[2];
  case REG_PCS3: return m_pcs[3];
  case REG_SP:   return m_sp;
  case REG_PC:   return m_pc;
  case REG_PPC:  return m_ppc;
  case REG_ST:   return m_st;
  case REG_RPC:  return m_rpc;
  case REG_VSMR: return m_vsmr;
  default:       return 0;
  }
}

// Raw register writes, like MAME's debugger state_add entries (no side effects,
// e.g. setting VSM does not update VSMR, C0/C1 do not update ST).
void CMB86233::SetReg(int id, UINT32 v)
{
  switch (id)
  {
  case REG_A:    m_a = v; break;
  case REG_B:    m_b = v; break;
  case REG_D:    m_d = v; break;
  case REG_P:    m_p = v; break;
  case REG_R:    m_r = UINT8(v); break;
  case REG_C0:   m_c0 = UINT8(v); break;
  case REG_C1:   m_c1 = UINT8(v); break;
  case REG_B0:   m_b0 = UINT16(v); break;
  case REG_B1:   m_b1 = UINT16(v); break;
  case REG_X0:   m_x0 = UINT16(v); break;
  case REG_X1:   m_x1 = UINT16(v); break;
  case REG_I0:   m_i0 = UINT16(v); break;
  case REG_I1:   m_i1 = UINT16(v); break;
  case REG_SFT:  m_sft = UINT8(v); break;
  case REG_VSM:  m_vsm = UINT8(v); break;
  case REG_MASK: m_mask = UINT16(v); break;
  case REG_M:    m_m = UINT16(v); break;
  case REG_PCS0: m_pcs[0] = UINT16(v); break;
  case REG_PCS1: m_pcs[1] = UINT16(v); break;
  case REG_PCS2: m_pcs[2] = UINT16(v); break;
  case REG_PCS3: m_pcs[3] = UINT16(v); break;
  case REG_SP:   m_sp = UINT16(v); break;
  case REG_PC:   m_pc = UINT16(v); break;
  case REG_PPC:  m_ppc = UINT16(v); break;
  case REG_ST:   m_st = v; break;
  case REG_RPC:  m_rpc = UINT8(v); break;
  case REG_VSMR: m_vsmr = UINT16(v); break;
  default:       break;
  }
}

std::string CMB86233::Disassemble(UINT32 opcode)
{
  return CMB86233Disassembler::Disassemble(opcode);
}

std::string CMB86233::Disassemble(UINT16 pc)
{
  return CMB86233Disassembler::Disassemble(prog_r(pc));
}

void CMB86233::SaveState(CBlockFile *StateFile, const char *name)
{
  StateFile->NewBlock(name, __FILE__);
  StateFile->Write(m_ppc);
  StateFile->Write(m_pc);
  StateFile->Write(m_st);
  StateFile->Write(m_sp);
  StateFile->Write(m_a);
  StateFile->Write(m_b);
  StateFile->Write(m_d);
  StateFile->Write(m_p);
  StateFile->Write(m_r);
  StateFile->Write(m_rpc);
  StateFile->Write(m_c0);
  StateFile->Write(m_c1);
  StateFile->Write(m_b0);
  StateFile->Write(m_b1);
  StateFile->Write(m_x0);
  StateFile->Write(m_x1);
  StateFile->Write(m_i0);
  StateFile->Write(m_i1);
  StateFile->Write(m_sft);
  StateFile->Write(m_vsm);
  StateFile->Write(m_vsmr);
  StateFile->Write(m_pcs);
  StateFile->Write(m_mask);
  StateFile->Write(m_m);
  StateFile->Write(m_gpio0);
  StateFile->Write(m_gpio1);
  StateFile->Write(m_gpio2);
  StateFile->Write(m_gpio3);
  StateFile->Write(m_alu_stmask);
  StateFile->Write(m_alu_stset);
  StateFile->Write(m_alu_r1);
  StateFile->Write(m_alu_r2);
  // Supermodel additions: halt line and on-chip RAM (saved by the driver in MAME)
  StateFile->Write(m_halt);
  StateFile->Write(m_ram);
}

void CMB86233::LoadState(CBlockFile *StateFile, const char *name)
{
  if (Result::OKAY != StateFile->FindBlock(name))
  {
    ErrorLog("Unable to load MB86233 state. Save state file is corrupt.");
    return;
  }
  StateFile->Read(m_ppc);
  StateFile->Read(m_pc);
  StateFile->Read(m_st);
  StateFile->Read(m_sp);
  StateFile->Read(m_a);
  StateFile->Read(m_b);
  StateFile->Read(m_d);
  StateFile->Read(m_p);
  StateFile->Read(m_r);
  StateFile->Read(m_rpc);
  StateFile->Read(m_c0);
  StateFile->Read(m_c1);
  StateFile->Read(m_b0);
  StateFile->Read(m_b1);
  StateFile->Read(m_x0);
  StateFile->Read(m_x1);
  StateFile->Read(m_i0);
  StateFile->Read(m_i1);
  StateFile->Read(m_sft);
  StateFile->Read(m_vsm);
  StateFile->Read(m_vsmr);
  StateFile->Read(m_pcs);
  StateFile->Read(m_mask);
  StateFile->Read(m_m);
  StateFile->Read(m_gpio0);
  StateFile->Read(m_gpio1);
  StateFile->Read(m_gpio2);
  StateFile->Read(m_gpio3);
  StateFile->Read(m_alu_stmask);
  StateFile->Read(m_alu_stset);
  StateFile->Read(m_alu_r1);
  StateFile->Read(m_alu_r2);
  StateFile->Read(m_halt);
  StateFile->Read(m_ram);
  m_stall = false;
}
