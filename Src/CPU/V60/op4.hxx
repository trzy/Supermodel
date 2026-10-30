// license:BSD-3-Clause
// copyright-holders:Farfetch'd, R. Belmont
// Ported to Supermodel from MAME (src/devices/cpu/v60/op4.hxx).
/*
    FULLY TRUSTED
*/

uint32_t CV60::opBGT8() /* TRUSTED */
{
	NORMALIZEFLAGS();

	if (!((_S ^ _OV) | _Z))
	{
		PC += (int8_t)OpRead8(PC + 1);
		return 0;
	}

	return 2;
}

uint32_t CV60::opBGT16() /* TRUSTED */
{
	NORMALIZEFLAGS();

	if (!((_S ^ _OV) | _Z))
	{
		PC += (int16_t)OpRead16(PC + 1);
		return 0;
	}

	return 3;
}


uint32_t CV60::opBGE8() /* TRUSTED */
{
	NORMALIZEFLAGS();

	if (!(_S ^ _OV))
	{
		PC += (int8_t)OpRead8(PC + 1);
		return 0;
	}

	return 2;
}

uint32_t CV60::opBGE16() /* TRUSTED */
{
	NORMALIZEFLAGS();

	if (!(_S ^ _OV))
	{
		PC += (int16_t)OpRead16(PC + 1);
		return 0;
	}

	return 3;
}

uint32_t CV60::opBLT8() /* TRUSTED */
{
	NORMALIZEFLAGS();

	if ((_S ^ _OV))
	{
		PC += (int8_t)OpRead8(PC + 1);
		return 0;
	}

	return 2;
}

uint32_t CV60::opBLT16() /* TRUSTED */
{
	NORMALIZEFLAGS();

	if ((_S ^ _OV))
	{
		PC += (int16_t)OpRead16(PC + 1);
		return 0;
	}

	return 3;
}


uint32_t CV60::opBLE8() /* TRUSTED */
{
	NORMALIZEFLAGS();

	if (((_S ^ _OV) | _Z))
	{
		PC += (int8_t)OpRead8(PC + 1);
		return 0;
	}

	return 2;
}

uint32_t CV60::opBLE16() /* TRUSTED */
{
	NORMALIZEFLAGS();

	if (((_S ^ _OV) | _Z))
	{
		PC += (int16_t)OpRead16(PC + 1);
		return 0;
	}

	return 3;
}

uint32_t CV60::opBH8() /* TRUSTED */
{
	if (!(_CY | _Z))
	{
		PC += (int8_t)OpRead8(PC + 1);
		return 0;
	}

	return 2;
}

uint32_t CV60::opBH16() /* TRUSTED */
{
	if (!(_CY | _Z))
	{
		PC += (int16_t)OpRead16(PC + 1);
		return 0;
	}

	return 3;
}

uint32_t CV60::opBNH8() /* TRUSTED */
{
	if ((_CY | _Z))
	{
		PC += (int8_t)OpRead8(PC + 1);
		return 0;
	}

	return 2;
}

uint32_t CV60::opBNH16() /* TRUSTED */
{
	if ((_CY | _Z))
	{
		PC += (int16_t)OpRead16(PC + 1);
		return 0;
	}

	return 3;
}

uint32_t CV60::opBNL8() /* TRUSTED */
{
	if (!(_CY))
	{
		PC += (int8_t)OpRead8(PC + 1);
		return 0;
	}

	return 2;
}

uint32_t CV60::opBNL16() /* TRUSTED */
{
	if (!(_CY))
	{
		PC += (int16_t)OpRead16(PC + 1);
		return 0;
	}

	return 3;
}

uint32_t CV60::opBL8() /* TRUSTED */
{
	if ((_CY))
	{
		PC += (int8_t)OpRead8(PC + 1);
		return 0;
	}

	return 2;
}

uint32_t CV60::opBL16() /* TRUSTED */
{
	if ((_CY))
	{
		PC += (int16_t)OpRead16(PC + 1);
		return 0;
	}

	return 3;
}

uint32_t CV60::opBNE8() /* TRUSTED */
{
	if (!(_Z))
	{
		PC += (int8_t)OpRead8(PC + 1);
		return 0;
	}

	return 2;
}

uint32_t CV60::opBNE16() /* TRUSTED */
{
	if (!(_Z))
	{
		PC += (int16_t)OpRead16(PC + 1);
		return 0;
	}

	return 3;
}

uint32_t CV60::opBE8() /* TRUSTED */
{
	if ((_Z))
	{
		PC += (int8_t)OpRead8(PC + 1);
		return 0;
	}

	return 2;
}

uint32_t CV60::opBE16() /* TRUSTED */
{
	if ((_Z))
	{
		PC += (int16_t)OpRead16(PC + 1);
		return 0;
	}

	return 3;
}

uint32_t CV60::opBNV8() /* TRUSTED */
{
	if (!(_OV))
	{
		PC += (int8_t)OpRead8(PC + 1);
		return 0;
	}

	return 2;
}

uint32_t CV60::opBNV16() /* TRUSTED */
{
	if (!(_OV))
	{
		PC += (int16_t)OpRead16(PC + 1);
		return 0;
	}

	return 3;
}

uint32_t CV60::opBV8() /* TRUSTED */
{
	if ((_OV))
	{
		PC += (int8_t)OpRead8(PC + 1);
		return 0;
	}

	return 2;
}

uint32_t CV60::opBV16() /* TRUSTED */
{
	if ((_OV))
	{
		PC += (int16_t)OpRead16(PC + 1);
		return 0;
	}

	return 3;
}

uint32_t CV60::opBP8() /* TRUSTED */
{
	if (!(_S))
	{
		PC += (int8_t)OpRead8(PC + 1);
		return 0;
	}

	return 2;
}

uint32_t CV60::opBP16() /* TRUSTED */
{
	if (!(_S))
	{
		PC += (int16_t)OpRead16(PC + 1);
		return 0;
	}

	return 3;
}

uint32_t CV60::opBN8() /* TRUSTED */
{
	if ((_S))
	{
		PC += (int8_t)OpRead8(PC + 1);
		return 0;
	}

	return 2;
}

uint32_t CV60::opBN16() /* TRUSTED */
{
	if ((_S))
	{
		PC += (int16_t)OpRead16(PC + 1);
		return 0;
	}

	return 3;
}

uint32_t CV60::opBR8() /* TRUSTED */
{
	PC += (int8_t)OpRead8(PC + 1);
	return 0;
}

uint32_t CV60::opBR16() /* TRUSTED */
{
	PC += (int16_t)OpRead16(PC + 1);
	return 0;
}

uint32_t CV60::opBSR() /* TRUSTED */
{
	// Save Next PC onto the stack
	SP -= 4;
	m_program->write_dword_unaligned(SP, PC + 3);

	// Jump to subroutine
	PC += (int16_t)OpRead16(PC + 1);
	return 0;
}
