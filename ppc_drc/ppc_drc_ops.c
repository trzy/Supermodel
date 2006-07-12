static UINT32 drc_invalid_area(void)
{
	error("drc_invalid_area: PC: %08X", ppc.pc);
}

#define PPCREG(x)		((UINT32)&ppc.r[x])

static int block_cycle_count;
static UINT32 block_start_pc;

static UINT32 drc_pc;

static UINT32 drc_recompile_block(void)
{
	UINT32 res = 0;

	block_start_pc = ppc.pc;
	drc_pc = ppc.pc;

//	printf("Recompile block start: %08X\n", drc_pc);

	block_cycle_count = 0;
	do
	{
		void **lookup;
		UINT32 op;

		// point the PC lookup to this instruction
		lookup = drccore_get_lookup_ptr(drc_pc);

		*lookup = gen_get_cache_pos();

		drccore_insert_sanity_check(drc_pc);
		//drccore_insert_cycle_check(1, drc_pc);

		gen(SUBIM, (UINT32)&ppc_icount, 1);
		
		op = READ32(drc_pc);
		switch (op >> 26)
		{
			case 19:	res = drc_op_table19[(op >> 1) & 0x3ff](op); break;
			case 31:	res = drc_op_table31[(op >> 1) & 0x3ff](op); break;
			case 59:	res = drc_op_table59[(op >> 1) & 0x3ff](op); break;
			case 63:	res = drc_op_table63[(op >> 1) & 0x3ff](op); break;
			default:	res = drc_op_table[op >> 26](op); break;
		}

		block_cycle_count++;
		drc_pc += 4;
	} while (res == 0);

//	printf("Recompile block end: %08X\n", drc_pc);
}

static UINT32 cr_flag_eq = 0x02;
static UINT32 cr_flag_lt = 0x08;
static UINT32 cr_flag_gt = 0x04;

static void insert_set_cr0(int reg)
{
	gen(CMPI, reg, 0);
	gen(CMOVZMR, REG_EAX, (UINT32)&cr_flag_eq);
	gen(CMOVLMR, REG_EAX, (UINT32)&cr_flag_lt);
	gen(CMOVGMR, REG_EAX, (UINT32)&cr_flag_gt);
	gen(BTIM, (UINT32)&ppc.xer, 31);
	gen(ADCI, REG_EAX, 0);
	gen(MOVR8M8, (UINT32)&ppc.cr[0], REG_AL);
}



static UINT32 drc_add(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_ADD
	
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_addx), 0);
	gen(ADDI, REG_ESP, 4);

#else
	
	if (OEBIT)
	{
		gen(PUSHI, op, 0);
		gen(CALLI, (UINT32)(ppc_addx), 0);
		gen(ADDI, REG_ESP, 4);
	}
	else
	{
		gen(MOVMR, REG_EDX, PPCREG(RA));
		gen(ADDMR, REG_EDX, PPCREG(RB));
		gen(MOVRM, PPCREG(RT), REG_EDX);
		if (RCBIT)
		{
			insert_set_cr0(REG_EDX);
		}
	}

#endif
	return 0;
}

static UINT32 drc_addc(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_ADDC

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_addcx), 0);
	gen(ADDI, REG_ESP, 4);

#else

	if (OEBIT)
	{
		gen(PUSHI, op, 0);
		gen(CALLI, (UINT32)(ppc_addcx), 0);
		gen(ADDI, REG_ESP, 4);
	}
	else
	{
		gen(MOVMR, REG_EBX, (UINT32)&ppc.xer);
		gen(ANDI, REG_EBX, ~XER_CA);			// clear carry
		gen(MOVMR, REG_EDX, PPCREG(RA));
		gen(ADDMR, REG_EDX, PPCREG(RB));
		gen(MOVRM, PPCREG(RT), REG_EDX);
		gen(SETCR8, REG_AL, 0);
		gen(SHLI, REG_EAX, 29);
		gen(OR, REG_EBX, REG_EAX);
		gen(MOVRM, (UINT32)&ppc.xer, REG_EBX);
		if (RCBIT)
		{
			insert_set_cr0(REG_EDX);
		}
	}

#endif
	return 0;
}

static UINT32 drc_adde(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_ADDE

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_addex), 0);
	gen(ADDI, REG_ESP, 4);

#else

	if (OEBIT)
	{
		gen(PUSHI, op, 0);
		gen(CALLI, (UINT32)(ppc_addex), 0);
		gen(ADDI, REG_ESP, 4);
	}
	else
	{
		gen(MOVMR, REG_EBX, (UINT32)&ppc.xer);
		gen(BTRI, REG_EBX, 29);			// move CA bit to carry and clear it
		gen(MOVMR, REG_EDX, PPCREG(RA));
		gen(ADCMR, REG_EDX, PPCREG(RB));
		gen(SETCR8, REG_AL, 0);
		gen(SHLI, REG_EAX, 29);
		gen(OR, REG_EBX, REG_EAX);
		gen(MOVRM, (UINT32)&ppc.xer, REG_EBX);
		gen(MOVRM, PPCREG(RT), REG_EDX);
		if (RCBIT)
		{
			insert_set_cr0(REG_EDX);
		}
	}

#endif
	return 0;
}

static UINT32 drc_addi(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_ADDI

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_addi), 0);
	gen(ADDI, REG_ESP, 4);

#else

	if (RA == 0)
	{
		gen(MOVIM, PPCREG(RT), SIMM16);
	}
	else
	{
		gen(MOVMR, REG_EDX, PPCREG(RA));
		gen(ADDI, REG_EDX, SIMM16);
		gen(MOVRM, PPCREG(RT), REG_EDX);
	}

#endif
	return 0;
}

static UINT32 drc_addic(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_ADDIC

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_addic), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVMR, REG_EBX, (UINT32)&ppc.xer);
	gen(ANDI, REG_EBX, ~XER_CA);
	gen(MOVMR, REG_EDX, PPCREG(RA));
	gen(ADDI, REG_EDX, SIMM16);
	gen(MOVRM, PPCREG(RT), REG_EDX);
	gen(SETCR8, REG_AL, 0);
	gen(SHLI, REG_EAX, 29);
	gen(OR, REG_EBX, REG_EAX);
	gen(MOVRM, (UINT32)&ppc.xer, REG_EBX);

#endif
	return 0;
}

static UINT32 drc_addic_rc(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_ADDIC_RC

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_addic_rc), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVMR, REG_EBX, (UINT32)&ppc.xer);
	gen(ANDI, REG_EBX, ~XER_CA);
	gen(MOVMR, REG_EDX, PPCREG(RA));
	gen(ADDI, REG_EDX, SIMM16);
	gen(MOVRM, PPCREG(RT), REG_EDX);
	gen(SETCR8, REG_AL, 0);
	gen(SHLI, REG_EAX, 29);
	gen(OR, REG_EBX, REG_EAX);
	gen(MOVRM, (UINT32)&ppc.xer, REG_EBX);

	insert_set_cr0(REG_EDX);

#endif
	return 0;
}

static UINT32 drc_addis(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_ADDIS

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_addis), 0);
	gen(ADDI, REG_ESP, 4);

#else

	if (RA == 0)
	{
		gen(MOVIM, PPCREG(RT), UIMM16 << 16);
	}
	else
	{
		gen(MOVMR, REG_EDX, PPCREG(RA));
		gen(ADDI, REG_EDX, UIMM16 << 16);
		gen(MOVRM, PPCREG(RT), REG_EDX);
	}

#endif
	return 0;
}

// NOTE: not tested!
static UINT32 drc_addme(UINT32 op)
{
#if !COMPILE_OPS || DISABLE_UNTESTED_OPS  || DONT_COMPILE_ADDME

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_addmex), 0);
	gen(ADDI, REG_ESP, 4);

#else

	if (OEBIT)
	{
		gen(PUSHI, op, 0);
		gen(CALLI, (UINT32)(ppc_addmex), 0);
		gen(ADDI, REG_ESP, 4);
	}
	else
	{
		gen(MOVMR, REG_EBX, (UINT32)&ppc.xer);
		gen(BTRI, REG_EBX, 29);			// move CA bit to carry and clear it
		gen(MOVMR, REG_EDX, PPCREG(RA));
		gen(ADCI, REG_EDX, -1);
		gen(SETCR8, REG_AL, 0);
		gen(SHLI, REG_EAX, 29);
		gen(OR, REG_EBX, REG_EAX);
		gen(MOVRM, (UINT32)&ppc.xer, REG_EBX);
		gen(MOVRM, PPCREG(RT), REG_EDX);
		if (RCBIT)
		{
			insert_set_cr0(REG_EDX);
		}
	}

#endif
	return 0;
}

static UINT32 drc_addze(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_ADDZE

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_addzex), 0);
	gen(ADDI, REG_ESP, 4);

#else

	if (OEBIT)
	{
		gen(PUSHI, op, 0);
		gen(CALLI, (UINT32)(ppc_addzex), 0);
		gen(ADDI, REG_ESP, 4);
	}
	else
	{
		gen(MOVMR, REG_EBX, (UINT32)&ppc.xer);
		gen(BTRI, REG_EBX, 29);			// move CA bit to carry and clear it
		gen(MOVMR, REG_EDX, PPCREG(RA));
		gen(ADCI, REG_EDX, 0);
		gen(SETCR8, REG_AL, 0);
		gen(SHLI, REG_EAX, 29);
		gen(OR, REG_EBX, REG_EAX);
		gen(MOVRM, (UINT32)&ppc.xer, REG_EBX);
		gen(MOVRM, PPCREG(RT), REG_EDX);
		if (RCBIT)
		{
			insert_set_cr0(REG_EDX);
		}
	}

#endif
	return 0;
}

static UINT32 drc_and(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_AND

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_andx), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVMR, REG_EDX, PPCREG(RS));
	gen(ANDMR, REG_EDX, PPCREG(RB));
	gen(MOVRM, PPCREG(RA), REG_EDX);
	if (RCBIT)
	{
		insert_set_cr0(REG_EDX);
	}

#endif
	return 0;
}

static UINT32 drc_andc(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_ANDC

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_andcx), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVMR, REG_EDX, PPCREG(RS));
	gen(MOVMR, REG_EAX, PPCREG(RB));
	gen(NOT, REG_EAX, 0);
	gen(AND, REG_EDX, REG_EAX);
	gen(MOVRM, PPCREG(RA), REG_EDX);
	if (RCBIT)
	{
		insert_set_cr0(REG_EDX);
	}

#endif
	return 0;
}

static UINT32 drc_andi_rc(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_ANDI_RC

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_andi_rc), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVMR, REG_EDX, PPCREG(RS));
	gen(ANDI, REG_EDX, UIMM16);
	gen(MOVRM, PPCREG(RA), REG_EDX);
	
	insert_set_cr0(REG_EDX);

#endif
	return 0;
}

static UINT32 drc_andis_rc(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_ANDIS_RC

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_andis_rc), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVMR, REG_EDX, PPCREG(RS));
	gen(ANDI, REG_EDX, UIMM16 << 16);
	gen(MOVRM, PPCREG(RA), REG_EDX);
	
	insert_set_cr0(REG_EDX);

#endif
	return 0;
}

static UINT32 drc_b(UINT32 op)
{
	void *jmpptr;
	UINT32 newpc;
	INT32 li = op & 0x3fffffc;

	drccore_insert_cycle_check(block_cycle_count, drc_pc);

	if (li & 0x2000000)
	{
		li |= 0xfc000000;
	}

	if (AABIT)
	{
		newpc = li;
	}
	else
	{
		newpc = drc_pc + li;
	}

	if (LKBIT)
	{
		gen(MOVIM, (UINT32)&ppc.lr, drc_pc+4);
	}

	//drccore_insert_cycle_check(block_cycle_count);

	jmpptr = drccore_get_lookup_ptr(newpc);
	gen(MOVIM, (UINT32)&ppc.pc, newpc);
	gen(JMPM, (UINT32)jmpptr, 0);

	return DRC_END_BLOCK;
}

