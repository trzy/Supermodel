// license:BSD-3-Clause
// copyright-holders:Ernesto Corvi
// Ported to Supermodel from MAME (src/devices/cpu/mb86233/mb86233d.h).

#ifndef INCLUDED_MB86233DISASM_H
#define INCLUDED_MB86233DISASM_H

#include "Types.h"
#include <string>

/*
 * MAME's mb86233_disassembler, reduced to a static function taking the
 * opcode word. One opcode is always one word (opcode_alignment() == 1);
 * MAME's step flags (STEP_COND/OVER/OUT) are not returned.
 */
class CMB86233Disassembler
{
public:
	static std::string Disassemble(UINT32 opcode);

private:
	static const char *const regnames[0x40];
	static std::string condition(unsigned int cond, bool invert);
	static std::string regs(UINT32 reg);
	static std::string memory(UINT32 reg, bool x1, bool bank);
	static std::string alu0_func(UINT32 alu);
};

#endif // INCLUDED_MB86233DISASM_H
