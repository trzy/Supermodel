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
 * I960.cpp
 *
 * Based on original work by Farfetch'd and R. Belmont in MAME. Portions
 * Copyright (c) Farfetch'd and R. Belmont, used under the BSD 3-Clause
 * License: see Docs/README.txt.
 *
 * Ported to Supermodel from MAME (src/devices/cpu/i960/i960.cpp). The
 * instruction code is MAME's; changes are marked "Supermodel:".
 */

#include "I960.h"
#include "I960Disasm.h"
#include "BlockFile.h"
#include "OSD/Logger.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <limits>
#include <stdexcept>

#ifdef _MSC_VER
/* logb prototype is different for MS Visual C */
#include <cfloat>
#define logb _logb
#endif


/******************************************************************************
 Supermodel: stand-ins for the MAME helpers the core uses
******************************************************************************/

namespace
{
  // Supermodel: out of range conversions give the largest positive or negative
  // value (80960KB manual, cvtri); a C++ cast is undefined there
  template <class T> T SaturateToInt(double v)
  {
    if (std::isnan(v))
      return std::numeric_limits<T>::min();
    if (v >= (double) std::numeric_limits<T>::max())
      return std::numeric_limits<T>::max();
    if (v <= (double) std::numeric_limits<T>::min())
      return std::numeric_limits<T>::min();
    return (T) v;
  }

  struct I960FatalError : public std::runtime_error
  {
    explicit I960FatalError(const char *msg) : std::runtime_error(msg) {}
  };

  inline float    u2f(uint32_t v) { float f; memcpy(&f, &v, sizeof(f)); return f; }
  inline uint32_t f2u(float f)    { uint32_t v; memcpy(&v, &f, sizeof(v)); return v; }
  inline double   u2d(uint64_t v) { double d; memcpy(&d, &v, sizeof(d)); return d; }
  inline uint64_t d2u(double d)   { uint64_t v; memcpy(&v, &d, sizeof(v)); return v; }
  inline int32_t  sext(uint32_t value, int width) { return int32_t(value << (32 - width)) >> (32 - width); }
  inline uint32_t rotl32(uint32_t v, unsigned n) { n &= 31; return n ? (v << n) | (v >> (32 - n)) : v; }   // std::rotl (C++20)
  inline uint64_t mulu_32x32(uint32_t a, uint32_t b) { return uint64_t(a) * uint64_t(b); }
  inline bool     DWORD_ALIGNED_(uint32_t a) { return (a & 3) == 0; }
  inline bool     WORD_ALIGNED_(uint32_t a)  { return (a & 1) == 0; }
}

// MAME's fatalerror() throws; here it logs and throws an exception caught by Run()
#define fatalerror(...) FatalError(__VA_ARGS__)

void CI960::FatalError(const char *fmt, ...)
{
  char buf[512];
  va_list vl;
  va_start(vl, fmt);
  vsnprintf(buf, sizeof(buf), fmt, vl);
  va_end(vl);
  ErrorLog("i960: %s", buf);
  throw I960FatalError(buf);
}

uint32_t CI960::Cache::read_dword(uint32_t a)
{
  for (int i = 0; i < cpu->m_numFetch; i++)
  {
    const FetchRegion &r = cpu->m_fetch[i];
    uint32_t off = a - r.base;
    if (off < r.size && r.size - off >= 4)
    {
      const UINT8 *p = r.ptr + off;
      return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
    }
  }
  cpu->m_fetching = true;
  uint32_t v = cpu->m_program.read_dword(a);
  cpu->m_fetching = false;
  return v;
}


/******************************************************************************
 MAME's i960 core
******************************************************************************/

uint32_t CI960::i960_read_dword_unaligned(uint32_t address)
{
	if (!DWORD_ALIGNED_(address))
		return m_program.read_byte(address) | m_program.read_byte(address+1)<<8 | m_program.read_byte(address+2)<<16 | m_program.read_byte(address+3)<<24;
	else
		return m_program.read_dword(address);
}

std::pair<uint32_t, uint16_t> CI960::i960_read_dword_unaligned_flags(uint32_t address)
{
	if (!DWORD_ALIGNED_(address)) {
		auto v = m_program.read_byte_flags(address);
		return std::pair<uint32_t, uint16_t>(v.first | m_program.read_byte(address+1)<<8 | m_program.read_byte(address+2)<<16 | m_program.read_byte(address+3)<<24, v.second);
	} else
		return m_program.read_dword_flags(address);
}

uint16_t CI960::i960_read_word_unaligned(uint32_t address)
{
	if (!WORD_ALIGNED_(address))
		return m_program.read_byte(address) | m_program.read_byte(address+1)<<8;
	else
		return m_program.read_word(address);
}

void CI960::i960_write_dword_unaligned(uint32_t address, uint32_t data)
{
	if (!DWORD_ALIGNED_(address))
	{
		m_program.write_byte(address, data & 0xff);
		m_program.write_byte(address+1, (data>>8)&0xff);
		m_program.write_byte(address+2, (data>>16)&0xff);
		m_program.write_byte(address+3, (data>>24)&0xff);
	}
	else
	{
		m_program.write_dword(address, data);
	}
}

uint16_t CI960::i960_write_dword_unaligned_flags(uint32_t address, uint32_t data)
{
	if (!DWORD_ALIGNED_(address))
	{
		uint16_t flags = m_program.write_byte_flags(address, data & 0xff);
		m_program.write_byte(address+1, (data>>8)&0xff);
		m_program.write_byte(address+2, (data>>16)&0xff);
		m_program.write_byte(address+3, (data>>24)&0xff);
		return flags;
	}
	else
	{
		return m_program.write_dword_flags(address, data);
	}
}

void CI960::i960_write_word_unaligned(uint32_t address, uint16_t data)
{
	if (!WORD_ALIGNED_(address))
	{
		m_program.write_byte(address, data & 0xff);
		m_program.write_byte(address+1, (data>>8)&0xff);
	}
	else
	{
		m_program.write_word(address, data);
	}
}

void CI960::send_iac(uint32_t adr)
{
	uint32_t iac[4];
	iac[0] = m_program.read_dword(adr);
	iac[1] = m_program.read_dword(adr+4);
	iac[2] = m_program.read_dword(adr+8);
	iac[3] = m_program.read_dword(adr+12);

	switch(iac[0]>>24) {
	case 0x40:  // generate irq
		DebugLog("I960: %x: IAC %08x %08x %08x %08x (generate IRQ)\n", m_PIP, iac[0], iac[1], iac[2], iac[3]);
		break;
	case 0x41:  // test for pending interrupts
		DebugLog("I960: %x: IAC %08x %08x %08x %08x (test for pending interrupts)\n", m_PIP, iac[0], iac[1], iac[2], iac[3]);
		check_pending_irqs();
		break;
	case 0x80:  // store SAT & PRCB in memory
		m_program.write_dword(iac[1], m_SAT);
		m_program.write_dword(iac[1]+4, m_PRCB);
		break;
	case 0x89:  // invalidate internal instruction cache
		DebugLog("I960: %x: IAC %08x %08x %08x %08x (invalidate internal instruction cache)\n", m_PIP, iac[0], iac[1], iac[2], iac[3]);
		// we do not emulate the instruction cache, so this is safe to ignore
		break;
	case 0x8f:  // enable/disable breakpoints
		DebugLog("I960: %x: IAC %08x %08x %08x %08x (enable/disable breakpoints)\n", m_PIP, iac[0], iac[1], iac[2], iac[3]);
		// processor breakpoints are not emulated, safe to ignore
		break;
	case 0x91:  // stop processor
		DebugLog("I960: %x: IAC %08x %08x %08x %08x (stop processor)\n", m_PIP, iac[0], iac[1], iac[2], iac[3]);
		break;
	case 0x92:  // continue initialization
		DebugLog("I960: %x: IAC %08x %08x %08x %08x (continue initialization)\n", m_PIP, iac[0], iac[1], iac[2], iac[3]);
		break;
	case 0x93: // reinit
		m_SAT  = iac[1];
		m_PRCB = iac[2];
		m_IP   = iac[3];
		break;
	default:
		fatalerror("I960: %x: IAC %08x %08x %08x %08x\n", m_PIP, iac[0], iac[1], iac[2], iac[3]);
	}
}

uint32_t CI960::get_ea(uint32_t opcode)
{
	int abase = (opcode >> 14) & 0x1f;
	if(!(opcode & 0x00001000)) { // MEMA
		uint32_t offset = opcode & 0x1fff;
		if(!(opcode & 0x2000))
			return offset;
		else
			return m_r[abase]+offset;
	} else {                     // MEMB
		int index = opcode & 0x1f;
		int scale = (opcode >> 7) & 0x7;
		int mode  = (opcode >> 10) & 0xf;
		uint32_t ret;

		switch(mode) {
		case 0x4:
			return m_r[abase];

		case 0x5:   // address of this instruction + the offset dword + 8
			// which in reality is "address of next instruction + the offset dword"
			ret = m_cache.read_dword(m_IP);
			m_IP += 4;
			ret += m_IP;
			return ret;

		case 0x7:
			return m_r[abase] + (m_r[index] << scale);

		case 0xc:
			ret = m_cache.read_dword(m_IP);
			m_IP += 4;
			return ret;

		case 0xd:
			ret = m_cache.read_dword(m_IP) + m_r[abase];
			m_IP += 4;
			return ret;

		case 0xe:
			ret = m_cache.read_dword(m_IP) + (m_r[index] << scale);
			m_IP += 4;
			return ret;

		case 0xf:
			ret = m_cache.read_dword(m_IP) + m_r[abase] + (m_r[index] << scale);
			m_IP += 4;
			return ret;

		default:
			fatalerror("I960: %x: unhandled MEMB mode %x\n", m_PIP, mode);
		}
	}
}

