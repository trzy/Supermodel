// license:BSD-3-Clause
// copyright-holders:Farfetch'd, R. Belmont
// Ported to Supermodel from MAME (src/devices/cpu/v60/am2.hxx).
// AM2 Functions (for ReadAMAddress)
// *********************************

uint32_t CV60::am2Register()
{
	m_amflag = 1;
	m_amout = m_modval & 0x1F;
	return 1;
}

uint32_t CV60::am2RegisterIndirect()
{
	m_amflag = 0;
	m_amout = m_reg[m_modval & 0x1F];
	return 1;
}

uint32_t CV60::bam2RegisterIndirect()
{
	m_amflag = 0;
	m_amout = m_reg[m_modval & 0x1F];
	m_bamoffset = 0;
	return 1;
}

uint32_t CV60::am2RegisterIndirectIndexed()
{
	m_amflag = 0;

	switch (m_moddim)
	{
	case 0:
		m_amout = m_reg[m_modval2 & 0x1F] + m_reg[m_modval & 0x1F];
		break;
	case 1:
		m_amout = m_reg[m_modval2 & 0x1F] + m_reg[m_modval & 0x1F] * 2;
		break;
	case 2:
		m_amout = m_reg[m_modval2 & 0x1F] + m_reg[m_modval & 0x1F] * 4;
		break;
	case 3:
		m_amout = m_reg[m_modval2 & 0x1F] + m_reg[m_modval & 0x1F] * 8;
		break;
	}

	return 2;
}

uint32_t CV60::bam2RegisterIndirectIndexed()
{
	m_amflag = 0;
	m_amout = m_reg[m_modval2 & 0x1F];
	m_bamoffset = m_reg[m_modval & 0x1F];
	return 2;
}

uint32_t CV60::am2Autoincrement()
{
	m_amflag = 0;
	m_amout = m_reg[m_modval & 0x1F];

	switch (m_moddim)
	{
	case 0:
		m_reg[m_modval & 0x1F] += 1;
		break;
	case 1:
		m_reg[m_modval & 0x1F] += 2;
		break;
	case 2:
		m_reg[m_modval & 0x1F] += 4;
		break;
	case 3:
		m_reg[m_modval & 0x1F] += 8;
		break;
	}

	return 1;
}

uint32_t CV60::bam2Autoincrement()
{
	m_amflag = 0;
	m_amout = m_reg[m_modval & 0x1F];
	m_bamoffset = 0;

	switch (m_moddim)
	{
	case 10:
		m_reg[m_modval & 0x1F] +=1;
		break;
	case 11:
		m_reg[m_modval & 0x1F] +=4;
		break;
	default:
		fatalerror("CPU - AM2 - 7 (t0 PC=%x)\n", PC);
		break;
	}

	return 1;
}

uint32_t CV60::am2Autodecrement()
{
	m_amflag = 0;

	switch (m_moddim)
	{
	case 0:
		m_reg[m_modval & 0x1F] -= 1;
		break;
	case 1:
		m_reg[m_modval & 0x1F] -= 2;
		break;
	case 2:
		m_reg[m_modval & 0x1F] -= 4;
		break;
	case 3:
		m_reg[m_modval & 0x1F] -= 8;
		break;
	}

	m_amout = m_reg[m_modval & 0x1F];
	return 1;
}

uint32_t CV60::bam2Autodecrement()
{
	m_amflag = 0;
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
		fatalerror("CPU - BAM2 - 7 (PC=%06x)\n", PC);
		break;
	}

	m_amout = m_reg[m_modval & 0x1F];
	return 1;
}


uint32_t CV60::am2Displacement8()
{
	m_amflag = 0;
	m_amout = m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1);

	return 2;
}

uint32_t CV60::bam2Displacement8()
{
	m_amflag = 0;
	m_amout = m_reg[m_modval & 0x1F];
	m_bamoffset = (int8_t)OpRead8(m_modadd + 1);

	return 2;
}

uint32_t CV60::am2Displacement16()
{
	m_amflag = 0;
	m_amout = m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1);

	return 3;
}

uint32_t CV60::bam2Displacement16()
{
	m_amflag = 0;
	m_amout = m_reg[m_modval & 0x1F];
	m_bamoffset = (int16_t)OpRead16(m_modadd + 1);

	return 3;
}

