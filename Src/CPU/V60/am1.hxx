// license:BSD-3-Clause
// copyright-holders:Farfetch'd, R. Belmont
// Ported to Supermodel from MAME (src/devices/cpu/v60/am1.hxx).
// AM1 Functions (for ReadAM)
// **************************

uint32_t CV60::am1Register()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = (uint8_t)m_reg[m_modval & 0x1F];
		break;
	case 1:
		m_amout = (uint16_t)m_reg[m_modval & 0x1F];
		break;
	case 2:
		m_amout = m_reg[m_modval & 0x1F];
		break;
	}

	return 1;
}

uint32_t CV60::am1RegisterIndirect()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_reg[m_modval & 0x1F]);
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(m_reg[m_modval & 0x1F]);
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F]);
		break;
	}

	return 1;
}

uint32_t CV60::bam1RegisterIndirect()
{
	m_bamoffset = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F]);
	return 1;
}

uint32_t CV60::am1RegisterIndirectIndexed()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_reg[m_modval2 & 0x1F] + m_reg[m_modval & 0x1F]);
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(m_reg[m_modval2 & 0x1F] + m_reg[m_modval & 0x1F] * 2);
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + m_reg[m_modval & 0x1F] * 4);
		break;
	}

	return 2;
}

uint32_t CV60::bam1RegisterIndirectIndexed()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + m_bamoffset / 8);
	m_bamoffset&=7;
	return 2;
}

uint32_t CV60::am1Autoincrement()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_reg[m_modval & 0x1F]);
		m_reg[m_modval & 0x1F]++;
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(m_reg[m_modval & 0x1F]);
		m_reg[m_modval & 0x1F] +=2;
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F]);
		m_reg[m_modval & 0x1F] +=4;
		break;
	}

	return 1;
}

uint32_t CV60::bam1Autoincrement()
{
	m_bamoffset = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F]);
	switch (m_moddim)
	{
	case 10:
		m_reg[m_modval & 0x1F] +=1;
		break;
	case 11:
		m_reg[m_modval & 0x1F] +=4;
		break;
	default:
		fatalerror("CPU - BAM1 - 7\n");
		break;
	}
	return 1;
}

uint32_t CV60::am1Autodecrement()
{
	switch (m_moddim)
	{
	case 0:
		m_reg[m_modval & 0x1F]--;
		m_amout = m_program->read_byte(m_reg[m_modval & 0x1F]);
		break;
	case 1:
		m_reg[m_modval & 0x1F]-=2;
		m_amout = m_program->read_word_unaligned(m_reg[m_modval & 0x1F]);
		break;
	case 2:
		m_reg[m_modval & 0x1F]-=4;
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F]);
		break;
	}

	return 1;
}

uint32_t CV60::bam1Autodecrement()
{
	m_bamoffset = 0;
	switch (m_moddim)
	{
	case 10:
		m_reg[m_modval & 0x1F]-=1;
		break;
	case 11:
		m_reg[m_modval & 0x1F]-=4;
		break;
	default:
		fatalerror("CPU - BAM1 - 7\n");
		break;
	}
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F]);
	return 1;
}

uint32_t CV60::am1Displacement8()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1));
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1));
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1));
		break;
	}

	return 2;
}

uint32_t CV60::bam1Displacement8()
{
	m_bamoffset = m_program->read_byte(m_modadd + 1);
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + m_bamoffset / 8);
	m_bamoffset&=7;
	return 2;
}


uint32_t CV60::am1Displacement16()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1));
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1));
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1));
		break;
	}

	return 3;
}

uint32_t CV60::bam1Displacement16()
{
	m_bamoffset = OpRead16(m_modadd + 1);
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + m_bamoffset / 8);
	m_bamoffset&=7;
	return 3;
}

uint32_t CV60::am1Displacement32()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1));
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1));
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1));
		break;
	}

	return 5;
}

uint32_t CV60::bam1Displacement32()
{
	m_bamoffset = OpRead32(m_modadd + 1);
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + m_bamoffset / 8);
	m_bamoffset&=7;
	return 5;
}

uint32_t CV60::am1DisplacementIndexed8()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2) + m_reg[m_modval & 0x1F]);
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2) + m_reg[m_modval & 0x1F] * 2);
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2) + m_reg[m_modval & 0x1F] * 4);
		break;
	}

	return 3;
}

uint32_t CV60::bam1DisplacementIndexed8()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 3;
}