// i960 Extended-real register image: words 0/1 hold the 64-bit fraction with
// an explicit integer bit in bit 63, word 2 holds the sign in bit 15 and the
// 15-bit biased exponent (bias 16383) in bits 14:0, with the upper 16 bits zero.
static void double_to_extended(double val, uint32_t *words)
{
	const uint64_t bits = d2u(val);
	const uint32_t sign = uint32_t(bits >> 63) << 15;
	uint64_t frac;
	uint32_t e;

	if(std::isnan(val) || std::isinf(val))
	{
		frac = (1ULL << 63) | ((bits & 0x000fffffffffffffULL) << 11);
		e = 0x7fff;
	}
	else if(val == 0.0)
	{
		frac = 0;
		e = 0;
	}
	else
	{
		int exp2;
		const double m = frexp(fabs(val), &exp2);   // 0.5 <= m < 1, exact for denormals too
		frac = uint64_t(ldexp(m, 64));              // integer bit lands in bit 63
		e = exp2 - 1 + 16383;
	}

	words[0] = uint32_t(frac);
	words[1] = uint32_t(frac >> 32);
	words[2] = sign | e;
}

static double extended_to_double(const uint32_t *words)
{
	const uint64_t frac = words[0] | (uint64_t(words[1]) << 32);
	const uint32_t e = words[2] & 0x7fff;
	double val;

	if(e == 0x7fff)
	{
		if(!(frac << 1))    // integer bit only: infinity
		{
			val = std::numeric_limits<double>::infinity();
		}
		else                // NaN: keep whatever payload fits, always quiet
		{
			val = u2d(0x7ff8000000000000ULL | ((frac & 0x7fffffffffffffffULL) >> 11));
		}
	}
	else
	{
		// value = fraction * 2^(exponent - bias - 63); e == 0 is a denormal (exponent 1)
		val = ldexp(double(frac), int(e ? e : 1) - 16383 - 63);
	}

	return ((words[2] >> 15) & 1) ? -val : val;
}

uint32_t CI960::get_1_ri(uint32_t opcode)
{
	if(!(opcode & 0x00000800))
		return m_r[opcode & 0x1f];
	else
		return opcode & 0x1f;
}

uint32_t CI960::get_2_ri(uint32_t opcode)
{
	if(!(opcode & 0x00001000))
		return m_r[(opcode>>14) & 0x1f];
	else
		return (opcode>>14) & 0x1f;
}

uint64_t CI960::get_2_ri64(uint32_t opcode)
{
	if(!(opcode & 0x00001000))
		return m_r[(opcode>>14) & 0x1f] | ((uint64_t)m_r[((opcode>>14) & 0x1f)+1]<<32);
	else
		return (opcode>>14) & 0x1f;
}

void CI960::set_ri(uint32_t opcode, uint32_t val)
{
	if(!(opcode & 0x00002000))
		m_r[(opcode>>19) & 0x1f] = val;
	else {
		fatalerror("I960: %x: set_ri on literal?\n", m_PIP);
	}
}

void CI960::set_ri2(uint32_t opcode, uint32_t val, uint32_t val2)
{
	if(!(opcode & 0x00002000))
	{
		m_r[(opcode>>19) & 0x1f] = val;
		m_r[((opcode>>19) & 0x1f)+1] = val2;
	}
	else {
		fatalerror("I960: %x: set_ri2 on literal?\n", m_PIP);
	}
}

void CI960::set_ri64(uint32_t opcode, uint64_t val)
{
	if(!(opcode & 0x00002000)) {
		m_r[(opcode>>19) & 0x1f] = uint32_t(val);
		m_r[((opcode>>19) & 0x1f)+1] = uint32_t(val >> 32);
	} else
		fatalerror("I960: %x: set_ri64 on literal?\n", m_PIP);
}

double CI960::get_1_rif(uint32_t opcode)
{
	if(!(opcode & 0x00000800))
		return u2f(m_r[opcode & 0x1f]);
	else {
		int idx = opcode & 0x1f;
		if(idx < 4)
			return m_fp[idx];
		if(idx == 0x16)
			return 1.0;
		return 0.0;
	}
}

double CI960::get_2_rif(uint32_t opcode)
{
	if(!(opcode & 0x00001000))
		return u2f(m_r[(opcode>>14) & 0x1f]);
	else {
		int idx = (opcode>>14) & 0x1f;
		if(idx < 4)
			return m_fp[idx];
		if(idx == 0x16)
			return 1.0;
		return 0.0;
	}
}

void CI960::set_rif(uint32_t opcode, double val)
{
	if(!(opcode & 0x00002000))
		m_r[(opcode>>19) & 0x1f] = f2u(float(val));
	else if(!(opcode & 0x00e00000))
		m_fp[(opcode>>19) & 3] = val;
	else
		fatalerror("I960: %x: set_rif on literal?\n", m_PIP);
}

double CI960::get_1_rifl(uint32_t opcode)
{
	if(!(opcode & 0x00000800)) {
		uint64_t v = m_r[opcode & 0x1e];
		v |= ((uint64_t)(m_r[(opcode & 0x1e)+1]))<<32;
		return u2d(v);
	} else {
		int idx = opcode & 0x1f;
		if(idx < 4)
			return m_fp[idx];
		if(idx == 0x16)
			return 1.0;
		return 0.0;
	}
}

double CI960::get_2_rifl(uint32_t opcode)
{
	if(!(opcode & 0x00001000)) {
		uint64_t v = m_r[(opcode >> 14) & 0x1e];
		v |= ((uint64_t)(m_r[((opcode>>14) & 0x1e)+1]))<<32;
		return u2d(v);
	} else {
		int idx = (opcode>>14) & 0x1f;
		if(idx < 4)
			return m_fp[idx];
		if(idx == 0x16)
			return 1.0;
		return 0.0;
	}
}

void CI960::set_rifl(uint32_t opcode, double val)
{
	if(!(opcode & 0x00002000)) {
		uint64_t v = d2u(val);
		m_r[(opcode>>19) & 0x1e] = uint32_t(v);
		m_r[((opcode>>19) & 0x1e)+1] = uint32_t(v>>32);
	} else if(!(opcode & 0x00e00000))
		m_fp[(opcode>>19) & 3] = val;
	else
		fatalerror("I960: %x: set_rifl on literal?\n", m_PIP);
}

uint32_t CI960::get_1_ci(uint32_t opcode)
{
	if(!(opcode & 0x00002000))
		return m_r[(opcode >> 19) & 0x1f];
	else
		return (opcode >> 19) & 0x1f;
}

uint32_t CI960::get_2_ci(uint32_t opcode)
{
	return m_r[(opcode >> 14) & 0x1f];
}

uint32_t CI960::get_disp(uint32_t opcode)
{
	return sext(opcode, 24) - 4;
}

uint32_t CI960::get_disp_s(uint32_t opcode)
{
	return sext(opcode, 13) - 4;
}

void CI960::cmp_s(int32_t v1, int32_t v2)
{
	m_AC &= ~7;
	if(v1<v2)
		m_AC |= 4;
	else if(v1 == v2)
		m_AC |= 2;
	else
		m_AC |= 1;
}

void CI960::cmp_u(uint32_t v1, uint32_t v2)
{
	m_AC &= ~7;
	if(v1<v2)
		m_AC |= 4;
	else if(v1 == v2)
		m_AC |= 2;
	else
		m_AC |= 1;
}

void CI960::concmp_s(int32_t v1, int32_t v2)
{
	m_AC &= ~7;
	if(v1 <= v2)
		m_AC |= 2;
	else
		m_AC |= 1;
}

void CI960::concmp_u(uint32_t v1, uint32_t v2)
{
	m_AC &= ~7;
	if(v1 <= v2)
		m_AC |= 2;
	else
		m_AC |= 1;
}

void CI960::cmp_d(double v1, double v2)
{
	m_AC &= ~7;
	if(v1<v2)
		m_AC |= 4;
	else if(v1 == v2)
		m_AC |= 2;
	else if(v1 > v2)
		m_AC |= 1;
}

void CI960::bxx(uint32_t opcode, int mask)
{
	if(m_AC & mask) {
		m_IP += get_disp(opcode);
		m_IP &= ~3;
	}
}

void CI960::fxx(uint32_t opcode, int mask)
{
	if(m_AC & mask) {
		fatalerror("Taking the fault on a FAULT insn not yet supported\n");
	}
}

void CI960::bxx_s(uint32_t opcode, int mask)
{
	if(m_AC & mask) {
		m_IP += get_disp_s(opcode);
		m_IP &= ~3;
	}
}

void CI960::test(uint32_t opcode, int mask)
{
	if(m_AC & mask)
		m_r[(opcode>>19) & 0x1f] = 1;
	else
		m_r[(opcode>>19) & 0x1f] = 0;
}

double CI960::round_to_int(double val)
{
	// apply rounding mode
	switch ((m_AC >> 30) & 3)
	{
	case 0: return nearbyint(val);   // Supermodel: nearest even (MAME: round(), halves away from zero)
	case 1: return floor(val);
	case 2: return ceil(val);
	default: return trunc(val);
	}
}


// interrupt dispatch
void CI960::take_interrupt(int vector, int lvl)
{
	int int_tab =  m_program.read_dword(m_PRCB+20);    // interrupt table
	int int_SP  =  m_program.read_dword(m_PRCB+24);    // interrupt stack
	int SP;
	uint32_t IRQV;

	IRQV = m_program.read_dword(int_tab + 36 + (vector-8)*4);

	// start the process
	if(!(m_PC & 0x2000))    // if this is a nested interrupt, don't re-get int_SP
	{
		SP = int_SP;
	}
	else
	{
		SP = m_r[I960_SP];
	}

	SP = (SP + 63) & ~63;
	SP += 64;   // add padding to prevent buffer underflow when saving processor state

	do_call(IRQV, 7, SP);

	// save the processor state
	m_program.write_dword(m_r[I960_FP]-16, m_PC);
	m_program.write_dword(m_r[I960_FP]-12, m_AC);
	// store the vector
	m_program.write_dword(m_r[I960_FP]-8, vector-8);

	m_PC &= ~0x001f0401;    // clear priority (bits 16-20), trace-fault pending (bit 10) and trace enable (bit 0)
	m_PC |= (lvl<<16);      // set CPU level to current IRQ level
	m_PC |= 0x2002;         // set supervisor mode & interrupt flag
}