uint32_t CV60::am2Displacement32()
{
	m_amflag = 0;
	m_amout = m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1);

	return 5;
}

uint32_t CV60::bam2Displacement32()
{
	m_amflag = 0;
	m_amout = m_reg[m_modval & 0x1F];
	m_bamoffset = OpRead32(m_modadd + 1);

	return 5;
}

uint32_t CV60::am2DisplacementIndexed8()
{
	m_amflag = 0;

	switch (m_moddim)
	{
	case 0:
		m_amout = m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2) + m_reg[m_modval & 0x1F];
		break;
	case 1:
		m_amout = m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2) + m_reg[m_modval & 0x1F] * 2;
		break;
	case 2:
		m_amout = m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2) + m_reg[m_modval & 0x1F] * 4;
		break;
	case 3:
		m_amout = m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2) + m_reg[m_modval & 0x1F] * 8;
		break;
	}

	return 3;
}

uint32_t CV60::bam2DisplacementIndexed8()
{
	m_amflag = 0;
	m_amout = m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2);
	m_bamoffset = m_reg[m_modval & 0x1F];

	return 3;
}

uint32_t CV60::am2DisplacementIndexed16()
{
	m_amflag = 0;

	switch (m_moddim)
	{
	case 0:
		m_amout = m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2) + m_reg[m_modval & 0x1F];
		break;
	case 1:
		m_amout = m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2) + m_reg[m_modval & 0x1F] * 2;
		break;
	case 2:
		m_amout = m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2) + m_reg[m_modval & 0x1F] * 4;
		break;
	case 3:
		m_amout = m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2) + m_reg[m_modval & 0x1F] * 8;
		break;
	}

	return 4;
}

uint32_t CV60::bam2DisplacementIndexed16()
{
	m_amflag = 0;
	m_amout = m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2);
	m_bamoffset = m_reg[m_modval & 0x1F];

	return 4;
}

uint32_t CV60::am2DisplacementIndexed32()
{
	m_amflag = 0;

	switch (m_moddim)
	{
	case 0:
		m_amout = m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F];
		break;
	case 1:
		m_amout = m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F] * 2;
		break;
	case 2:
		m_amout = m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F] * 4;
		break;
	case 3:
		m_amout = m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F] * 8;
		break;
	}

	return 6;
}

uint32_t CV60::bam2DisplacementIndexed32()
{
	m_amflag = 0;
	m_amout = m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2);
	m_bamoffset = m_reg[m_modval & 0x1F];

	return 6;
}

uint32_t CV60::am2PCDisplacement8()
{
	m_amflag = 0;
	m_amout = PC + (int8_t)OpRead8(m_modadd + 1);

	return 2;
}

uint32_t CV60::bam2PCDisplacement8()
{
	m_amflag = 0;
	m_amout = PC;
	m_bamoffset = (int8_t)OpRead8(m_modadd + 1);

	return 2;
}

uint32_t CV60::am2PCDisplacement16()
{
	m_amflag = 0;
	m_amout = PC + (int16_t)OpRead16(m_modadd + 1);

	return 3;
}

uint32_t CV60::bam2PCDisplacement16()
{
	m_amflag = 0;
	m_amout = PC;
	m_bamoffset = (int16_t)OpRead16(m_modadd + 1);

	return 3;
}

uint32_t CV60::am2PCDisplacement32()
{
	m_amflag = 0;
	m_amout = PC + OpRead32(m_modadd + 1);

	return 5;
}

uint32_t CV60::bam2PCDisplacement32()
{
	m_amflag = 0;
	m_amout = PC;
	m_bamoffset = OpRead32(m_modadd + 1);

	return 5;
}


uint32_t CV60::am2PCDisplacementIndexed8()
{
	m_amflag = 0;

	switch (m_moddim)
	{
	case 0:
		m_amout = PC + (int8_t)OpRead8(m_modadd + 2) + m_reg[m_modval & 0x1F];
		break;
	case 1:
		m_amout = PC + (int8_t)OpRead8(m_modadd + 2) + m_reg[m_modval & 0x1F] * 2;
		break;
	case 2:
		m_amout = PC + (int8_t)OpRead8(m_modadd + 2) + m_reg[m_modval & 0x1F] * 4;
		break;
	case 3:
		m_amout = PC + (int8_t)OpRead8(m_modadd + 2) + m_reg[m_modval & 0x1F] * 8;
		break;
	}

	return 3;
}