uint32_t CV60::am1DisplacementIndexed16()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2) + m_reg[m_modval & 0x1F]);
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2) + m_reg[m_modval & 0x1F] * 2);
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2) + m_reg[m_modval & 0x1F] * 4);
		break;
	}

	return 4;
}

uint32_t CV60::bam1DisplacementIndexed16()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 4;
}

uint32_t CV60::am1DisplacementIndexed32()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F]);
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F] * 2);
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F] * 4);
		break;
	}

	return 6;
}

uint32_t CV60::bam1DisplacementIndexed32()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 6;
}


uint32_t CV60::am1PCDisplacement8()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(PC + (int8_t)OpRead8(m_modadd + 1));
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(PC + (int8_t)OpRead8(m_modadd + 1));
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1));
		break;
	}

	return 2;
}

uint32_t CV60::bam1PCDisplacement8()
{
	m_bamoffset = OpRead8(m_modadd + 1);
	m_amout = m_program->read_dword_unaligned(PC + m_bamoffset / 8);
	m_bamoffset&=7;
	return 2;
}

uint32_t CV60::am1PCDisplacement16()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(PC + (int16_t)OpRead16(m_modadd + 1));
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(PC + (int16_t)OpRead16(m_modadd + 1));
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1));
		break;
	}

	return 3;
}

uint32_t CV60::bam1PCDisplacement16()
{
	m_bamoffset = OpRead16(m_modadd + 1);
	m_amout = m_program->read_dword_unaligned(PC + m_bamoffset / 8);
	m_bamoffset&=7;
	return 3;
}

uint32_t CV60::am1PCDisplacement32()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(PC + OpRead32(m_modadd + 1));
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(PC + OpRead32(m_modadd + 1));
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1));
		break;
	}

	return 5;
}

uint32_t CV60::bam1PCDisplacement32()
{
	m_bamoffset = OpRead32(m_modadd + 1);
	m_amout = m_program->read_dword_unaligned(PC + m_bamoffset / 8);
	m_bamoffset&=7;
	return 5;
}

uint32_t CV60::am1PCDisplacementIndexed8()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(PC + (int8_t)OpRead8(m_modadd + 2) + m_reg[m_modval & 0x1F]);
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(PC + (int8_t)OpRead8(m_modadd + 2) + m_reg[m_modval & 0x1F] * 2);
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 2) + m_reg[m_modval & 0x1F] * 4);
		break;
	}

	return 3;
}

uint32_t CV60::bam1PCDisplacementIndexed8()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 2) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 3;
}


uint32_t CV60::am1PCDisplacementIndexed16()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(PC + (int16_t)OpRead16(m_modadd + 2) + m_reg[m_modval & 0x1F]);
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(PC + (int16_t)OpRead16(m_modadd + 2) + m_reg[m_modval & 0x1F] * 2);
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 2) + m_reg[m_modval & 0x1F] * 4);
		break;
	}

	return 4;
}

uint32_t CV60::bam1PCDisplacementIndexed16()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 2) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 4;
}

uint32_t CV60::am1PCDisplacementIndexed32()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(PC + OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F]);
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(PC + OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F] * 2);
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F] * 4);
		break;
	}

	return 6;
}

uint32_t CV60::bam1PCDisplacementIndexed32()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 2) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 6;
}

uint32_t CV60::am1DisplacementIndirect8()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1)));
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1)));
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1)));
		break;
	}

	return 2;
}

uint32_t CV60::bam1DisplacementIndirect8()
{
	m_bamoffset = 0;
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1)));
	return 2;
}

uint32_t CV60::am1DisplacementIndirect16()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1)));
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1)));
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1)));
		break;
	}

	return 3;
}

uint32_t CV60::bam1DisplacementIndirect16()
{
	m_bamoffset = 0;
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1)));
	return 3;
}

uint32_t CV60::am1DisplacementIndirect32()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1)));
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1)));
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1)));
		break;
	}

	return 5;
}

uint32_t CV60::bam1DisplacementIndirect32()
{
	m_bamoffset = 0;
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1)));
	return 5;
}

uint32_t CV60::am1DisplacementIndirectIndexed8()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2)) + m_reg[m_modval & 0x1F]);
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 2);
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 4);
		break;
	}

	return 3;
}

uint32_t CV60::bam1DisplacementIndirectIndexed8()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2)) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 3;
}

uint32_t CV60::am1DisplacementIndirectIndexed16()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2)) + m_reg[m_modval & 0x1F]);
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 2);
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 4);
		break;
	}

	return 4;
}