void CI960::check_immediate_irqs()
{
	int cpu_pri = (m_PC >> 16) & 0x1f;

	// Supermodel: a held line (see execute_set_input) is latched once nothing
	// else is and the CPU's priority allows it
	if (!m_immediate_irq)
	{
		for (int line = 3; line >= 0; line--)
		{
			if (m_irq_line_state[line] != IRQ_HELD)
				continue;
			int vector = (m_ICR >> (8 * line)) & 0xff;
			int priority = vector / 8;
			if (vector && (!m_immediate_irq || priority > m_immediate_pri) && ((cpu_pri < priority) || (priority == 31)))
			{
				m_immediate_irq = 1;
				m_immediate_vector = vector;
				m_immediate_pri = priority;
			}
		}
		if (m_immediate_irq)
		{
			for (int line = 0; line < 4; line++)
			{
				if (m_irq_line_state[line] == IRQ_HELD && ((m_ICR >> (8 * line)) & 0xff) == (uint32_t) m_immediate_vector)
					m_irq_line_state[line] = ASSERT_LINE;
			}
		}
	}

	if ((m_immediate_irq) && ((cpu_pri < m_immediate_pri) || (m_immediate_pri == 31)))
	{
		take_interrupt(m_immediate_vector, m_immediate_pri);
		m_immediate_irq = 0;
	}
}

void CI960::check_pending_irqs()
{
	int int_tab = m_program.read_dword(m_PRCB + 20);    // interrupt table
	int cpu_pri = (m_PC >> 16) & 0x1f;
	int pending_pri = m_program.read_dword(int_tab);    // read pending priorities
	int take = -1;
	static const uint32_t lvlmask[4] = { 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000 };

	for (int lvl = 31; lvl >= 0; lvl--) {
		if ((pending_pri & (1u << lvl)) && ((cpu_pri < lvl) || (lvl == 31))) {
			int word, wordl, wordh;

			// figure out which word contains this level's priorities
			word = ((lvl / 4) * 4) + 4; // (lvl/4) = word address, *4 for byte address, +4 to skip pending priorities
			wordl = (lvl % 4) * 8;
			wordh = (wordl + 8) - 1;

			int vword = m_program.read_dword(int_tab + word);

			// take the first vector we find for this level
			for (int irq = wordh; irq >= wordl; irq--) {
				if (vword & (1u << irq)) {
					// clear pending bit
					vword &= ~(1u << irq);
					m_program.write_dword(int_tab + word, vword);
					take = irq;
					break;
				}
			}

			// if no vectors were found at our level, it's an error
			if (take == -1) {
				DebugLog("i960: ERROR! no vector found for pending level %d\n", lvl);

				// try to recover...
				pending_pri &= ~(1u << lvl);
				m_program.write_dword(int_tab, pending_pri);
				return;
			}

			// if no vectors are waiting for this level, clear the level bit
			if (!(vword & lvlmask[lvl % 4])) {
				pending_pri &= ~(1u << lvl);
				m_program.write_dword(int_tab, pending_pri);
			}

			take += ((lvl / 4) * 32);

			take_interrupt(take, lvl);
			return;
		}
	}
}

void CI960::do_call(uint32_t adr, int type, uint32_t stack)
{
	int i;
	uint32_t FP;

	// call and callx take 9 cycles base
	m_icount -= 9;

	// set the new RIP
	m_r[I960_RIP] = m_IP;
//  osd_printf_debug("CALL (type %d): FP %x, %x => %x, stack %x, rcache_pos %d\n", type, m_r[I960_FP], m_r[I960_RIP], adr, stack, m_rcache_pos);

	// are we out of cache entries?
	if (m_rcache_pos >= I960_RCACHE_SIZE) {
		// flush the current register set to the current frame
		FP = m_r[I960_FP] & ~0x3f;
		for (i = 0; i < 16; i++) {
			m_program.write_dword(FP + (i*4), m_r[i]);
		}
	}
	else    // a cache entry is available, use it
	{
		memcpy(&m_rcache[m_rcache_pos][0], m_r, 0x10 * sizeof(uint32_t));
		m_rcache_frame_addr[m_rcache_pos] = m_r[I960_FP] & ~0x3f;
	}
	m_rcache_pos++;

	m_IP = adr;
	m_r[I960_PFP] = m_r[I960_FP] & ~7;
	m_r[I960_PFP] |= type;

	if(type == 7) { // interrupts need special handling
		// set the stack to the passed-in value to properly handle nested interrupts
		// (can't set it externally or the original program's SP will be lost)
		m_r[I960_SP] = stack;
	}

	m_r[I960_FP]  = (m_r[I960_SP] + 63) & ~63;
	m_r[I960_SP]  = m_r[I960_FP] + 64;
}

void CI960::do_ret_0()
{
//  int type = m_r[I960_PFP] & 7;

	m_r[I960_FP] = m_r[I960_PFP] & ~0x3f;

	m_rcache_pos--;

	// normal situation: if we're still above rcache size, we're not in cache.
	// abnormal situation (after the app does a FLUSHREG): rcache_pos will be 0
	// coming in, but we must still treat it as a not-in-cache situation.
	if ((m_rcache_pos >= I960_RCACHE_SIZE) || (m_rcache_pos < 0))
	{
		int i;
		for(i=0; i<0x10; i++)
			m_r[i] = m_program.read_dword(m_r[I960_FP]+4*i);

		if (m_rcache_pos < 0)
		{
			m_rcache_pos = 0;
		}
	}
	else
	{
		memcpy(m_r, m_rcache[m_rcache_pos], 0x10*sizeof(uint32_t));
	}

//  osd_printf_debug("RET (type %d): FP %x, %x => %x, rcache_pos %d\n", type, m_r[I960_FP], m_IP, m_r[I960_RIP], m_rcache_pos);
	m_IP = m_r[I960_RIP];
}

void CI960::do_ret()
{
	uint32_t x, y;
	m_icount -= 7;
	switch(m_r[I960_PFP] & 7) {
	case 0:
		do_ret_0();
		break;

	case 7:
		x = m_program.read_dword(m_r[I960_FP]-16);
		y = m_program.read_dword(m_r[I960_FP]-12);
		do_ret_0();
		m_AC = y;
		// #### test supervisor
		m_PC = x;

		// check for another IRQ now that we're back
		check_pending_irqs();
		break;

	default:
		fatalerror("I960: %x: Unsupported return mode %d\n", m_PIP, m_r[I960_PFP] & 7);
	}
}

// if last opcode was a multi dword burst read opcode save the data here
// i.e. Model 2 FIFO reads with ldl, ldt, ldq
void CI960::burst_stall_save(uint32_t t1, uint32_t t2, int index, int size, bool iswriteop)
{
	m_stall_state.t1 = t1;
	m_stall_state.t2 = t2;
	m_stall_state.index = index;
	m_stall_state.size = size;
	m_stall_state.iswriteop = iswriteop;
	m_stall_state.burst_mode = true;
}

// resume from a burst stall opcode
void CI960::execute_burst_stall_op(uint32_t opcode)
{
	int i;
	// in case opcode uses an operand call effective address function to fix IP register
	(void)get_ea(opcode);

	// check if our data is ready
	for(i=m_stall_state.index ; i<m_stall_state.size ;i++)
	{
		// count down 1 icount for every op
		m_icount--;
		if(m_stall_state.iswriteop == true)
			i960_write_dword_unaligned(m_stall_state.t1, m_r[m_stall_state.t2+i]);
		else
			m_r[m_stall_state.t2+i] = i960_read_dword_unaligned(m_stall_state.t1);

		// if the host returned stall just save the index and try again on a later moment
		if(m_stalled == true)
		{
			m_stall_state.index = i;
			return;
		}
	}

	// clear stall burst mode
	m_stall_state.burst_mode = false;
	// now that we are done we might as well check if there's an irq too
	check_immediate_irqs();
}