uint32_t CV60::bam2PCDisplacementIndexed8()
{
	m_amflag = 0;
	m_amout = PC + (int8_t)OpRead8(m_modadd + 2);
	m_bamoffset = m_reg[m_modval & 0x1F];

	return 3;
}

uint32_t CV60::am2PCDisplacementIndexed16()
{
	m_amflag = 0;

	switch (m_moddim)
	{
	case 0:
		m_amout = PC + (int16_t)OpRead16(m_modadd + 2) + m_reg[m_modval & 0x1F];
		break;
	case 1:
		m_amout = PC + (int16_t)OpRead16(m_modadd + 2) + m_reg[m_modval & 0x1F] * 2;
		break;
	case 2:
		m_amout = PC + (int16_t)OpRead16(m_modadd + 2) + m_reg[m_modval & 0x1F] * 4;
		break;
	case 3:
		m_amout = PC + (int16_t)OpRead16(m_modadd + 2) + m_reg[m_modval & 0x1F] * 8;
		break;
	}

	return 4;
}

uint32_t CV60::bam2PCDisplacementIndexed16()
{
	m_amflag = 0;
	m_amout = PC + (int16_t)OpRead16(m_modadd + 2);
	m_bamoffset = m_reg[m_modval & 0x1F];

	return 4;
}

uint32_t CV60::am2PCDisplacementIndexed32()
{
	m_amflag = 0;

	switch (m_moddim)
	{
	case 0:
		m_amout = PC + OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F];
		break;
	case 1:
		m_amout = PC + OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F] * 2;
		break;
	case 2:
		m_amout = PC + OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F] * 4;
		break;
	case 3:
		m_amout = PC + OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F] * 8;
		break;
	}

	return 6;
}

uint32_t CV60::bam2PCDisplacementIndexed32()
{
	m_amflag = 0;
	m_amout = PC + OpRead32(m_modadd + 2);
	m_bamoffset = m_reg[m_modval & 0x1F];

	return 6;
}

uint32_t CV60::am2DisplacementIndirect8()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1));

	return 2;
}

uint32_t CV60::bam2DisplacementIndirect8()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1));
	m_bamoffset = 0;
	return 2;
}

uint32_t CV60::am2DisplacementIndirect16()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1));

	return 3;
}

uint32_t CV60::bam2DisplacementIndirect16()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1));
	m_bamoffset = 0;
	return 3;
}

uint32_t CV60::am2DisplacementIndirect32()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1));

	return 5;
}

uint32_t CV60::bam2DisplacementIndirect32()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1));
	m_bamoffset = 0;

	return 5;
}

uint32_t CV60::am2DisplacementIndirectIndexed8()
{
	m_amflag = 0;

	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2)) + m_reg[m_modval & 0x1F];
		break;
	case 1:
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 2;
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 4;
		break;
	case 3:
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 8;
		break;
	}

	return 3;
}

uint32_t CV60::bam2DisplacementIndirectIndexed8()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int8_t)OpRead8(m_modadd + 2));
	m_bamoffset = m_reg[m_modval & 0x1F];

	return 3;
}

uint32_t CV60::am2DisplacementIndirectIndexed16()
{
	m_amflag = 0;

	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2)) + m_reg[m_modval & 0x1F];
		break;
	case 1:
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 2;
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 4;
		break;
	case 3:
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 8;
		break;
	}

	return 4;
}

uint32_t CV60::bam2DisplacementIndirectIndexed16()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + (int16_t)OpRead16(m_modadd + 2));
	m_bamoffset = m_reg[m_modval & 0x1F];

	return 4;
}

uint32_t CV60::am2DisplacementIndirectIndexed32()
{
	m_amflag = 0;

	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F];
		break;
	case 1:
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 2;
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 4;
		break;
	case 3:
		m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 8;
		break;
	}

	return 6;
}