uint32_t CV60::bam1DisplacementIndirectIndexed16()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2)) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 4;
}

uint32_t CV60::am1DisplacementIndirectIndexed32()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F]);
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 2);
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 4);
		break;
	}

	return 6;
}

uint32_t CV60::bam1DisplacementIndirectIndexed32()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2)) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 6;
}

uint32_t CV60::am1PCDisplacementIndirect8()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1)));
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1)));
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1)));
		break;
	}

	return 2;
}

uint32_t CV60::bam1PCDisplacementIndirect8()
{
	m_bamoffset = 0;
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1)));
	return 2;
}

uint32_t CV60::am1PCDisplacementIndirect16()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1)));
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1)));
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1)));
		break;
	}

	return 3;
}

uint32_t CV60::bam1PCDisplacementIndirect16()
{
	m_bamoffset = 0;
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1)));
	return 3;
}

uint32_t CV60::am1PCDisplacementIndirect32()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1)));
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1)));
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1)));
		break;
	}

	return 5;
}

uint32_t CV60::bam1PCDisplacementIndirect32()
{
	m_bamoffset = 0;
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1)));
	return 5;
}

uint32_t CV60::am1PCDisplacementIndirectIndexed8()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 2)) + m_reg[m_modval & 0x1F]);
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 2);
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 4);
		break;
	}

	return 3;
}

uint32_t CV60::bam1PCDisplacementIndirectIndexed8()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 2)) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 3;
}

uint32_t CV60::am1PCDisplacementIndirectIndexed16()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 2)) + m_reg[m_modval & 0x1F]);
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 2);
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 4);
		break;
	}

	return 4;
}

uint32_t CV60::bam1PCDisplacementIndirectIndexed16()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 2)) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 4;
}

uint32_t CV60::am1PCDisplacementIndirectIndexed32()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F]);
		break;
	case 1:
		m_amout = m_program->read_word_unaligned(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 2);
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 4);
		break;
	}

	return 6;
}

uint32_t CV60::bam1PCDisplacementIndirectIndexed32()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 2)) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 6;
}

uint32_t CV60::am1DoubleDisplacement8()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1)) + (int8_t)OpRead8(m_modadd + 2));
		break;

	case 1:
		m_amout = m_program->read_word_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1)) + (int8_t)OpRead8(m_modadd + 2));
		break;

	case 2:
		m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1)) + (int8_t)OpRead8(m_modadd + 2));
		break;
	}

	return 3;
}

uint32_t CV60::bam1DoubleDisplacement8()
{
	m_bamoffset = OpRead8(m_modadd + 2);
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1)) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 3;
}

uint32_t CV60::am1DoubleDisplacement16()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1)) + (int16_t)OpRead16(m_modadd + 3));
		break;

	case 1:
		m_amout = m_program->read_word_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1)) + (int16_t)OpRead16(m_modadd + 3));
		break;

	case 2:
		m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1)) + (int16_t)OpRead16(m_modadd + 3));
		break;
	}

	return 5;
}

uint32_t CV60::bam1DoubleDisplacement16()
{
	m_bamoffset = OpRead16(m_modadd + 3);
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1)) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 5;
}

uint32_t CV60::am1DoubleDisplacement32()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1)) + OpRead32(m_modadd + 5));
		break;

	case 1:
		m_amout = m_program->read_word_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1)) + OpRead32(m_modadd + 5));
		break;

	case 2:
		m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1)) + OpRead32(m_modadd + 5));
		break;
	}

	return 9;
}

uint32_t CV60::bam1DoubleDisplacement32()
{
	m_bamoffset = OpRead32(m_modadd + 5);
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1)) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 9;
}

uint32_t CV60::am1PCDoubleDisplacement8()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1)) + (int8_t)OpRead8(m_modadd + 2));
		break;

	case 1:
		m_amout = m_program->read_word_unaligned(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1)) + (int8_t)OpRead8(m_modadd + 2));
		break;

	case 2:
		m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1)) + (int8_t)OpRead8(m_modadd + 2));
		break;
	}

	return 3;
}

uint32_t CV60::bam1PCDoubleDisplacement8()
{
	m_bamoffset = OpRead8(m_modadd + 2);
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1)) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 3;
}

uint32_t CV60::am1PCDoubleDisplacement16()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1)) + (int16_t)OpRead16(m_modadd + 3));
		break;

	case 1:
		m_amout = m_program->read_word_unaligned(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1)) + (int16_t)OpRead16(m_modadd + 3));
		break;

	case 2:
		m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1)) + (int16_t)OpRead16(m_modadd + 3));
		break;
	}

	return 5;
}