void CI960::execute_op(uint32_t opcode)
{
	uint32_t t1, t2;
	double t1f, t2f;

	switch(opcode >> 24) {
		case 0x08: // b
			m_icount--;
			m_IP += get_disp(opcode);
			break;

		case 0x09: // call
			do_call(m_IP+get_disp(opcode), 0, m_r[I960_SP]);
			break;

		case 0x0a: // ret
			do_ret();
			break;

		case 0x0b: // bal
			m_icount -= 5;
			m_r[0x1e] = m_IP;
			m_IP += get_disp(opcode);
			break;

		case 0x10: // bno
			m_icount--;
			if(!(m_AC & 7)) {
				m_IP += get_disp(opcode);
			}
			break;

		case 0x11: // bg
			m_icount--;
			bxx(opcode, 1);
			break;

		case 0x12: // be
			m_icount--;
			bxx(opcode, 2);
			break;

		case 0x13: // bge
			m_icount--;
			bxx(opcode, 3);
			break;

		case 0x14: // bl
			m_icount--;
			bxx(opcode, 4);
			break;

		case 0x15: // bne
			m_icount--;
			bxx(opcode, 5);
			break;

		case 0x16: // ble
			m_icount--;
			bxx(opcode, 6);
			break;

		case 0x17: // bo
			m_icount--;
			bxx(opcode, 7);
			break;

		case 0x18: // faultno
			// Supermodel: faults aren't raised, so nothing (MAME branched like bno)
			m_icount--;
			break;

		case 0x19: // faultg
			m_icount--;
			fxx(opcode, 1);
			break;

		case 0x1a: // faulte
			m_icount--;
			fxx(opcode, 2);
			break;

		case 0x1b: // faultge
			m_icount--;
			fxx(opcode, 3);
			break;

		case 0x1c: // faultl
			m_icount--;
			fxx(opcode, 4);
			break;

		case 0x1d: // faultne
			m_icount--;
			fxx(opcode, 5);
			break;

		case 0x1e: // faultle
			m_icount--;
			fxx(opcode, 6);
			break;

		case 0x1f: // faulto
			m_icount--;
			fxx(opcode, 7);
			break;

		case 0x20: // testno
			m_icount--;
			if(!(m_AC & 7))
				m_r[(opcode>>19) & 0x1f] = 1;
			else
				m_r[(opcode>>19) & 0x1f] = 0;
			break;

		case 0x21: // testg
			m_icount--;
			test(opcode, 1);
			break;

		case 0x22: // teste
			m_icount--;
			test(opcode, 2);
			break;

		case 0x23: // testge
			m_icount--;
			test(opcode, 3);
			break;

		case 0x24: // testl
			m_icount--;
			test(opcode, 4);
			break;

		case 0x25: // testne
			m_icount--;
			test(opcode, 5);
			break;

		case 0x26: // testle
			m_icount--;
			test(opcode, 6);
			break;

		case 0x27: // testo
			m_icount--;
			test(opcode, 7);
			break;

		case 0x30: // bbc
			m_icount -= 4;
			t1 = get_1_ci(opcode) & 0x1f;
			t2 = get_2_ci(opcode);
			if(!(t2 & (1u<<t1))) {
				m_AC = (m_AC & ~7) | 2;
				m_IP += get_disp_s(opcode);
			} else
				m_AC &= ~7;
			break;

		case 0x31: // cmp0bg
			m_icount -= 4;
			t1 = get_1_ci(opcode);
			t2 = get_2_ci(opcode);
			cmp_u(t1, t2);
			bxx_s(opcode, 1);
			break;

		case 0x32: // cmpobe
			m_icount -= 4;
			t1 = get_1_ci(opcode);
			t2 = get_2_ci(opcode);
			cmp_u(t1, t2);
			bxx_s(opcode, 2);
			break;

		case 0x33: // cmpobge
			m_icount -= 4;
			t1 = get_1_ci(opcode);
			t2 = get_2_ci(opcode);
			cmp_u(t1, t2);
			bxx_s(opcode, 3);
			break;

		case 0x34: // cmpobl
			m_icount -= 4;
			t1 = get_1_ci(opcode);
			t2 = get_2_ci(opcode);
			cmp_u(t1, t2);
			bxx_s(opcode, 4);
			break;

		case 0x35: // cmpobne
			m_icount -= 4;
			t1 = get_1_ci(opcode);
			t2 = get_2_ci(opcode);
			cmp_u(t1, t2);
			bxx_s(opcode, 5);
			break;

		case 0x36: // cmpoble
			m_icount -= 4;
			t1 = get_1_ci(opcode);
			t2 = get_2_ci(opcode);
			cmp_u(t1, t2);
			bxx_s(opcode, 6);
			break;

		case 0x37: // bbs
			m_icount -= 4;
			t1 = get_1_ci(opcode) & 0x1f;
			t2 = get_2_ci(opcode);
			if(t2 & (1u<<t1)) {
				m_AC = (m_AC & ~7) | 2;
				m_IP += get_disp_s(opcode);
			} else
				m_AC &= ~7;
			break;

		// Supermodel: cmpibno / cmpibo (missing in MAME): one of g/e/l is always
		// set, so "no condition" never branches and "ordered" always does
		case 0x38: // cmpibno
			m_icount -= 4;
			t1 = get_1_ci(opcode);
			t2 = get_2_ci(opcode);
			cmp_s(t1, t2);
			break;

		case 0x3f: // cmpibo
			m_icount -= 4;
			t1 = get_1_ci(opcode);
			t2 = get_2_ci(opcode);
			cmp_s(t1, t2);
			bxx_s(opcode, 7);
			break;

		case 0x39: // cmpibg
			m_icount -= 4;
			t1 = get_1_ci(opcode);
			t2 = get_2_ci(opcode);
			cmp_s(t1, t2);
			bxx_s(opcode, 1);
			break;

		case 0x3a: // cmpibe
			m_icount -= 4;
			t1 = get_1_ci(opcode);
			t2 = get_2_ci(opcode);
			cmp_s(t1, t2);
			bxx_s(opcode, 2);
			break;

		case 0x3b: // cmpibge
			m_icount -= 4;
			t1 = get_1_ci(opcode);
			t2 = get_2_ci(opcode);
			cmp_s(t1, t2);
			bxx_s(opcode, 3);
			break;

		case 0x3c: // cmpibl
			m_icount -= 4;
			t1 = get_1_ci(opcode);
			t2 = get_2_ci(opcode);
			cmp_s(t1, t2);
			bxx_s(opcode, 4);
			break;

		case 0x3d: // cmpibne
			m_icount -= 4;
			t1 = get_1_ci(opcode);
			t2 = get_2_ci(opcode);
			cmp_s(t1, t2);
			bxx_s(opcode, 5);
			break;

		case 0x3e: // cmpible
			m_icount -= 4;
			t1 = get_1_ci(opcode);
			t2 = get_2_ci(opcode);
			cmp_s(t1, t2);
			bxx_s(opcode, 6);
			break;

		case 0x58:
			switch((opcode >> 7) & 0xf) {
			case 0x0: // notbit
				m_icount -= 2;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t2 ^ (1u<<(t1 & 31)));
				break;

			case 0x1: // and
				m_icount--;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t2 & t1);
				break;

			case 0x2: // andnot
				m_icount--;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t2 & ~t1);
				break;

			case 0x3: // setbit
				m_icount -= 2;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t2 | (1u<<(t1 & 31)));
				break;

			case 0x4: // notand
				m_icount--;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, (~t2) & t1);
				break;

			case 0x6: // xor
				m_icount--;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t2 ^ t1);
				break;

			case 0x7: // or
				m_icount--;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t2 | t1);
				break;

			case 0x8: // nor
				m_icount--;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, ((~t2) & (~t1)));
				break;

			case 0x9: // xnor
				m_icount--;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, ~(t2 ^ t1));
				break;

			case 0xa: // not
				m_icount--;
				t1 = get_1_ri(opcode);
				set_ri(opcode, ~t1);
				break;

			case 0xb: // ornot
				m_icount--;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t2 | ~t1);
				break;

			case 0xc: // clrbit
				m_icount -= 2;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t2 & ~(1u<<(t1 & 31)));
				break;

			case 0xd: // notor
				m_icount--;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, (~t2) | t1);
				break;

			case 0xe: // nand
				m_icount -= 2;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, ~t2 | ~t1);
				break;

			case 0xf: // alterbit
				m_icount -= 2;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				if(m_AC & 2)
					set_ri(opcode, t2 | (1u<<(t1 & 31)));
				else
					set_ri(opcode, t2 & ~(1u<<(t1 & 31)));
				break;

			default:
				fatalerror("I960: %x: Unhandled 58.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x59:
			switch((opcode >> 7) & 0xf) {
			case 0x0: // addo
				m_icount--;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t2+t1);
				break;

			case 0x1: // addi
				// #### overflow
				m_icount--;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t2+t1);
				break;

			case 0x2: // subo
				m_icount--;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t2-t1);
				break;

			case 0x3: // subi
				// #### overflow
				m_icount--;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t2-t1);
				break;

			case 0x8: // shro
				m_icount--;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t1 >= 32 ? 0 : t2>>t1);
				break;

			case 0xa: // shrdi
				m_icount--;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				if(t1 >= 32)
					set_ri(opcode, 0);
				else if(((int32_t)t2) < 0) {
					if(t2 & ((1u<<t1)-1))
						set_ri(opcode, (((int32_t)t2)>>t1)+1);
					else
						set_ri(opcode, ((int32_t)t2)>>t1);
				} else
					set_ri(opcode, t2>>t1);
				break;

			case 0xb: // shri
				m_icount--;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				if(t1 >= 32)
					set_ri(opcode, (int32_t)t2 < 0 ? -1 : 0);
				else
					set_ri(opcode, ((int32_t)t2)>>t1);
				break;

			case 0xc: // shlo
				m_icount--;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t1 >= 32 ? 0 : t2<<t1);
				break;

			case 0xd: // rotate
				m_icount--;
				t1 = get_1_ri(opcode) & 0x1f;
				t2 = get_2_ri(opcode);
				set_ri(opcode, rotl32(t2, t1));
				break;

			case 0xe: // shli
				// missing overflow
				m_icount--;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				// TODO: on later models, sign is always preserved even upon overflow
				set_ri(opcode, t1 >= 32 ? 0 : t2<<t1);
				break;

			default:
				fatalerror("I960: %x: Unhandled 59.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x5a:
			switch((opcode >> 7) & 0xf) {
			case 0x0: // cmpo
				m_icount--;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				cmp_u(t1, t2);
				break;

			case 0x1: // cmpi
				m_icount--;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				cmp_s(t1, t2);
				break;

			case 0x2: // concmpo
				m_icount--;
				if(!(m_AC & 0x4)) {
					t1 = get_1_ri(opcode);
					t2 = get_2_ri(opcode);
					concmp_u(t1, t2);
				}
				break;

			case 0x3: // concmpi
				m_icount--;
				if(!(m_AC & 0x4)) {
					t1 = get_1_ri(opcode);
					t2 = get_2_ri(opcode);
					concmp_s(t1, t2);
				}
				break;

			case 0x4: // cmpinco
				m_icount -= 2;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				cmp_u(t1, t2);
				set_ri(opcode, t2+1);
				break;

			case 0x5: // cmpinci
				m_icount -= 2;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				cmp_s(t1, t2);
				set_ri(opcode, t2+1);
				break;

			case 0x6: // cmpdeco
				m_icount -= 2;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				cmp_u(t1, t2);
				set_ri(opcode, t2-1);
				break;

			case 0x7: // cmpdeci
				m_icount -= 2;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				cmp_s(t1, t2);
				set_ri(opcode, t2-1);
				break;

			case 0xc: // scanbyte
				m_icount -= 2;
				m_AC &= ~7;     // clear CC
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				if ((t1 & 0xff000000) == (t2 & 0xff000000) ||
					(t1 & 0x00ff0000) == (t2 & 0x00ff0000) ||
					(t1 & 0x0000ff00) == (t2 & 0x0000ff00) ||
					(t1 & 0x000000ff) == (t2 & 0x000000ff))
				{
					m_AC |= 2;
				}
				break;

			case 0xe: // chkbit
				m_icount -= 2;
				t1 = get_1_ri(opcode) & 0x1f;
				t2 = get_2_ri(opcode);
				if(t2 & (1u<<t1))
					m_AC = (m_AC & ~7) | 2;
				else
					m_AC &= ~7;
				break;

			default:
				fatalerror("I960: %x: Unhandled 5a.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x5b:
			switch((opcode >> 7) & 0xf) {
			case 0x0:   // addc
				{
					uint64_t res;

					m_icount -= 2;
					t1 = get_1_ri(opcode);
					t2 = get_2_ri(opcode);
					// Supermodel: summed in 64 bits (MAME added in 32 bits, so the
					// carry out was always lost) and cc = 0CV like subc (MAME left
					// bit 2 as it was)
					res = (uint64_t)t2 + (uint64_t)t1 + ((m_AC>>1)&1);
					set_ri(opcode, res&0xffffffff);

					m_AC &= ~0x7;   // cc = 0CV
					// set carry
					m_AC |= ((res) & (((uint64_t)1) << 32)) ? 0x2 : 0;
					// set overflow
					m_AC |= (((res) ^ (t1)) & ((res) ^ (t2)) & 0x80000000) ? 1: 0;
				}
				break;

			case 0x2:   // subc
				{
					uint64_t res;

					m_icount -= 2;
					t1 = get_1_ri(opcode);
					t2 = get_2_ri(opcode);
					// dst = src2 - src1 - 1 + C, or src2 + ~src1 + C
					res = (uint64_t)t2 + (uint64_t)(uint32_t)~t1 + ((m_AC>>1)&1);
					set_ri(opcode, res&0xffffffff);

					m_AC &= ~0x7;   // cc = 0CV
					// set carry
					m_AC |= ((res) & (((uint64_t)1) << 32)) ? 0x2 : 0;
					// set overflow
					m_AC |= (((t2) ^ (t1)) & ((t2) ^ (res)) & 0x80000000) ? 1 : 0;
				}
				break;

			default:
				fatalerror("I960: %x: Unhandled 5b.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x5c:
			switch((opcode >> 7) & 0xf) {
			case 0xc: // mov
				m_icount -= 2;
				t1 = get_1_ri(opcode);
				set_ri(opcode, t1);
				break;

			default:
				fatalerror("I960: %x: Unhandled 5c.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x5d:
			switch((opcode >> 7) & 0xf) {
			case 0xc: // movl
				m_icount -= 2;
				t2 = (opcode>>19) & 0x1e;
				if(opcode & 0x00000800) { // litteral
					t1 = opcode & 0x1f;
					m_r[t2] = m_r[t2+1] = t1;
				} else
					memcpy(m_r+t2, m_r+(opcode & 0x1f), 2*sizeof(uint32_t));
				break;

			default:
				fatalerror("I960: %x: Unhandled 5d.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x5e:
			switch((opcode >> 7) & 0xf) {
			case 0xc: // movt
				m_icount -= 3;
				t2 = (opcode>>19) & 0x1c;
				if(opcode & 0x00000800) { // litteral
					t1 = opcode & 0x1f;
					m_r[t2] = m_r[t2+1] = m_r[t2+2]= t1;
				} else
					memcpy(m_r+t2, m_r+(opcode & 0x1f), 3*sizeof(uint32_t));
				break;

			default:
				fatalerror("I960: %x: Unhandled 5e.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x5f:
			switch((opcode >> 7) & 0xf) {
			case 0xc: // movq
				m_icount -= 4;
				t2 = (opcode>>19) & 0x1c;
				if(opcode & 0x00000800) { // litteral
					t1 = opcode & 0x1f;
					m_r[t2] = m_r[t2+1] = m_r[t2+2] = m_r[t2+3] = t1;
				} else
					memcpy(m_r+t2, m_r+(opcode & 0x1f), 4*sizeof(uint32_t));
				break;

			default:
				fatalerror("I960: %x: Unhandled 5f.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x60:
			switch((opcode >> 7) & 0xf) {
			case 0x0: // synmov
				m_icount -= 6;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				// interrupt control register
				if(t1 == 0xff000004)
					m_ICR = m_program.read_dword(t2);
				else
					m_program.write_dword(t1,    m_program.read_dword(t2));
				m_AC = (m_AC & ~7) | 2;
				break;

			case 0x2: // synmovq
				m_icount -= 12;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				if(t1 == 0xff000010)
					send_iac(t2);
				else {
					m_program.write_dword(t1,    m_program.read_dword(t2));
					m_program.write_dword(t1+4,  m_program.read_dword(t2+4));
					m_program.write_dword(t1+8,  m_program.read_dword(t2+8));
					m_program.write_dword(t1+12, m_program.read_dword(t2+12));
				}
				m_AC = (m_AC & ~7) | 2;
				break;

			default:
				fatalerror("I960: %x: Unhandled 60.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x64:
			switch((opcode >> 7) & 0xf) {
			case 0x0: // spanbit
				{
					uint32_t res = 0xffffffff;
					int i;

					m_icount -= 10;

					t1 = get_1_ri(opcode);
					m_AC &= ~7;

					for (i = 31; i >= 0; i--)
					{
						if (!(t1 & (1u<<i)))
						{
							m_AC |= 2;
							res = i;
							break;
						}
					}

					set_ri(opcode, res);
				}
				break;

			case 0x1: // scanbit
				{
					uint32_t res = 0xffffffff;
					int i;

					m_icount -= 10;

					t1 = get_1_ri(opcode);
					m_AC &= ~7;

					for (i = 31; i >= 0; i--)
					{
						if (t1 & (1u<<i))
						{
							m_AC |= 2;
							res = i;
							break;
						}
					}

					set_ri(opcode, res);
				}
				break;

			case 0x4: // dmovt
				/*
				    The dmovt instruction moves a 32-bit word from one register to another
				    and tests the least-significant byte of the operand to determine if it is a
				    valid ASCII-coded decimal digit (001100002 through 001110012,
				    corresponding to the decimal digits 0 through 9). For valid digits, the
				    condition code (CC) is set to 000; otherwise the condition code is set to
				    010.
				*/
				m_icount -= 7;
				t1 = get_1_ri(opcode);
				set_ri(opcode, t1);
				m_AC &= ~7;   // Supermodel: only the condition code (MAME: &= 0xfff8)
				if ((t1 & 0xff) < 0x30 || (t1 & 0xff) > 0x39)
					m_AC |= 2;
				break;

			case 0x5: // modac
				m_icount -= 10;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, m_AC);
				m_AC = (m_AC & ~t1) | (t2 & t1);
				break;

			default:
				fatalerror("I960: %x: Unhandled 64.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x65:
			switch((opcode >> 7) & 0xf) {
			// Supermodel: modify and extract (missing in MAME), from the 80960KB
			// manual; src/dst is both an operand and the result
			case 0x0: // modify mask, src, src/dst
				m_icount -= 4;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, (t2 & t1) | (m_r[(opcode>>19) & 0x1f] & ~t1));
				break;

			case 0x1: // extract bitpos, len, src/dst
				m_icount -= 7;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				{
					uint32_t v = t1 >= 32 ? 0 : m_r[(opcode>>19) & 0x1f] >> t1;
					set_ri(opcode, t2 >= 32 ? v : v & ~(0xffffffffu << t2));
				}
				break;

			case 0x5: // modpc
				m_icount -= 10;
				t1 = m_PC;
				t2 = get_2_ri(opcode);
				m_PC = (m_PC & ~t2) | (m_r[(opcode>>19) & 0x1f] & t2);
				set_ri(opcode, t1);
				if ((t1 >> 16 & 0x1f) > (m_PC >> 16 & 0x1f))
					check_pending_irqs();
				break;

			default:
				fatalerror("I960: %x: Unhandled 65.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x66:
			switch((opcode >> 7) & 0xf) {
			case 0x0: // calls
				t1 = get_1_ri(opcode);
				t2 = m_program.read_dword(m_SAT + 152);    // get pointer to system procedure table
				t2 = m_program.read_dword(t2 + 48 + (t1 * 4));
				if ((t2 & 3) != 0)
				{
					fatalerror("I960: system calls that jump into supervisor mode aren't yet supported\n");
				}
				do_call(t2, 0, m_r[I960_SP]);
				break;

			case 0xd: // flushreg
				if (m_rcache_pos > 4)
				{
					m_rcache_pos = 4;
				}
				for(t1=0; t1 < uint32_t(m_rcache_pos); t1++)
				{
					int i;

					for (i = 0; i < 0x10; i++)
					{
						m_program.write_dword(m_rcache_frame_addr[t1] + (i * sizeof(uint32_t)), m_rcache[t1][i]);
					}
				}
				m_rcache_pos = 0;
				break;

			case 0xf: // syncf: waits for pending faults; none are raised here (Supermodel, missing in MAME)
				m_icount -= 4;
				break;

			default:
				fatalerror("I960: %x: Unhandled 66.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x67:
			switch((opcode >> 7) & 0xf) {
			case 0x0: // emul
				m_icount -= 37;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);

				set_ri64(opcode, mulu_32x32(t1, t2));
				break;

			case 0x1: // ediv
				m_icount -= 37;
				{
					uint64_t src1, src2;

					src1 = get_1_ri(opcode);
					src2 = get_2_ri64(opcode);

					// Supermodel: a zero divisor raises the zero-divide fault and
					// leaves the destination alone (MAME crashed the host)
					if (src1 == 0)
						DebugLog("i960: %x: ediv by zero\n", m_PIP);
					else
						set_ri2(opcode, uint32_t(src2 % src1), uint32_t(src2 / src1));   // 32-bit remainder and quotient
				}
				break;

			case 0x4: // cvtir
				m_icount -= 30;
				t1 = get_1_ri(opcode);
				set_rif(opcode, (double)(int32_t)t1);
				break;

			case 0x5: // cvtilr
				m_icount -= 30;
				t1 = get_1_ri(opcode);
				set_rifl(opcode, (double)(int32_t)t1);
				break;

			case 0x6: // scalerl
				m_icount -= 30;
				t1 = get_1_ri(opcode);
				t2f = get_2_rifl(opcode);
				set_rifl(opcode, ldexp(t2f, (int32_t)t1));
				break;

			case 0x7: // scaler
				m_icount -= 30;
				t1 = get_1_ri(opcode);
				t2f = get_2_rif(opcode);
			set_rif(opcode, ldexp(t2f, (int32_t)t1));
				break;

			default:
				fatalerror("I960: %x: Unhandled 67.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x68:
			switch((opcode >> 7) & 0xf) {
			case 0x0: // atanr
				m_icount -= 267;
				t1f = get_1_rif(opcode);
				t2f = get_2_rif(opcode);
				set_rif(opcode, atan2(t2f, t1f));
				break;

			case 0x1: // logepr
				m_icount -= 400;
				t1f = get_1_rif(opcode);
				t2f = get_2_rif(opcode);
				set_rif(opcode, t2f*log1p(t1f)/log(2.0));
				break;

			case 0x2: // logr
				m_icount -= 438;
				t1f = get_1_rif(opcode);
				t2f = get_2_rif(opcode);
				set_rif(opcode, t2f*log2(t1f));
				break;

			case 0x3: // remr
				m_icount -= 67; // (67 to 75878 depending on opcodes!!!)
				t1f = get_1_rif(opcode);
				t2f = get_2_rif(opcode);
				set_rif(opcode, fmod(t2f, t1f));
				break;

			case 0x5: // cmpr
				m_icount -= 10;
				t1f = get_1_rif(opcode);
				t2f = get_2_rif(opcode);
				cmp_d(t1f, t2f);
				break;

			case 0x8: // sqrtr
				m_icount -= 104;
				t1f = get_1_rif(opcode);
				set_rif(opcode, sqrt(t1f));
				break;

			case 0x9: // expr
				m_icount -= 334; // checkme
				t1f = get_1_rif(opcode);
				set_rif(opcode, expm1(t1f*log(2.0)));
				break;

			case 0xa: // logbnr
				m_icount -= 37;
				t1f = get_1_rif(opcode);
				set_rif(opcode, logb(t1f));
				break;

			case 0xb: // roundr
				m_icount -= 69;
				t1f = get_1_rif(opcode);
				set_rif(opcode, round_to_int(t1f));
				break;

			case 0xc: // sinr
				m_icount -= 406;
				t1f = get_1_rif(opcode);
				set_rif(opcode, sin(t1f));
				break;

			case 0xd: // cosr
				m_icount -= 406;
				t1f = get_1_rif(opcode);
				set_rif(opcode, cos(t1f));
				break;

			case 0xe: // tanr
				m_icount -= 293;
				t1f = get_1_rif(opcode);
				set_rif(opcode, tan(t1f));
				break;

			default:
				fatalerror("I960: %x: Unhandled 68.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x69:
			switch((opcode >> 7) & 0xf) {
			case 0x0: // atanrl
				m_icount -= 350;
				t1f = get_1_rifl(opcode);
				t2f = get_2_rifl(opcode);
				set_rifl(opcode, atan2(t2f, t1f));
				break;

			case 0x2: // logrl
				m_icount -= 438;
				t1f = get_1_rifl(opcode);
				t2f = get_2_rifl(opcode);
				set_rifl(opcode, t2f*log2(t1f));
				break;

			case 0x5: // cmprl
				m_icount -= 12;
				t1f = get_1_rifl(opcode);
				t2f = get_2_rifl(opcode);
				cmp_d(t1f, t2f);
				break;

			case 0x8: // sqrtrl
				m_icount -= 104;
				t1f = get_1_rifl(opcode);
				set_rifl(opcode, sqrt(t1f));
				break;

			case 0x9: // exprl
				m_icount -= 334;
				t1f = get_1_rifl(opcode);
				set_rifl(opcode, expm1(t1f*log(2.0)));
				break;

			case 0xa: // logbnrl
				m_icount -= 37;
				t1f = get_1_rifl(opcode);
				set_rifl(opcode, logb(t1f));
				break;

			case 0xb: // roundrl
				m_icount -= 70;
				t1f = get_1_rifl(opcode);
				set_rifl(opcode, round_to_int(t1f));
				break;

			case 0xc: // sinrl
				m_icount -= 441;
				t1f = get_1_rifl(opcode);
				set_rifl(opcode, sin(t1f));
				break;

			case 0xd: // cosrl
				m_icount -= 441;
				t1f = get_1_rifl(opcode);
				set_rifl(opcode, cos(t1f));
				break;

			case 0xe: // tanrl
				m_icount -= 323;
				t1f = get_1_rifl(opcode);
				set_rifl(opcode, tan(t1f));
				break;

			default:
				fatalerror("I960: %x: Unhandled 69.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x6c:
			switch((opcode >> 7) & 0xf) {
			case 0x0: // cvtri
				m_icount -= 33;
				t1f = get_1_rif(opcode);
				set_ri(opcode, SaturateToInt<int32_t>(round_to_int(t1f)));
				break;

			case 0x1: // cvtril
				m_icount -= 35;
				t1f = get_1_rif(opcode);
				set_ri64(opcode, SaturateToInt<int64_t>(round_to_int(t1f)));
				break;

			case 0x2: // cvtzri
				m_icount -= 43;
				t1f = get_1_rif(opcode);
				set_ri(opcode, SaturateToInt<int32_t>(t1f));
				break;

			case 0x3: // cvtzril
				m_icount -= 44;
				t1f = get_1_rif(opcode);
				set_ri64(opcode, SaturateToInt<int64_t>(t1f));
				break;

			case 0x9: // movr
				m_icount -= 5;
				t1f = get_1_rif(opcode);
				set_rif(opcode, t1f);
				break;

			default:
				fatalerror("I960: %x: Unhandled 6c.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x6d:
			switch((opcode >> 7) & 0xf) {
			case 0x9: // movrl
				m_icount -= 6;
				t1f = get_1_rifl(opcode);
				set_rifl(opcode, t1f);
				break;

			default:
				fatalerror("I960: %x: Unhandled 6d.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x6e:
			switch((opcode >> 7) & 0xf) {
			case 0x1: // movre (undocumented encoding, used by Dead or Alive to save/restore fp0-fp3)
			case 0x9: // movre
				{
					uint32_t v[3];

					m_icount -= 8;

					// source: three g/l registers (multiple of 4), a floating-point register or a literal
					if(!(opcode & 0x00000800))
					{
						const int src = opcode & 0x1c;
						v[0] = m_r[src];
						v[1] = m_r[src+1];
						v[2] = m_r[src+2] & 0xffff;    // upper 16 bits of the third word are truncated
					}
					else
					{
						const int idx = opcode & 0x1f;
						if(idx < 4)
							double_to_extended(m_fp[idx], v);
						else
							double_to_extended((idx == 0x16) ? 1.0 : 0.0, v);
					}

					// destination: three g/l registers (multiple of 4) or a floating-point register
					if(!(opcode & 0x00002000))
					{
						const int dst = (opcode>>19) & 0x1c;
						m_r[dst] = v[0];
						m_r[dst+1] = v[1];
						m_r[dst+2] = v[2];
					}
					else if(!(opcode & 0x00e00000))
						m_fp[(opcode>>19) & 3] = extended_to_double(v);
					else
						fatalerror("i960: %x: movre to literal?\n", m_PIP);
				}
				break;
			case 0x2: // cpysre
				m_icount -= 8;
				t1f = get_1_rifl(opcode);
				t2f = get_2_rifl(opcode);

				if (!std::signbit(t2f))   // Supermodel: -0.0 is negative
					set_rifl(opcode, fabs(t1f));
				else
					set_rifl(opcode, -fabs(t1f));
				break;
			default:
				fatalerror("I960: %x: Unhandled 6e.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x70:
			switch((opcode >> 7) & 0xf) {
			case 0x1: // mulo
				m_icount -= 18;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t2*t1);
				break;

			case 0x8: // remo
				m_icount -= 37;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				// Supermodel: zero divisor (zero-divide fault, destination unchanged; MAME crashed the host)
				if (t1 == 0)
					DebugLog("i960: %x: remo by zero\n", m_PIP);
				else
					set_ri(opcode, t2%t1);
				break;

			case 0xb: // divo
				m_icount -= 37;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				if (t1 == 0)    // HACK!
					set_ri(opcode, 0);
				else
					set_ri(opcode, t2/t1);
				break;

			default:
				fatalerror("I960: %x: Unhandled 70.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x74:
			switch((opcode >> 7) & 0xf) {
			case 0x1: // muli
				m_icount -= 18;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				// Supermodel: the low 32 bits of the product, multiplied unsigned
				// (a signed overflow is undefined in C++)
				set_ri(opcode, t2*t1);
				break;

			// Supermodel: remi, modi and divi leave the destination unchanged on a
			// zero divisor (zero-divide fault), and -0x80000000 / -1 (integer
			// overflow fault) gives -0x80000000 remainder 0; MAME crashed the host
			// on both
			case 0x8: // remi
				m_icount -= 37;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				if (t1 == 0)
					DebugLog("i960: %x: remi by zero\n", m_PIP);
				else if ((int32_t)t1 == -1)
					set_ri(opcode, 0);
				else
					set_ri(opcode, ((int32_t)t2)%((int32_t)t1));
				break;

			case 0x9:{// modi
				int32_t src1, src2, dst;
				m_icount -= 37;
				src1 = (int32_t)get_1_ri(opcode);
				src2 = (int32_t)get_2_ri(opcode);
				if (src1 == 0)
				{
					DebugLog("i960: %x: modi by zero\n", m_PIP);
					break;
				}
				dst = (src1 == -1) ? 0 : src2 - ((src2/src1)*src1);
				if(((src1 ^ src2) < 0) && (dst != 0))   // operands of opposite sign (the product would overflow)
					dst += src1;
				set_ri(opcode, dst);
				break;
			}

			case 0xb: // divi
				m_icount -= 37;
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				if (t1 == 0)
					DebugLog("i960: %x: divi by zero\n", m_PIP);
				else if ((int32_t)t1 == -1)
					set_ri(opcode, 0u - t2);
				else
					set_ri(opcode, ((int32_t)t2)/((int32_t)t1));
				break;

			default:
				fatalerror("I960: %x: Unhandled 74.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x78:
			switch((opcode >> 7) & 0xf) {
			case 0xb: // divr
				m_icount -= 35;
				t1f = get_1_rif(opcode);
				t2f = get_2_rif(opcode);
				set_rif(opcode, t2f/t1f);
				break;

			case 0xc: // mulr
				m_icount -= 18;
				t1f = get_1_rif(opcode);
				t2f = get_2_rif(opcode);
				set_rif(opcode, t2f*t1f);
				break;

			case 0xd: // subr
				m_icount -= 10;
				t1f = get_1_rif(opcode);
				t2f = get_2_rif(opcode);
				set_rif(opcode, t2f-t1f);
				break;

			case 0xf: // addr
				m_icount -= 10;
				t1f = get_1_rif(opcode);
				t2f = get_2_rif(opcode);
				set_rif(opcode, t2f+t1f);
				break;

			default:
				fatalerror("I960: %x: Unhandled 78.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x79:
			switch((opcode >> 7) & 0xf) {
			case 0xb: // divrl
				m_icount -= 77;
				t1f = get_1_rifl(opcode);
				t2f = get_2_rifl(opcode);
				set_rifl(opcode, t2f/t1f);
				break;

			case 0xc: // mulrl
				m_icount -= 36;
				t1f = get_1_rifl(opcode);
				t2f = get_2_rifl(opcode);
				set_rifl(opcode, t2f*t1f);
				break;

			case 0xd: // subrl
				m_icount -= 13;
				t1f = get_1_rifl(opcode);
				t2f = get_2_rifl(opcode);
				set_rifl(opcode, t2f-t1f);
				break;

			case 0xf: // addrl
				m_icount -= 13;
				t1f = get_1_rifl(opcode);
				t2f = get_2_rifl(opcode);
				set_rifl(opcode, t2f+t1f);
				break;

			default:
				fatalerror("I960: %x: Unhandled 79.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x80: { // ldob
			m_icount -= 4;
			uint8_t v = m_program.read_byte(get_ea(opcode));
			if(!m_stalled)
				m_r[(opcode>>19)&0x1f] = v;
			break;
		}

		case 0x82: // stob
			m_icount -= 2;
			m_program.write_byte(get_ea(opcode), m_r[(opcode>>19)&0x1f]);
			break;

		case 0x84: // bx
			m_icount -= 3;
			m_IP = get_ea(opcode);
			break;

		case 0x85: // balx
			m_icount -= 5;
			t1 = get_ea(opcode);
			m_r[(opcode>>19)&0x1f] = m_IP;
			m_IP = t1;
			break;

		case 0x86: // callx
			t1 = get_ea(opcode);
			do_call(t1, 0, m_r[I960_SP]);
			break;

		case 0x88: { // ldos
			m_icount -= 4;
			uint16_t v = i960_read_word_unaligned(get_ea(opcode));
			if(!m_stalled)
				m_r[(opcode>>19)&0x1f] = v;
			break;
		}

		case 0x8a: // stos
			m_icount -= 2;
			i960_write_word_unaligned(get_ea(opcode), m_r[(opcode>>19)&0x1f]);
			break;

		case 0x8c: // lda
			m_icount--;
			m_r[(opcode>>19)&0x1f] = get_ea(opcode);
			break;

		case 0x90: { // ld
			m_icount -= 4;
			uint32_t v = i960_read_dword_unaligned(get_ea(opcode));
			if(!m_stalled)
				m_r[(opcode>>19)&0x1f] = v;
			break;
		}

		case 0x92: // st
			m_icount -= 2;
			i960_write_dword_unaligned(get_ea(opcode), m_r[(opcode>>19)&0x1f]);
			break;

		case 0x98:{// ldl
			int i;
			m_icount -= 5;
			t1 = get_ea(opcode);
			t2 = (opcode>>19)&0x1e;
			for(i=0; i<2; i++) {
				auto pack = i960_read_dword_unaligned_flags(t1);
				if(m_stalled)
				{
					burst_stall_save(t1,t2,i,2,false);
					return;
				}
				m_r[t2+i] = pack.first;
				if(pack.second & BURST)
					t1 += 4;
			}
			break;
		}

		case 0x9a:{// stl
			int i;
			m_icount -= 3;
			t1 = get_ea(opcode);
			t2 = (opcode>>19)&0x1e;
			for(i=0; i<2; i++) {
				auto flags = i960_write_dword_unaligned_flags(t1, m_r[t2+i]);
				if(m_stalled)
				{
					burst_stall_save(t1,t2,i,2,true);
					return;
				}
				if(flags & BURST)
					t1 += 4;
			}
			break;
		}

		case 0xa0:{// ldt
			int i;
			m_icount -= 6;
			t1 = get_ea(opcode);
			t2 = (opcode>>19)&0x1c;
			for(i=0; i<3; i++) {
				auto pack = i960_read_dword_unaligned_flags(t1);
				if(m_stalled)
				{
					burst_stall_save(t1,t2,i,3,false);
					return;
				}
				m_r[t2+i] = pack.first;
				if(pack.second & BURST)
					t1 += 4;
			}
			break;
		}

		case 0xa2:{// stt
			int i;
			m_icount -= 4;
			t1 = get_ea(opcode);
			t2 = (opcode>>19)&0x1c;
			for(i=0; i<3; i++) {
				auto flags = i960_write_dword_unaligned_flags(t1, m_r[t2+i]);
				if(m_stalled)
				{
					burst_stall_save(t1,t2,i,3,true);
					return;
				}
				if(flags & BURST)
					t1 += 4;
			}
			break;
		}

		case 0xb0:{// ldq
			int i;
			m_icount -= 7;
			t1 = get_ea(opcode);
			t2 = (opcode>>19)&0x1c;
			for(i=0; i<4; i++) {
				auto pack = i960_read_dword_unaligned_flags(t1);
				if(m_stalled)
				{
					burst_stall_save(t1,t2,i,4,false);
					return;
				}
				m_r[t2+i] = pack.first;
				if(pack.second & BURST)
					t1 += 4;
			}
			break;
		}

		case 0xb2:{// stq
			int i;
			m_icount -= 5;
			t1 = get_ea(opcode);
			t2 = (opcode>>19)&0x1c;
			for(i=0; i<4; i++) {
				auto flags = i960_write_dword_unaligned_flags(t1, m_r[t2+i]);
				if(m_stalled)
				{
					burst_stall_save(t1,t2,i,4,true);
					return;
				}
				if(flags & BURST)
					t1 += 4;
			}
			break;
		}

		case 0xc0: { // ldib
			m_icount -= 4;
			int8_t v = m_program.read_byte(get_ea(opcode));
			if(!m_stalled)
				m_r[(opcode>>19)&0x1f] = v;
			break;
		}

		case 0xc2: // stib
			m_icount -= 2;
			m_program.write_byte(get_ea(opcode), m_r[(opcode>>19)&0x1f]);
			break;

		case 0xc8: { // ldis
			m_icount -= 4;
			int16_t v = i960_read_word_unaligned(get_ea(opcode));
			if(!m_stalled)
				m_r[(opcode>>19)&0x1f] = v;
			break;
		}

		case 0xca: // stis
			m_icount -= 2;
			i960_write_word_unaligned(get_ea(opcode), m_r[(opcode>>19)&0x1f]);
			break;

		default:
			fatalerror("I960: %x: Unhandled %02x\n", m_PIP, opcode >> 24);
	}

}

void CI960::execute_run()
{
	uint32_t opcode;

	// delay checking irqs if we are in burst stall mode
	if(m_stall_state.burst_mode == false)
		check_immediate_irqs();

	while(m_icount > 0) {
		m_PIP = m_IP;
		if (m_trace)
			m_trace(m_IP);   // debugger_instruction_hook

		opcode = m_cache.read_dword(m_IP);
		m_IP += 4;

		m_stalled = false;

		if(m_stall_state.burst_mode == true)
			execute_burst_stall_op(opcode);
		else
			execute_op(opcode);

		// Supermodel: an interrupt raised by this instruction (a write to the
		// interrupt controller or a device) is taken before the next one, as
		// MAME's resync on set_input_line does, not at the next timeslice.
		// Daytona USA masks its sound interrupt around its sound queue updates
		// and relies on this: taken late, the interrupt lands inside the next
		// update, the queue count goes wrong and the music is lost.
		if ((m_immediate_irq || IRQHeld()) && !m_stall_state.burst_mode)
			check_immediate_irqs();
	}
}

void CI960::execute_set_input(int irqline, int state)
{
	if ((m_irq_line_state[irqline] != CLEAR_LINE) == (state != CLEAR_LINE))
		return;

	m_irq_line_state[irqline] = state;

	int cpu_pri = (m_PC>>16)&0x1f;
	int vector =0;
	int priority;

	// We support the 4 external IRQ lines in "normal" mode only.
	// The i960's interrupt support is a bit more complete than that,
	// but Namco and Sega both went for the cheapest solution.

	switch (irqline)
	{
		case 0:
			vector = m_ICR & 0xff;
			break;

		case 1:
			vector = (m_ICR>>8)&0xff;
			break;

		case 2:
			vector = (m_ICR>>16)&0xff;
			break;

		case 3:
			vector = (m_ICR>>24)&0xff;
			break;
	}

	if(!vector)
	{
		DebugLog("i960: interrupt line %d in IAC mode, unsupported!\n", irqline);
		return;
	}


	priority = vector / 8;

	if(state) {
		// check if we can take this "right now"
		if (((cpu_pri < priority) || (priority == 31)) && (m_immediate_irq == 0))
		{
			m_immediate_irq = 1;
			m_immediate_vector = vector;
			m_immediate_pri = priority;
		}
		else
		{
			// Supermodel: MAME stores the interrupt in the interrupt table's
			// pending bits here. Model 2's lines stay asserted until the game
			// acknowledges them, and Sky Target's table is in ROM, where the
			// pending bits are lost: a vblank raised while a timer interrupt
			// was waiting was never taken, its line never dropped and there
			// were no more vblank interrupts (no sound: the sound queue is
			// sent from the vblank handler). The line is held instead, and
			// taken when the CPU's priority allows (check_immediate_irqs).
			m_irq_line_state[irqline] = IRQ_HELD;
		}

		// (MAME acks the line here, standard_irq_callback(); no one listens on Model 2)
	}
}

void CI960::i960_stall()
{
	m_stalled = true;
	m_IP = m_PIP;
}


/******************************************************************************
 Supermodel interface
******************************************************************************/

CI960::CI960()
	: m_stalled(false), m_rcache_pos(0), m_SAT(0), m_PRCB(0), m_PC(0), m_AC(0), m_IP(0), m_PIP(0), m_ICR(0),
	  m_immediate_irq(0), m_immediate_vector(0), m_immediate_pri(0), m_icount(0), m_numFetch(0), m_fatal(false)
{
	std::fill(std::begin(m_r), std::end(m_r), 0);
	std::fill(std::begin(m_rcache_frame_addr), std::end(m_rcache_frame_addr), 0);
	std::fill(std::begin(m_fp), std::end(m_fp), 0);
	for (int i = 0; i < I960_RCACHE_SIZE; i++)
		std::fill(std::begin(m_rcache[i]), std::end(m_rcache[i]), 0);
	std::fill(std::begin(m_irq_line_state), std::end(m_irq_line_state), CLEAR_LINE);
	m_cache.cpu = this;
}

void CI960::AttachBus(II960Bus *bus)
{
	m_program.bus = bus;
}

void CI960::SetFetchRegion(UINT32 base, UINT32 size, const UINT8 *ptr)
{
	if (m_numFetch < MAX_FETCH_REGIONS)
		m_fetch[m_numFetch++] = { base, size, ptr };
}

void CI960::ClearFetchRegions()
{
	m_numFetch = 0;
}

// device_reset
void CI960::Reset()
{
	m_fatal = false;
	m_stalled = false;

	m_SAT        = m_program.read_dword(0);
	m_PRCB       = m_program.read_dword(4);
	m_IP         = m_program.read_dword(12);
	m_PC         = 0x001f2002;
	m_AC         = 0;
	m_ICR        = 0xff000000;
	m_immediate_irq = 0;

	memset(m_r, 0, sizeof(m_r));
	memset(m_rcache, 0, sizeof(m_rcache));

	m_r[I960_FP] = m_program.read_dword(m_PRCB+24);
	m_r[I960_SP] = m_r[I960_FP] + 64;
	m_rcache_pos = 0;

	m_stall_state.burst_mode = false;

	// (device_start)
	m_immediate_vector = 0;
	m_immediate_pri = 0;
	std::fill(std::begin(m_rcache_frame_addr), std::end(m_rcache_frame_addr), 0);
	std::fill(std::begin(m_fp), std::end(m_fp), 0.0);
	std::fill(std::begin(m_irq_line_state), std::end(m_irq_line_state), CLEAR_LINE);
	m_PIP = 0;
}

int CI960::Run(int cycles)
{
	if (m_fatal)
		return cycles;
	m_icount = cycles;
	m_cyclesRequested = cycles;
	m_inRun = true;
	try
	{
		execute_run();
	}
	catch (const I960FatalError &)
	{
		m_fatal = true;
		m_icount = 0;
	}
	m_inRun = false;
	int used = m_cyclesRequested - m_icount;
	m_icount = 0;
	return used;
}

// Supermodel: MAME keeps retrying the stalled instruction until the slice runs
// out; nothing can change before the other chips run, so the slice ends here,
// and only the cycles run up to the stall are used
void CI960::Stall()
{
	i960_stall();
	m_cyclesRequested -= m_icount;
	m_icount = 0;
}

void CI960::SetIRQLine(int line, bool asserted)
{
	if (line >= 0 && line < 4)
		execute_set_input(line, asserted ? ASSERT_LINE : CLEAR_LINE);
}

UINT32 CI960::GetReg(unsigned n) const
{
	if (n < 32)
		return m_r[n];
	switch (n)
	{
	case REG_SAT:  return m_SAT;
	case REG_PRCB: return m_PRCB;
	case REG_PC:   return m_PC;
	case REG_AC:   return m_AC;
	case REG_IP:   return m_IP;
	case REG_PIP:  return m_PIP;
	default:       return 0;
	}
}

void CI960::SetReg(unsigned n, UINT32 v)
{
	if (n < 32)
	{
		m_r[n] = v;
		return;
	}
	switch (n)
	{
	case REG_SAT:  m_SAT = v; break;
	case REG_PRCB: m_PRCB = v; break;
	case REG_PC:   m_PC = v; break;
	case REG_AC:   m_AC = v; break;
	case REG_IP:   m_IP = v; break;
	case REG_PIP:  m_PIP = v; break;
	default:       break;
	}
}

const char *CI960::GetRegName(unsigned n)
{
	static const char *const names[NUM_REGS] = {
		"pfp", "sp", "rip", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
		"g0", "g1", "g2", "g3", "g4", "g5", "g6", "g7", "g8", "g9", "g10", "g11", "g12", "g13", "g14", "fp",
		"sat", "prcb", "pc", "ac", "ip", "pip"
	};
	return n < NUM_REGS ? names[n] : "?";
}

std::string CI960::Disassemble(UINT32 pc, unsigned *length)
{
	return CI960Disassembler::Disassemble(pc, m_program.read_dword(pc), m_program.read_dword(pc + 4), length);
}

void CI960::SaveState(CBlockFile *StateFile, const char *name)
{
	StateFile->NewBlock(name, __FILE__);
	StateFile->Write(m_IP);
	StateFile->Write(m_PIP);
	StateFile->Write(m_SAT);
	StateFile->Write(m_PRCB);
	StateFile->Write(m_PC);
	StateFile->Write(m_AC);
	StateFile->Write(m_ICR);
	StateFile->Write(m_r);
	StateFile->Write(m_fp);
	StateFile->Write(m_rcache);
	StateFile->Write(m_rcache_frame_addr);
	StateFile->Write(m_rcache_pos);
	StateFile->Write(m_immediate_irq);
	StateFile->Write(m_immediate_vector);
	StateFile->Write(m_immediate_pri);
	StateFile->Write(m_stalled);
	StateFile->Write(m_stall_state.index);
	StateFile->Write(m_stall_state.size);
	StateFile->Write(m_stall_state.t1);
	StateFile->Write(m_stall_state.t2);
	StateFile->Write(m_stall_state.burst_mode);
	StateFile->Write(m_stall_state.iswriteop);
	StateFile->Write(m_irq_line_state);
}

void CI960::LoadState(CBlockFile *StateFile, const char *name)
{
	if (Result::OKAY != StateFile->FindBlock(name))
	{
		ErrorLog("Unable to load i960 state. Save state file is corrupt.");
		return;
	}
	StateFile->Read(m_IP);
	StateFile->Read(m_PIP);
	StateFile->Read(m_SAT);
	StateFile->Read(m_PRCB);
	StateFile->Read(m_PC);
	StateFile->Read(m_AC);
	StateFile->Read(m_ICR);
	StateFile->Read(m_r);
	StateFile->Read(m_fp);
	StateFile->Read(m_rcache);
	StateFile->Read(m_rcache_frame_addr);
	StateFile->Read(m_rcache_pos);
	StateFile->Read(m_immediate_irq);
	StateFile->Read(m_immediate_vector);
	StateFile->Read(m_immediate_pri);
	StateFile->Read(m_stalled);
	StateFile->Read(m_stall_state.index);
	StateFile->Read(m_stall_state.size);
	StateFile->Read(m_stall_state.t1);
	StateFile->Read(m_stall_state.t2);
	StateFile->Read(m_stall_state.burst_mode);
	StateFile->Read(m_stall_state.iswriteop);
	StateFile->Read(m_irq_line_state);
	m_fatal = false;
}
