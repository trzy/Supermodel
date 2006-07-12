void ppc603_exception(int exception)
{
	switch( exception )
	{
		case EXCEPTION_IRQ:		/* External Interrupt */
			if( ppc_get_msr() & MSR_EE ) {
				UINT32 msr = ppc_get_msr();

				SRR0 = ppc.npc;
				SRR1 = msr & 0xff73;

				msr &= ~(MSR_POW | MSR_EE | MSR_PR | MSR_FP | MSR_FE0 | MSR_SE | MSR_BE | MSR_FE1 | MSR_IR | MSR_DR | MSR_RI);
				if( msr & MSR_ILE )
					msr |= MSR_LE;
				else
					msr &= ~MSR_LE;
				ppc_set_msr(msr);

				if( msr & MSR_IP )
					ppc.npc = 0xfff00000 | 0x0500;
				else
					ppc.npc = 0x00000000 | 0x0500;

				ppc.interrupt_pending &= ~0x1;
				ppc_change_pc(ppc.npc);
			}
			break;

		case EXCEPTION_DECREMENTER:		/* Decrementer overflow exception */
			if( ppc_get_msr() & MSR_EE ) {
				UINT32 msr = ppc_get_msr();

				SRR0 = ppc.npc;
				SRR1 = msr & 0xff73;

				msr &= ~(MSR_POW | MSR_EE | MSR_PR | MSR_FP | MSR_FE0 | MSR_SE | MSR_BE | MSR_FE1 | MSR_IR | MSR_DR | MSR_RI);
				if( msr & MSR_ILE )
					msr |= MSR_LE;
				else
					msr &= ~MSR_LE;
				ppc_set_msr(msr);

				if( msr & MSR_IP )
					ppc.npc = 0xfff00000 | 0x0900;
				else
					ppc.npc = 0x00000000 | 0x0900;

				ppc.interrupt_pending &= ~0x2;
				ppc_change_pc(ppc.npc);
			}
			break;

		case EXCEPTION_TRAP:			/* Program exception / Trap */
			{
				UINT32 msr = ppc_get_msr();

				SRR0 = ppc.pc;
				SRR1 = (msr & 0xff73) | 0x20000;	/* 0x20000 = TRAP bit */

				msr &= ~(MSR_POW | MSR_EE | MSR_PR | MSR_FP | MSR_FE0 | MSR_SE | MSR_BE | MSR_FE1 | MSR_IR | MSR_DR | MSR_RI);
				if( msr & MSR_ILE )
					msr |= MSR_LE;
				else
					msr &= ~MSR_LE;
				ppc_set_msr(msr);

				if( msr & MSR_IP )
					ppc.npc = 0xfff00000 | 0x0700;
				else
					ppc.npc = 0x00000000 | 0x0700;
				ppc_change_pc(ppc.npc);
			}
			break;

		case EXCEPTION_SYSTEM_CALL:		/* System call */
			{
				UINT32 msr = ppc_get_msr();

				SRR0 = ppc.npc;
				SRR1 = (msr & 0xff73);

				msr &= ~(MSR_POW | MSR_EE | MSR_PR | MSR_FP | MSR_FE0 | MSR_SE | MSR_BE | MSR_FE1 | MSR_IR | MSR_DR | MSR_RI);
				if( msr & MSR_ILE )
					msr |= MSR_LE;
				else
					msr &= ~MSR_LE;
				ppc_set_msr(msr);

				if( msr & MSR_IP )
					ppc.npc = 0xfff00000 | 0x0c00;
				else
					ppc.npc = 0x00000000 | 0x0c00;
				ppc_change_pc(ppc.npc);
			}
			break;

		case EXCEPTION_SMI:
			if( ppc_get_msr() & MSR_EE ) {
				UINT32 msr = ppc_get_msr();

				SRR0 = ppc.npc;
				SRR1 = msr & 0xff73;

				msr &= ~(MSR_POW | MSR_EE | MSR_PR | MSR_FP | MSR_FE0 | MSR_SE | MSR_BE | MSR_FE1 | MSR_IR | MSR_DR | MSR_RI);
				if( msr & MSR_ILE )
					msr |= MSR_LE;
				else
					msr &= ~MSR_LE;
				ppc_set_msr(msr);

				if( msr & MSR_IP )
					ppc.npc = 0xfff00000 | 0x1400;
				else
					ppc.npc = 0x00000000 | 0x1400;

				ppc.interrupt_pending &= ~0x4;
				ppc_change_pc(ppc.npc);
			}
			break;

		case EXCEPTION_DSI:
			{
				UINT32 msr = ppc_get_msr();

				SRR0 = ppc.npc;
				SRR1 = msr & 0xff73;

				msr &= ~(MSR_POW | MSR_EE | MSR_PR | MSR_FP | MSR_FE0 | MSR_SE | MSR_BE | MSR_FE1 | MSR_IR | MSR_DR | MSR_RI);
				if( msr & MSR_ILE )
					msr |= MSR_LE;
				else
					msr &= ~MSR_LE;
				ppc_set_msr(msr);

				if( msr & MSR_IP )
					ppc.npc = 0xfff00000 | 0x0300;
				else
					ppc.npc = 0x00000000 | 0x0300;

				ppc.interrupt_pending &= ~0x4;
				ppc_change_pc(ppc.npc);
			}
			break;

		case EXCEPTION_ISI:
			{
				UINT32 msr = ppc_get_msr();

				SRR0 = ppc.npc;
				SRR1 = msr & 0xff73;

				msr &= ~(MSR_POW | MSR_EE | MSR_PR | MSR_FP | MSR_FE0 | MSR_SE | MSR_BE | MSR_FE1 | MSR_IR | MSR_DR | MSR_RI);
				if( msr & MSR_ILE )
					msr |= MSR_LE;
				else
					msr &= ~MSR_LE;
				ppc_set_msr(msr);

				if( msr & MSR_IP )
					ppc.npc = 0xfff00000 | 0x0400;
				else
					ppc.npc = 0x00000000 | 0x0400;

				ppc.interrupt_pending &= ~0x4;
				ppc_change_pc(ppc.npc);
			}
			break;

		default:
			error("ppc: Unhandled exception %d", exception);
			break;
	}
}

