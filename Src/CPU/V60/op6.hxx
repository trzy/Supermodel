// license:BSD-3-Clause
// copyright-holders:Farfetch'd, R. Belmont
// Ported to Supermodel from MAME (src/devices/cpu/v60/op6.hxx).
/*
    FULLY TRUSTED
*/

uint32_t CV60::opTB(int reg) /* TRUSTED */
{
	if (m_reg[reg] == 0)
	{
		PC += (int16_t)OpRead16(PC + 2);
		return 0;
	}

	return 4;
}

uint32_t CV60::opDBGT(int reg) /* TRUSTED */
{
	m_reg[reg]--;

	NORMALIZEFLAGS();
	if ((m_reg[reg] != 0) && !((_S ^ _OV) | _Z))
	{
		PC += (int16_t)OpRead16(PC + 2);
		return 0;
	}

	return 4;
}

uint32_t CV60::opDBLE(int reg) /* TRUSTED */
{
	m_reg[reg]--;

	NORMALIZEFLAGS();
	if ((m_reg[reg] != 0) && ((_S ^ _OV) | _Z))
	{
		PC += (int16_t)OpRead16(PC + 2);
		return 0;
	}

	return 4;
}


uint32_t CV60::opDBGE(int reg) /* TRUSTED */
{
	m_reg[reg]--;

	NORMALIZEFLAGS();
	if ((m_reg[reg] != 0) && !(_S ^ _OV))
	{
		PC += (int16_t)OpRead16(PC + 2);
		return 0;
	}

	return 4;
}

uint32_t CV60::opDBLT(int reg) /* TRUSTED */
{
	m_reg[reg]--;

	NORMALIZEFLAGS();
	if ((m_reg[reg] != 0) && (_S ^ _OV))
	{
		PC += (int16_t)OpRead16(PC + 2);
		return 0;
	}

	return 4;
}

uint32_t CV60::opDBH(int reg) /* TRUSTED */
{
	m_reg[reg]--;

	if ((m_reg[reg] != 0) && !(_CY | _Z))
	{
		PC += (int16_t)OpRead16(PC + 2);
		return 0;
	}

	return 4;
}

uint32_t CV60::opDBNH(int reg) /* TRUSTED */
{
	m_reg[reg]--;

	if ((m_reg[reg] != 0) && (_CY | _Z))
	{
		PC += (int16_t)OpRead16(PC + 2);
		return 0;
	}

	return 4;
}


uint32_t CV60::opDBL(int reg) /* TRUSTED */
{
	m_reg[reg]--;

	if ((m_reg[reg] != 0) && (_CY))
	{
		PC += (int16_t)OpRead16(PC + 2);
		return 0;
	}

	return 4;
}

uint32_t CV60::opDBNL(int reg) /* TRUSTED */
{
	m_reg[reg]--;

	if ((m_reg[reg] != 0) && !(_CY))
	{
		PC += (int16_t)OpRead16(PC + 2);
		return 0;
	}

	return 4;
}

uint32_t CV60::opDBE(int reg) /* TRUSTED */
{
	m_reg[reg]--;

	if ((m_reg[reg] != 0) && (_Z))
	{
		PC += (int16_t)OpRead16(PC + 2);
		return 0;
	}

	return 4;
}

uint32_t CV60::opDBNE(int reg) /* TRUSTED */
{
	m_reg[reg]--;

	if ((m_reg[reg] != 0) && !(_Z))
	{
		PC += (int16_t)OpRead16(PC + 2);
		return 0;
	}

	return 4;
}

uint32_t CV60::opDBV(int reg) /* TRUSTED */
{
	m_reg[reg]--;

	if ((m_reg[reg] != 0) && (_OV))
	{
		PC += (int16_t)OpRead16(PC + 2);
		return 0;
	}

	return 4;
}

uint32_t CV60::opDBNV(int reg) /* TRUSTED */
{
	m_reg[reg]--;

	if ((m_reg[reg] != 0) && !(_OV))
	{
		PC += (int16_t)OpRead16(PC + 2);
		return 0;
	}

	return 4;
}

uint32_t CV60::opDBN(int reg) /* TRUSTED */
{
	m_reg[reg]--;

	if ((m_reg[reg] != 0) && (_S))
	{
		PC += (int16_t)OpRead16(PC + 2);
		return 0;
	}

	return 4;
}

uint32_t CV60::opDBP(int reg) /* TRUSTED */
{
	m_reg[reg]--;

	if ((m_reg[reg] != 0) && !(_S))
	{
		PC += (int16_t)OpRead16(PC + 2);
		return 0;
	}

	return 4;
}

uint32_t CV60::opDBR(int reg) /* TRUSTED */
{
	m_reg[reg]--;

	if (m_reg[reg] != 0)
	{
		PC += (int16_t)OpRead16(PC + 2);
		return 0;
	}

	return 4;
}

const CV60::op6_func CV60::s_OpC6Table[8] = /* TRUSTED */
{
	&CV60::opDBV,
	&CV60::opDBL,
	&CV60::opDBE,
	&CV60::opDBNH,
	&CV60::opDBN,
	&CV60::opDBR,
	&CV60::opDBLT,
	&CV60::opDBLE
};

const CV60::op6_func CV60::s_OpC7Table[8] = /* TRUSTED */
{
	&CV60::opDBNV,
	&CV60::opDBNL,
	&CV60::opDBNE,
	&CV60::opDBH,
	&CV60::opDBP,
	&CV60::opTB,
	&CV60::opDBGE,
	&CV60::opDBGT
};


uint32_t CV60::opC6() /* TRUSTED */
{
	uint8_t appb = OpRead8(PC + 1);
	return (this->*s_OpC6Table[appb >> 5])(appb & 0x1f);
}

uint32_t CV60::opC7() /* TRUSTED */
{
	uint8_t appb = OpRead8(PC + 1);
	return (this->*s_OpC7Table[appb >> 5])(appb & 0x1f);
}