uint32_t CV60::bam2DisplacementIndirectIndexed32()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval2 & 0x1F] + OpRead32(m_modadd + 2));
	m_bamoffset = m_reg[m_modval & 0x1F];

	return 6;
}

uint32_t CV60::am2PCDisplacementIndirect8()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1));

	return 2;
}

uint32_t CV60::bam2PCDisplacementIndirect8()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1));
	m_bamoffset = 0;

	return 2;
}

uint32_t CV60::am2PCDisplacementIndirect16()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1));

	return 3;
}

uint32_t CV60::bam2PCDisplacementIndirect16()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1));
	m_bamoffset = 0;

	return 3;
}

uint32_t CV60::am2PCDisplacementIndirect32()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1));

	return 5;
}

uint32_t CV60::bam2PCDisplacementIndirect32()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1));
	m_bamoffset = 0;

	return 5;
}

uint32_t CV60::am2PCDisplacementIndirectIndexed8()
{
	m_amflag = 0;

	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 2)) + m_reg[m_modval & 0x1F];
		break;
	case 1:
		m_amout = m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 2;
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 4;
		break;
	case 3:
		m_amout = m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 8;
		break;
	}

	return 3;
}

uint32_t CV60::bam2PCDisplacementIndirectIndexed8()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 2));
	m_bamoffset = m_reg[m_modval & 0x1F];

	return 3;
}

uint32_t CV60::am2PCDisplacementIndirectIndexed16()
{
	m_amflag = 0;

	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 2)) + m_reg[m_modval & 0x1F];
		break;
	case 1:
		m_amout = m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 2;
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 4;
		break;
	case 3:
		m_amout = m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 8;
		break;
	}

	return 4;
}


uint32_t CV60::bam2PCDisplacementIndirectIndexed16()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 2));
	m_bamoffset = m_reg[m_modval & 0x1F];

	return 4;
}


uint32_t CV60::am2PCDisplacementIndirectIndexed32()
{
	m_amflag = 0;

	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F];
		break;
	case 1:
		m_amout = m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 2;
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 4;
		break;
	case 3:
		m_amout = m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 8;
		break;
	}

	return 6;
}

uint32_t CV60::bam2PCDisplacementIndirectIndexed32()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 2));
	m_bamoffset = m_reg[m_modval & 0x1F];

	return 6;
}

uint32_t CV60::am2DoubleDisplacement8()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1)) + (int8_t)OpRead8(m_modadd + 2);

	return 3;
}

uint32_t CV60::bam2DoubleDisplacement8()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int8_t)OpRead8(m_modadd + 1));
	m_bamoffset = (int8_t)OpRead8(m_modadd + 2);

	return 3;
}

uint32_t CV60::am2DoubleDisplacement16()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1)) + (int16_t)OpRead16(m_modadd + 3);

	return 5;
}

uint32_t CV60::bam2DoubleDisplacement16()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + (int16_t)OpRead16(m_modadd + 1));
	m_bamoffset = (int8_t)OpRead8(m_modadd + 3);

	return 5;
}

uint32_t CV60::am2DoubleDisplacement32()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1)) + OpRead32(m_modadd + 5);

	return 9;
}

uint32_t CV60::bam2DoubleDisplacement32()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(m_reg[m_modval & 0x1F] + OpRead32(m_modadd + 1));
	m_bamoffset = OpRead32(m_modadd + 5);

	return 9;
}


uint32_t CV60::am2PCDoubleDisplacement8()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1)) + (int8_t)OpRead8(m_modadd + 2);

	return 3;
}

uint32_t CV60::bam2PCDoubleDisplacement8()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + (int8_t)OpRead8(m_modadd + 1));
	m_bamoffset = (int8_t)OpRead8(m_modadd + 2);

	return 3;
}

uint32_t CV60::am2PCDoubleDisplacement16()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1)) + (int16_t)OpRead16(m_modadd + 3);

	return 5;
}

uint32_t CV60::bam2PCDoubleDisplacement16()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + (int16_t)OpRead16(m_modadd + 1));
	m_bamoffset = (int8_t)OpRead8(m_modadd + 3);

	return 5;
}

uint32_t CV60::am2PCDoubleDisplacement32()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1)) + OpRead32(m_modadd + 5);

	return 9;
}