static UINT32 drc_bc(UINT32 op)
{
	void *jmpptr;
	UINT32 newpc;
	int bo = BO;
	int bi = BI;
	int gen_jmp1 = 0;
	int gen_jmp2 = 0;
	JUMP_TARGET skip_branch1, skip_branch2;
	init_jmp_target(&skip_branch1);
	init_jmp_target(&skip_branch2);

	if (AABIT)
	{
		newpc = SIMM16 & ~0x3;
	}
	else
	{
		newpc = drc_pc + (SIMM16 & ~0x3);
	}

	if (newpc >= block_start_pc && newpc < drc_pc)
	{
		// jumped inside the current block
		// probably a long loop, so cycle length is the length of the loop
		drccore_insert_cycle_check(abs((newpc-drc_pc)), drc_pc);
	}
	else
	{
		drccore_insert_cycle_check(block_cycle_count, drc_pc);
	}

	if (LKBIT)
	{
		gen(MOVIM, (UINT32)&ppc.lr, drc_pc+4);
	}

	// if BO[2] == 0, update CTR and check CTR condition
	if ((bo & 0x4) == 0)
	{
		gen_jmp1 = 1;
		gen(SUBIM, (UINT32)&ppc.ctr, 1);

		// if BO[1] == 0, branch if CTR != 0
		if ((bo & 0x2) == 0)
		{
			gen_jmp(JZ, &skip_branch1);
		}
		else
		{
			gen_jmp(JNZ, &skip_branch1);
		}
	}

	// if BO[0] == 0, check condition
	if ((bo & 0x10) == 0)
	{
		gen_jmp2 = 1;
		gen(MOVZ_M8R32, REG_EAX, &ppc.cr[bi/4]);		
		gen(TESTI, REG_EAX, 1 << (3 - ((bi) & 0x3)));

		// if BO[3] == 0, branch if condition == FALSE (bit zero)
		if ((bo & 0x8) == 0)
		{
			gen_jmp(JNZ, &skip_branch2);
		}
		// if BO[3] == 1, branch if condition == TRUE (bit not zero)
		else
		{
			gen_jmp(JZ, &skip_branch2);
		}
	}

	// take the branch
	jmpptr = drccore_get_lookup_ptr(newpc);
	gen(MOVIM, (UINT32)&ppc.pc, newpc);
	gen(JMPM, (UINT32)jmpptr, 0);

	// skip the branch
	if (gen_jmp1)
	{
		gen_jmp_target(&skip_branch1);
	}
	if (gen_jmp2)
	{
		gen_jmp_target(&skip_branch2);
	}

	return 0;
}

static UINT32 drc_bcctr(UINT32 op)
{
	UINT32 newpc;
	int bo = BO;
	int bi = BI;
	int gen_jmp1 = 0;
	int gen_jmp2 = 0;
	JUMP_TARGET skip_branch1, skip_branch2;
	init_jmp_target(&skip_branch1);
	init_jmp_target(&skip_branch2);

	drccore_insert_cycle_check(block_cycle_count, drc_pc);

	if (LKBIT)
	{
		gen(MOVIM, (UINT32)&ppc.lr, drc_pc+4);
	}
	
	if (bo == 20)		// Condition is always true, basic block ends here
	{
		gen(MOVMR, REG_EDI, (UINT32)&ppc.ctr);
		gen(MOVRM, (UINT32)&ppc.pc, REG_EDI);
		drccore_insert_dispatcher();
		return DRC_END_BLOCK;
	}
	else
	{
		// if BO[2] == 0, update CTR and check CTR condition
		if ((bo & 0x4) == 0)
		{
			gen_jmp1 = 1;
			gen(SUBIM, (UINT32)&ppc.ctr, 1);

			// if BO[1] == 0, branch if CTR != 0
			if ((bo & 0x2) == 0)
			{
				gen_jmp(JZ, &skip_branch1);
			}
			else
			{
				gen_jmp(JNZ, &skip_branch1);
			}
		}

		// if BO[0] == 0, check condition
		if ((bo & 0x10) == 0)
		{
			gen_jmp2 = 1;
			gen(MOVZ_M8R32, REG_EAX, &ppc.cr[bi/4]);		
			gen(TESTI, REG_EAX, 1 << (3 - ((bi) & 0x3)));

			// if BO[3] == 0, branch if condition == FALSE (bit zero)
			if ((bo & 0x8) == 0)
			{
				gen_jmp(JNZ, &skip_branch2);
			}
			// if BO[3] == 1, branch if condition == TRUE (bit not zero)
			else
			{
				gen_jmp(JZ, &skip_branch2);
			}
		}

		// take the branch
		gen(MOVMR, REG_EDI, (UINT32)&ppc.ctr);
		gen(MOVRM, (UINT32)&ppc.pc, REG_EDI);
		drccore_insert_dispatcher();

		// skip the branch
		if (gen_jmp1)
		{
			gen_jmp_target(&skip_branch1);
		}
		if (gen_jmp2)
		{
			gen_jmp_target(&skip_branch2);
		}
	}
	return 0;
}

static UINT32 drc_bclr(UINT32 op)
{
	UINT32 newpc;
	int bo = BO;
	int bi = BI;
	int gen_jmp1 = 0;
	int gen_jmp2 = 0;
	JUMP_TARGET skip_branch1, skip_branch2;
	init_jmp_target(&skip_branch1);
	init_jmp_target(&skip_branch2);

	drccore_insert_cycle_check(block_cycle_count, drc_pc);

	gen(MOVMR, REG_EDI, (UINT32)&ppc.lr);

	if (LKBIT)
	{
		gen(MOVIM, (UINT32)&ppc.lr, drc_pc+4);
	}
	
	if (bo == 20)		// Condition is always true, basic block ends here
	{
		gen(MOVRM, (UINT32)&ppc.pc, REG_EDI);
		drccore_insert_dispatcher();
		return DRC_END_BLOCK;
	}
	else
	{
		// if BO[2] == 0, update CTR and check CTR condition
		if ((bo & 0x4) == 0)
		{
			gen_jmp1 = 1;
			gen(SUBIM, (UINT32)&ppc.ctr, 1);

			// if BO[1] == 0, branch if CTR != 0
			if ((bo & 0x2) == 0)
			{
				gen_jmp(JZ, &skip_branch1);
			}
			else
			{
				gen_jmp(JNZ, &skip_branch1);
			}
		}

		// if BO[0] == 0, check condition
		if ((bo & 0x10) == 0)
		{
			gen_jmp2 = 1;
			gen(MOVZ_M8R32, REG_EAX, &ppc.cr[bi/4]);		
			gen(TESTI, REG_EAX, 1 << (3 - ((bi) & 0x3)));

			// if BO[3] == 0, branch if condition == FALSE (bit zero)
			if ((bo & 0x8) == 0)
			{
				gen_jmp(JNZ, &skip_branch2);
			}
			// if BO[3] == 1, branch if condition == TRUE (bit not zero)
			else
			{
				gen_jmp(JZ, &skip_branch2);
			}
		}

		// take the branch
		gen(MOVRM, (UINT32)&ppc.pc, REG_EDI);
		drccore_insert_dispatcher();

		// skip the branch
		if (gen_jmp1)
		{
			gen_jmp_target(&skip_branch1);
		}
		if (gen_jmp2)
		{
			gen_jmp_target(&skip_branch2);
		}
	}
	return 0;
}

static UINT32 drc_cmp(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_CMP

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_cmp), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVMR, REG_EDX, PPCREG(RA));
	gen(CMPMR, REG_EDX, PPCREG(RB));
	gen(CMOVZMR, REG_EAX, (UINT32)&cr_flag_eq);
	gen(CMOVLMR, REG_EAX, (UINT32)&cr_flag_lt);
	gen(CMOVGMR, REG_EAX, (UINT32)&cr_flag_gt);
	gen(BTIM, (UINT32)&ppc.xer, 31);
	gen(ADCI, REG_EAX, 0);
	gen(MOVR8M8, (UINT32)&ppc.cr[CRFD], REG_AL);

#endif
	return 0;
}

static UINT32 drc_cmpi(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_CMPI

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_cmpi), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVMR, REG_EDX, PPCREG(RA));
	gen(CMPI, REG_EDX, SIMM16);
	gen(CMOVZMR, REG_EAX, (UINT32)&cr_flag_eq);
	gen(CMOVLMR, REG_EAX, (UINT32)&cr_flag_lt);
	gen(CMOVGMR, REG_EAX, (UINT32)&cr_flag_gt);
	gen(BTIM, (UINT32)&ppc.xer, 31);
	gen(ADCI, REG_EAX, 0);
	gen(MOVR8M8, (UINT32)&ppc.cr[CRFD], REG_AL);

#endif
	return 0;
}

static UINT32 drc_cmpl(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_CMPL

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_cmpl), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVMR, REG_EDX, PPCREG(RA));
	gen(CMPMR, REG_EDX, PPCREG(RB));
	gen(CMOVZMR, REG_EAX, (UINT32)&cr_flag_eq);
	gen(CMOVBMR, REG_EAX, (UINT32)&cr_flag_lt);
	gen(CMOVAMR, REG_EAX, (UINT32)&cr_flag_gt);
	gen(BTIM, (UINT32)&ppc.xer, 31);
	gen(ADCI, REG_EAX, 0);
	gen(MOVR8M8, (UINT32)&ppc.cr[CRFD], REG_AL);

#endif
	return 0;
}

static UINT32 drc_cmpli(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_CMPLI

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_cmpli), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVMR, REG_EDX, PPCREG(RA));
	gen(CMPI, REG_EDX, SIMM16);
	gen(CMOVZMR, REG_EAX, (UINT32)&cr_flag_eq);
	gen(CMOVBMR, REG_EAX, (UINT32)&cr_flag_lt);
	gen(CMOVAMR, REG_EAX, (UINT32)&cr_flag_gt);
	gen(BTIM, (UINT32)&ppc.xer, 31);
	gen(ADCI, REG_EAX, 0);
	gen(MOVR8M8, (UINT32)&ppc.cr[CRFD], REG_AL);

#endif
	return 0;
}

// NOTE: not tested!
static UINT32 drc_cntlzw(UINT32 op)
{
#if !COMPILE_OPS || DISABLE_UNTESTED_OPS

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_cntlzw), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(XOR, REG_EBX, REG_EBX);
	gen(MOVI, REG_EDX, 31);
	gen(MOVMR, REG_EAX, PPCREG(RS));
	gen(BSR, REG_EAX, REG_EAX);
	gen(SETZR8, REG_BL, 0);			// if zero is set then source is 0. add zero bit to 31 -> so result is 32
	gen(SUB, REG_EDX, REG_EAX);
	gen(ADD, REG_EDX, REG_EBX);
	gen(MOVRM, PPCREG(RA), REG_EDX);
	if (RCBIT)
	{
		insert_set_cr0(REG_EDX);
	}

#endif
	return 0;
}

static UINT32 drc_crand(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_crand), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_crandc(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_crandc), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_creqv(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_creqv), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_crnand(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_crnand), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_crnor(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_crnor), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_cror(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_cror), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_crorc(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_crorc), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_crxor(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_crxor), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_dcbf(UINT32 op)
{
	return 0;
}

static UINT32 drc_dcbi(UINT32 op)
{
	return 0;
}

static UINT32 drc_dcbst(UINT32 op)
{
	return 0;
}

static UINT32 drc_dcbt(UINT32 op)
{
	return 0;
}

static UINT32 drc_dcbtst(UINT32 op)
{
	return 0;
}

static UINT32 drc_dcbz(UINT32 op)
{
	return 0;
}

static UINT32 drc_divw(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_divwx), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_divwu(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_divwux), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_eieio(UINT32 op)
{
	return 0;
}

// NOTE: not tested!
static UINT32 drc_eqv(UINT32 op)
{
#if !COMPILE_OPS || DISABLE_UNTESTED_OPS

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_eqvx), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVMR, REG_EDX, PPCREG(RS));
	gen(XORMR, REG_EDX, PPCREG(RB));
	gen(NOT, REG_EDX, 0);
	gen(MOVRM, PPCREG(RA), REG_EDX);
	if (RCBIT)
	{
		insert_set_cr0(REG_EDX);
	}

#endif
	return 0;
}

static UINT32 drc_extsb(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_EXTSB

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_extsbx), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVMR, REG_EDX, PPCREG(RS));
	gen(MOVS_R8R32, REG_EDX, REG_DL);
	gen(MOVRM, PPCREG(RA), REG_EDX);
	if (RCBIT)
	{
		insert_set_cr0(REG_EDX);
	}

#endif
	return 0;
}

static UINT32 drc_extsh(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_EXTSH

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_extshx), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVMR, REG_EDX, PPCREG(RS));
	gen(MOVS_R16R32, REG_EDX, REG_DX);
	gen(MOVRM, PPCREG(RA), REG_EDX);
	if (RCBIT)
	{
		insert_set_cr0(REG_EDX);
	}

