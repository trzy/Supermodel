// license:BSD-3-Clause
// copyright-holders:Farfetch'd, R. Belmont
// Ported to Supermodel from MAME (src/devices/cpu/v60/am3.hxx).
// AM3 Functions (for ReadAM)
// **************************

uint32_t CV60::am3Register()
{
	switch (m_moddim)
	{
	case 0:
		SETREG8(m_reg[m_modval & 0x1F], m_modwritevalb);
		break;
	case 1:
		SETREG16(m_reg[m_modval & 0x1F], m_modwritevalh);
		break;
	case 2:
		m_reg[m_modval & 0x1F] = m_modwritevalw;
		break;
	}

	return 1;
}

uint32_t CV60::am3RegisterIndirect()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_reg[m_modval & 0x1F], m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_reg[m_modval & 0x1F], m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_reg[m_modval & 0x1F], m_modwritevalw);
		break;
	}

	return 1;
}

uint32_t CV60::am3RegisterIndirectIndexed()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_reg[m_modval2 & 0x1F] + m_reg[m_modval & 0x1F], m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_reg[m_modval2 & 0x1F] + m_reg[m_modval & 0x1F] * 2, m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_reg[m_modval2 & 0x1F] + m_reg[m_modval & 0x1F] * 4, m_modwritevalw);
		break;
	}

	return 2;
}

uint32_t CV60::am3Autoincrement()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_reg[m_modval & 0x1F], m_modwritevalb);
		m_reg[m_modval & 0x1F] += 1;
		break;
	case 1:
		m_program->write_word_unaligned(m_reg[m_modval & 0x1F], m_modwritevalh);
		m_reg[m_modval & 0x1F] += 2;
		break;
	case 2:
		m_program->write_dword_unaligned(m_reg[m_modval & 0x1F], m_modwritevalw);
		m_reg[m_modval & 0x1F] += 4;
		break;
	}

	return 1;
}

uint32_t CV60::am3Autodecrement()
{
	switch (m_moddim)
	{
	case 0:
		m_reg[m_modval & 0x1F] -= 1;
		m_program->write_byte(m_reg[m_modval & 0x1F], m_modwritevalb);
		break;
	case 1:
		m_reg[m_modval & 0x1F] -= 2;
		m_program->write_word_unaligned(m_reg[m_modval & 0x1F], m_modwritevalh);
		break;
	case 2:
		m_reg[m_modval & 0x1F] -= 4;
		m_program->write_dword_unaligned(m_reg[m_modval & 0x1F], m_modwritevalw);
		break;
	}

	return 1;
}

uint32_t CV60::am3Displacement8()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1), m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1), m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1), m_modwritevalw);
		break;
	}

	return 2;
}

uint32_t CV60::am3Displacement16()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1), m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1), m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1), m_modwritevalw);
		break;
	}

	return 3;
}

uint32_t CV60::am3Displacement32()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1), m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1), m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1), m_modwritevalw);
		break;
	}

	return 5;
}


uint32_t CV60::am3DisplacementIndexed8()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2) + m_reg[m_modval & 0x1F], m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2) + m_reg[m_modval & 0x1F] * 2, m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2) + m_reg[m_modval & 0x1F] * 4, m_modwritevalw);
		break;
	}

	return 3;
}

uint32_t CV60::am3DisplacementIndexed16()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2) + m_reg[m_modval & 0x1F], m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2) + m_reg[m_modval & 0x1F] * 2, m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2) + m_reg[m_modval & 0x1F] * 4, m_modwritevalw);
		break;
	}

	return 4;
}

uint32_t CV60::am3DisplacementIndexed32()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F], m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F] * 2, m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F] * 4, m_modwritevalw);
		break;
	}

	return 6;
}


uint32_t CV60::am3PCDisplacement8()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(PC + (int8_t)OpRead8(m_modadd + 1), m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(PC + (int8_t)OpRead8(m_modadd + 1), m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1), m_modwritevalw);
		break;
	}

	return 2;
}

