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
 * MB86233Disasm.h
 *
 * Based on original work by Ernesto Corvi in MAME. Portions Copyright (c)
 * Ernesto Corvi, used under the BSD 3-Clause License: see Docs/README.txt.
 *
 * Ported to Supermodel from MAME (src/devices/cpu/mb86233/mb86233d.h).
 */

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