#endif
	return 0;
}

static UINT32 drc_icbi(UINT32 op)
{
	return 0;
}

static UINT32 drc_isync(UINT32 op)
{
	return 0;
}

static UINT32 drc_lbz(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_LBZ

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_lbz), 0);
	gen(ADDI, REG_ESP, 4);

#else

	if (RA == 0)
	{
		gen(PUSHI, SIMM16, 0);
	}
	else
	{
		gen(MOVMR, REG_EDX, PPCREG(RA));
		gen(ADDI, REG_EDX, SIMM16);
		gen(PUSH, REG_EDX, 0);
	}
	gen(CALLI, (UINT32)m3_ppc_read_8, 0);
	gen(ADDI, REG_ESP, 4);
	gen(MOVZ_R8R32, REG_EAX, REG_AL);
	gen(MOVRM, PPCREG(RT), REG_EAX);

#endif
	return 0;
}

static UINT32 drc_lbzu(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_LBZU

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_lbzu), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVMR, REG_EDX, PPCREG(RA));
	gen(ADDI, REG_EDX, SIMM16);
	gen(MOVRM, PPCREG(RA), REG_EDX);
	gen(PUSH, REG_EDX, 0);
	gen(CALLI, (UINT32)m3_ppc_read_8, 0);
	gen(ADDI, REG_ESP, 4);
	gen(MOVZ_R8R32, REG_EAX, REG_AL);
	gen(MOVRM, PPCREG(RT), REG_EAX);

#endif
	return 0;
}

static UINT32 drc_lbzux(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_LBZUX

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_lbzux), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVMR, REG_EDX, PPCREG(RA));
	gen(ADDMR, REG_EDX, PPCREG(RB));
	gen(MOVRM, PPCREG(RA), REG_EDX);
	gen(PUSH, REG_EDX, 0);
	gen(CALLI, (UINT32)m3_ppc_read_8, 0);
	gen(ADDI, REG_ESP, 4);
	gen(MOVZ_R8R32, REG_EAX, REG_AL);
	gen(MOVRM, PPCREG(RT), REG_EAX);

#endif
	return 0;
}

static UINT32 drc_lbzx(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_LBZX

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_lbzx), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVMR, REG_EDX, PPCREG(RB));
	if (RA != 0)
	{
		gen(ADDMR, REG_EDX, PPCREG(RA));
	}
	gen(PUSH, REG_EDX, 0);
	gen(CALLI, (UINT32)m3_ppc_read_8, 0);
	gen(ADDI, REG_ESP, 4);
	gen(MOVZ_R8R32, REG_EAX, REG_AL);
	gen(MOVRM, PPCREG(RT), REG_EAX);

#endif
	return 0;
}

static UINT32 drc_lha(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_LHA

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_lha), 0);
	gen(ADDI, REG_ESP, 4);

#else

	if (RA == 0)
	{
		gen(PUSHI, SIMM16, 0);
	}
	else
	{
		gen(MOVMR, REG_EDX, PPCREG(RA));
		gen(ADDI, REG_EDX, SIMM16);
		gen(PUSH, REG_EDX, 0);
	}
	gen(CALLI, (UINT32)m3_ppc_read_16, 0);
	gen(ADDI, REG_ESP, 4);
	gen(MOVS_R16R32, REG_EAX, REG_AX);
	gen(MOVRM, PPCREG(RT), REG_EAX);

#endif
	return 0;
}

static UINT32 drc_lhau(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_LHAU

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_lhau), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVMR, REG_EDX, PPCREG(RA));
	gen(ADDI, REG_EDX, SIMM16);
	gen(MOVRM, PPCREG(RA), REG_EDX);
	gen(PUSH, REG_EDX, 0);
	gen(CALLI, (UINT32)m3_ppc_read_16, 0);
	gen(ADDI, REG_ESP, 4);
	gen(MOVS_R16R32, REG_EAX, REG_AX);
	gen(MOVRM, PPCREG(RT), REG_EAX);

#endif
	return 0;
}

// NOTE: not tested!
static UINT32 drc_lhaux(UINT32 op)
{
#if !COMPILE_OPS || DISABLE_UNTESTED_OPS

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_lhaux), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVMR, REG_EDX, PPCREG(RA));
	gen(ADDMR, REG_EDX, PPCREG(RB));
	gen(MOVRM, PPCREG(RA), REG_EDX);
	gen(PUSH, REG_EDX, 0);
	gen(CALLI, (UINT32)m3_ppc_read_16, 0);
	gen(ADDI, REG_ESP, 4);
	gen(MOVS_R16R32, REG_EAX, REG_AX);
	gen(MOVRM, PPCREG(RT), REG_EAX);

#endif
	return 0;
}

// NOTE: not tested!
static UINT32 drc_lhax(UINT32 op)
{
#if !COMPILE_OPS || DISABLE_UNTESTED_OPS

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_lhax), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVMR, REG_EDX, PPCREG(RB));
	if (RA != 0)
	{
		gen(ADDMR, REG_EDX, PPCREG(RA));
	}
	gen(PUSH, REG_EDX, 0);
	gen(CALLI, (UINT32)m3_ppc_read_16, 0);
	gen(ADDI, REG_ESP, 4);
	gen(MOVS_R16R32, REG_EAX, REG_AX);
	gen(MOVRM, PPCREG(RT), REG_EAX);

#endif
	return 0;
}

// NOTE: not tested!
static UINT32 drc_lhbrx(UINT32 op)
{
#if !COMPILE_OPS || DISABLE_UNTESTED_OPS || DONT_COMPILE_LHBRX

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_lhbrx), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVMR, REG_EDX, PPCREG(RB));
	if (RA != 0)
	{
		gen(ADDMR, REG_EDX, PPCREG(RA));
	}
	gen(PUSH, REG_EDX, 0);
	gen(CALLI, (UINT32)m3_ppc_read_16, 0);
	gen(ADDI, REG_ESP, 4);
	gen(XCHGR8R8, REG_AL, REG_AH);
	gen(MOVRM, PPCREG(RT), REG_EAX);

#endif
	return 0;
}

static UINT32 drc_lhz(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_LHZ

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_lhz), 0);
	gen(ADDI, REG_ESP, 4);

#else

	if (RA == 0)
	{
		gen(PUSHI, SIMM16, 0);
	}
	else
	{
		gen(MOVMR, REG_EDX, PPCREG(RA));
		gen(ADDI, REG_EDX, SIMM16);
		gen(PUSH, REG_EDX, 0);
	}
	gen(CALLI, (UINT32)m3_ppc_read_16, 0);
	gen(ADDI, REG_ESP, 4);
	gen(MOVZ_R16R32, REG_EAX, REG_AX);
	gen(MOVRM, PPCREG(RT), REG_EAX);

#endif
	return 0;
}

static UINT32 drc_lhzu(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_LHZU

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_lhzu), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVMR, REG_EDX, PPCREG(RA));
	gen(ADDI, REG_EDX, SIMM16);
	gen(MOVRM, PPCREG(RA), REG_EDX);
	gen(PUSH, REG_EDX, 0);
	gen(CALLI, (UINT32)m3_ppc_read_16, 0);
	gen(ADDI, REG_ESP, 4);
	gen(MOVZ_R16R32, REG_EAX, REG_AX);
	gen(MOVRM, PPCREG(RT), REG_EAX);

#endif
	return 0;
}

// NOTE: not tested!
static UINT32 drc_lhzux(UINT32 op)
{
#if !COMPILE_OPS || DISABLE_UNTESTED_OPS

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_lhzux), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVMR, REG_EDX, PPCREG(RA));
	gen(ADDMR, REG_EDX, PPCREG(RB));
	gen(MOVRM, PPCREG(RA), REG_EDX);
	gen(PUSH, REG_EDX, 0);
	gen(CALLI, (UINT32)m3_ppc_read_16, 0);
	gen(ADDI, REG_ESP, 4);
	gen(MOVZ_R16R32, REG_EAX, REG_AX);
	gen(MOVRM, PPCREG(RT), REG_EAX);

#endif
	return 0;
}

static UINT32 drc_lhzx(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_LHZX

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_lhzx), 0);
	gen(ADDI, REG_ESP, 4);

#else
	
	gen(MOVMR, REG_EDX, PPCREG(RB));
	if (RA != 0)
	{
		gen(ADDMR, REG_EDX, PPCREG(RA));
	}
	gen(PUSH, REG_EDX, 0);
	gen(CALLI, (UINT32)m3_ppc_read_16, 0);
	gen(ADDI, REG_ESP, 4);
	gen(MOVZ_R16R32, REG_EAX, REG_AX);
	gen(MOVRM, PPCREG(RT), REG_EAX);

#endif
	return 0;
}

static UINT32 drc_lmw(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_lmw), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_lswi(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_lswi), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_lswx(UINT32 op)
{
	error("PPCDRC: drc_lswx\n");
	return 0;
}

static UINT32 drc_lwarx(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_lwarx), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

// FIXME: why does the compiled version have a huge hit on performance?
static UINT32 drc_lwbrx(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_LWBRX

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_lwbrx), 0);
	gen(ADDI, REG_ESP, 4);
	
#else

	gen(MOVMR, REG_EDX, PPCREG(RB));
	if (RA != 0)
	{
		gen(ADDMR, REG_EDX, PPCREG(RA));
	}
	gen(PUSH, REG_EDX, 0);
	gen(CALLI, (UINT32)m3_ppc_read_32, 0);
	gen(ADDI, REG_ESP, 4);
	gen(BSWAP, REG_EAX, 0);
	gen(MOVRM, PPCREG(RT), REG_EAX);

#endif
	return 0;
}

static UINT32 drc_lwz(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_LWZ

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_lwz), 0);
	gen(ADDI, REG_ESP, 4);

#else

#if ENABLE_FASTRAM_PATH
	if (RA == 0)
	{
		UINT32 address = SIMM16;
		if (address < 0x800000)
		{
			gen(MOVMR, REG_EAX, (UINT32)ram + address);
			gen(BSWAP, REG_EAX, 0);
			gen(MOVRM, PPCREG(RT), REG_EAX);
		}
		else
		{
			gen(PUSHI, SIMM16, 0);
			gen(CALLI, (UINT32)m3_ppc_read_32, 0);
			gen(ADDI, REG_ESP, 4);
			gen(MOVRM, PPCREG(RT), REG_EAX);
		}
	}
	else
	{
		JUMP_TARGET lwz_slow_path, lwz_end;
		init_jmp_target(&lwz_slow_path);
		init_jmp_target(&lwz_end);

		gen(MOVMR, REG_EDX, PPCREG(RA));
		gen(ADDI, REG_EDX, SIMM16);
		gen(CMPI, REG_EDX, 0x800000);
		gen_jmp(JAE, &lwz_slow_path);
		// fast path
		gen_mov_dpr_to_reg(REG_EAX, ram, REG_EDX);
		gen(BSWAP, REG_EAX, 0);
		gen_jmp(JMP, &lwz_end);
		// slow path
		gen_jmp_target(&lwz_slow_path);
		gen(PUSH, REG_EDX, 0);
		gen(CALLI, (UINT32)m3_ppc_read_32, 0);
		gen(ADDI, REG_ESP, 4);
		// end
		gen_jmp_target(&lwz_end);
		gen(MOVRM, PPCREG(RT), REG_EAX);
	}
#else
	if (RA == 0)
	{
		gen(PUSHI, SIMM16, 0);
	}
	else
	{
		gen(MOVMR, REG_EDX, PPCREG(RA));
		gen(ADDI, REG_EDX, SIMM16);
		gen(PUSH, REG_EDX, 0);
	}
	gen(CALLI, (UINT32)m3_ppc_read_32, 0);
	gen(ADDI, REG_ESP, 4);
	gen(MOVRM, PPCREG(RT), REG_EAX);
#endif

#endif
	return 0;
}

static UINT32 drc_lwzu(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_LWZU

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_lwzu), 0);
	gen(ADDI, REG_ESP, 4);

#else

