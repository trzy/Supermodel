// license:BSD-3-Clause
// copyright-holders:Farfetch'd, R. Belmont
// Ported to Supermodel from MAME (src/devices/cpu/v60/op12.hxx).
/*
 * DIVX: the second operand should be treated as dword instead of word
 * GETATE, GETPTE and GETRA should not be used
 * UPDPSW: _CY and _OV must be cleared or unchanged? I suppose
 *   cleared, like TEST being done on the mask operand.
 * MOVT: I cannot understand exactly what happens to the result
 *   when an overflow occurs
 *
 * Unimplemented opcodes:
 * ROTC, UPDATE, UPDPTE
 */


/*
 *  Macro to access data in operands decoded with ReadAMAddress()
 */

#define F12LOADOPBYTE(num)                          \
	if (m_flag##num)                                \
		appb = (uint8_t)m_reg[m_op##num];         \
	else                                                \
		appb = m_program->read_byte(m_op##num);

#define F12LOADOPHALF(num)                          \
	if (m_flag##num)                                \
		apph = (uint16_t)m_reg[m_op##num];        \
	else                                                \
		apph = m_program->read_word_unaligned(m_op##num);

#define F12LOADOPWORD(num)                          \
	if (m_flag##num)                                \
		appw = m_reg[m_op##num];                \
	else                                                \
		appw = m_program->read_dword_unaligned(m_op##num);

#define F12STOREOPBYTE(num)                         \
	if (m_flag##num)                                \
		SETREG8(m_reg[m_op##num], appb);        \
	else                                                \
		m_program->write_byte(m_op##num, appb);

#define F12STOREOPHALF(num)                         \
	if (m_flag##num)                                \
		SETREG16(m_reg[m_op##num], apph);       \
	else                                                \
		m_program->write_word_unaligned(m_op##num, apph);

#define F12STOREOPWORD(num)                         \
	if (m_flag##num)                                \
		m_reg[m_op##num] = appw;                \
	else                                                \
		m_program->write_dword_unaligned(m_op##num, appw);

#define F12LOADOP1BYTE()  F12LOADOPBYTE(1)
#define F12LOADOP1HALF()  F12LOADOPHALF(1)
#define F12LOADOP1WORD()  F12LOADOPWORD(1)

#define F12LOADOP2BYTE()  F12LOADOPBYTE(2)
#define F12LOADOP2HALF()  F12LOADOPHALF(2)
#define F12LOADOP2WORD()  F12LOADOPWORD(2)

#define F12STOREOP1BYTE()  F12STOREOPBYTE(1)
#define F12STOREOP1HALF()  F12STOREOPHALF(1)
#define F12STOREOP1WORD()  F12STOREOPWORD(1)

#define F12STOREOP2BYTE()  F12STOREOPBYTE(2)
#define F12STOREOP2HALF()  F12STOREOPHALF(2)
#define F12STOREOP2WORD()  F12STOREOPWORD(2)

#define F12END()                                  \
	return m_amlength1 + m_amlength2 + 2;


// Decode the first operand of the instruction and prepare
// writing to the second operand.
void CV60::F12DecodeFirstOperand(am_func DecodeOp1, uint8_t dim1)
{
	m_instflags = OpRead8(PC + 1);

	// Check if F1 or F2
	if (m_instflags & 0x80)
	{
		m_moddim = dim1;
		m_modm = m_instflags & 0x40;
		m_modadd = PC + 2;
		m_amlength1 = (this->*DecodeOp1)();
		m_op1 = m_amout;
		m_flag1 = m_amflag;
	}
	else
	{
		// Check D flag
		if (m_instflags & 0x20)
		{
			m_moddim = dim1;
			m_modm = m_instflags & 0x40;
			m_modadd = PC + 2;
			m_amlength1 = (this->*DecodeOp1)();
			m_op1 = m_amout;
			m_flag1 = m_amflag;
		}
		else
		{
			if (DecodeOp1 == &CV60::ReadAM)
			{
				switch (dim1)
				{
				case 0:
					m_op1 = (uint8_t)m_reg[m_instflags & 0x1F];
					break;
				case 1:
					m_op1 = (uint16_t)m_reg[m_instflags & 0x1F];
					break;
				case 2:
					m_op1 = m_reg[m_instflags & 0x1F];
					break;
				}

				m_flag1 = 0;
			}
			else
			{
				m_flag1 = 1;
				m_op1 = m_instflags & 0x1F;
			}

			m_amlength1 = 0;
		}
	}
}

void CV60::F12WriteSecondOperand(uint8_t dim2)
{
	m_moddim = dim2;

	// Check if F1 or F2
	if (m_instflags & 0x80)
	{
		m_modm = m_instflags & 0x20;
		m_modadd = PC + 2 + m_amlength1;
		m_moddim = dim2;
		m_amlength2 = WriteAM();
	}
	else
	{
		// Check D flag
		if (m_instflags & 0x20)
		{
			switch (dim2)
			{
			case 0:
				SETREG8(m_reg[m_instflags & 0x1F], m_modwritevalb);
				break;
			case 1:
				SETREG16(m_reg[m_instflags & 0x1F], m_modwritevalh);
				break;
			case 2:
				m_reg[m_instflags & 0x1F] = m_modwritevalw;
				break;
			}

			m_amlength2 = 0;
		}
		else
		{
			m_modm = m_instflags & 0x40;
			m_modadd = PC + 2;
			m_moddim = dim2;
			m_amlength2 = WriteAM();
		}
	}
}



// Decode both format 1 / 2 operands
void CV60::F12DecodeOperands(am_func DecodeOp1, uint8_t dim1, am_func DecodeOp2, uint8_t dim2)
{
	uint8_t _if12 = OpRead8(PC + 1);

	// Check if F1 or F2
	if (_if12 & 0x80)
	{
		m_moddim = dim1;
		m_modm = _if12 & 0x40;
		m_modadd = PC + 2;
		m_amlength1 = (this->*DecodeOp1)();
		m_op1 = m_amout;
		m_flag1 = m_amflag;

		m_moddim = dim2;
		m_modm = _if12 & 0x20;
		m_modadd = PC + 2 + m_amlength1;
		m_amlength2 = (this->*DecodeOp2)();
		m_op2 = m_amout;
		m_flag2 = m_amflag;
	}
	else
	{
		// Check D flag
		if (_if12 & 0x20)
		{
			if (DecodeOp2 == &CV60::ReadAMAddress)
			{
				m_op2 = _if12 & 0x1F;
				m_flag2 = 1;
			}
			else
			{
				switch (dim2)
				{
				case 0:
					m_op2 = (uint8_t)m_reg[_if12 & 0x1F];
					break;
				case 1:
					m_op2 = (uint16_t)m_reg[_if12 & 0x1F];
					break;
				case 2:
					m_op2 = m_reg[_if12 & 0x1F];
					break;
				}
			}

			m_amlength2 = 0;

			m_moddim = dim1;
			m_modm = _if12 & 0x40;
			m_modadd = PC + 2;
			m_amlength1 = (this->*DecodeOp1)();
			m_op1 = m_amout;
			m_flag1 = m_amflag;
		}
		else
		{
			if (DecodeOp1 == &CV60::ReadAMAddress)
			{
				m_op1 = _if12 & 0x1F;
				m_flag1 = 1;
			}
			else
			{
				switch (dim1)
				{
				case 0:
					m_op1 = (uint8_t)m_reg[_if12 & 0x1F];
					break;
				case 1:
					m_op1 = (uint16_t)m_reg[_if12 & 0x1F];
					break;
				case 2:
					m_op1 = m_reg[_if12 & 0x1F];
					break;
				}
			}
			m_amlength1 = 0;

			m_moddim = dim2;
			m_modm = _if12 & 0x40;
			m_modadd = PC + 2 + m_amlength1;
			m_amlength2 = (this->*DecodeOp2)();
			m_op2 = m_amout;
			m_flag2 = m_amflag;
		}
	}
}

uint32_t CV60::opADDB() /* TRUSTED (C too!)*/
{
	uint8_t appb;
	F12DecodeOperands(&CV60::ReadAM, 0,&CV60::ReadAMAddress, 0);

	F12LOADOP2BYTE();

	ADDB(appb, (uint8_t)m_op1, 0);

	F12STOREOP2BYTE();
	F12END();
}

uint32_t CV60::opADDH() /* TRUSTED (C too!)*/
{
	uint16_t apph;
	F12DecodeOperands(&CV60::ReadAM, 1,&CV60::ReadAMAddress, 1);

	F12LOADOP2HALF();

	ADDW(apph, (uint16_t)m_op1, 0);

	F12STOREOP2HALF();
	F12END();
}

uint32_t CV60::opADDW() /* TRUSTED (C too!) */
{
	uint32_t appw;
	F12DecodeOperands(&CV60::ReadAM, 2,&CV60::ReadAMAddress, 2);

	F12LOADOP2WORD();

	ADDL(appw, (uint32_t)m_op1, 0);

	F12STOREOP2WORD();
	F12END();
}

uint32_t CV60::opADDCB()
{
	uint8_t appb;
	F12DecodeOperands(&CV60::ReadAM, 0,&CV60::ReadAMAddress, 0);

	F12LOADOP2BYTE();

	ADDB(appb, (uint8_t)m_op1, _CY?1:0);

	F12STOREOP2BYTE();
	F12END();
}

uint32_t CV60::opADDCH()
{
	uint16_t apph;
	F12DecodeOperands(&CV60::ReadAM, 1,&CV60::ReadAMAddress, 1);

	F12LOADOP2HALF();

	ADDW(apph, (uint16_t)m_op1, _CY?1:0);

	F12STOREOP2HALF();
	F12END();
}

uint32_t CV60::opADDCW()
{
	uint32_t appw;
	F12DecodeOperands(&CV60::ReadAM, 2,&CV60::ReadAMAddress, 2);

	F12LOADOP2WORD();

	ADDL(appw, (uint32_t)m_op1, _CY?1:0);

	F12STOREOP2WORD();
	F12END();
}

uint32_t CV60::opANDB() /* TRUSTED */
{
	uint8_t appb;
	F12DecodeOperands(&CV60::ReadAM, 0,&CV60::ReadAMAddress, 0);

	F12LOADOP2BYTE();

	appb &= m_op1;
	_OV = 0;
	_S = ((appb & 0x80) != 0);
	_Z = (appb == 0);

	F12STOREOP2BYTE();
	F12END();
}

uint32_t CV60::opANDH() /* TRUSTED */
{
	uint16_t apph;
	F12DecodeOperands(&CV60::ReadAM, 1,&CV60::ReadAMAddress, 1);

	F12LOADOP2HALF();

	apph &= m_op1;
	_OV = 0;
	_S = ((apph & 0x8000) != 0);
	_Z = (apph == 0);

	F12STOREOP2HALF();
	F12END();
}

uint32_t CV60::opANDW() /* TRUSTED */
{
	uint32_t appw;
	F12DecodeOperands(&CV60::ReadAM, 2,&CV60::ReadAMAddress, 2);

	F12LOADOP2WORD();

	appw &= m_op1;
	_OV = 0;
	_S = ((appw & 0x80000000) != 0);
	_Z = (appw == 0);

	F12STOREOP2WORD();
	F12END();
}

uint32_t CV60::opCALL() /* TRUSTED */
{
	F12DecodeOperands(&CV60::ReadAMAddress, 0,&CV60::ReadAMAddress, 2);

	SP -= 4;
	m_program->write_dword_unaligned(SP, AP);
	AP = m_op2;

	SP -= 4;
	m_program->write_dword_unaligned(SP, PC + m_amlength1 + m_amlength2 + 2);
	PC = m_op1;

	return 0;
}

uint32_t CV60::opCHKAR()
{
	F12DecodeOperands(&CV60::ReadAM, 0,&CV60::ReadAM, 0);

	// No MMU and memory permissions yet @@@
	_Z = 1;
	_CY = 0;
	_S = 0;

	F12END();
}

uint32_t CV60::opCHKAW()
{
	F12DecodeOperands(&CV60::ReadAM, 0,&CV60::ReadAM, 0);

	// No MMU and memory permissions yet @@@
	_Z = 1;
	_CY = 0;
	_S = 0;

	F12END();
}

uint32_t CV60::opCHKAE()
{
	F12DecodeOperands(&CV60::ReadAM, 0,&CV60::ReadAM, 0);

	// No MMU and memory permissions yet @@@
	_Z = 1;
	_CY = 0;
	_S = 0;

	F12END();
}

uint32_t CV60::opCHLVL()
{
	uint32_t oldPSW;

	F12DecodeOperands(&CV60::ReadAM, 0,&CV60::ReadAM, 0);

	if (m_op1 > 3)
	{
		fatalerror("Illegal data field on opCHLVL, PC=%x\n", PC);
	}

	oldPSW = v60_update_psw_for_exception(0, m_op1);

	SP -= 4;
	m_program->write_dword_unaligned(SP, m_op2);

	SP -= 4;
	m_program->write_dword_unaligned(SP, EXCEPTION_CODE_AND_SIZE(0x1800 + m_op1 * 0x100, 8));

	SP -= 4;
	m_program->write_dword_unaligned(SP, oldPSW);

	SP -= 4;
	m_program->write_dword_unaligned(SP, PC + m_amlength1 + m_amlength2 + 2);

	PC = GETINTVECT(24 + m_op1);

	return 0;
}

uint32_t CV60::opCLR1() /* TRUSTED */
{
	uint32_t appw;
	F12DecodeOperands(&CV60::ReadAM, 2,&CV60::ReadAMAddress, 2);

	F12LOADOP2WORD();

	_CY = ((appw & (1 << m_op1)) != 0);
	_Z = !(_CY);

	appw &= ~(1 << m_op1);

	F12STOREOP2WORD();
	F12END();
}

uint32_t CV60::opCMPB() /* TRUSTED (C too!) */
{
	uint8_t appb;
	F12DecodeOperands(&CV60::ReadAM, 0,&CV60::ReadAM, 0);

	appb = (uint8_t)m_op2;
	SUBB(appb, (uint8_t)m_op1, 0);

	F12END();
}

uint32_t CV60::opCMPH() /* TRUSTED (C too!) */
{
	uint16_t apph;
	F12DecodeOperands(&CV60::ReadAM, 1,&CV60::ReadAM, 1);

	apph = (uint16_t)m_op2;
	SUBW(apph, (uint16_t)m_op1, 0);

	F12END();
}


uint32_t CV60::opCMPW() /* TRUSTED (C too!)*/
{
	F12DecodeOperands(&CV60::ReadAM, 2,&CV60::ReadAM, 2);

	SUBL(m_op2, (uint32_t)m_op1, 0);

	F12END();
}

uint32_t CV60::opDIVB() /* TRUSTED */
{
	uint8_t appb;
	F12DecodeOperands(&CV60::ReadAM, 0,&CV60::ReadAMAddress, 0);

	F12LOADOP2BYTE();

	_OV = ((appb == 0x80) && (m_op1 == 0xFF));
	if (m_op1 && !_OV)
		appb= (int8_t)appb / (int8_t)m_op1;
	_Z = (appb == 0);
	_S = ((appb & 0x80) != 0);

	F12STOREOP2BYTE();
	F12END();
}

uint32_t CV60::opDIVH() /* TRUSTED */
{
	uint16_t apph;
	F12DecodeOperands(&CV60::ReadAM, 1,&CV60::ReadAMAddress, 1);

	F12LOADOP2HALF();

	_OV = ((apph == 0x8000) && (m_op1 == 0xFFFF));
	if (m_op1 && !_OV)
		apph = (int16_t)apph / (int16_t)m_op1;
	_Z = (apph == 0);
	_S = ((apph & 0x8000) != 0);

	F12STOREOP2HALF();
	F12END();
}

uint32_t CV60::opDIVW() /* TRUSTED */
{
	uint32_t appw;
	F12DecodeOperands(&CV60::ReadAM, 2,&CV60::ReadAMAddress, 2);

	F12LOADOP2WORD();

	_OV = ((appw == 0x80000000) && (m_op1 == 0xFFFFFFFF));
	if (m_op1 && !_OV)
		appw = (int32_t)appw / (int32_t)m_op1;
	_Z = (appw == 0);
	_S = ((appw & 0x80000000) != 0);

	F12STOREOP2WORD();
	F12END();
}

uint32_t CV60::opDIVX()
{
	int32_t a, b;
	int64_t dv;

	F12DecodeOperands(&CV60::ReadAM, 2,&CV60::ReadAMAddress, 3);

	if (m_flag2)
		dv = ((uint64_t)m_reg[(m_op2 & 0x1F) + 1] << 32) | m_reg[m_op2 & 0x1F];
	else
		dv = m_program->read_qword_unaligned(m_op2);

	// Supermodel fix: MAME divides unconditionally, which faults on the host
	// (crashing the emulator) for a zero divisor or INT64_MIN / -1, and writes
	// a truncated quotient when it doesn't fit. As the manual: the destination
	// is left unchanged and OV set (the zero divide trap is not emulated).
	{
		int32_t divisor = (int32_t)m_op1;
		if (divisor == 0 || (divisor == -1 && dv == INT64_MIN))
		{
			_OV = 1;
			F12END();
		}
		int64_t q = dv / divisor;
		if (q < INT32_MIN || q > INT32_MAX)
		{
			_OV = 1;
			F12END();
		}
		_OV = 0;
	}

	a = div_64x32_rem(dv, m_op1, b);

	_S = ((a & 0x80000000) != 0);
	_Z = (a == 0);

	if (m_flag2)
	{
		m_reg[m_op2 & 0x1F] = a;
		m_reg[(m_op2 & 0x1F) + 1] = b;
	}
	else
	{
		m_program->write_dword_unaligned(m_op2, a);
		m_program->write_dword_unaligned(m_op2 + 4, b);
	}

	F12END();
}

uint32_t CV60::opDIVUX()
{
	uint32_t a, b;
	uint64_t dv;

	F12DecodeOperands(&CV60::ReadAM, 2,&CV60::ReadAMAddress, 3);

	if (m_flag2)
		dv = ((uint64_t)m_reg[(m_op2 & 0x1F) + 1] << 32) | m_reg[m_op2 & 0x1F];
	else
		dv = m_program->read_qword_unaligned(m_op2);

	// Supermodel fix: as DIVX, no host division by zero and no truncated quotient
	if (m_op1 == 0 || dv / m_op1 > UINT32_MAX)
	{
		_OV = 1;
		F12END();
	}
	_OV = 0;

	a = divu_64x32_rem(dv, m_op1, b);

	_S = ((a & 0x80000000) != 0);
	_Z = (a == 0);

	if (m_flag2)
	{
		m_reg[m_op2 & 0x1F] = a;
		m_reg[(m_op2 & 0x1F) + 1] = b;
	}
	else
	{
		m_program->write_dword_unaligned(m_op2, a);
		m_program->write_dword_unaligned(m_op2 + 4, b);
	}

	F12END();
}


uint32_t CV60::opDIVUB() /* TRUSTED */
{
	uint8_t appb;
	F12DecodeOperands(&CV60::ReadAM, 0,&CV60::ReadAMAddress, 0);

	F12LOADOP2BYTE();

	_OV = 0;
	if (m_op1)  appb /= (uint8_t)m_op1;
	_Z = (appb == 0);
	_S = ((appb & 0x80) != 0);

	F12STOREOP2BYTE();
	F12END();
}

uint32_t CV60::opDIVUH() /* TRUSTED */
{
	uint16_t apph;
	F12DecodeOperands(&CV60::ReadAM, 1,&CV60::ReadAMAddress, 1);

	F12LOADOP2HALF();

	_OV = 0;
	if (m_op1)  apph /= (uint16_t)m_op1;
	_Z = (apph == 0);
	_S = ((apph & 0x8000) != 0);

	F12STOREOP2HALF();
	F12END();
}

uint32_t CV60::opDIVUW() /* TRUSTED */
{
	uint32_t appw;
	F12DecodeOperands(&CV60::ReadAM, 2,&CV60::ReadAMAddress, 2);

	F12LOADOP2WORD();

	_OV = 0;
	if (m_op1)  appw /= m_op1;
	_Z = (appw == 0);
	_S = ((appw & 0x80000000) != 0);

	F12STOREOP2WORD();
	F12END();
}

uint32_t CV60::opINB()
{
	F12DecodeFirstOperand(&CV60::ReadAMAddress, 0);
	m_modwritevalb = m_io->read_byte(m_op1);

	if ( m_stall_io )
	{
		m_stall_io = 0;
		return 0;
	}

	F12WriteSecondOperand(0);
	F12END();
}

uint32_t CV60::opINH()
{
	F12DecodeFirstOperand(&CV60::ReadAMAddress, 1);
	m_modwritevalh = m_io->read_word_unaligned(m_op1);

	if ( m_stall_io )
	{
		m_stall_io = 0;
		return 0;
	}

	F12WriteSecondOperand(1);
	F12END();
}

uint32_t CV60::opINW()
{
	F12DecodeFirstOperand(&CV60::ReadAMAddress, 2);
	m_modwritevalw = m_io->read_dword_unaligned(m_op1);

	if ( m_stall_io )
	{
		m_stall_io = 0;
		return 0;
	}

	F12WriteSecondOperand(2);
	F12END();
}

uint32_t CV60::opLDPR()
{
	F12DecodeOperands(&CV60::ReadAMAddress, 2,&CV60::ReadAM, 2);
	if (m_op2 <= 28)
	{
		if (m_flag1)
			m_reg[m_op2 + 36] = m_reg[m_op1];
		else
			m_reg[m_op2 + 36] = m_op1;
	}
	else
	{
		fatalerror("Invalid operand on LDPR PC=%x\n", PC);
	}
	F12END();
}

uint32_t CV60::opLDTASK()
{
	int i;
	F12DecodeOperands(&CV60::ReadAMAddress, 2,&CV60::ReadAM, 2);

	v60WritePSW(v60ReadPSW() & 0xefffffff);

	TR = m_op2;

	TKCW = m_program->read_dword_unaligned(m_op2);
	m_op2 += 4;
	if(SYCW & 0x100) {
		L0SP = m_program->read_dword_unaligned(m_op2);
		m_op2 += 4;
	}
	if(SYCW & 0x200) {
		L1SP = m_program->read_dword_unaligned(m_op2);
		m_op2 += 4;
	}
	if(SYCW & 0x400) {
		L2SP = m_program->read_dword_unaligned(m_op2);
		m_op2 += 4;
	}
	if(SYCW & 0x800) {
		L3SP = m_program->read_dword_unaligned(m_op2);
		m_op2 += 4;
	}

	v60ReloadStack();

	// 31 registers supported, _not_ 32
	for(i = 0; i < 31; i++)
		if(m_op1 & (1 << i)) {
			m_reg[i] = m_program->read_dword_unaligned(m_op2);
			m_op2 += 4;
		}

	// #### Ignore the virtual addressing crap.

	F12END();
}

uint32_t CV60::opMOVD() /* TRUSTED */
{
	uint64_t d;

	F12DecodeOperands(&CV60::ReadAMAddress, 3,&CV60::ReadAMAddress, 3);

	if (m_flag1)
		d = ((uint64_t)m_reg[(m_op1 & 0x1F) + 1] << 32) | m_reg[m_op1 & 0x1F];
	else
		d = m_program->read_qword_unaligned(m_op1);

	if (m_flag2)
	{
		m_reg[m_op2 & 0x1F] = (uint32_t)d;
		m_reg[(m_op2 & 0x1F) + 1] = (uint32_t)(d >> 32);
	}
	else
		m_program->write_qword_unaligned(m_op2, d);

	F12END();
}

uint32_t CV60::opMOVB() /* TRUSTED */
{
	F12DecodeFirstOperand(&CV60::ReadAM, 0);
	m_modwritevalb = (uint8_t)m_op1;
	F12WriteSecondOperand(0);
	F12END();
}

uint32_t CV60::opMOVH() /* TRUSTED */
{
	F12DecodeFirstOperand(&CV60::ReadAM, 1);
	m_modwritevalh = (uint16_t)m_op1;
	F12WriteSecondOperand(1);
	F12END();
}

uint32_t CV60::opMOVW() /* TRUSTED */
{
	F12DecodeFirstOperand(&CV60::ReadAM, 2);
	m_modwritevalw = m_op1;
	F12WriteSecondOperand(2);
	F12END();
}

uint32_t CV60::opMOVEAB() /* TRUSTED */
{
	F12DecodeFirstOperand(&CV60::ReadAMAddress, 0);
	m_modwritevalw = m_op1;
	F12WriteSecondOperand(2);
	F12END();
}

uint32_t CV60::opMOVEAH() /* TRUSTED */
{
	F12DecodeFirstOperand(&CV60::ReadAMAddress, 1);
	m_modwritevalw = m_op1;
	F12WriteSecondOperand(2);
	F12END();
}

uint32_t CV60::opMOVEAW() /* TRUSTED */
{
	F12DecodeFirstOperand(&CV60::ReadAMAddress, 2);
	m_modwritevalw = m_op1;
	F12WriteSecondOperand(2);
	F12END();
}

uint32_t CV60::opMOVSBH() /* TRUSTED */
{
	F12DecodeFirstOperand(&CV60::ReadAM, 0);
	m_modwritevalh = (int8_t)(m_op1 & 0xFF);
	F12WriteSecondOperand(1);
	F12END();
}

uint32_t CV60::opMOVSBW() /* TRUSTED */
{
	F12DecodeFirstOperand(&CV60::ReadAM, 0);
	m_modwritevalw = (int8_t)(m_op1 & 0xFF);
	F12WriteSecondOperand(2);
	F12END();
}

uint32_t CV60::opMOVSHW() /* TRUSTED */
{
	F12DecodeFirstOperand(&CV60::ReadAM, 1);
	m_modwritevalw = (int16_t)(m_op1 & 0xFFFF);
	F12WriteSecondOperand(2);
	F12END();
}

uint32_t CV60::opMOVTHB()
{
	F12DecodeFirstOperand(&CV60::ReadAM, 1);
	m_modwritevalb = (uint8_t)(m_op1 & 0xFF);

	// Check for overflow: the truncated bits must match the sign
	//  of the result, otherwise overflow
	if (((m_modwritevalb & 0x80) == 0x80 && ((m_op1 & 0xFF00) == 0xFF00)) ||
			((m_modwritevalb & 0x80) == 0 && ((m_op1 & 0xFF00) == 0x0000)))
		_OV = 0;
	else
		_OV = 1;

	F12WriteSecondOperand(0);
	F12END();
}

uint32_t CV60::opMOVTWB()
{
	F12DecodeFirstOperand(&CV60::ReadAM, 2);
	m_modwritevalb = (uint8_t)(m_op1 & 0xFF);

	// Check for overflow: the truncated bits must match the sign
	//  of the result, otherwise overflow
	if (((m_modwritevalb & 0x80) == 0x80 && ((m_op1 & 0xFFFFFF00) == 0xFFFFFF00)) ||
			((m_modwritevalb & 0x80) == 0 && ((m_op1 & 0xFFFFFF00) == 0x00000000)))
		_OV = 0;
	else
		_OV = 1;

	F12WriteSecondOperand(0);
	F12END();
}

uint32_t CV60::opMOVTWH()
{
	F12DecodeFirstOperand(&CV60::ReadAM, 2);
	m_modwritevalh = (uint16_t)(m_op1 & 0xFFFF);

	// Check for overflow: the truncated bits must match the sign
	//  of the result, otherwise overflow
	if (((m_modwritevalh & 0x8000) == 0x8000 && ((m_op1 & 0xFFFF0000) == 0xFFFF0000)) ||
			((m_modwritevalh & 0x8000) == 0 && ((m_op1 & 0xFFFF0000) == 0x00000000)))
		_OV = 0;
	else
		_OV = 1;

	F12WriteSecondOperand(1);
	F12END();
}


uint32_t CV60::opMOVZBH() /* TRUSTED */
{
	F12DecodeFirstOperand(&CV60::ReadAM, 0);
	m_modwritevalh = (uint16_t)m_op1;
	F12WriteSecondOperand(1);
	F12END();
}

uint32_t CV60::opMOVZBW() /* TRUSTED */
{
	F12DecodeFirstOperand(&CV60::ReadAM, 0);
	m_modwritevalw = m_op1;
	F12WriteSecondOperand(2);
	F12END();
}

uint32_t CV60::opMOVZHW() /* TRUSTED */
{
	F12DecodeFirstOperand(&CV60::ReadAM, 1);
	m_modwritevalw = m_op1;
	F12WriteSecondOperand(2);
	F12END();
}

uint32_t CV60::opMULB()
{
	uint8_t appb;
	uint32_t tmp;
	F12DecodeOperands(&CV60::ReadAM, 0,&CV60::ReadAMAddress, 0);

	F12LOADOP2BYTE();

	tmp = (int8_t)appb * (int32_t)(int8_t)m_op1;
	appb = tmp;
	_Z = (appb == 0);
	_S = ((appb & 0x80) != 0);
	_OV = ((int32_t)tmp != (int8_t)appb);	// Supermodel fix: the signed product doesn't fit (MAME tests the unsigned high bits)

	F12STOREOP2BYTE();
	F12END();
}

uint32_t CV60::opMULH()
{
	uint16_t apph;
	uint32_t tmp;
	F12DecodeOperands(&CV60::ReadAM, 1,&CV60::ReadAMAddress, 1);

	F12LOADOP2HALF();

	tmp = (int16_t)apph * (int32_t)(int16_t)m_op1;
	apph = tmp;
	_Z = (apph == 0);
	_S = ((apph & 0x8000) != 0);
	_OV = ((int32_t)tmp != (int16_t)apph);	// Supermodel fix: the signed product doesn't fit (MAME tests the unsigned high bits)

	F12STOREOP2HALF();
	F12END();
}

uint32_t CV60::opMULW()
{
	uint32_t appw;
	uint64_t tmp;
	F12DecodeOperands(&CV60::ReadAM, 2,&CV60::ReadAMAddress, 2);

	F12LOADOP2WORD();

	tmp = mul_32x32(appw, m_op1);
	appw = tmp;
	_Z = (appw == 0);
	_S = ((appw & 0x80000000) != 0);
	_OV = ((int64_t)tmp != (int32_t)appw);	// Supermodel fix: the signed product doesn't fit (MAME tests the unsigned high bits)

	F12STOREOP2WORD();
	F12END();
}

uint32_t CV60::opMULUB()
{
	uint8_t appb;
	uint32_t tmp;
	F12DecodeOperands(&CV60::ReadAM, 0,&CV60::ReadAMAddress, 0);

	F12LOADOP2BYTE();

	tmp = appb * (uint8_t)m_op1;
	appb = tmp;
	_Z = (appb == 0);
	_S = ((appb & 0x80) != 0);
	_OV = ((tmp >> 8) != 0);

	F12STOREOP2BYTE();
	F12END();
}

uint32_t CV60::opMULUH()
{
	uint16_t apph;
	uint32_t tmp;
	F12DecodeOperands(&CV60::ReadAM, 1,&CV60::ReadAMAddress, 1);

	F12LOADOP2HALF();

	tmp = apph * (uint16_t)m_op1;
	apph = tmp;
	_Z = (apph == 0);
	_S = ((apph & 0x8000) != 0);
	_OV = ((tmp >> 16) != 0);

	F12STOREOP2HALF();
	F12END();
}

uint32_t CV60::opMULUW()
{
	uint32_t appw;
	uint64_t tmp;
	F12DecodeOperands(&CV60::ReadAM, 2,&CV60::ReadAMAddress, 2);

	F12LOADOP2WORD();

	tmp = mulu_32x32(appw, m_op1);
	appw = tmp;
	_Z = (appw == 0);
	_S = ((appw & 0x80000000) != 0);
	_OV = ((tmp >> 32) != 0);

	F12STOREOP2WORD();
	F12END();
}

uint32_t CV60::opNEGB() /* TRUSTED  (C too!)*/
{
	F12DecodeFirstOperand(&CV60::ReadAM, 0);

	m_modwritevalb = 0;
	SUBB(m_modwritevalb, (int8_t)m_op1, 0);
	_CY = m_modwritevalb ? 1 : 0;

	F12WriteSecondOperand(0);
	F12END();
}

uint32_t CV60::opNEGH() /* TRUSTED  (C too!)*/
{
	F12DecodeFirstOperand(&CV60::ReadAM, 1);

	m_modwritevalh = 0;
	SUBW(m_modwritevalh, (int16_t)m_op1, 0);
	_CY = m_modwritevalh ? 1 : 0;

	F12WriteSecondOperand(1);
	F12END();
}

uint32_t CV60::opNEGW() /* TRUSTED  (C too!)*/
{
	F12DecodeFirstOperand(&CV60::ReadAM, 2);

	m_modwritevalw = 0;
	SUBL(m_modwritevalw, (int32_t)m_op1, 0);
	_CY = m_modwritevalw ? 1 : 0;

	F12WriteSecondOperand(2);
	F12END();
}

uint32_t CV60::opNOTB() /* TRUSTED */
{
	F12DecodeFirstOperand(&CV60::ReadAM, 0);
	m_modwritevalb=~m_op1;

	_OV = 0;
	_S = ((m_modwritevalb & 0x80) != 0);
	_Z = (m_modwritevalb == 0);

	F12WriteSecondOperand(0);
	F12END();
}

uint32_t CV60::opNOTH() /* TRUSTED */
{
	F12DecodeFirstOperand(&CV60::ReadAM, 1);
	m_modwritevalh=~m_op1;

	_OV = 0;
	_S = ((m_modwritevalh & 0x8000) != 0);
	_Z = (m_modwritevalh == 0);

	F12WriteSecondOperand(1);
	F12END();
}

uint32_t CV60::opNOTW() /* TRUSTED */
{
	F12DecodeFirstOperand(&CV60::ReadAM, 2);
	m_modwritevalw=~m_op1;

	_OV = 0;
	_S = ((m_modwritevalw & 0x80000000) != 0);
	_Z = (m_modwritevalw == 0);

	F12WriteSecondOperand(2);
	F12END();
}

uint32_t CV60::opNOT1() /* TRUSTED */
{
	uint32_t appw;
	F12DecodeOperands(&CV60::ReadAM, 2,&CV60::ReadAMAddress, 2);

	F12LOADOP2WORD();

	_CY = ((appw & (1 << m_op1)) != 0);
	_Z = !(_CY);

	if (_CY)
		appw &= ~(1 << m_op1);
	else
		appw |= (1 << m_op1);

	F12STOREOP2WORD();
	F12END();
}

uint32_t CV60::opORB() /* TRUSTED  (C too!)*/
{
	uint8_t appb;
	F12DecodeOperands(&CV60::ReadAM, 0,&CV60::ReadAMAddress, 0);

	F12LOADOP2BYTE();

	ORB(appb, (uint8_t)m_op1);

	F12STOREOP2BYTE();
	F12END();
}

uint32_t CV60::opORH() /* TRUSTED (C too!)*/
{
	uint16_t apph;
	F12DecodeOperands(&CV60::ReadAM, 1,&CV60::ReadAMAddress, 1);

	F12LOADOP2HALF();

	ORW(apph, (uint16_t)m_op1);

	F12STOREOP2HALF();
	F12END();
}

uint32_t CV60::opORW() /* TRUSTED (C too!) */
{
	uint32_t appw;
	F12DecodeOperands(&CV60::ReadAM, 2,&CV60::ReadAMAddress, 2);

	F12LOADOP2WORD();

	ORL(appw, (uint32_t)m_op1);

	F12STOREOP2WORD();
	F12END();
}

uint32_t CV60::opOUTB()
{
	F12DecodeOperands(&CV60::ReadAM, 0,&CV60::ReadAMAddress, 2);
	m_io->write_byte(m_op2,(uint8_t)m_op1);
	F12END();
}

uint32_t CV60::opOUTH()
{
	F12DecodeOperands(&CV60::ReadAM, 1,&CV60::ReadAMAddress, 2);
	m_io->write_word_unaligned(m_op2,(uint16_t)m_op1);
	F12END();
}

uint32_t CV60::opOUTW()
{
	F12DecodeOperands(&CV60::ReadAM, 2,&CV60::ReadAMAddress, 2);
	m_io->write_dword_unaligned(m_op2, m_op1);
	F12END();
}

uint32_t CV60::opREMB()
{
	uint8_t appb;
	F12DecodeOperands(&CV60::ReadAM, 0,&CV60::ReadAMAddress, 0);

	F12LOADOP2BYTE();

	_OV = 0;
	if (m_op1)
		appb= (int8_t)appb % (int8_t)m_op1;
	_Z = (appb == 0);
	_S = ((appb & 0x80) != 0);

	F12STOREOP2BYTE();
	F12END();
}

uint32_t CV60::opREMH()
{
	uint16_t apph;
	F12DecodeOperands(&CV60::ReadAM, 1,&CV60::ReadAMAddress, 1);

	F12LOADOP2HALF();

	_OV = 0;
	if (m_op1)
		apph = (int16_t)apph % (int16_t)m_op1;
	_Z = (apph == 0);
	_S = ((apph & 0x8000) != 0);

	F12STOREOP2HALF();
	F12END();
}

uint32_t CV60::opREMW()
{
	uint32_t appw;
	F12DecodeOperands(&CV60::ReadAM, 2,&CV60::ReadAMAddress, 2);

	F12LOADOP2WORD();

	_OV = 0;
	if ((int32_t)m_op1 == -1)
		appw = 0;	// Supermodel fix: INT_MIN % -1 faults on the host (crashing the emulator); the remainder is 0 for any value
	else if (m_op1)
		appw = (int32_t)appw % (int32_t)m_op1;
	_Z = (appw == 0);
	_S = ((appw & 0x80000000) != 0);

	F12STOREOP2WORD();
	F12END();
}

uint32_t CV60::opREMUB()
{
	uint8_t appb;
	F12DecodeOperands(&CV60::ReadAM, 0,&CV60::ReadAMAddress, 0);

	F12LOADOP2BYTE();

	_OV = 0;
	if (m_op1)
		appb %= (uint8_t)m_op1;
	_Z = (appb == 0);
	_S = ((appb & 0x80) != 0);

	F12STOREOP2BYTE();
	F12END();
}

uint32_t CV60::opREMUH()
{
	uint16_t apph;
	F12DecodeOperands(&CV60::ReadAM, 1,&CV60::ReadAMAddress, 1);

	F12LOADOP2HALF();

	_OV = 0;
	if (m_op1)
		apph %= (uint16_t)m_op1;
	_Z = (apph == 0);
	_S = ((apph & 0x8000) != 0);

	F12STOREOP2HALF();
	F12END();
}

uint32_t CV60::opREMUW()
{
	uint32_t appw;
	F12DecodeOperands(&CV60::ReadAM, 2,&CV60::ReadAMAddress, 2);

	F12LOADOP2WORD();

	_OV = 0;
	if (m_op1)
		appw %= m_op1;
	_Z = (appw == 0);
	_S = ((appw & 0x80000000) != 0);

	F12STOREOP2WORD();
	F12END();
}

uint32_t CV60::opROTB() /* TRUSTED */
{
	uint8_t appb;
	int8_t i, count;

	F12DecodeOperands(&CV60::ReadAM, 0,&CV60::ReadAMAddress, 0);

	F12LOADOP2BYTE();

	count = (int8_t)(m_op1 & 0xFF);
	if (count > 0)
	{
		for (i = 0;i < count;i++)
			appb = (appb << 1) | ((appb & 0x80) >> 7);

		_CY = (appb & 0x1) != 0;
	}
	else if (count < 0)
	{
		count=-count;
		for (i = 0;i < count;i++)
			appb = (appb >> 1) | ((appb & 0x1) << 7);

		_CY = (appb & 0x80) != 0;
	}
	else
		_CY = 0;

	_OV = 0;
	_S = (appb & 0x80) != 0;
	_Z = (appb == 0);

	F12STOREOP2BYTE();
	F12END();
}

uint32_t CV60::opROTH() /* TRUSTED */
{
	uint16_t apph;
	int8_t i, count;

	F12DecodeOperands(&CV60::ReadAM, 0,&CV60::ReadAMAddress, 1);

	F12LOADOP2HALF();

	count = (int8_t)(m_op1 & 0xFF);
	if (count > 0)
	{
		for (i = 0;i < count;i++)
			apph = (apph << 1) | ((apph & 0x8000) >> 15);

		_CY = (apph & 0x1) != 0;
	}
	else if (count < 0)
	{
		count=-count;
		for (i = 0;i < count;i++)
			apph = (apph >> 1) | ((apph & 0x1) << 15);

		_CY = (apph & 0x8000) != 0;
	}
	else
		_CY = 0;

	_OV = 0;
	_S = (apph & 0x8000) != 0;
	_Z = (apph == 0);

	F12STOREOP2HALF();
	F12END();
}

uint32_t CV60::opROTW() /* TRUSTED */
{
	uint32_t appw;
	int8_t i, count;

	F12DecodeOperands(&CV60::ReadAM, 0,&CV60::ReadAMAddress, 2);

	F12LOADOP2WORD();

	count = (int8_t)(m_op1 & 0xFF);
	if (count > 0)
	{
		for (i = 0;i < count;i++)
			appw = (appw << 1) | ((appw & 0x80000000) >> 31);

		_CY = (appw & 0x1) != 0;
	}
	else if (count < 0)
	{
		count=-count;
		for (i = 0;i < count;i++)
			appw = (appw >> 1) | ((appw & 0x1) << 31);

		_CY = (appw & 0x80000000) != 0;
	}
	else
		_CY = 0;

	_OV = 0;
	_S = (appw & 0x80000000) != 0;
	_Z = (appw == 0);

	F12STOREOP2WORD();
	F12END();
}

uint32_t CV60::opROTCB() /* TRUSTED */
{
	uint8_t appb;
	int8_t i, cy, count;

	F12DecodeOperands(&CV60::ReadAM, 0,&CV60::ReadAMAddress, 0);

	F12LOADOP2BYTE();
	NORMALIZEFLAGS();

	count = (int8_t)(m_op1 & 0xFF);
	if (count > 0)
	{
		for (i = 0;i < count;i++)
		{
			cy = _CY;
			_CY = (uint8_t)((appb & 0x80) >> 7);
			appb = (appb << 1) | cy;
		}
	}
	else if (count < 0)
	{
		count=-count;
		for (i = 0;i < count;i++)
		{
			cy = _CY;
			_CY = (appb & 1);
			appb = (appb >> 1) | (cy << 7);
		}
	}
	else
		_CY = 0;

	_OV = 0;
	_S = (appb & 0x80) != 0;
	_Z = (appb == 0);

	F12STOREOP2BYTE();
	F12END();
}

uint32_t CV60::opROTCH() /* TRUSTED */
{
	uint16_t apph;
	int8_t i, cy, count;

	F12DecodeOperands(&CV60::ReadAM, 0,&CV60::ReadAMAddress, 1);

	F12LOADOP2HALF();
	NORMALIZEFLAGS();

	count = (int8_t)(m_op1 & 0xFF);
	if (count > 0)
	{
		for (i = 0;i < count;i++)
		{
			cy = _CY;
			_CY = (uint8_t)((apph & 0x8000) >> 15);
			apph = (apph << 1) | cy;
		}
	}
	else if (count < 0)
	{
		count=-count;
		for (i = 0;i < count;i++)
		{
			cy = _CY;
			_CY = (uint8_t)(apph & 1);
			apph = (apph >> 1) | ((uint16_t)cy << 15);
		}
	}
	else
		_CY = 0;

	_OV = 0;
	_S = (apph & 0x8000) != 0;
	_Z = (apph == 0);

	F12STOREOP2HALF();
	F12END();
}

uint32_t CV60::opROTCW() /* TRUSTED */
{
	uint32_t appw;
	int8_t i, cy, count;

	F12DecodeOperands(&CV60::ReadAM, 0,&CV60::ReadAMAddress, 2);

	F12LOADOP2WORD();
	NORMALIZEFLAGS();

	count = (int8_t)(m_op1 & 0xFF);
	if (count > 0)
	{
		for (i = 0;i < count;i++)
		{
			cy = _CY;
			_CY = (uint8_t)((appw & 0x80000000) >> 31);
			appw = (appw << 1) | cy;
		}
	}
	else if (count < 0)
	{
		count=-count;
		for (i = 0;i < count;i++)
		{
			cy = _CY;
			_CY = (uint8_t)(appw & 1);
			appw = (appw >> 1) | ((uint32_t)cy << 31);
		}
	}
	else
		_CY = 0;

	_OV = 0;
	_S = (appw & 0x80000000) != 0;
	_Z = (appw == 0);

	F12STOREOP2WORD();
	F12END();
}

uint32_t CV60::opRVBIT()
{
	F12DecodeFirstOperand(&CV60::ReadAM, 0);

	m_modwritevalb = bitswap<8>(m_op1, 0, 1, 2, 3, 4, 5, 6, 7);

	F12WriteSecondOperand(0);
	F12END();
}

uint32_t CV60::opRVBYT() /* TRUSTED */
{
	F12DecodeFirstOperand(&CV60::ReadAM, 2);

	m_modwritevalw = swapendian_int32(m_op1);

	F12WriteSecondOperand(2);
	F12END();
}

uint32_t CV60::opSET1() /* TRUSTED */
{
	uint32_t appw;
	F12DecodeOperands(&CV60::ReadAM, 2,&CV60::ReadAMAddress, 2);

	F12LOADOP2WORD();

	_CY = ((appw & (1 << m_op1)) != 0);
	_Z = !(_CY);

	appw |= (1 << m_op1);

	F12STOREOP2WORD();
	F12END();
}


uint32_t CV60::opSETF()
{
	F12DecodeFirstOperand(&CV60::ReadAM, 0);

	// Normalize the flags
	NORMALIZEFLAGS();

	switch (m_op1 & 0xF)
	{
	case 0:
		if (!_OV) m_modwritevalb = 0;
		else m_modwritevalb = 1;
		break;
	case 1:
		if (_OV) m_modwritevalb = 0;
		else m_modwritevalb = 1;
		break;
	case 2:
		if (!_CY) m_modwritevalb = 0;
		else m_modwritevalb = 1;
		break;
	case 3:
		if (_CY) m_modwritevalb = 0;
		else m_modwritevalb = 1;
		break;
	case 4:
		if (!_Z) m_modwritevalb = 0;
		else m_modwritevalb = 1;
		break;
	case 5:
		if (_Z) m_modwritevalb = 0;
		else m_modwritevalb = 1;
		break;
	case 6:
		if (!(_CY | _Z)) m_modwritevalb = 0;
		else m_modwritevalb = 1;
		break;
	case 7:
		if ((_CY | _Z)) m_modwritevalb = 0;
		else m_modwritevalb = 1;
		break;
	case 8:
		if (!_S) m_modwritevalb = 0;
		else m_modwritevalb = 1;
		break;
	case 9:
		if (_S) m_modwritevalb = 0;
		else m_modwritevalb = 1;
		break;
	case 10:
		m_modwritevalb = 1;
		break;
	case 11:
		m_modwritevalb = 0;
		break;
	case 12:
		if (!(_S^_OV)) m_modwritevalb = 0;
		else m_modwritevalb = 1;
		break;
	case 13:
		if ((_S^_OV)) m_modwritevalb = 0;
		else m_modwritevalb = 1;
		break;
	case 14:
		if (!((_S^_OV)|_Z)) m_modwritevalb = 0;
		else m_modwritevalb = 1;
		break;
	case 15:
		if (((_S^_OV)|_Z)) m_modwritevalb = 0;
		else m_modwritevalb = 1;
		break;
	}

	F12WriteSecondOperand(0);

	F12END();
}

/*
#define SHIFTLEFT_OY(val, count, bitsize) \
{\
    uint32_t tmp = ((val) >> (bitsize - 1)) & 1; \
    tmp <<= count; \
    tmp -= 1; \
    tmp <<= (bitsize - (count)); \
    _OV = (((val) & tmp) != tmp); \
    _CY = (((val) & (1 << (count - 1))) != 0); \
}
*/

// During the shift, the overflow is set if the sign bit changes at any point during the shift
#define SHIFTLEFT_OV(val, count, bitsize) \
{\
	uint32_t tmp; \
	if (count == 32) \
		tmp = 0xFFFFFFFF; \
	else \
		tmp = ((1 << (count)) - 1); \
	tmp <<= (bitsize - (count)); \
	if (((val) >> (bitsize - 1)) & 1) \
		_OV = (((val) & tmp) != tmp); \
	else \
		_OV = (((val) & tmp) != 0); \
}

#define SHIFTLEFT_CY(val, count, bitsize) \
	_CY = (uint8_t)(((val) >> (bitsize - count)) & 1);



#define SHIFTARITHMETICRIGHT_OV(val, count, bitsize) \
	_OV = 0;

#define SHIFTARITHMETICRIGHT_CY(val, count, bitsize) \
	_CY = (uint8_t)(((val) >> (count - 1)) & 1);



uint32_t CV60::opSHAB()
{
	uint8_t appb;
	int8_t count;

	F12DecodeOperands(&CV60::ReadAM, 0,&CV60::ReadAMAddress, 0);

	F12LOADOP2BYTE();

	count = (int8_t)(m_op1 & 0xFF);

	// Special case: destination unchanged, flags set
	if (count == 0)
	{
		_CY = _OV = 0;
		SetSZPF_Byte(appb);
	}
	else if (count > 0)
	{
		SHIFTLEFT_OV(appb, count, 8);

		// @@@ Undefined what happens to CY when count >= bitsize
		SHIFTLEFT_CY(appb, count, 8);

		// do the actual shift...
		if (count >= 8)
			appb = 0;
		else
			appb <<= count;

		// and set zero and sign
		SetSZPF_Byte(appb);
	}
	else
	{
		count = -count;

		SHIFTARITHMETICRIGHT_OV(appb, count, 8);
		SHIFTARITHMETICRIGHT_CY(appb, count, 8);

		if (count >= 8)
			appb = (appb & 0x80) ? 0xFF : 0;
		else
			appb = ((int8_t)appb) >> count;

		SetSZPF_Byte(appb);
	}

//  osd_printf_debug("SHAB: %x _CY: %d _Z: %d _OV: %d _S: %d\n", appb, _CY, _Z, _OV, _S);

	F12STOREOP2BYTE();
	F12END();
}

uint32_t CV60::opSHAH()
{
	uint16_t apph;
	int8_t count;

	F12DecodeOperands(&CV60::ReadAM, 0,&CV60::ReadAMAddress, 1);

	F12LOADOP2HALF();

	count = (int8_t)(m_op1 & 0xFF);

	// Special case: destination unchanged, flags set
	if (count == 0)
	{
		_CY = _OV = 0;
		SetSZPF_Word(apph);
	}
	else if (count > 0)
	{
		SHIFTLEFT_OV(apph, count, 16);

		// @@@ Undefined what happens to CY when count >= bitsize
		SHIFTLEFT_CY(apph, count, 16);

		// do the actual shift...
		if (count >= 16)
			apph = 0;
		else
			apph <<= count;

		// and set zero and sign
		SetSZPF_Word(apph);
	}
	else
	{
		count = -count;

		SHIFTARITHMETICRIGHT_OV(apph, count, 16);
		SHIFTARITHMETICRIGHT_CY(apph, count, 16);

		if (count >= 16)
			apph = (apph & 0x8000) ? 0xFFFF : 0;
		else
			apph = ((int16_t)apph) >> count;

		SetSZPF_Word(apph);
	}

//  osd_printf_debug("SHAH: %x >> %d = %x _CY: %d _Z: %d _OV: %d _S: %d\n", oldval, count, apph, _CY, _Z, _OV, _S);

	F12STOREOP2HALF();
	F12END();
}

uint32_t CV60::opSHAW()
{
	uint32_t appw;
	int8_t count;

	F12DecodeOperands(&CV60::ReadAM, 0,&CV60::ReadAMAddress, 2);

	F12LOADOP2WORD();

	count = (int8_t)(m_op1 & 0xFF);

	// Special case: destination unchanged, flags set
	if (count == 0)
	{
		_CY = _OV = 0;
		SetSZPF_Long(appw);
	}
	else if (count > 0)
	{
		SHIFTLEFT_OV(appw, count, 32);

		// @@@ Undefined what happens to CY when count >= bitsize
		SHIFTLEFT_CY(appw, count, 32);

		// do the actual shift...
		if (count >= 32)
			appw = 0;
		else
			appw <<= count;

		// and set zero and sign
		SetSZPF_Long(appw);
	}
	else
	{
		count = -count;

		SHIFTARITHMETICRIGHT_OV(appw, count, 32);
		SHIFTARITHMETICRIGHT_CY(appw, count, 32);

		if (count >= 32)
			appw = (appw & 0x80000000) ? 0xFFFFFFFF : 0;
		else
			appw = ((int32_t)appw) >> count;

		SetSZPF_Long(appw);
	}

//  osd_printf_debug("SHAW: %x >> %d = %x _CY: %d _Z: %d _OV: %d _S: %d\n", oldval, count, appw, _CY, _Z, _OV, _S);

	F12STOREOP2WORD();
	F12END();
}


uint32_t CV60::opSHLB() /* TRUSTED */
{
	uint8_t appb;
	int8_t count;
	uint32_t tmp;

	F12DecodeOperands(&CV60::ReadAM, 0,&CV60::ReadAMAddress, 0);

	F12LOADOP2BYTE();

	count = (int8_t)(m_op1 & 0xFF);
	if (count > 0)
	{
		// left shift flags:
		// carry gets the last bit shifted out,
		// overflow is always CLEARed

		_OV = 0;  // default to no overflow

		// now handle carry
		tmp = appb & 0xff;
		tmp <<= count;
		SetCFB(tmp);    // set carry properly

		// do the actual shift...
		appb <<= count;

		// and set zero and sign
		SetSZPF_Byte(appb);
	}
	else
	{
		if (count == 0)
		{
			// special case: clear carry and overflow, do nothing else
			_CY = _OV = 0;
			SetSZPF_Byte(appb); // doc. is unclear if this is true...
		}
		else
		{
			// right shift flags:
			// carry = last bit shifted out
			// overflow always cleared
			tmp = appb & 0xff;
			tmp >>= ((-count) - 1);
			_CY = (uint8_t)(tmp & 0x1);
			_OV = 0;

			appb >>= -count;
			SetSZPF_Byte(appb);
		}
	}

//  osd_printf_debug("SHLB: %x _CY: %d _Z: %d _OV: %d _S: %d\n", appb, _CY, _Z, _OV, _S);

	F12STOREOP2BYTE();
	F12END();
}

uint32_t CV60::opSHLH() /* TRUSTED */
{
	uint16_t apph;
	int8_t count;
	uint32_t tmp;

	F12DecodeOperands(&CV60::ReadAM, 0,&CV60::ReadAMAddress, 1);

	F12LOADOP2HALF();

	count = (int8_t)(m_op1 & 0xFF);
//  osd_printf_debug("apph: %x count: %d  ", apph, count);
	if (count > 0)
	{
		// left shift flags:
		// carry gets the last bit shifted out,
		// overflow is always CLEARed

		_OV = 0;

		// now handle carry
		tmp = apph & 0xffff;
		tmp <<= count;
		SetCFW(tmp);    // set carry properly

		// do the actual shift...
		apph <<= count;

		// and set zero and sign
		SetSZPF_Word(apph);
	}
	else
	{
		if (count == 0)
		{
			// special case: clear carry and overflow, do nothing else
			_CY = _OV = 0;
			SetSZPF_Word(apph); // doc. is unclear if this is true...
		}
		else
		{
			// right shift flags:
			// carry = last bit shifted out
			// overflow always cleared
			tmp = apph & 0xffff;
			tmp >>= ((-count) - 1);
			_CY = (uint8_t)(tmp & 0x1);
			_OV = 0;

			apph >>= -count;
			SetSZPF_Word(apph);
		}
	}

//  osd_printf_debug("SHLH: %x _CY: %d _Z: %d _OV: %d _S: %d\n", apph, _CY, _Z, _OV, _S);

	F12STOREOP2HALF();
	F12END();
}

uint32_t CV60::opSHLW() /* TRUSTED */
{
	uint32_t appw;
	int8_t count;
	uint64_t tmp;

	F12DecodeOperands(&CV60::ReadAM, 0,&CV60::ReadAMAddress, 2);

	F12LOADOP2WORD();

	count = (int8_t)(m_op1 & 0xFF);
	if (count > 0)
	{
		// left shift flags:
		// carry gets the last bit shifted out,
		// overflow is always CLEARed

		_OV = 0;

		// now handle carry
		tmp = appw & 0xffffffff;
		tmp <<= count;
		SetCFL(tmp);    // set carry properly

		// do the actual shift...
		appw <<= count;

		// and set zero and sign
		SetSZPF_Long(appw);
	}
	else
	{
		if (count == 0)
		{
			// special case: clear carry and overflow, do nothing else
			_CY = _OV = 0;
			SetSZPF_Long(appw); // doc. is unclear if this is true...
		}
		else
		{
			// right shift flags:
			// carry = last bit shifted out
			// overflow always cleared
			tmp = (uint64_t)(appw & 0xffffffff);
			tmp >>= ((-count) - 1);
			_CY = (uint8_t)(tmp & 0x1);
			_OV = 0;

			appw >>= -count;
			SetSZPF_Long(appw);
		}
	}

	//osd_printf_debug("SHLW: %x _CY: %d _Z: %d _OV: %d _S: %d\n", appw, _CY, _Z, _OV, _S);

	F12STOREOP2WORD();
	F12END();
}

uint32_t CV60::opSTPR()
{
	F12DecodeFirstOperand(&CV60::ReadAM, 2);
	if (m_op1 <= 28)
		m_modwritevalw = m_reg[m_op1 + 36];
	else
	{
		fatalerror("Invalid operand on STPR PC=%x\n", PC);
	}
	F12WriteSecondOperand(2);
	F12END();
}


uint32_t CV60::opSUBB() /* TRUSTED (C too!) */
{
	uint8_t appb;
	F12DecodeOperands(&CV60::ReadAM, 0,&CV60::ReadAMAddress, 0);

	F12LOADOP2BYTE();

	SUBB(appb, (uint8_t)m_op1, 0);

	F12STOREOP2BYTE();
	F12END();
}

uint32_t CV60::opSUBH() /* TRUSTED (C too!) */
{
	uint16_t apph;
	F12DecodeOperands(&CV60::ReadAM, 1,&CV60::ReadAMAddress, 1);

	F12LOADOP2HALF();

	SUBW(apph, (uint16_t)m_op1, 0);

	F12STOREOP2HALF();
	F12END();
}

uint32_t CV60::opSUBW() /* TRUSTED (C too!) */
{
	uint32_t appw;
	F12DecodeOperands(&CV60::ReadAM, 2,&CV60::ReadAMAddress, 2);

	F12LOADOP2WORD();

	SUBL(appw, (uint32_t)m_op1, 0);

	F12STOREOP2WORD();
	F12END();
}


uint32_t CV60::opSUBCB()
{
	uint8_t appb;
	F12DecodeOperands(&CV60::ReadAM, 0,&CV60::ReadAMAddress, 0);

	F12LOADOP2BYTE();

	SUBB(appb, (uint8_t)m_op1, _CY?1:0);

	F12STOREOP2BYTE();
	F12END();
}

uint32_t CV60::opSUBCH()
{
	uint16_t apph;
	F12DecodeOperands(&CV60::ReadAM, 1,&CV60::ReadAMAddress, 1);

	F12LOADOP2HALF();

	SUBW(apph, (uint16_t)m_op1, _CY?1:0);

	F12STOREOP2HALF();
	F12END();
}

uint32_t CV60::opSUBCW()
{
	uint32_t appw;
	F12DecodeOperands(&CV60::ReadAM, 2,&CV60::ReadAMAddress, 2);

	F12LOADOP2WORD();

	SUBL(appw, (uint32_t)m_op1, _CY?1:0);

	F12STOREOP2WORD();
	F12END();
}

uint32_t CV60::opTEST1()
{
	F12DecodeOperands(&CV60::ReadAM, 2,&CV60::ReadAM, 2);

	_CY = ((m_op2 & (1 << m_op1)) != 0);
	_Z = !(_CY);

	F12END();
}

uint32_t CV60::opUPDPSWW()
{
	F12DecodeOperands(&CV60::ReadAM, 2,&CV60::ReadAM, 2);

	/* can only modify condition code and control fields */
	m_op2 &= 0xFFFFFF;
	m_op1 &= 0xFFFFFF;
	v60WritePSW((v60ReadPSW() & (~m_op2)) | (m_op1 & m_op2));

	F12END();
}

uint32_t CV60::opUPDPSWH()
{
	F12DecodeOperands(&CV60::ReadAM, 2,&CV60::ReadAM, 2);

	/* can only modify condition code fields */
	m_op2 &= 0xFFFF;
	m_op1 &= 0xFFFF;
	v60WritePSW((v60ReadPSW() & (~m_op2)) | (m_op1 & m_op2));

	F12END();
}

uint32_t CV60::opXCHB() /* TRUSTED */
{
	uint8_t appb, temp;

	F12DecodeOperands(&CV60::ReadAMAddress, 0,&CV60::ReadAMAddress, 0);

	F12LOADOP1BYTE();
	temp = appb;
	F12LOADOP2BYTE();
	F12STOREOP1BYTE();
	appb = temp;
	F12STOREOP2BYTE();

	F12END()
}

uint32_t CV60::opXCHH() /* TRUSTED */
{
	uint16_t apph, temp;

	F12DecodeOperands(&CV60::ReadAMAddress, 1,&CV60::ReadAMAddress, 1);

	F12LOADOP1HALF();
	temp = apph;
	F12LOADOP2HALF();
	F12STOREOP1HALF();
	apph = temp;
	F12STOREOP2HALF();

	F12END()
}

uint32_t CV60::opXCHW() /* TRUSTED */
{
	uint32_t appw, temp;

	F12DecodeOperands(&CV60::ReadAMAddress, 2,&CV60::ReadAMAddress, 2);

	F12LOADOP1WORD();
	temp = appw;
	F12LOADOP2WORD();
	F12STOREOP1WORD();
	appw = temp;
	F12STOREOP2WORD();

	F12END()
}

uint32_t CV60::opXORB() /* TRUSTED (C too!) */
{
	uint8_t appb;
	F12DecodeOperands(&CV60::ReadAM, 0,&CV60::ReadAMAddress, 0);

	F12LOADOP2BYTE();

	XORB(appb, (uint8_t)m_op1);

	F12STOREOP2BYTE();
	F12END();
}

uint32_t CV60::opXORH() /* TRUSTED (C too!) */
{
	uint16_t apph;
	F12DecodeOperands(&CV60::ReadAM, 1,&CV60::ReadAMAddress, 1);

	F12LOADOP2HALF();

	XORW(apph, (uint16_t)m_op1);

	F12STOREOP2HALF();
	F12END();
}

uint32_t CV60::opXORW() /* TRUSTED (C too!) */
{
	uint32_t appw;
	F12DecodeOperands(&CV60::ReadAM, 2,&CV60::ReadAMAddress, 2);

	F12LOADOP2WORD();

	XORL(appw, (uint32_t)m_op1);

	F12STOREOP2WORD();
	F12END();
}

uint32_t CV60::opMULX()
{
	int32_t a, b;
	int64_t res;

	F12DecodeOperands(&CV60::ReadAM, 2,&CV60::ReadAMAddress, 3);

	if (m_flag2)
	{
		a = m_reg[m_op2 & 0x1F];
	}
	else
	{
		a = m_program->read_dword_unaligned(m_op2);
	}

	res = mul_32x32(a, m_op1);

	b = (int32_t)((res >> 32)&0xffffffff);
	a = (int32_t)(res & 0xffffffff);

	_S = ((b & 0x80000000) != 0);
	_Z = (a == 0 && b == 0);

	if (m_flag2)
	{
		m_reg[m_op2 & 0x1F] = a;
		m_reg[(m_op2 & 0x1F) + 1] = b;
	}
	else
		m_program->write_qword_unaligned(m_op2, res);

	F12END();
}

uint32_t CV60::opMULUX()
{
	int32_t a, b;
	uint64_t res;

	F12DecodeOperands(&CV60::ReadAM, 2,&CV60::ReadAMAddress, 3);

	if (m_flag2)
	{
		a = m_reg[m_op2 & 0x1F];
	}
	else
	{
		a = m_program->read_dword_unaligned(m_op2);
	}

	res = mulu_32x32(a, m_op1);
	b = (int32_t)((res >> 32)&0xffffffff);
	a = (int32_t)(res & 0xffffffff);

	_S = ((b & 0x80000000) != 0);
	_Z = (a == 0 && b == 0);

	if (m_flag2)
	{
		m_reg[m_op2 & 0x1F] = a;
		m_reg[(m_op2 & 0x1F) + 1] = b;
	}
	else
		m_program->write_qword_unaligned(m_op2, res);

	F12END();
}