uint32_t CV60::bam1PCDoubleDisplacement16()
{
	m_bamoffset = OpRead16(m_modadd + 3);
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1)) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 5;
}


uint32_t CV60::am1PCDoubleDisplacement32()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1)) + OpRead32(m_modadd + 5));
		break;

	case 1:
		m_amout = m_program->read_word_unaligned(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1)) + OpRead32(m_modadd + 5));
		break;

	case 2:
		m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1)) + OpRead32(m_modadd + 5));
		break;
	}

	return 9;
}

uint32_t CV60::bam1PCDoubleDisplacement32()
{
	m_bamoffset = OpRead32(m_modadd + 5);
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1)) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 9;
}

uint32_t CV60::am1DirectAddress()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(OpRead32(m_modadd + 1));
		break;

	case 1:
		m_amout = m_program->read_word_unaligned(OpRead32(m_modadd + 1));
		break;

	case 2:
		m_amout = m_program->read_dword_unaligned(OpRead32(m_modadd + 1));
		break;
	}

	return 5;
}

uint32_t CV60::bam1DirectAddress()
{
	m_bamoffset = 0;
	m_amout = m_program->read_dword_unaligned(OpRead32(m_modadd + 1));
	return 5;
}

uint32_t CV60::am1DirectAddressIndexed()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F]);
		break;

	case 1:
		m_amout = m_program->read_word_unaligned(OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F] * 2);
		break;

	case 2:
		m_amout = m_program->read_dword_unaligned(OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F] * 4);
		break;
	}

	return 6;
}

uint32_t CV60::bam1DirectAddressIndexed()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(OpRead32(m_modadd + 2) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 6;
}

uint32_t CV60::am1DirectAddressDeferred()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_program->read_dword_unaligned(OpRead32(m_modadd + 1)));
		break;

	case 1:
		m_amout = m_program->read_word_unaligned(m_program->read_dword_unaligned(OpRead32(m_modadd + 1)));
		break;

	case 2:
		m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(OpRead32(m_modadd + 1)));
		break;
	}

	return 5;
}

uint32_t CV60::bam1DirectAddressDeferred()
{
	m_bamoffset = 0;
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(OpRead32(m_modadd + 1)));
	return 5;
}

uint32_t CV60::am1DirectAddressDeferredIndexed()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_byte(m_program->read_dword_unaligned(OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F]);
		break;

	case 1:
		m_amout = m_program->read_word_unaligned(m_program->read_dword_unaligned(OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 2);
		break;

	case 2:
		m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 4);
		break;
	}

	return 6;
}

uint32_t CV60::bam1DirectAddressDeferredIndexed()
{
	m_bamoffset = m_reg[m_modval & 0x1F];
	m_amout = m_program->read_dword_unaligned(m_program->read_dword_unaligned(OpRead32(m_modadd + 2)) + m_bamoffset / 8);
	m_bamoffset&=7;
	return 6;
}

uint32_t CV60::am1Immediate()
{
	switch (m_moddim)
	{
	case 0:
		m_amout = OpRead8(m_modadd + 1);
		return 2;

	case 1:
		m_amout = OpRead16(m_modadd + 1);
		return 3;

	case 2:
		m_amout = OpRead32(m_modadd + 1);
		return 5;
	}

	// It should not be here!  Written to avoid warning
	assert(0);
	return 1;
}

uint32_t CV60::am1ImmediateQuick()
{
	m_amout = m_modval & 0xF;
	return 1;
}




// AM1 Tables (for ReadAM)
// ***********************

uint32_t CV60::am1Error1()
{
	fatalerror("CPU - AM1 - 1 (PC=%06x)\n", PC);
}

uint32_t CV60::bam1Error1()
{
	fatalerror("CPU - BAM1 - 1 (PC=%06x)\n", PC);
}

uint32_t CV60::am1Error2()
{
	fatalerror("CPU - AM1 - 2 (PC=%06x)\n", PC);
}

uint32_t CV60::bam1Error2()
{
	fatalerror("CPU - BAM1 - 2 (PC=%06x)\n", PC);
}

uint32_t CV60::am1Error3()
{
	fatalerror("CPU - AM1 - 3 (PC=%06x)\n", PC);
}

uint32_t CV60::bam1Error3()
{
	fatalerror("CPU - BAM1 - 3 (PC=%06x)\n", PC);
}