#if ENABLE_FASTRAM_PATH
	{
		JUMP_TARGET lwzu_slow_path, lwzu_end;
		init_jmp_target(&lwzu_slow_path);
		init_jmp_target(&lwzu_end);

		gen(MOVMR, REG_EDX, PPCREG(RA));
		gen(ADDI, REG_EDX, SIMM16);
		gen(MOVRM, PPCREG(RA), REG_EDX);
		gen(CMPI, REG_EDX, 0x800000);
		gen_jmp(JAE, &lwzu_slow_path);
		// fast path
		gen_mov_dpr_to_reg(REG_EAX, ram, REG_EDX);
		gen(BSWAP, REG_EAX, 0);
		gen_jmp(JMP, &lwzu_end);
		// slow path
		gen_jmp_target(&lwzu_slow_path);
		gen(PUSH, REG_EDX, 0);
		gen(CALLI, (UINT32)m3_ppc_read_32, 0);
		gen(ADDI, REG_ESP, 4);
		// end
		gen_jmp_target(&lwzu_end);
		gen(MOVRM, PPCREG(RT), REG_EAX);
	}
#else
	gen(MOVMR, REG_EDX, PPCREG(RA));
	gen(ADDI, REG_EDX, SIMM16);
	gen(MOVRM, PPCREG(RA), REG_EDX);
	gen(PUSH, REG_EDX, 0);
	gen(CALLI, (UINT32)m3_ppc_read_32, 0);
	gen(ADDI, REG_ESP, 4);
	gen(MOVRM, PPCREG(RT), REG_EAX);
#endif

#endif
	return 0;
}

static UINT32 drc_lwzux(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_LWZUX

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_lwzux), 0);
	gen(ADDI, REG_ESP, 4);

#else

#if ENABLE_FASTRAM_PATH
	{
		JUMP_TARGET lwzux_slow_path, lwzux_end;
		init_jmp_target(&lwzux_slow_path);
		init_jmp_target(&lwzux_end);

		gen(MOVMR, REG_EDX, PPCREG(RA));
		gen(ADDMR, REG_EDX, PPCREG(RB));
		gen(MOVRM, PPCREG(RA), REG_EDX);
		gen(CMPI, REG_EDX, 0x800000);
		gen_jmp(JAE, &lwzux_slow_path);
		// fast path
		gen_mov_dpr_to_reg(REG_EAX, ram, REG_EDX);
		gen(BSWAP, REG_EAX, 0);
		gen_jmp(JMP, &lwzux_end);
		// slow path
		gen_jmp_target(&lwzux_slow_path);
		gen(PUSH, REG_EDX, 0);
		gen(CALLI, (UINT32)m3_ppc_read_32, 0);
		gen(ADDI, REG_ESP, 4);
		// end
		gen_jmp_target(&lwzux_end);
		gen(MOVRM, PPCREG(RT), REG_EAX);
	}
#else
	gen(MOVMR, REG_EDX, PPCREG(RA));
	gen(ADDMR, REG_EDX, PPCREG(RB));
	gen(MOVRM, PPCREG(RA), REG_EDX);
	gen(PUSH, REG_EDX, 0);
	gen(CALLI, (UINT32)m3_ppc_read_32, 0);
	gen(ADDI, REG_ESP, 4);
	gen(MOVRM, PPCREG(RT), REG_EAX);
#endif

#endif
	return 0;
}

static UINT32 drc_lwzx(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_LWZX

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_lwzx), 0);
	gen(ADDI, REG_ESP, 4);

#else

#if ENABLE_FASTRAM_PATH
	{
		JUMP_TARGET lwzx_slow_path, lwzx_end;
		init_jmp_target(&lwzx_slow_path);
		init_jmp_target(&lwzx_end);

		gen(MOVMR, REG_EDX, PPCREG(RB));
		if (RA != 0)
		{
			gen(ADDMR, REG_EDX, PPCREG(RA));
		}
		gen(CMPI, REG_EDX, 0x800000);
		gen_jmp(JAE, &lwzx_slow_path);
		// fast path
		gen_mov_dpr_to_reg(REG_EAX, ram, REG_EDX);
		gen(BSWAP, REG_EAX, 0);
		gen_jmp(JMP, &lwzx_end);
		// slow path
		gen_jmp_target(&lwzx_slow_path);
		gen(PUSH, REG_EDX, 0);
		gen(CALLI, (UINT32)m3_ppc_read_32, 0);
		gen(ADDI, REG_ESP, 4);
		// end
		gen_jmp_target(&lwzx_end);
		gen(MOVRM, PPCREG(RT), REG_EAX);
	}
#else
	gen(MOVMR, REG_EDX, PPCREG(RB));
	if (RA != 0)
	{
		gen(ADDMR, REG_EDX, PPCREG(RA));
	}
	gen(PUSH, REG_EDX, 0);
	gen(CALLI, (UINT32)m3_ppc_read_32, 0);
	gen(ADDI, REG_ESP, 4);
	gen(MOVRM, PPCREG(RT), REG_EAX);
#endif

#endif
	return 0;
}

static UINT32 drc_mcrf(UINT32 op)
{
#if !COMPILE_OPS

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_mcrf), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVM8R8, REG_AL, (UINT32)&ppc.cr[RA >> 2]);
	gen(MOVR8M8, (UINT32)&ppc.cr[RT >> 2], REG_AL);

#endif
	return 0;
}

static UINT32 drc_mcrxr(UINT32 op)
{
	error("PPCDRC: drc mcrxr\n");
	return 0;
}

static UINT32 drc_mfcr(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_mfcr), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_mfmsr(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_mfmsr), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_mfspr(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_MFSPR

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_mfspr), 0);
	gen(ADDI, REG_ESP, 4);

#else

	if (SPR == SPR_LR)
	{
		gen(MOVMR, REG_EAX, (UINT32)&ppc.lr);
	}
	else if (SPR == SPR_CTR)
	{
		gen(MOVMR, REG_EAX, (UINT32)&ppc.ctr);
	}
	else if (SPR == SPR_XER)
	{
		gen(MOVMR, REG_EAX, (UINT32)&ppc.xer);
	}
	else
	{
		// non-optimized cases
		gen(PUSHI, SPR, 0);
		gen(CALLI, (UINT32)ppc_get_spr, 0);
		gen(ADDI, REG_ESP, 4);
	}
	gen(MOVRM, PPCREG(RT), REG_EAX);

#endif
	return 0;
}

static UINT32 drc_mtcrf(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_mtcrf), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_mtmsr(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_mtmsr), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_mtspr(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_MTSPR

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_mtspr), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVMR, REG_EAX, PPCREG(RS));
	if (SPR == SPR_LR)
	{
		gen(MOVRM, (UINT32)&ppc.lr, REG_EAX);
	}
	else if (SPR == SPR_CTR)
	{
		gen(MOVRM, (UINT32)&ppc.ctr, REG_EAX);
	}
	else if (SPR == SPR_XER)
	{
		gen(MOVRM, (UINT32)&ppc.xer, REG_EAX);
	}
	else
	{
		// non-optimized cases
		gen(PUSH, REG_EAX, 0);
		gen(PUSHI, SPR, 0);
		gen(CALLI, (UINT32)ppc_set_spr, 0);
		gen(ADDI, REG_ESP, 8);
	}

#endif
	return 0;
}

static UINT32 drc_mulhw(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_MULHW

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_mulhwx), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVMR, REG_EAX, PPCREG(RA));
	gen(MOVMR, REG_EBX, PPCREG(RB));
	gen(IMUL, REG_EBX, 0);
	gen(MOVRM, PPCREG(RT), REG_EDX);
	if (RCBIT)
	{
		insert_set_cr0(REG_EDX);
	}

#endif
	return 0;
}

static UINT32 drc_mulhwu(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_MULHWU

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_mulhwux), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVMR, REG_EAX, PPCREG(RA));
	gen(MOVMR, REG_EBX, PPCREG(RB));
	gen(MUL, REG_EBX, 0);
	gen(MOVRM, PPCREG(RT), REG_EDX);
	if (RCBIT)
	{
		insert_set_cr0(REG_EDX);
	}

#endif
	return 0;
}

static UINT32 drc_mulli(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_MULLI

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_mulli), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVMR, REG_EAX, PPCREG(RA));
	gen(MOVI, REG_EBX, SIMM16);
	gen(IMUL, REG_EBX, 0);
	gen(MOVRM, PPCREG(RT), REG_EAX);

#endif
	return 0;
}

static UINT32 drc_mullw(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_MULLW

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_mullwx), 0);
	gen(ADDI, REG_ESP, 4);

#else

	if (OEBIT)
	{
		gen(PUSHI, op, 0);
		gen(CALLI, (UINT32)(ppc_mullwx), 0);
		gen(ADDI, REG_ESP, 4);
	}
	else
	{
		gen(MOVMR, REG_EAX, PPCREG(RA));
		gen(MOVMR, REG_EBX, PPCREG(RB));
		gen(MUL, REG_EBX, 0);
		gen(MOVRM, PPCREG(RT), REG_EAX);
		if (RCBIT)
		{
			insert_set_cr0(REG_EAX);
		}
	}

#endif
	return 0;
}

static UINT32 drc_nand(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_NAND

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_nandx), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVMR, REG_EDX, PPCREG(RS));
	gen(ANDMR, REG_EDX, PPCREG(RB));
	gen(NOT, REG_EDX, 0);
	gen(MOVRM, PPCREG(RA), REG_EDX);
	if (RCBIT)
	{
		insert_set_cr0(REG_EDX);
	}

#endif
	return 0;
}

static UINT32 drc_neg(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_NEG

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_negx), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVMR, REG_EDX, PPCREG(RA));
	gen(NEG, REG_EDX, 0);
	gen(MOVRM, PPCREG(RT), REG_EDX);
	if (RCBIT)
	{
		insert_set_cr0(REG_EDX);
	}

#endif
	return 0;
}

static UINT32 drc_nor(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_NOR

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_norx), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVMR, REG_EDX, PPCREG(RS));
	gen(ORMR, REG_EDX, PPCREG(RB));
	gen(NOT, REG_EDX, 0);
	gen(MOVRM, PPCREG(RA), REG_EDX);
	if (RCBIT)
	{
		insert_set_cr0(REG_EDX);
	}

#endif
	return 0;
}

static UINT32 drc_or(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_OR

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_orx), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVMR, REG_EDX, PPCREG(RS));
	gen(ORMR, REG_EDX, PPCREG(RB));
	gen(MOVRM, PPCREG(RA), REG_EDX);
	if (RCBIT)
	{
		insert_set_cr0(REG_EDX);
	}

#endif
	return 0;
}

static UINT32 drc_orc(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_ORC

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_orcx), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVMR, REG_EDX, PPCREG(RB));
	gen(NOT, REG_EDX, 0);
	gen(ORMR, REG_EDX, PPCREG(RS));
	gen(MOVRM, PPCREG(RA), REG_EDX);
	if (RCBIT)
	{
		insert_set_cr0(REG_EDX);
	}

#endif
	return 0;
}

static UINT32 drc_ori(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_ORI

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_ori), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVMR, REG_EDX, PPCREG(RS));
	gen(ORI, REG_EDX, UIMM16);
	gen(MOVRM, PPCREG(RA), REG_EDX);

#endif
	return 0;
}

static UINT32 drc_oris(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_ORIS

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_oris), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVMR, REG_EDX, PPCREG(RS));
	gen(ORI, REG_EDX, UIMM16 << 16);
	gen(MOVRM, PPCREG(RA), REG_EDX);

#endif
	return 0;
}

static UINT32 drc_rfi(UINT32 op)
{
	gen(MOVMR, REG_EDX, (UINT32)&ppc.srr1);
	gen(PUSH, REG_EDX, 0);
	gen(CALLI, (UINT32)ppc_set_msr, 0);
	gen(ADDI, REG_ESP, 4);

	gen(MOVMR, REG_EAX, (UINT32)&ppc.srr0);
	gen(MOVRM, (UINT32)&ppc.pc, REG_EAX);

	drccore_insert_dispatcher();

	return DRC_END_BLOCK;
}

static UINT32 drc_rlwimi(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_RLWIMI

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_rlwimix), 0);
	gen(ADDI, REG_ESP, 4);

#else

	UINT32 mask = GET_ROTATE_MASK(MB, ME);

	gen(MOVMR, REG_EDX, PPCREG(RS));
	gen(MOVMR, REG_EAX, PPCREG(RA));
	gen(ROLI, REG_EDX, SH);
	gen(ANDI, REG_EDX, mask);
	gen(ANDI, REG_EAX, ~mask);
	gen(OR, REG_EDX, REG_EAX);
	gen(MOVRM, PPCREG(RA), REG_EDX);
	if (RCBIT)
	{
		insert_set_cr0(REG_EDX);
	}