uint32_t CV60::am3PCDisplacement16()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(PC + (int16_t)OpRead16(m_modadd + 1), m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(PC + (int16_t)OpRead16(m_modadd + 1), m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1), m_modwritevalw);
		break;
	}

	return 3;
}

uint32_t CV60::am3PCDisplacement32()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(PC + OpRead32(m_modadd + 1), m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(PC + OpRead32(m_modadd + 1), m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(PC + OpRead32(m_modadd + 1), m_modwritevalw);
		break;
	}

	return 5;
}

uint32_t CV60::am3PCDisplacementIndexed8()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(PC + (int8_t)OpRead8(m_modadd + 2) + m_reg[m_modval & 0x1F], m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(PC + (int8_t)OpRead8(m_modadd + 2) + m_reg[m_modval & 0x1F] * 2, m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 2) + m_reg[m_modval & 0x1F] * 4, m_modwritevalw);
		break;
	}

	return 3;
}

uint32_t CV60::am3PCDisplacementIndexed16()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(PC + (int16_t)OpRead16(m_modadd + 2) + m_reg[m_modval & 0x1F], m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(PC + (int16_t)OpRead16(m_modadd + 2) + m_reg[m_modval & 0x1F] * 2, m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 2) + m_reg[m_modval & 0x1F] * 4, m_modwritevalw);
		break;
	}

	return 4;
}

uint32_t CV60::am3PCDisplacementIndexed32()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(PC + OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F], m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(PC + OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F] * 2, m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(PC + OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F] * 4, m_modwritevalw);
		break;
	}

	return 6;
}

uint32_t CV60::am3DisplacementIndirect8()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1)), m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1)), m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1)), m_modwritevalw);
		break;
	}

	return 2;
}

uint32_t CV60::am3DisplacementIndirect16()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1)), m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1)), m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1)), m_modwritevalw);
		break;
	}

	return 3;
}

uint32_t CV60::am3DisplacementIndirect32()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1)), m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1)), m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1)), m_modwritevalw);
		break;
	}

	return 5;
}


uint32_t CV60::am3DisplacementIndirectIndexed8()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2)) + m_reg[m_modval & 0x1F], m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 2, m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 4, m_modwritevalw);
		break;
	}

	return 3;
}

uint32_t CV60::am3DisplacementIndirectIndexed16()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2)) + m_reg[m_modval & 0x1F], m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 2, m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 4, m_modwritevalw);
		break;
	}

	return 4;
}

uint32_t CV60::am3DisplacementIndirectIndexed32()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F], m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 2, m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 4, m_modwritevalw);
		break;
	}

	return 6;
}

uint32_t CV60::am3PCDisplacementIndirect8()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1)), m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1)), m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1)), m_modwritevalw);
		break;
	}

	return 2;
}

uint32_t CV60::am3PCDisplacementIndirect16()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1)), m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1)), m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1)), m_modwritevalw);
		break;
	}

	return 3;
}

uint32_t CV60::am3PCDisplacementIndirect32()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1)), m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1)), m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1)), m_modwritevalw);
		break;
	}

	return 5;
}


uint32_t CV60::am3PCDisplacementIndirectIndexed8()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 2)) + m_reg[m_modval & 0x1F], m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 2, m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 4, m_modwritevalw);
		break;
	}

	return 3;
}

uint32_t CV60::am3PCDisplacementIndirectIndexed16()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 2)) + m_reg[m_modval & 0x1F], m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 2, m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 4, m_modwritevalw);
		break;
	}

	return 4;
}

uint32_t CV60::am3PCDisplacementIndirectIndexed32()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F], m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 2, m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 4, m_modwritevalw);
		break;
	}

	return 6;
}


uint32_t CV60::am3DoubleDisplacement8()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1)) + (int8_t)OpRead8(m_modadd + 2), m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1)) + (int8_t)OpRead8(m_modadd + 2), m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1)) + (int8_t)OpRead8(m_modadd + 2), m_modwritevalw);
		break;
	}

	return 3;
}