uint32_t CV60::am1Error4()
{
	fatalerror("CPU - AM1 - 4 (PC=%06x)\n", PC);
}

uint32_t CV60::bam1Error4()
{
	fatalerror("CPU - BAM1 - 4 (PC=%06x)\n", PC);
}

uint32_t CV60::am1Error5()
{
	fatalerror("CPU - AM1 - 5 (PC=%06x)\n", PC);
}

uint32_t CV60::bam1Error5()
{
	fatalerror("CPU - BAM1 - 5 (PC=%06x)\n", PC);
}

uint32_t CV60::bam1Error6()
{
	fatalerror("CPU - BAM1 - 6 (PC=%06x)\n", PC);
}

const CV60::am_func CV60::s_AMTable1_G7a[16] =
{
	&CV60::am1PCDisplacementIndexed8,
	&CV60::am1PCDisplacementIndexed16,
	&CV60::am1PCDisplacementIndexed32,
	&CV60::am1DirectAddressIndexed,
	&CV60::am1Error5,
	&CV60::am1Error5,
	&CV60::am1Error5,
	&CV60::am1Error5,
	&CV60::am1PCDisplacementIndirectIndexed8,
	&CV60::am1PCDisplacementIndirectIndexed16,
	&CV60::am1PCDisplacementIndirectIndexed32,
	&CV60::am1DirectAddressDeferredIndexed,
	&CV60::am1Error5,
	&CV60::am1Error5,
	&CV60::am1Error5,
	&CV60::am1Error5
};

const CV60::am_func CV60::s_BAMTable1_G7a[16] =
{
	&CV60::bam1PCDisplacementIndexed8,
	&CV60::bam1PCDisplacementIndexed16,
	&CV60::bam1PCDisplacementIndexed32,
	&CV60::bam1DirectAddressIndexed,
	&CV60::bam1Error5,
	&CV60::bam1Error5,
	&CV60::bam1Error5,
	&CV60::bam1Error5,
	&CV60::bam1PCDisplacementIndirectIndexed8,
	&CV60::bam1PCDisplacementIndirectIndexed16,
	&CV60::bam1PCDisplacementIndirectIndexed32,
	&CV60::bam1DirectAddressDeferredIndexed,
	&CV60::bam1Error5,
	&CV60::bam1Error5,
	&CV60::bam1Error5,
	&CV60::bam1Error5
};


uint32_t CV60::am1Group7a()
{
	if (!(m_modval2 & 0x10))
		return am1Error4();

	return (this->*s_AMTable1_G7a[m_modval2 & 0xF])();
}

uint32_t CV60::bam1Group7a()
{
	if (!(m_modval2 & 0x10))
		return bam1Error4();

	return (this->*s_BAMTable1_G7a[m_modval2 & 0xF])();
}

const CV60::am_func CV60::s_AMTable1_G7[32] =
{
	&CV60::am1ImmediateQuick,
	&CV60::am1ImmediateQuick,
	&CV60::am1ImmediateQuick,
	&CV60::am1ImmediateQuick,
	&CV60::am1ImmediateQuick,
	&CV60::am1ImmediateQuick,
	&CV60::am1ImmediateQuick,
	&CV60::am1ImmediateQuick,
	&CV60::am1ImmediateQuick,
	&CV60::am1ImmediateQuick,
	&CV60::am1ImmediateQuick,
	&CV60::am1ImmediateQuick,
	&CV60::am1ImmediateQuick,
	&CV60::am1ImmediateQuick,
	&CV60::am1ImmediateQuick,
	&CV60::am1ImmediateQuick,
	&CV60::am1PCDisplacement8,
	&CV60::am1PCDisplacement16,
	&CV60::am1PCDisplacement32,
	&CV60::am1DirectAddress,
	&CV60::am1Immediate,
	&CV60::am1Error2,
	&CV60::am1Error2,
	&CV60::am1Error2,
	&CV60::am1PCDisplacementIndirect8,
	&CV60::am1PCDisplacementIndirect16,
	&CV60::am1PCDisplacementIndirect32,
	&CV60::am1DirectAddressDeferred,
	&CV60::am1PCDoubleDisplacement8,
	&CV60::am1PCDoubleDisplacement16,
	&CV60::am1PCDoubleDisplacement32,
	&CV60::am1Error2
};