static void ppc603_set_smi_line(int state)
{
	if( state ) {
		ppc.interrupt_pending |= 0x4;
	}
}

static void ppc603_check_interrupts(void)
{
	if (MSR & MSR_EE)
	{
		if (ppc.interrupt_pending != 0)
		{
			if (ppc.interrupt_pending & 0x1)
			{
				ppc603_exception(EXCEPTION_IRQ);
			}
			else if (ppc.interrupt_pending & 0x2)
			{
				ppc603_exception(EXCEPTION_DECREMENTER);
			}	
			else if (ppc.interrupt_pending & 0x4)
			{
				ppc603_exception(EXCEPTION_SMI);
			}
		}
	}
}

void ppc_reset(void)
{
	ppc.pc = ppc.npc = 0xfff00100;

	ppc_set_msr(0x40);
	ppc_change_pc(ppc.pc);

	ppc.hid0 = 1;

	ppc.interrupt_pending = 0;
}

int ppc_execute(int cycles)
{
	UINT32 opcode;
	ppc_icount = cycles;
	ppc_tb_base_icount = cycles;
	ppc_dec_base_icount = cycles + ppc.dec_frac;

	// check if decrementer exception occurs during execution
	if ((UINT32)(DEC - (cycles / (bus_freq_multiplier * 2))) > (UINT32)(DEC))
	{
		ppc_dec_trigger_cycle = ((cycles / (bus_freq_multiplier * 2)) - DEC) * 4;
	}
	else
	{
		ppc_dec_trigger_cycle = 0x7fffffff;
	}

	ppc_change_pc(ppc.npc);

	/*{
		char string1[200];
		char string2[200];
		opcode = BSWAP32(*ppc.op);
		DisassemblePowerPC(opcode, ppc.npc, string1, string2, TRUE);
		printf("%08X: %s %s\n", ppc.npc, string1, string2);
	}*/
	printf("trigger cycle %d (%08X)\n", ppc_dec_trigger_cycle, ppc_dec_trigger_cycle);
	printf("tb = %08X %08X\n", (UINT32)(ppc.tb >> 32), (UINT32)(ppc.tb));

	ppc603_check_interrupts();

	while( ppc_icount > 0 )
	{
		ppc.pc = ppc.npc;

		opcode = BSWAP32(*ppc.op++);

		ppc.npc = ppc.pc + 4;
		switch(opcode >> 26)
		{
			case 19:	optable19[(opcode >> 1) & 0x3ff](opcode); break;
			case 31:	optable31[(opcode >> 1) & 0x3ff](opcode); break;
			case 59:	optable59[(opcode >> 1) & 0x3ff](opcode); break;
			case 63:	optable63[(opcode >> 1) & 0x3ff](opcode); break;
			default:	optable[opcode >> 26](opcode); break;
		}

		ppc_icount--;

		if(ppc_icount == ppc_dec_trigger_cycle)
		{
			printf("dec int at %d\n", ppc_icount);
			ppc.interrupt_pending |= 0x2;
			ppc603_check_interrupts();
		}

		//ppc603_check_interrupts();
	}

	// update timebase
	// timebase is incremented once every four core clock cycles, so adjust the cycles accordingly
	ppc.tb += ((ppc_tb_base_icount - ppc_icount) / 4);

	// update decrementer
	ppc.dec_frac = ((ppc_dec_base_icount - ppc_icount) % (bus_freq_multiplier * 2));
	DEC -= ((ppc_dec_base_icount - ppc_icount) / (bus_freq_multiplier * 2));

	/*
	{
		char string1[200];
		char string2[200];
		opcode = BSWAP32(*ppc.op);
		DisassemblePowerPC(opcode, ppc.npc, string1, string2, TRUE);
		printf("%08X: %s %s\n", ppc.npc, string1, string2);
	}
	*/

	return cycles - ppc_icount;
}