uint32_t CV60::am3DoubleDisplacement16()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1)) + (int16_t)OpRead16(m_modadd + 3), m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1)) + (int16_t)OpRead16(m_modadd + 3), m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1)) + (int16_t)OpRead16(m_modadd + 3), m_modwritevalw);
		break;
	}

	return 5;
}

uint32_t CV60::am3DoubleDisplacement32()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1)) + OpRead32(m_modadd + 5), m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1)) + OpRead32(m_modadd + 5), m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1)) + OpRead32(m_modadd + 5), m_modwritevalw);
		break;
	}

	return 9;
}


uint32_t CV60::am3PCDoubleDisplacement8()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1)) + (int8_t)OpRead8(m_modadd + 2), m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1)) + (int8_t)OpRead8(m_modadd + 2), m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1)) + (int8_t)OpRead8(m_modadd + 2), m_modwritevalw);
		break;
	}

	return 3;
}

uint32_t CV60::am3PCDoubleDisplacement16()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1)) + (int16_t)OpRead16(m_modadd + 3), m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1)) + (int16_t)OpRead16(m_modadd + 3), m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1)) + (int16_t)OpRead16(m_modadd + 3), m_modwritevalw);
		break;
	}

	return 5;
}

uint32_t CV60::am3PCDoubleDisplacement32()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1)) + OpRead32(m_modadd + 5), m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1)) + OpRead32(m_modadd + 5), m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1)) + OpRead32(m_modadd + 5), m_modwritevalw);
		break;
	}

	return 9;
}

uint32_t CV60::am3DirectAddress()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(OpRead32(m_modadd + 1), m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(OpRead32(m_modadd + 1), m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(OpRead32(m_modadd + 1), m_modwritevalw);
		break;
	}

	return 5;
}

uint32_t CV60::am3DirectAddressIndexed()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F], m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F] * 2, m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F] * 4, m_modwritevalw);
		break;
	}

	return 6;
}

uint32_t CV60::am3DirectAddressDeferred()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_program->read_dword_unaligned(OpRead32(m_modadd + 1)), m_modwritevalb);
		break;
	case 1:
		m_program->write_word_unaligned(m_program->read_dword_unaligned(OpRead32(m_modadd + 1)), m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_program->read_dword_unaligned(OpRead32(m_modadd + 1)), m_modwritevalw);
		break;
	}

	return 5;
}