const CV60::am_func CV60::s_BAMTable1_G7[32] =
{
	&CV60::bam1Error6,
	&CV60::bam1Error6,
	&CV60::bam1Error6,
	&CV60::bam1Error6,
	&CV60::bam1Error6,
	&CV60::bam1Error6,
	&CV60::bam1Error6,
	&CV60::bam1Error6,
	&CV60::bam1Error6,
	&CV60::bam1Error6,
	&CV60::bam1Error6,
	&CV60::bam1Error6,
	&CV60::bam1Error6,
	&CV60::bam1Error6,
	&CV60::bam1Error6,
	&CV60::bam1Error6,
	&CV60::bam1PCDisplacement8,
	&CV60::bam1PCDisplacement16,
	&CV60::bam1PCDisplacement32,
	&CV60::bam1DirectAddress,
	&CV60::bam1Error6,
	&CV60::bam1Error2,
	&CV60::bam1Error2,
	&CV60::bam1Error2,
	&CV60::bam1PCDisplacementIndirect8,
	&CV60::bam1PCDisplacementIndirect16,
	&CV60::bam1PCDisplacementIndirect32,
	&CV60::bam1DirectAddressDeferred,
	&CV60::bam1PCDoubleDisplacement8,
	&CV60::bam1PCDoubleDisplacement16,
	&CV60::bam1PCDoubleDisplacement32,
	&CV60::bam1Error2
};



const CV60::am_func CV60::s_AMTable1_G6[8] =
{
	&CV60::am1DisplacementIndexed8,
	&CV60::am1DisplacementIndexed16,
	&CV60::am1DisplacementIndexed32,
	&CV60::am1RegisterIndirectIndexed,
	&CV60::am1DisplacementIndirectIndexed8,
	&CV60::am1DisplacementIndirectIndexed16,
	&CV60::am1DisplacementIndirectIndexed32,
	&CV60::am1Group7a
};

const CV60::am_func CV60::s_BAMTable1_G6[8] =
{
	&CV60::bam1DisplacementIndexed8,
	&CV60::bam1DisplacementIndexed16,
	&CV60::bam1DisplacementIndexed32,
	&CV60::bam1RegisterIndirectIndexed,
	&CV60::bam1DisplacementIndirectIndexed8,
	&CV60::bam1DisplacementIndirectIndexed16,
	&CV60::bam1DisplacementIndirectIndexed32,
	&CV60::bam1Group7a
};


uint32_t CV60::am1Group6()
{
	m_modval2 = OpRead8(m_modadd + 1);
	return (this->*s_AMTable1_G6[m_modval2 >> 5])();
}

uint32_t CV60::bam1Group6()
{
	m_modval2 = OpRead8(m_modadd + 1);
	return (this->*s_BAMTable1_G6[m_modval2 >> 5])();
}


uint32_t CV60::am1Group7()
{
	return (this->*s_AMTable1_G7[m_modval & 0x1F])();
}

uint32_t CV60::bam1Group7()
{
	return (this->*s_BAMTable1_G7[m_modval & 0x1F])();
}

const CV60::am_func CV60::s_AMTable1[2][8] =
{
	{
		&CV60::am1Displacement8,
		&CV60::am1Displacement16,
		&CV60::am1Displacement32,
		&CV60::am1RegisterIndirect,
		&CV60::am1DisplacementIndirect8,
		&CV60::am1DisplacementIndirect16,
		&CV60::am1DisplacementIndirect32,
		&CV60::am1Group7
	},

	{
		&CV60::am1DoubleDisplacement8,
		&CV60::am1DoubleDisplacement16,
		&CV60::am1DoubleDisplacement32,
		&CV60::am1Register,
		&CV60::am1Autoincrement,
		&CV60::am1Autodecrement,
		&CV60::am1Group6,
		&CV60::am1Error1
	}
};


const CV60::am_func CV60::s_BAMTable1[2][8] =
{
	{
		&CV60::bam1Displacement8,
		&CV60::bam1Displacement16,
		&CV60::bam1Displacement32,
		&CV60::bam1RegisterIndirect,
		&CV60::bam1DisplacementIndirect8,
		&CV60::bam1DisplacementIndirect16,
		&CV60::bam1DisplacementIndirect32,
		&CV60::bam1Group7
	},

	{
		&CV60::bam1DoubleDisplacement8,
		&CV60::bam1DoubleDisplacement16,
		&CV60::bam1DoubleDisplacement32,
		&CV60::bam1Error6,
		&CV60::bam1Autoincrement,
		&CV60::bam1Autodecrement,
		&CV60::bam1Group6,
		&CV60::bam1Error1
	}
};