#endif
	return 0;
}

static UINT32 drc_rlwinm(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_RLWINM

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_rlwinmx), 0);
	gen(ADDI, REG_ESP, 4);

#else

	UINT32 mask = GET_ROTATE_MASK(MB, ME);

	gen(MOVMR, REG_EDX, PPCREG(RS));
	gen(ROLI, REG_EDX, SH);
	gen(ANDI, REG_EDX, mask);
	gen(MOVRM, PPCREG(RA), REG_EDX);
	if (RCBIT)
	{
		insert_set_cr0(REG_EDX);
	}

#endif
	return 0;
}

static UINT32 drc_rlwnm(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_RLWNM

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_rlwnmx), 0);
	gen(ADDI, REG_ESP, 4);

#else

	UINT32 mask = GET_ROTATE_MASK(MB, ME);

	gen(MOVMR, REG_ECX, PPCREG(RB));
	gen(MOVMR, REG_EDX, PPCREG(RS));
	gen(ROLCL, REG_EDX, 0);
	gen(ANDI, REG_EDX, mask);
	gen(MOVRM, PPCREG(RA), REG_EDX);
	if (RCBIT)
	{
		insert_set_cr0(REG_EDX);
	}

#endif
	return 0;
}

static UINT32 drc_sc(UINT32 op)
{
	// generate exception
	{
		JUMP_TARGET clear_le, exception_base_0;
		init_jmp_target(&clear_le);
		init_jmp_target(&exception_base_0);

		gen(MOVIM, (UINT32)&ppc.srr0, drc_pc+4);

		gen(MOVMR, REG_EAX, (UINT32)&ppc.msr);
		gen(ANDI, REG_EAX, 0xff73);
		gen(MOVRM, (UINT32)&ppc.srr1, REG_EAX);

		// Clear POW, EE, PR, FP, FE0, SE, BE, FE1, IR, DR, RI
		gen(ANDI, REG_EAX, ~(MSR_POW | MSR_EE | MSR_PR | MSR_FP | MSR_FE0 | MSR_SE | MSR_BE | MSR_FE1 | MSR_IR | MSR_DR | MSR_RI));

		// Set LE to ILE
		gen(ANDI, REG_EAX, ~MSR_LE);		// clear LE first
		gen(TESTI, REG_EAX, MSR_ILE);
		gen_jmp(JZ, &clear_le);				// if Z == 0, bit = 1
		gen(ORI, REG_EAX, MSR_LE);			// set LE
		gen_jmp_target(&clear_le);

		gen(MOVRM, (UINT32)&ppc.msr, REG_EAX);

		gen(MOVI, REG_EDI, 0x00000c00);			// exception vector
		gen(TESTI, REG_EAX, MSR_IP);
		gen_jmp(JZ, &exception_base_0);			// if Z == 1, bit = 0 means base == 0x00000000
		gen(ORI, REG_EDI, 0xfff00000);
		gen_jmp_target(&exception_base_0);

		gen(MOVRM, (UINT32)&ppc.pc, REG_EDI);
		drccore_insert_dispatcher();
	}

	return 0;
}

// FIXME: why does the compiled version slow things down?
static UINT32 drc_slw(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_SLW

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_slwx), 0);
	gen(ADDI, REG_ESP, 4);

#else

	JUMP_TARGET slw_zero, slw_end;
	init_jmp_target(&slw_zero);
	init_jmp_target(&slw_end);

	gen(MOVMR, REG_ECX, PPCREG(RB));
	gen(MOVMR, REG_EDX, PPCREG(RS));
	gen(ANDI, REG_ECX, 0x3f);
	gen(CMPI, REG_ECX, 0x1f);
	gen_jmp(JA, &slw_zero);
	gen(SHLCL, REG_EDX, 0);
	gen_jmp(JMP, &slw_end);
	gen_jmp_target(&slw_zero);
	gen(MOVI, REG_EDX, 0);
	gen_jmp_target(&slw_end);
	gen(MOVRM, PPCREG(RA), REG_EDX);
	if (RCBIT)
	{
		insert_set_cr0(REG_EDX);
	}

#endif
	return 0;
}

static UINT32 drc_sraw(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_srawx), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_srawi(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_srawix), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

// FIXME: why does the compiled version slow things down?
static UINT32 drc_srw(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_SRW

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_srwx), 0);
	gen(ADDI, REG_ESP, 4);

#else

	JUMP_TARGET srw_zero, srw_end;
	init_jmp_target(&srw_zero);
	init_jmp_target(&srw_end);

	gen(MOVMR, REG_ECX, PPCREG(RB));
	gen(MOVMR, REG_EDX, PPCREG(RS));
	gen(ANDI, REG_ECX, 0x3f);
	gen(CMPI, REG_ECX, 0x1f);
	gen_jmp(JA, &srw_zero);
	gen(SHRCL, REG_EDX, 0);
	gen_jmp(JMP, &srw_end);
	gen_jmp_target(&srw_zero);
	gen(MOVI, REG_EDX, 0);
	gen_jmp_target(&srw_end);
	gen(MOVRM, PPCREG(RA), REG_EDX);
	if (RCBIT)
	{
		insert_set_cr0(REG_EDX);
	}

#endif
	return 0;
}

static UINT32 drc_stb(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_STB

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_stb), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVMR, REG_EDX, PPCREG(RS));
	gen(MOVZ_R8R32, REG_EDX, REG_DL);
	gen(PUSH, REG_EDX, 0);

	if (RA == 0)
	{
		gen(PUSHI, SIMM16, 0);
	}
	else
	{
		gen(MOVMR, REG_EAX, PPCREG(RA));
		gen(ADDI, REG_EAX, SIMM16);
		gen(PUSH, REG_EAX, 0);
	}

	gen(CALLI, (UINT32)m3_ppc_write_8, 0);
	gen(ADDI, REG_ESP, 8);

#endif
	return 0;
}

static UINT32 drc_stbu(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_STBU

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_stbu), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVMR, REG_EDX, PPCREG(RS));
	gen(MOVZ_R8R32, REG_EDX, REG_DL);
	gen(PUSH, REG_EDX, 0);

	gen(MOVMR, REG_EAX, PPCREG(RA));
	gen(ADDI, REG_EAX, SIMM16);
	gen(MOVRM, PPCREG(RA), REG_EAX);
	gen(PUSH, REG_EAX, 0);

	gen(CALLI, (UINT32)m3_ppc_write_8, 0);
	gen(ADDI, REG_ESP, 8);

#endif
	return 0;
}

static UINT32 drc_stbux(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_STBUX

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_stbux), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVMR, REG_EDX, PPCREG(RS));
	gen(MOVZ_R8R32, REG_EDX, REG_DL);
	gen(PUSH, REG_EDX, 0);

	gen(MOVMR, REG_EAX, PPCREG(RA));
	gen(ADDMR, REG_EAX, PPCREG(RB));
	gen(MOVRM, PPCREG(RA), REG_EAX);
	gen(PUSH, REG_EAX, 0);

	gen(CALLI, (UINT32)m3_ppc_write_8, 0);
	gen(ADDI, REG_ESP, 8);

#endif
	return 0;
}

static UINT32 drc_stbx(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_STBX

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_stbx), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVMR, REG_EDX, PPCREG(RS));
	gen(MOVZ_R8R32, REG_EDX, REG_DL);
	gen(PUSH, REG_EDX, 0);

	gen(MOVMR, REG_EAX, PPCREG(RB));
	if (RA != 0)
	{
		gen(ADDMR, REG_EAX, PPCREG(RA));
	}
	gen(PUSH, REG_EAX, 0);

	gen(CALLI, (UINT32)m3_ppc_write_8, 0);
	gen(ADDI, REG_ESP, 8);

#endif
	return 0;
}

static UINT32 drc_sth(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_STH

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_sth), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVMR, REG_EDX, PPCREG(RS));
	gen(MOVZ_R16R32, REG_EDX, REG_DX);
	gen(PUSH, REG_EDX, 0);

	if (RA == 0)
	{
		gen(PUSHI, SIMM16, 0);
	}
	else
	{
		gen(MOVMR, REG_EAX, PPCREG(RA));
		gen(ADDI, REG_EAX, SIMM16);
		gen(PUSH, REG_EAX, 0);
	}

	gen(CALLI, (UINT32)m3_ppc_write_16, 0);
	gen(ADDI, REG_ESP, 8);

#endif
	return 0;
}

static UINT32 drc_sthbrx(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_sthbrx), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_sthu(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_sthu), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_sthux(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_sthux), 0);
	gen(ADDI, REG_ESP, 4);
	return 0;
}

static UINT32 drc_sthx(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_sthx), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_stmw(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_stmw), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_stswi(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_stswi), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_stswx(UINT32 op)
{
	error("PPCDRC: drc_stswx\n");
	return 0;
}

static UINT32 drc_stw(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_STW

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_stw), 0);
	gen(ADDI, REG_ESP, 4);

#else

#if ENABLE_FASTRAM_PATH
	if (RA == 0)
	{
		UINT32 address = SIMM16;
		gen(MOVMR, REG_EDX, PPCREG(RS));
		if (address < 0x800000)
		{
			gen(BSWAP, REG_EDX, 0);
			gen(MOVRM, (UINT32)ram + address, REG_EDX);
		}
		else
		{
			gen(PUSH, REG_EDX, 0);
			gen(PUSHI, SIMM16, 0);
			gen(CALLI, (UINT32)m3_ppc_write_32, 0);
			gen(ADDI, REG_ESP, 8);
		}
	}
	else
	{
		JUMP_TARGET stw_slow_path, stw_end;
		init_jmp_target(&stw_slow_path);
		init_jmp_target(&stw_end);

		gen(MOVMR, REG_EDX, PPCREG(RS));
		gen(MOVMR, REG_EAX, PPCREG(RA));
		gen(ADDI, REG_EAX, SIMM16);
		gen(CMPI, REG_EAX, 0x800000);
		gen_jmp(JAE, &stw_slow_path);
		// fast path
		gen(BSWAP, REG_EDX, 0);
		gen_mov_reg_to_dpr(REG_EDX, ram, REG_EAX);
		gen_jmp(JMP, &stw_end);
		// slow path
		gen_jmp_target(&stw_slow_path);
		gen(PUSH, REG_EDX, 0);
		gen(PUSH, REG_EAX, 0);
		gen(CALLI, (UINT32)m3_ppc_write_32, 0);
		gen(ADDI, REG_ESP, 8);
		// end
		gen_jmp_target(&stw_end);
	}
#else
	gen(MOVMR, REG_EDX, PPCREG(RS));
	gen(PUSH, REG_EDX, 0);

	if (RA == 0)
	{
		gen(PUSHI, SIMM16, 0);
	}
	else
	{
		gen(MOVMR, REG_EAX, PPCREG(RA));
		gen(ADDI, REG_EAX, SIMM16);
		gen(PUSH, REG_EAX, 0);
	}

	gen(CALLI, (UINT32)m3_ppc_write_32, 0);
	gen(ADDI, REG_ESP, 8);
#endif

#endif
	return 0;
}

static UINT32 drc_stwbrx(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_stwbrx), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_stwcx_rc(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_stwcx_rc), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_stwu(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_STWU

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_stwu), 0);
	gen(ADDI, REG_ESP, 4);

#else

#if ENABLE_FASTRAM_PATH
	{
		JUMP_TARGET stwu_slow_path, stwu_end;
		init_jmp_target(&stwu_slow_path);
		init_jmp_target(&stwu_end);

		gen(MOVMR, REG_EDX, PPCREG(RS));
		gen(MOVMR, REG_EAX, PPCREG(RA));
		gen(ADDI, REG_EAX, SIMM16);
		gen(MOVRM, PPCREG(RA), REG_EAX);
		gen(CMPI, REG_EAX, 0x800000);
		gen_jmp(JAE, &stwu_slow_path);
		// fast path
		gen(BSWAP, REG_EDX, 0);
		gen_mov_reg_to_dpr(REG_EDX, ram, REG_EAX);
		gen_jmp(JMP, &stwu_end);
		// slow path
		gen_jmp_target(&stwu_slow_path);
		gen(PUSH, REG_EDX, 0);
		gen(PUSH, REG_EAX, 0);
		gen(CALLI, (UINT32)m3_ppc_write_32, 0);
		gen(ADDI, REG_ESP, 8);
		// end
		gen_jmp_target(&stwu_end);
	}