uint32_t CV60::am3DirectAddressDeferredIndexed()
{
	switch (m_moddim)
	{
	case 0:
		m_program->write_byte(m_program->read_dword_unaligned(OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F], m_modwritevalb);
		break;
	// Supermodel fix: the index is scaled by the data size, as in every other
	// indexed mode (MAME doesn't scale it here)
	case 1:
		m_program->write_word_unaligned(m_program->read_dword_unaligned(OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 2, m_modwritevalh);
		break;
	case 2:
		m_program->write_dword_unaligned(m_program->read_dword_unaligned(OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 4, m_modwritevalw);
		break;
	}

	return 6;
}

uint32_t CV60::am3Immediate()
{
	fatalerror("CPU - AM3 - IMM (PC=%06x)\n", PC);
}

uint32_t CV60::am3ImmediateQuick()
{
	fatalerror("CPU - AM3 - IMMQ (PC=%06x)\n", PC);
}



// AM3 Tables (for ReadAMAddress)
// ******************************

uint32_t CV60::am3Error1()
{
	fatalerror("CPU - AM3 - 1 (PC=%06x)\n", PC);
}

uint32_t CV60::am3Error2()
{
	fatalerror("CPU - AM3 - 2 (PC=%06x)\n", PC);
}

uint32_t CV60::am3Error3()
{
	fatalerror("CPU - AM3 - 3 (PC=%06x)\n", PC);
}

uint32_t CV60::am3Error4()
{
	fatalerror("CPU - AM3 - 4 (PC=%06x)\n", PC);
}

uint32_t CV60::am3Error5()
{
	fatalerror("CPU - AM3 - 5 (PC=%06x)\n", PC);
}

const CV60::am_func CV60::s_AMTable3_G7a[16] =
{
	&CV60::am3PCDisplacementIndexed8,
	&CV60::am3PCDisplacementIndexed16,
	&CV60::am3PCDisplacementIndexed32,
	&CV60::am3DirectAddressIndexed,
	&CV60::am3Error5,
	&CV60::am3Error5,
	&CV60::am3Error5,
	&CV60::am3Error5,
	&CV60::am3PCDisplacementIndirectIndexed8,
	&CV60::am3PCDisplacementIndirectIndexed16,
	&CV60::am3PCDisplacementIndirectIndexed32,
	&CV60::am3DirectAddressDeferredIndexed,
	&CV60::am3Error5,
	&CV60::am3Error5,
	&CV60::am3Error5,
	&CV60::am3Error5
};

uint32_t CV60::am3Group7a()
{
	if (!(m_modval2 & 0x10))
		return am3Error4();

	return (this->*s_AMTable3_G7a[m_modval2 & 0xF])();
}

const CV60::am_func CV60::s_AMTable3_G7[32] =
{
	&CV60::am3ImmediateQuick,
	&CV60::am3ImmediateQuick,
	&CV60::am3ImmediateQuick,
	&CV60::am3ImmediateQuick,
	&CV60::am3ImmediateQuick,
	&CV60::am3ImmediateQuick,
	&CV60::am3ImmediateQuick,
	&CV60::am3ImmediateQuick,
	&CV60::am3ImmediateQuick,
	&CV60::am3ImmediateQuick,
	&CV60::am3ImmediateQuick,
	&CV60::am3ImmediateQuick,
	&CV60::am3ImmediateQuick,
	&CV60::am3ImmediateQuick,
	&CV60::am3ImmediateQuick,
	&CV60::am3ImmediateQuick,
	&CV60::am3PCDisplacement8,
	&CV60::am3PCDisplacement16,
	&CV60::am3PCDisplacement32,
	&CV60::am3DirectAddress,
	&CV60::am3Immediate,
	&CV60::am3Error2,
	&CV60::am3Error2,
	&CV60::am3Error2,
	&CV60::am3PCDisplacementIndirect8,
	&CV60::am3PCDisplacementIndirect16,
	&CV60::am3PCDisplacementIndirect32,
	&CV60::am3DirectAddressDeferred,
	&CV60::am3PCDoubleDisplacement8,
	&CV60::am3PCDoubleDisplacement16,
	&CV60::am3PCDoubleDisplacement32,
	&CV60::am3Error2
};

const CV60::am_func CV60::s_AMTable3_G6[8] =
{
	&CV60::am3DisplacementIndexed8,
	&CV60::am3DisplacementIndexed16,
	&CV60::am3DisplacementIndexed32,
	&CV60::am3RegisterIndirectIndexed,
	&CV60::am3DisplacementIndirectIndexed8,
	&CV60::am3DisplacementIndirectIndexed16,
	&CV60::am3DisplacementIndirectIndexed32,
	&CV60::am3Group7a
};




uint32_t CV60::am3Group6()
{
	m_modval2 = OpRead8(m_modadd + 1);
	return (this->*s_AMTable3_G6[m_modval2 >> 5])();
}


uint32_t CV60::am3Group7()
{
	return (this->*s_AMTable3_G7[m_modval & 0x1F])();
}



const CV60::am_func CV60::s_AMTable3[2][8] =
{
	{
		&CV60::am3Displacement8,
		&CV60::am3Displacement16,
		&CV60::am3Displacement32,
		&CV60::am3RegisterIndirect,
		&CV60::am3DisplacementIndirect8,
		&CV60::am3DisplacementIndirect16,
		&CV60::am3DisplacementIndirect32,
		&CV60::am3Group7
	},

	{
		&CV60::am3DoubleDisplacement8,
		&CV60::am3DoubleDisplacement16,
		&CV60::am3DoubleDisplacement32,
		&CV60::am3Register,
		&CV60::am3Autoincrement,
		&CV60::am3Autodecrement,
		&CV60::am3Group6,
		&CV60::am3Error1
	}
};