uint32_t CV60::bam2PCDoubleDisplacement32()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(PC + OpRead32(m_modadd + 1));
	m_bamoffset = OpRead32(m_modadd + 5);

	return 9;
}

uint32_t CV60::am2DirectAddress()
{
	m_amflag = 0;
	m_amout = OpRead32(m_modadd + 1);

	return 5;
}

uint32_t CV60::bam2DirectAddress()
{
	m_amflag = 0;
	m_amout = OpRead32(m_modadd + 1);
	m_bamoffset = 0;

	return 5;
}

uint32_t CV60::am2DirectAddressIndexed()
{
	m_amflag = 0;

	switch (m_moddim)
	{
	case 0:
		m_amout = OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F];
		break;
	case 1:
		m_amout = OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F] * 2;
		break;
	case 2:
		m_amout = OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F] * 4;
		break;
	case 3:
		m_amout = OpRead32(m_modadd + 2) + m_reg[m_modval & 0x1F] * 8;
		break;
	}

	return 6;
}

uint32_t CV60::bam2DirectAddressIndexed()
{
	m_amflag = 0;
	m_amout = OpRead32(m_modadd + 2);
	m_bamoffset = m_reg[m_modval & 0x1F];

	return 6;
}

uint32_t CV60::am2DirectAddressDeferred()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(OpRead32(m_modadd + 1));

	return 5;
}

uint32_t CV60::bam2DirectAddressDeferred()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(OpRead32(m_modadd + 1));
	m_bamoffset = 0;

	return 5;
}