#else
	gen(MOVMR, REG_EDX, PPCREG(RS));
	gen(PUSH, REG_EDX, 0);

	gen(MOVMR, REG_EAX, PPCREG(RA));
	gen(ADDI, REG_EAX, SIMM16);
	gen(MOVRM, PPCREG(RA), REG_EAX);
	gen(PUSH, REG_EAX, 0);

	gen(CALLI, (UINT32)m3_ppc_write_32, 0);
	gen(ADDI, REG_ESP, 8);
#endif

#endif
	return 0;
}

static UINT32 drc_stwux(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_STWUX

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_stwux), 0);
	gen(ADDI, REG_ESP, 4);

#else
	
#if ENABLE_FASTRAM_PATH
	{
		JUMP_TARGET stwux_slow_path, stwux_end;
		init_jmp_target(&stwux_slow_path);
		init_jmp_target(&stwux_end);

		gen(MOVMR, REG_EDX, PPCREG(RS));
		gen(MOVMR, REG_EAX, PPCREG(RA));
		gen(ADDMR, REG_EAX, PPCREG(RB));
		gen(MOVRM, PPCREG(RA), REG_EAX);
		gen(CMPI, REG_EAX, 0x800000);
		gen_jmp(JAE, &stwux_slow_path);
		// fast path
		gen(BSWAP, REG_EDX, 0);
		gen_mov_reg_to_dpr(REG_EDX, ram, REG_EAX);
		gen_jmp(JMP, &stwux_end);
		// slow path
		gen_jmp_target(&stwux_slow_path);
		gen(PUSH, REG_EDX, 0);
		gen(PUSH, REG_EAX, 0);
		gen(CALLI, (UINT32)m3_ppc_write_32, 0);
		gen(ADDI, REG_ESP, 8);
		// end
		gen_jmp_target(&stwux_end);
	}
#else
	gen(MOVMR, REG_EDX, PPCREG(RS));
	gen(PUSH, REG_EDX, 0);

	gen(MOVMR, REG_EAX, PPCREG(RA));
	gen(ADDMR, REG_EAX, PPCREG(RB));
	gen(MOVRM, PPCREG(RA), REG_EAX);
	gen(PUSH, REG_EAX, 0);

	gen(CALLI, (UINT32)m3_ppc_write_32, 0);
	gen(ADDI, REG_ESP, 8);
#endif

#endif
	return 0;
}

static UINT32 drc_stwx(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_STWX

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_stwx), 0);
	gen(ADDI, REG_ESP, 4);

#else

#if ENABLE_FASTRAM_PATH
	{
		JUMP_TARGET stwx_slow_path, stwx_end;
		init_jmp_target(&stwx_slow_path);
		init_jmp_target(&stwx_end);

		gen(MOVMR, REG_EDX, PPCREG(RS));
		gen(MOVMR, REG_EAX, PPCREG(RB));
		if (RA != 0)
		{
			gen(ADDMR, REG_EAX, PPCREG(RA));
		}
		gen(CMPI, REG_EAX, 0x800000);
		gen_jmp(JAE, &stwx_slow_path);
		// fast path
		gen(BSWAP, REG_EDX, 0);
		gen_mov_reg_to_dpr(REG_EDX, ram, REG_EAX);
		gen_jmp(JMP, &stwx_end);
		// slow path
		gen_jmp_target(&stwx_slow_path);
		gen(PUSH, REG_EDX, 0);
		gen(PUSH, REG_EAX, 0);
		gen(CALLI, (UINT32)m3_ppc_write_32, 0);
		gen(ADDI, REG_ESP, 8);
		// end
		gen_jmp_target(&stwx_end);
	}
#else
	gen(MOVMR, REG_EDX, PPCREG(RS));
	gen(PUSH, REG_EDX, 0);

	gen(MOVMR, REG_EAX, PPCREG(RB));
	if (RA != 0)
	{
		gen(ADDMR, REG_EAX, PPCREG(RA));
	}
	gen(PUSH, REG_EAX, 0);

	gen(CALLI, (UINT32)m3_ppc_write_32, 0);
	gen(ADDI, REG_ESP, 8);
#endif

#endif
	return 0;
}

// NOTE: not tested!
static UINT32 drc_subf(UINT32 op)
{
#if !COMPILE_OPS || DISABLE_UNTESTED_OPS || DONT_COMPILE_SUBF

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_subfx), 0);
	gen(ADDI, REG_ESP, 4);

#else

	if (OEBIT)
	{
		gen(PUSHI, op, 0);
		gen(CALLI, (UINT32)(ppc_subfx), 0);
		gen(ADDI, REG_ESP, 4);
	}
	else
	{
		gen(MOVMR, REG_EDX, PPCREG(RB));
		gen(SUBMR, REG_EDX, PPCREG(RA));
		gen(MOVRM, PPCREG(RT), REG_EDX);
		if (RCBIT)
		{
			insert_set_cr0(REG_EDX);
		}
	}

#endif
	return 0;
}

static UINT32 drc_subfc(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_SUBFC

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_subfcx), 0);
	gen(ADDI, REG_ESP, 4);

#else

	if (OEBIT)
	{
		gen(PUSHI, op, 0);
		gen(CALLI, (UINT32)(ppc_subfcx), 0);
		gen(ADDI, REG_ESP, 4);
	}
	else
	{
		gen(MOVMR, REG_EBX, (UINT32)&ppc.xer);
		gen(ANDI, REG_EBX, ~XER_CA);
		gen(MOVMR, REG_EDX, PPCREG(RB));
		gen(SUBMR, REG_EDX, PPCREG(RA));
		gen(SETNCR8, REG_AL, 0);
		gen(SHLI, REG_EAX, 29);
		gen(OR, REG_EBX, REG_EAX);
		gen(MOVRM, PPCREG(RT), REG_EDX);
		gen(MOVRM, (UINT32)&ppc.xer, REG_EBX);
		if (RCBIT)
		{
			insert_set_cr0(REG_EDX);
		}
	}

#endif
	return 0;
}

static UINT32 drc_subfe(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_subfex), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_subfic(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_SUBFIC

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_subfic), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVMR, REG_EBX, (UINT32)&ppc.xer);
	gen(ANDI, REG_EBX, ~XER_CA);
	gen(MOVI, REG_EDX, SIMM16);
	gen(SUBMR, REG_EDX, PPCREG(RA));
	gen(SETNCR8, REG_AL, 0);
	gen(SHLI, REG_EAX, 29);
	gen(OR, REG_EBX, REG_EAX);
	gen(MOVRM, PPCREG(RT), REG_EDX);
	gen(MOVRM, (UINT32)&ppc.xer, REG_EBX);

#endif
	return 0;
}

static UINT32 drc_subfme(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_subfmex), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_subfze(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_subfzex), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_sync(UINT32 op)
{
	return 0;
}

static UINT32 drc_tw(UINT32 op)
{
	JUMP_TARGET branch_link1;
	JUMP_TARGET branch_link2;
	JUMP_TARGET branch_link3;
	JUMP_TARGET branch_link4;
	JUMP_TARGET branch_link5;
	JUMP_TARGET branch_link6;
	int do_link1 = 0;
	int do_link2 = 0;
	int do_link3 = 0;
	int do_link4 = 0;
	int do_link5 = 0;
	init_jmp_target(&branch_link1);
	init_jmp_target(&branch_link2);
	init_jmp_target(&branch_link3);
	init_jmp_target(&branch_link4);
	init_jmp_target(&branch_link5);
	init_jmp_target(&branch_link6);

	gen(MOVMR, REG_EAX, (UINT32)&ppc.r[RA]);
	gen(MOVMR, REG_EDX, (UINT32)&ppc.r[RB]);
	gen(CMP, REG_EAX, REG_EDX);

	if (RT & 0x10)
	{
		gen_jmp(JL, &branch_link1);		// less than = signed <
		do_link1 = 1;
	}
	if (RT & 0x08)
	{
		gen_jmp(JG, &branch_link2);		// greater = signed >
		do_link2 = 1;
	}
	if (RT & 0x04)
	{
		gen_jmp(JZ, &branch_link3);		// equal
		do_link3 = 1;
	}
	if (RT & 0x02)
	{
		gen_jmp(JB, &branch_link4);		// below = unsigned <
		do_link4 = 1;
	}
	if (RT & 0x01)
	{
		gen_jmp(JA, &branch_link5);		// above = unsigned >
		do_link5 = 1;
	}

	gen_jmp(JMP, &branch_link6);

	if (do_link1)
	{
		gen_jmp_target(&branch_link1);
	}
	if (do_link2)
	{
		gen_jmp_target(&branch_link2);
	}
	if (do_link3)
	{
		gen_jmp_target(&branch_link3);
	}
	if (do_link4)
	{
		gen_jmp_target(&branch_link4);
	}
	if (do_link5)
	{
		gen_jmp_target(&branch_link5);
	}

	// generate exception
	{
		JUMP_TARGET clear_le, exception_base_0;
		init_jmp_target(&clear_le);
		init_jmp_target(&exception_base_0);

		gen(MOVIM, (UINT32)&ppc.srr0, drc_pc+4);

		gen(MOVMR, REG_EAX, (UINT32)&ppc.msr);
		gen(ANDI, REG_EAX, 0xff73);
		gen(MOVRM, (UINT32)&ppc.srr1, REG_EAX);

		// Clear POW, EE, PR, FP, FE0, SE, BE, FE1, IR, DR, RI
		gen(ANDI, REG_EAX, ~(MSR_POW | MSR_EE | MSR_PR | MSR_FP | MSR_FE0 | MSR_SE | MSR_BE | MSR_FE1 | MSR_IR | MSR_DR | MSR_RI));

		// Set LE to ILE
		gen(ANDI, REG_EAX, ~MSR_LE);		// clear LE first
		gen(TESTI, REG_EAX, MSR_ILE);
		gen_jmp(JZ, &clear_le);				// if Z == 0, bit = 1
		gen(ORI, REG_EAX, MSR_LE);			// set LE
		gen_jmp_target(&clear_le);

		gen(MOVRM, (UINT32)&ppc.msr, REG_EAX);

		gen(MOVI, REG_EDI, 0x00000700);			// exception vector
		gen(TESTI, REG_EAX, MSR_IP);
		gen_jmp(JZ, &exception_base_0);			// if Z == 1, bit = 0 means base == 0x00000000
		gen(ORI, REG_EDI, 0xfff00000);
		gen_jmp_target(&exception_base_0);

		gen(MOVRM, (UINT32)&ppc.pc, REG_EDI);
		drccore_insert_dispatcher();
	}

	gen_jmp_target(&branch_link6);

	return 0;
}

