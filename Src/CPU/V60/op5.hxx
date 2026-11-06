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
 * op5.hxx
 *
 *  HALT: must add log
 *
 * Based on original work by Farfetch'd and R. Belmont in MAME. Portions
 * Copyright (c) Farfetch'd and R. Belmont, used under the BSD 3-Clause
 * License: see Docs/README.txt.
 *
 * Ported to Supermodel from MAME (src/devices/cpu/v60/op5.hxx).
 */

uint32_t CV60::opBRK()
{
/*
    uint32_t oldPSW = v60_update_psw_for_exception(0, 0);

    SP -=4;
    m_program->write_dword_unaligned(SP, EXCEPTION_CODE_AND_SIZE(0x0d00, 4));
    SP -=4;
    m_program->write_dword_unaligned(SP, oldPSW);
    SP -=4;
    m_program->write_dword_unaligned(SP, PC + 1);
    PC = GETINTVECT(13);
*/
	logerror("Skipping BRK opcode! PC=%x", PC);

	return 1;
}

uint32_t CV60::opBRKV()
{
	uint32_t oldPSW = v60_update_psw_for_exception(0, 0);

	SP -=4;
	m_program->write_dword_unaligned(SP, PC);
	SP -=4;
	m_program->write_dword_unaligned(SP, EXCEPTION_CODE_AND_SIZE(0x1501, 4));
	SP -=4;
	m_program->write_dword_unaligned(SP, oldPSW);
	SP -=4;
	m_program->write_dword_unaligned(SP, PC + 1);
	PC = GETINTVECT(21);

	return 0;
}

uint32_t CV60::opCLRTLBA()
{
	// @@@ TLB not yet supported
	logerror("Skipping CLRTLBA opcode! PC=%x\n", PC);
	return 1;
}

uint32_t CV60::opDISPOSE()
{
	SP = FP;
	FP = m_program->read_dword_unaligned(SP);
	SP +=4;

	return 1;
}

uint32_t CV60::opHALT()
{
	// @@@ It should wait for an interrupt to occur
	//logerror("HALT found: skipping");
	return 1;
}

uint32_t CV60::opNOP() /* TRUSTED */
{
	return 1;
}

uint32_t CV60::opRSR()
{
	PC = m_program->read_dword_unaligned(SP);
	SP +=4;

	return 0;
}

uint32_t CV60::opTRAPFL()
{
	if ((TKCW & 0x1F0) & ((v60ReadPSW() & 0x1F00) >> 4))
	{
		// @@@ FPU exception
		fatalerror("Hit TRAPFL! PC=%x\n", PC);
	}

	return 1;
}
