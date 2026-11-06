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
 * I960Disasm.h
 *
 * Based on original work by Farfetch'd and R. Belmont in MAME. Portions
 * Copyright (c) Farfetch'd and R. Belmont, used under the BSD 3-Clause
 * License: see Docs/README.txt.
 *
 * Ported to Supermodel from MAME (src/devices/cpu/i960/i960dis.h).
 */

#ifndef INCLUDED_I960DISASM_H
#define INCLUDED_I960DISASM_H

#include <cstdint>
#include <ostream>
#include <string>

/*
 * MAME's i960_disassembler. Disassemble() takes the opcode word and the word
 * after it (MEMB's displacement) and returns the text; *length gets 4 or 8.
 * MAME's step flags (STEP_COND/OVER/OUT) are not returned.
 */
class CI960Disassembler
{
public:
	static std::string Disassemble(uint32_t pc, uint32_t opcode, uint32_t next, unsigned *length);

private:
	struct mnemonic_t
	{
		const char      *mnem;
		unsigned short  type;
		signed char     flags;
	};

	static const mnemonic_t mnemonic[256];
	static const mnemonic_t mnem_reg[197];
	static const char *const regnames[32];
	static const char *const fprnames[32];

	uint32_t Disassemble(std::ostream &stream, uint32_t pc, uint32_t iCode, uint32_t next);
	uint32_t dis_decode_invalid(std::ostream &stream, uint32_t iCode);
	uint32_t dis_decode_ctrl(std::ostream &stream, uint32_t iCode, uint32_t ip, signed char cnt);
	uint32_t dis_decode_cobr(std::ostream &stream, uint32_t iCode, uint32_t ip, signed char cnt);
	uint32_t dis_decode_mema(std::ostream &stream, uint32_t iCode, signed char cnt);
	uint32_t dis_decode_memb(std::ostream &stream, uint32_t iCode, uint32_t ip, uint32_t disp, signed char cnt);
	uint32_t dis_decode_reg(std::ostream &stream, uint32_t iCode);
};

#endif  // INCLUDED_I960DISASM_H