static UINT32 drc_twi(UINT32 op)
{
	JUMP_TARGET branch_link1;
	JUMP_TARGET branch_link2;
	JUMP_TARGET branch_link3;
	JUMP_TARGET branch_link4;
	JUMP_TARGET branch_link5;
	JUMP_TARGET branch_link6;
	int do_link1 = 0;
	int do_link2 = 0;
	int do_link3 = 0;
	int do_link4 = 0;
	int do_link5 = 0;
	init_jmp_target(&branch_link1);
	init_jmp_target(&branch_link2);
	init_jmp_target(&branch_link3);
	init_jmp_target(&branch_link4);
	init_jmp_target(&branch_link5);
	init_jmp_target(&branch_link6);

	gen(MOVMR, REG_EAX, (UINT32)&ppc.r[RA]);
	gen(MOVI, REG_EDX, SIMM16);
	gen(CMP, REG_EAX, REG_EDX);

	if (RT & 0x10)
	{
		gen_jmp(JL, &branch_link1);		// less than = signed <
		do_link1 = 1;
	}
	if (RT & 0x08)
	{
		gen_jmp(JG, &branch_link2);		// greater = signed >
		do_link2 = 1;
	}
	if (RT & 0x04)
	{
		gen_jmp(JZ, &branch_link3);		// equal
		do_link3 = 1;
	}
	if (RT & 0x02)
	{
		gen_jmp(JB, &branch_link4);		// below = unsigned <
		do_link4 = 1;
	}
	if (RT & 0x01)
	{
		gen_jmp(JA, &branch_link5);		// above = unsigned >
		do_link5 = 1;
	}

	gen_jmp(JMP, &branch_link6);

	if (do_link1)
	{
		gen_jmp_target(&branch_link1);
	}
	if (do_link2)
	{
		gen_jmp_target(&branch_link2);
	}
	if (do_link3)
	{
		gen_jmp_target(&branch_link3);
	}
	if (do_link4)
	{
		gen_jmp_target(&branch_link4);
	}
	if (do_link5)
	{
		gen_jmp_target(&branch_link5);
	}

	// generate exception
	{
		JUMP_TARGET clear_le, exception_base_0;
		init_jmp_target(&clear_le);
		init_jmp_target(&exception_base_0);

		gen(MOVIM, (UINT32)&ppc.srr0, drc_pc+4);

		gen(MOVMR, REG_EAX, (UINT32)&ppc.msr);
		gen(ANDI, REG_EAX, 0xff73);
		gen(MOVRM, (UINT32)&ppc.srr1, REG_EAX);

		// Clear POW, EE, PR, FP, FE0, SE, BE, FE1, IR, DR, RI
		gen(ANDI, REG_EAX, ~(MSR_POW | MSR_EE | MSR_PR | MSR_FP | MSR_FE0 | MSR_SE | MSR_BE | MSR_FE1 | MSR_IR | MSR_DR | MSR_RI));

		// Set LE to ILE
		gen(ANDI, REG_EAX, ~MSR_LE);		// clear LE first
		gen(TESTI, REG_EAX, MSR_ILE);
		gen_jmp(JZ, &clear_le);				// if Z == 0, bit = 1
		gen(ORI, REG_EAX, MSR_LE);			// set LE
		gen_jmp_target(&clear_le);

		gen(MOVRM, (UINT32)&ppc.msr, REG_EAX);

		gen(MOVI, REG_EDI, 0x00000700);			// exception vector
		gen(TESTI, REG_EAX, MSR_IP);
		gen_jmp(JZ, &exception_base_0);			// if Z == 1, bit = 0 means base == 0x00000000
		gen(ORI, REG_EDI, 0xfff00000);
		gen_jmp_target(&exception_base_0);

		gen(MOVRM, (UINT32)&ppc.pc, REG_EDI);
		drccore_insert_dispatcher();
	}

	gen_jmp_target(&branch_link6);

	return 0;
}

static UINT32 drc_xor(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_XOR

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_xorx), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVMR, REG_EDX, PPCREG(RS));
	gen(XORMR, REG_EDX, PPCREG(RB));
	gen(MOVRM, PPCREG(RA), REG_EDX);
	if (RCBIT)
	{
		insert_set_cr0(REG_EDX);
	}

#endif
	return 0;
}

static UINT32 drc_xori(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_XORI

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_xori), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVMR, REG_EDX, PPCREG(RS));
	gen(XORI, REG_EDX, UIMM16);
	gen(MOVRM, PPCREG(RA), REG_EDX);

#endif
	return 0;
}

static UINT32 drc_xoris(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_XORIS

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_xoris), 0);
	gen(ADDI, REG_ESP, 4);

#else

	gen(MOVMR, REG_EDX, PPCREG(RS));
	gen(XORI, REG_EDX, UIMM16 << 16);
	gen(MOVRM, PPCREG(RA), REG_EDX);

#endif
	return 0;
}

static UINT32 drc_dccci(UINT32 op)
{
	return 0;
}

static UINT32 drc_dcread(UINT32 op)
{
	return 0;
}

static UINT32 drc_icbt(UINT32 op)
{
	return 0;
}

static UINT32 drc_iccci(UINT32 op)
{
	return 0;
}

static UINT32 drc_icread(UINT32 op)
{
	return 0;
}



static UINT32 drc_invalid(UINT32 op)
{
	error("PPCDRC: Invalid opcode %08X PC : %X\n", op, ppc.pc);
	return 0;
}

static UINT32 drc_lfs(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_LFS

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_lfs), 0);
	gen(ADDI, REG_ESP, 4);

#else

#if ENABLE_FASTRAM_PATH_FPU
	if (RA == 0)
	{
		UINT32 address = SIMM16;
		if (address < 0x800000)
		{
			gen(MOVMR, REG_EAX, (UINT32)ram + address);
			gen(BSWAP, REG_EAX, 0);
			gen(MOVDRX, REG_XMM0, REG_EAX);
			gen(CVTSS2SD, REG_XMM1, REG_XMM0);
			gen(MOVQXM, (UINT32)&ppc.fpr[RT], REG_XMM1);
		}
		else
		{
			gen(PUSHI, SIMM16, 0);
			gen(CALLI, (UINT32)m3_ppc_read_32, 0);
			gen(ADDI, REG_ESP, 4);
			gen(MOVDRX, REG_XMM0, REG_EAX);
			gen(CVTSS2SD, REG_XMM1, REG_XMM0);
			gen(MOVQXM, (UINT32)&ppc.fpr[RT], REG_XMM1);
		}
	}
	else
	{
		JUMP_TARGET lfs_slow_path, lfs_end;
		init_jmp_target(&lfs_slow_path);
		init_jmp_target(&lfs_end);

		gen(MOVMR, REG_EDX, PPCREG(RA));
		gen(ADDI, REG_EDX, SIMM16);
		gen(CMPI, REG_EDX, 0x800000);
		gen_jmp(JAE, &lfs_slow_path);
		// fast path
		gen_mov_dpr_to_reg(REG_EAX, ram, REG_EDX);
		gen(BSWAP, REG_EAX, 0);
		gen_jmp(JMP, &lfs_end);
		// slow path
		gen_jmp_target(&lfs_slow_path);
		gen(PUSH, REG_EDX, 0);
		gen(CALLI, (UINT32)m3_ppc_read_32, 0);
		gen(ADDI, REG_ESP, 4);
		// end
		gen_jmp_target(&lfs_end);
		gen(MOVDRX, REG_XMM0, REG_EAX);
		gen(CVTSS2SD, REG_XMM1, REG_XMM0);
		gen(MOVQXM, (UINT32)&ppc.fpr[RT], REG_XMM1);
	}
#else
	if (RA == 0)
	{
		gen(PUSHI, SIMM16, 0);
	}
	else
	{
		gen(MOVMR, REG_EDX, PPCREG(RA));
		gen(ADDI, REG_EDX, SIMM16);
		gen(PUSH, REG_EDX, 0);
	}
	gen(CALLI, (UINT32)m3_ppc_read_32, 0);
	gen(ADDI, REG_ESP, 4);
	gen(MOVDRX, REG_XMM0, REG_EAX);
	gen(CVTSS2SD, REG_XMM1, REG_XMM0);
	gen(MOVQXM, (UINT32)&ppc.fpr[RT], REG_XMM1);
#endif

#endif
	return 0;
}

static UINT32 drc_lfsu(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_LFSU

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_lfsu), 0);
	gen(ADDI, REG_ESP, 4);

#else

#if ENABLE_FASTRAM_PATH_FPU
	{
		JUMP_TARGET lfsu_slow_path, lfsu_end;
		init_jmp_target(&lfsu_slow_path);
		init_jmp_target(&lfsu_end);

		gen(MOVMR, REG_EDX, PPCREG(RA));
		gen(ADDI, REG_EDX, SIMM16);
		gen(MOVRM, PPCREG(RA), REG_EDX);
		gen(CMPI, REG_EDX, 0x800000);
		gen_jmp(JAE, &lfsu_slow_path);
		// fast path
		gen_mov_dpr_to_reg(REG_EAX, ram, REG_EDX);
		gen(BSWAP, REG_EAX, 0);
		gen_jmp(JMP, &lfsu_end);
		// slow path
		gen_jmp_target(&lfsu_slow_path);
		gen(PUSH, REG_EDX, 0);
		gen(CALLI, (UINT32)m3_ppc_read_32, 0);
		gen(ADDI, REG_ESP, 4);
		// end
		gen_jmp_target(&lfsu_end);
		gen(MOVDRX, REG_XMM0, REG_EAX);
		gen(CVTSS2SD, REG_XMM1, REG_XMM0);
		gen(MOVQXM, (UINT32)&ppc.fpr[RT], REG_XMM1);
	}
#else
	gen(MOVMR, REG_EDX, PPCREG(RA));
	gen(ADDI, REG_EDX, SIMM16);
	gen(MOVRM, PPCREG(RA), REG_EDX);
	gen(PUSH, REG_EDX, 0);
	gen(CALLI, (UINT32)m3_ppc_read_32, 0);
	gen(ADDI, REG_ESP, 4);
	gen(MOVDRX, REG_XMM0, REG_EAX);
	gen(CVTSS2SD, REG_XMM1, REG_XMM0);
	gen(MOVQXM, (UINT32)&ppc.fpr[RT], REG_XMM1);
#endif

#endif
	return 0;
}

static UINT32 drc_lfd(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_lfd), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_lfdu(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_lfdu), 0);
	gen(ADDI, REG_ESP, 4);


	return 0;
}

static UINT32 drc_stfs(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_STFS

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_stfs), 0);
	gen(ADDI, REG_ESP, 4);

#else

#if ENABLE_FASTRAM_PATH_FPU
	if (RA == 0)
	{
		UINT32 address = SIMM16;
		gen(MOVQMX, REG_XMM0, (UINT32)&ppc.fpr[RT]);
		gen(CVTSD2SS, REG_XMM1, REG_XMM0);
		gen(MOVDXR, REG_EDX, REG_XMM1);
		if (address < 0x800000)
		{
			gen(BSWAP, REG_EDX, 0);
			gen(MOVRM, (UINT32)ram + address, REG_EDX);
		}
		else
		{
			gen(PUSH, REG_EDX, 0);
			gen(PUSHI, SIMM16, 0);
			gen(CALLI, (UINT32)m3_ppc_write_32, 0);
			gen(ADDI, REG_ESP, 8);
		}
	}
	else
	{
		JUMP_TARGET stfs_slow_path, stfs_end;
		init_jmp_target(&stfs_slow_path);
		init_jmp_target(&stfs_end);

		gen(MOVQMX, REG_XMM0, (UINT32)&ppc.fpr[RT]);
		gen(CVTSD2SS, REG_XMM1, REG_XMM0);
		gen(MOVDXR, REG_EDX, REG_XMM1);
		gen(MOVMR, REG_EAX, PPCREG(RA));
		gen(ADDI, REG_EAX, SIMM16);
		gen(CMPI, REG_EAX, 0x800000);
		gen_jmp(JAE, &stfs_slow_path);
		// fast path
		gen(BSWAP, REG_EDX, 0);
		gen_mov_reg_to_dpr(REG_EDX, ram, REG_EAX);
		gen_jmp(JMP, &stfs_end);
		// slow path
		gen_jmp_target(&stfs_slow_path);
		gen(PUSH, REG_EDX, 0);
		gen(PUSH, REG_EAX, 0);
		gen(CALLI, (UINT32)m3_ppc_write_32, 0);
		gen(ADDI, REG_ESP, 8);
		// end
		gen_jmp_target(&stfs_end);
	}
#else
	gen(MOVQMX, REG_XMM0, (UINT32)&ppc.fpr[RT]);
	gen(CVTSD2SS, REG_XMM1, REG_XMM0);
	gen(MOVDXR, REG_EAX, REG_XMM1);
	gen(PUSH, REG_EAX, 0);

	if (RA == 0)
	{
		gen(PUSHI, SIMM16, 0);
	}
	else
	{
		gen(MOVMR, REG_EDX, PPCREG(RA));
		gen(ADDI, REG_EDX, SIMM16);
		gen(PUSH, REG_EDX, 0);
	}

	gen(CALLI, (UINT32)m3_ppc_write_32, 0);
	gen(ADDI, REG_ESP, 8);
#endif

#endif
	return 0;
}

static UINT32 drc_stfsu(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_STFSU

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_stfsu), 0);
	gen(ADDI, REG_ESP, 4);

#else

#if ENABLE_FASTRAM_PATH_FPU
	{
		JUMP_TARGET stfsu_slow_path, stfsu_end;
		init_jmp_target(&stfsu_slow_path);
		init_jmp_target(&stfsu_end);

		gen(MOVQMX, REG_XMM0, (UINT32)&ppc.fpr[RT]);
		gen(CVTSD2SS, REG_XMM1, REG_XMM0);
		gen(MOVDXR, REG_EDX, REG_XMM1);
		gen(MOVMR, REG_EAX, PPCREG(RA));
		gen(ADDI, REG_EAX, SIMM16);
		gen(MOVRM, PPCREG(RA), REG_EAX);
		gen(CMPI, REG_EAX, 0x800000);
		gen_jmp(JAE, &stfsu_slow_path);
		// fast path
		gen(BSWAP, REG_EDX, 0);
		gen_mov_reg_to_dpr(REG_EDX, ram, REG_EAX);
		gen_jmp(JMP, &stfsu_end);
		// slow path
		gen_jmp_target(&stfsu_slow_path);
		gen(PUSH, REG_EDX, 0);
		gen(PUSH, REG_EAX, 0);
		gen(CALLI, (UINT32)m3_ppc_write_32, 0);
		gen(ADDI, REG_ESP, 8);
		// end
		gen_jmp_target(&stfsu_end);
	}