uint32_t CV60::am2DirectAddressDeferredIndexed()
{
	m_amflag = 0;

	switch (m_moddim)
	{
	case 0:
		m_amout = m_program->read_dword_unaligned(OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F];
		break;
	case 1:
		m_amout = m_program->read_dword_unaligned(OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 2;
		break;
	case 2:
		m_amout = m_program->read_dword_unaligned(OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 4;
		break;
	case 3:
		m_amout = m_program->read_dword_unaligned(OpRead32(m_modadd + 2)) + m_reg[m_modval & 0x1F] * 8;
		break;
	}

	return 6;
}

uint32_t CV60::bam2DirectAddressDeferredIndexed()
{
	m_amflag = 0;
	m_amout = m_program->read_dword_unaligned(OpRead32(m_modadd + 2));
	m_bamoffset = m_reg[m_modval & 0x1F];

	return 6;
}

uint32_t CV60::am2Immediate()
{
	m_amflag = 0;
	return am1Immediate();
}

uint32_t CV60::am2ImmediateQuick()
{
	m_amflag = 0;
	return am1ImmediateQuick();
}


// AM2 Tables (for ReadAMAddress)
// ******************************

uint32_t CV60::am2Error1()
{
	// f1lap trips this, why?
	logerror("CPU - AM2 - 1 (PC=%06x)", PC);
	return 0;
}

uint32_t CV60::am2Error2()
{
	fatalerror("CPU - AM2 - 2 (PC=%06x)\n", PC);
}

uint32_t CV60::am2Error3()
{
	fatalerror("CPU - AM2 - 3 (PC=%06x)\n", PC);
}

uint32_t CV60::am2Error4()
{
	fatalerror("CPU - AM2 - 4 (PC=%06x)\n", PC);
}

uint32_t CV60::am2Error5()
{
	fatalerror("CPU - AM2 - 5 (PC=%06x)\n", PC);
}

uint32_t CV60::bam2Error1()
{
	fatalerror("CPU - BAM2 - 1 (PC=%06x)\n", PC);
}

uint32_t CV60::bam2Error2()
{
	fatalerror("CPU - BAM2 - 2 (PC=%06x)\n", PC);
}

uint32_t CV60::bam2Error3()
{
	fatalerror("CPU - BAM2 - 3 (PC=%06x)\n", PC);
}

uint32_t CV60::bam2Error4()
{
	fatalerror("CPU - BAM2 - 4 (PC=%06x)\n", PC);
}

uint32_t CV60::bam2Error5()
{
	fatalerror("CPU - BAM2 - 5 (PC=%06x)\n", PC);
}

uint32_t CV60::bam2Error6()
{
	fatalerror("CPU - BAM2 - 6 (PC=%06x)\n", PC);
}


const CV60::am_func CV60::s_AMTable2_G7a[16] =
{
	&CV60::am2PCDisplacementIndexed8,
	&CV60::am2PCDisplacementIndexed16,
	&CV60::am2PCDisplacementIndexed32,
	&CV60::am2DirectAddressIndexed,
	&CV60::am2Error5,
	&CV60::am2Error5,
	&CV60::am2Error5,
	&CV60::am2Error5,
	&CV60::am2PCDisplacementIndirectIndexed8,
	&CV60::am2PCDisplacementIndirectIndexed16,
	&CV60::am2PCDisplacementIndirectIndexed32,
	&CV60::am2DirectAddressDeferredIndexed,
	&CV60::am2Error5,
	&CV60::am2Error5,
	&CV60::am2Error5,
	&CV60::am2Error5
};

const CV60::am_func CV60::s_BAMTable2_G7a[16] =
{
	&CV60::bam2PCDisplacementIndexed8,
	&CV60::bam2PCDisplacementIndexed16,
	&CV60::bam2PCDisplacementIndexed32,
	&CV60::bam2DirectAddressIndexed,
	&CV60::bam2Error5,
	&CV60::bam2Error5,
	&CV60::bam2Error5,
	&CV60::bam2Error5,
	&CV60::bam2PCDisplacementIndirectIndexed8,
	&CV60::bam2PCDisplacementIndirectIndexed16,
	&CV60::bam2PCDisplacementIndirectIndexed32,
	&CV60::bam2DirectAddressDeferredIndexed,
	&CV60::bam2Error5,
	&CV60::bam2Error5,
	&CV60::bam2Error5,
	&CV60::bam2Error5
};

uint32_t CV60::am2Group7a()
{
	if (!(m_modval2 & 0x10))
		return am2Error4();

	return (this->*s_AMTable2_G7a[m_modval2 & 0xF])();
}

uint32_t CV60::bam2Group7a()
{
	if (!(m_modval2 & 0x10))
		return bam2Error4();

	return (this->*s_BAMTable2_G7a[m_modval2 & 0xF])();
}

const CV60::am_func CV60::s_AMTable2_G7[32] =
{
	&CV60::am2ImmediateQuick,
	&CV60::am2ImmediateQuick,
	&CV60::am2ImmediateQuick,
	&CV60::am2ImmediateQuick,
	&CV60::am2ImmediateQuick,
	&CV60::am2ImmediateQuick,
	&CV60::am2ImmediateQuick,
	&CV60::am2ImmediateQuick,
	&CV60::am2ImmediateQuick,
	&CV60::am2ImmediateQuick,
	&CV60::am2ImmediateQuick,
	&CV60::am2ImmediateQuick,
	&CV60::am2ImmediateQuick,
	&CV60::am2ImmediateQuick,
	&CV60::am2ImmediateQuick,
	&CV60::am2ImmediateQuick,
	&CV60::am2PCDisplacement8,
	&CV60::am2PCDisplacement16,
	&CV60::am2PCDisplacement32,
	&CV60::am2DirectAddress,
	&CV60::am2Immediate,
	&CV60::am2Error2,
	&CV60::am2Error2,
	&CV60::am2Error2,
	&CV60::am2PCDisplacementIndirect8,
	&CV60::am2PCDisplacementIndirect16,
	&CV60::am2PCDisplacementIndirect32,
	&CV60::am2DirectAddressDeferred,
	&CV60::am2PCDoubleDisplacement8,
	&CV60::am2PCDoubleDisplacement16,
	&CV60::am2PCDoubleDisplacement32,
	&CV60::am2Error2
};

const CV60::am_func CV60::s_BAMTable2_G7[32] =
{
	&CV60::bam2Error6,
	&CV60::bam2Error6,
	&CV60::bam2Error6,
	&CV60::bam2Error6,
	&CV60::bam2Error6,
	&CV60::bam2Error6,
	&CV60::bam2Error6,
	&CV60::bam2Error6,
	&CV60::bam2Error6,
	&CV60::bam2Error6,
	&CV60::bam2Error6,
	&CV60::bam2Error6,
	&CV60::bam2Error6,
	&CV60::bam2Error6,
	&CV60::bam2Error6,
	&CV60::bam2Error6,
	&CV60::bam2PCDisplacement8,
	&CV60::bam2PCDisplacement16,
	&CV60::bam2PCDisplacement32,
	&CV60::bam2DirectAddress,
	&CV60::bam2Error6,
	&CV60::bam2Error2,
	&CV60::bam2Error2,
	&CV60::bam2Error2,
	&CV60::bam2PCDisplacementIndirect8,
	&CV60::bam2PCDisplacementIndirect16,
	&CV60::bam2PCDisplacementIndirect32,
	&CV60::bam2DirectAddressDeferred,
	&CV60::bam2PCDoubleDisplacement8,
	&CV60::bam2PCDoubleDisplacement16,
	&CV60::bam2PCDoubleDisplacement32,
	&CV60::bam2Error2
};

const CV60::am_func CV60::s_AMTable2_G6[8] =
{
	&CV60::am2DisplacementIndexed8,
	&CV60::am2DisplacementIndexed16,
	&CV60::am2DisplacementIndexed32,
	&CV60::am2RegisterIndirectIndexed,
	&CV60::am2DisplacementIndirectIndexed8,
	&CV60::am2DisplacementIndirectIndexed16,
	&CV60::am2DisplacementIndirectIndexed32,
	&CV60::am2Group7a
};

const CV60::am_func CV60::s_BAMTable2_G6[8] =
{
	&CV60::bam2DisplacementIndexed8,
	&CV60::bam2DisplacementIndexed16,
	&CV60::bam2DisplacementIndexed32,
	&CV60::bam2RegisterIndirectIndexed,
	&CV60::bam2DisplacementIndirectIndexed8,
	&CV60::bam2DisplacementIndirectIndexed16,
	&CV60::bam2DisplacementIndirectIndexed32,
	&CV60::bam2Group7a
};




uint32_t CV60::am2Group6()
{
	m_modval2 = OpRead8(m_modadd + 1);
	return (this->*s_AMTable2_G6[m_modval2 >> 5])();
}
uint32_t CV60::bam2Group6()
{
	m_modval2 = OpRead8(m_modadd + 1);
	return (this->*s_BAMTable2_G6[m_modval2 >> 5])();
}

uint32_t CV60::am2Group7()
{
	return (this->*s_AMTable2_G7[m_modval & 0x1F])();
}
uint32_t CV60::bam2Group7()
{
	return (this->*s_BAMTable2_G7[m_modval & 0x1F])();
}


const CV60::am_func CV60::s_AMTable2[2][8] =
{
	{
		&CV60::am2Displacement8,
		&CV60::am2Displacement16,
		&CV60::am2Displacement32,
		&CV60::am2RegisterIndirect,
		&CV60::am2DisplacementIndirect8,
		&CV60::am2DisplacementIndirect16,
		&CV60::am2DisplacementIndirect32,
		&CV60::am2Group7
	},

	{
		&CV60::am2DoubleDisplacement8,
		&CV60::am2DoubleDisplacement16,
		&CV60::am2DoubleDisplacement32,
		&CV60::am2Register,
		&CV60::am2Autoincrement,
		&CV60::am2Autodecrement,
		&CV60::am2Group6,
		&CV60::am2Error1
	}
};

const CV60::am_func CV60::s_BAMTable2[2][8] =
{
	{
		&CV60::bam2Displacement8,
		&CV60::bam2Displacement16,
		&CV60::bam2Displacement32,
		&CV60::bam2RegisterIndirect,
		&CV60::bam2DisplacementIndirect8,
		&CV60::bam2DisplacementIndirect16,
		&CV60::bam2DisplacementIndirect32,
		&CV60::bam2Group7
	},

	{
		&CV60::bam2DoubleDisplacement8,
		&CV60::bam2DoubleDisplacement16,
		&CV60::bam2DoubleDisplacement32,
		&CV60::bam2Error6,
		&CV60::bam2Autoincrement,
		&CV60::bam2Autodecrement,
		&CV60::bam2Group6,
		&CV60::bam2Error1
	}
};
