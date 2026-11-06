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
 * am.hxx
 *
 * Based on original work by Farfetch'd and R. Belmont in MAME. Portions
 * Copyright (c) Farfetch'd and R. Belmont, used under the BSD 3-Clause
 * License: see Docs/README.txt.
 *
 * Ported to Supermodel from MAME (src/devices/cpu/v60/am.hxx).
 */

// NOTE for bit string / field addressing
// ************************************
// m_moddim must be passed as 10 for bit string instructions,
// and as 11 for bit field instructions




// Addressing mode functions and tables
#include "am1.hxx" // ReadAM
#include "am2.hxx" // ReadAMAddress
#include "am3.hxx" // WriteAM

/*
  Input:
  m_modadd
    m_moddim

  Output:
    m_amout
    amLength
*/

uint32_t CV60::ReadAM()
{
	m_modm = m_modm?1:0;
	m_modval = OpRead8(m_modadd);
	return (this->*s_AMTable1[m_modm][m_modval >> 5])();
}

uint32_t CV60::BitReadAM()
{
	m_modm = m_modm?1:0;
	m_modval = OpRead8(m_modadd);
	return (this->*s_BAMTable1[m_modm][m_modval >> 5])();
}



/*
  Input:
  m_modadd
    m_moddim

  Output:
    m_amout
    m_amflag
    amLength
*/

uint32_t CV60::ReadAMAddress()
{
	m_modm = m_modm?1:0;
	m_modval = OpRead8(m_modadd);
	return (this->*s_AMTable2[m_modm][m_modval >> 5])();
}

uint32_t CV60::BitReadAMAddress()
{
	m_modm = m_modm?1:0;
	m_modval = OpRead8(m_modadd);
	return (this->*s_BAMTable2[m_modm][m_modval >> 5])();
}

/*
  Input:
  m_modadd
    m_moddim
    m_modwritevalb / H/W

  Output:
    m_amout
    amLength
*/

uint32_t CV60::WriteAM()
{
	m_modm = m_modm?1:0;
	m_modval = OpRead8(m_modadd);
	return (this->*s_AMTable3[m_modm][m_modval >> 5])();
}