#else
	gen(MOVQMX, REG_XMM0, (UINT32)&ppc.fpr[RT]);
	gen(CVTSD2SS, REG_XMM1, REG_XMM0);
	gen(MOVDXR, REG_EAX, REG_XMM1);
	gen(PUSH, REG_EAX, 0);

	gen(MOVMR, REG_EDX, PPCREG(RA));
	gen(ADDI, REG_EDX, SIMM16);
	gen(MOVRM, PPCREG(RA), REG_EDX);
	gen(PUSH, REG_EDX, 0);

	gen(CALLI, (UINT32)m3_ppc_write_32, 0);
	gen(ADDI, REG_ESP, 8);
#endif

#endif
	return 0;
}

static UINT32 drc_stfd(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_stfd), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_stfdu(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_stfdu), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_lfdux(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_lfdux), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_lfdx(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_lfdx), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_lfsux(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_LFSUX

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_lfsux), 0);
	gen(ADDI, REG_ESP, 4);

#else

#if ENABLE_FASTRAM_PATH_FPU
	{
		JUMP_TARGET lfsux_slow_path, lfsux_end;
		init_jmp_target(&lfsux_slow_path);
		init_jmp_target(&lfsux_end);

		gen(MOVMR, REG_EDX, PPCREG(RA));
		gen(ADDMR, REG_EDX, PPCREG(RB));
		gen(MOVRM, PPCREG(RA), REG_EDX);
		gen(CMPI, REG_EDX, 0x800000);
		gen_jmp(JAE, &lfsux_slow_path);
		// fast path
		gen_mov_dpr_to_reg(REG_EAX, ram, REG_EDX);
		gen(BSWAP, REG_EAX, 0);
		gen_jmp(JMP, &lfsux_end);
		// slow path
		gen_jmp_target(&lfsux_slow_path);
		gen(PUSH, REG_EDX, 0);
		gen(CALLI, (UINT32)m3_ppc_read_32, 0);
		gen(ADDI, REG_ESP, 4);
		// end
		gen_jmp_target(&lfsux_end);
		gen(MOVDRX, REG_XMM0, REG_EAX);
		gen(CVTSS2SD, REG_XMM1, REG_XMM0);
		gen(MOVQXM, (UINT32)&ppc.fpr[RT], REG_XMM1);
	}
#else
	gen(MOVMR, REG_EDX, PPCREG(RA));
	gen(ADDMR, REG_EDX, PPCREG(RB));
	gen(MOVRM, PPCREG(RA), REG_EDX);
	gen(PUSH, REG_EDX, 0);
	gen(CALLI, (UINT32)m3_ppc_read_32, 0);
	gen(ADDI, REG_ESP, 4);
	gen(MOVDRX, REG_XMM0, REG_EAX);
	gen(CVTSS2SD, REG_XMM1, REG_XMM0);
	gen(MOVQXM, (UINT32)&ppc.fpr[RT], REG_XMM1);
#endif

#endif
	return 0;
}

static UINT32 drc_lfsx(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_LFSX

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_lfsx), 0);
	gen(ADDI, REG_ESP, 4);

#else

#if ENABLE_FASTRAM_PATH_FPU
	{
		JUMP_TARGET lfsx_slow_path, lfsx_end;
		init_jmp_target(&lfsx_slow_path);
		init_jmp_target(&lfsx_end);

		gen(MOVMR, REG_EDX, PPCREG(RB));
		if (RA != 0)
		{
			gen(ADDMR, REG_EDX, PPCREG(RA));
		}
		gen(CMPI, REG_EDX, 0x800000);
		gen_jmp(JAE, &lfsx_slow_path);
		// fast path
		gen_mov_dpr_to_reg(REG_EAX, ram, REG_EDX);
		gen(BSWAP, REG_EAX, 0);
		gen_jmp(JMP, &lfsx_end);
		// slow path
		gen_jmp_target(&lfsx_slow_path);
		gen(PUSH, REG_EDX, 0);
		gen(CALLI, (UINT32)m3_ppc_read_32, 0);
		gen(ADDI, REG_ESP, 4);
		// end
		gen_jmp_target(&lfsx_end);
		gen(MOVDRX, REG_XMM0, REG_EAX);
		gen(CVTSS2SD, REG_XMM1, REG_XMM0);
		gen(MOVQXM, (UINT32)&ppc.fpr[RT], REG_XMM1);
	}
#else
	gen(MOVMR, REG_EDX, PPCREG(RB));
	if (RA != 0)
	{
		gen(ADDMR, REG_EDX, PPCREG(RA));
	}
	gen(PUSH, REG_EDX, 0);
	gen(CALLI, (UINT32)m3_ppc_read_32, 0);
	gen(ADDI, REG_ESP, 4);
	gen(MOVDRX, REG_XMM0, REG_EAX);
	gen(CVTSS2SD, REG_XMM1, REG_XMM0);
	gen(MOVQXM, (UINT32)&ppc.fpr[RT], REG_XMM1);
#endif

#endif
	return 0;
}

static UINT32 drc_mfsr(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_mfsr), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_mfsrin(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_mfsrin), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_mftb(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_mftb), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_mtsr(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_mtsr), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_mtsrin(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_mtsrin), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_dcba(UINT32 op)
{
	return 0;
}

static UINT32 drc_stfdux(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_stfdux), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_stfdx(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_stfdx), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_stfiwx(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_stfiwx), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_stfsux(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_STFSUX

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_stfsux), 0);
	gen(ADDI, REG_ESP, 4);

#else

#if ENABLE_FASTRAM_PATH_FPU
	{
		JUMP_TARGET stfsux_slow_path, stfsux_end;
		init_jmp_target(&stfsux_slow_path);
		init_jmp_target(&stfsux_end);

		gen(MOVQMX, REG_XMM0, (UINT32)&ppc.fpr[RT]);
		gen(CVTSD2SS, REG_XMM1, REG_XMM0);
		gen(MOVDXR, REG_EDX, REG_XMM1);
		gen(MOVMR, REG_EAX, PPCREG(RA));
		gen(ADDMR, REG_EAX, PPCREG(RB));
		gen(MOVRM, PPCREG(RA), REG_EAX);
		gen(CMPI, REG_EAX, 0x800000);
		gen_jmp(JAE, &stfsux_slow_path);
		// fast path
		gen(BSWAP, REG_EDX, 0);
		gen_mov_reg_to_dpr(REG_EDX, ram, REG_EAX);
		gen_jmp(JMP, &stfsux_end);
		// slow path
		gen_jmp_target(&stfsux_slow_path);
		gen(PUSH, REG_EDX, 0);
		gen(PUSH, REG_EAX, 0);
		gen(CALLI, (UINT32)m3_ppc_write_32, 0);
		gen(ADDI, REG_ESP, 8);
		// end
		gen_jmp_target(&stfsux_end);
	}
#else
	gen(MOVQMX, REG_XMM0, (UINT32)&ppc.fpr[RT]);
	gen(CVTSD2SS, REG_XMM1, REG_XMM0);
	gen(MOVDXR, REG_EAX, REG_XMM1);
	gen(PUSH, REG_EAX, 0);

	gen(MOVMR, REG_EDX, PPCREG(RA));
	gen(ADDMR, REG_EDX, PPCREG(RB));
	gen(MOVRM, PPCREG(RA), REG_EDX);
	gen(PUSH, REG_EDX, 0);

	gen(CALLI, (UINT32)m3_ppc_write_32, 0);
	gen(ADDI, REG_ESP, 8);
#endif

#endif
	return 0;
}

static UINT32 drc_stfsx(UINT32 op)
{
#if !COMPILE_OPS || DONT_COMPILE_STFSX

	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_stfsx), 0);
	gen(ADDI, REG_ESP, 4);

#else

#if ENABLE_FASTRAM_PATH_FPU
	{
		JUMP_TARGET stfsux_slow_path, stfsux_end;
		init_jmp_target(&stfsux_slow_path);
		init_jmp_target(&stfsux_end);

		gen(MOVQMX, REG_XMM0, (UINT32)&ppc.fpr[RT]);
		gen(CVTSD2SS, REG_XMM1, REG_XMM0);
		gen(MOVDXR, REG_EDX, REG_XMM1);
		gen(MOVMR, REG_EAX, PPCREG(RB));
		if (RA != 0)
		{
			gen(ADDMR, REG_EAX, PPCREG(RA));
		}
		gen(CMPI, REG_EAX, 0x800000);
		gen_jmp(JAE, &stfsux_slow_path);
		// fast path
		gen(BSWAP, REG_EDX, 0);
		gen_mov_reg_to_dpr(REG_EDX, ram, REG_EAX);
		gen_jmp(JMP, &stfsux_end);
		// slow path
		gen_jmp_target(&stfsux_slow_path);
		gen(PUSH, REG_EDX, 0);
		gen(PUSH, REG_EAX, 0);
		gen(CALLI, (UINT32)m3_ppc_write_32, 0);
		gen(ADDI, REG_ESP, 8);
		// end
		gen_jmp_target(&stfsux_end);
	}
#else
	gen(MOVQMX, REG_XMM0, (UINT32)&ppc.fpr[RT]);
	gen(CVTSD2SS, REG_XMM1, REG_XMM0);
	gen(MOVDXR, REG_EAX, REG_XMM1);
	gen(PUSH, REG_EAX, 0);

	gen(MOVMR, REG_EDX, PPCREG(RB));
	if (RA != 0)
	{
		gen(ADDMR, REG_EDX, PPCREG(RA));
	}
	gen(PUSH, REG_EDX, 0);

	gen(CALLI, (UINT32)m3_ppc_write_32, 0);
	gen(ADDI, REG_ESP, 8);
#endif

#endif
	return 0;
}

static UINT32 drc_tlbia(UINT32 op)
{
	return 0;
}

static UINT32 drc_tlbie(UINT32 op)
{
	return 0;
}

static UINT32 drc_tlbsync(UINT32 op)
{
	return 0;
}

static UINT32 drc_eciwx(UINT32 op)
{
	error("PPCDRC: eciwx unimplemented\n");
	return 0;
}

static UINT32 drc_ecowx(UINT32 op)
{
	error("PPCDRC: ecowx unimplemented\n");
	return 0;
}

static UINT32 drc_fabs(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_fabsx), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_fadd(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_faddx), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_fcmpo(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_fcmpo), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_fcmpu(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_fcmpu), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_fctiw(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_fctiwx), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_fctiwz(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_fctiwzx), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_fdiv(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_fdivx), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_fmr(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_fmrx), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_fnabs(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_fnabsx), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_fneg(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_fnegx), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_frsp(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_frspx), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_frsqrte(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_frsqrtex), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_fsqrt(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_fsqrtx), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_fsub(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_fsubx), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_mffs(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_mffsx), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_mtfsb0(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_mtfsb0x), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_mtfsb1(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_mtfsb1x), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_mtfsf(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_mtfsfx), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_mtfsfi(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_mtfsfix), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_mcrfs(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_mcrfs), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_fadds(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_faddsx), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_fdivs(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_fdivsx), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_fres(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_fresx), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_fsqrts(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_fsqrtsx), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_fsubs(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_fsubsx), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_fmadd(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_fmaddx), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_fmsub(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_fmsubx), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_fmul(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_fmulx), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_fnmadd(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_fnmaddx), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_fnmsub(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_fnmsubx), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_fsel(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_fselx), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_fmadds(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_fmaddsx), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_fmsubs(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_fmsubsx), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_fmuls(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_fmulsx), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_fnmadds(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_fnmaddsx), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}

static UINT32 drc_fnmsubs(UINT32 op)
{
	gen(PUSHI, op, 0);
	gen(CALLI, (UINT32)(ppc_fnmsubsx), 0);
	gen(ADDI, REG_ESP, 4);

	return 0;
}
