/*
 * ppc_6xx.h
 *
 * PowerPC 6xx Emulation. To be included only by ppc.c
 */

/*
 * MSR Access
 */

INLINE void ppc_set_msr(u32 d){

	if((ppc.msr ^ d) & 0x00020000){

		// TGPR

		u32 temp[4];

		temp[0] = ppc.gpr[0];
		temp[1] = ppc.gpr[1];
		temp[2] = ppc.gpr[2];
		temp[3] = ppc.gpr[3];

		ppc.gpr[0] = ppc.tgpr[0];
		ppc.gpr[1] = ppc.tgpr[1];
		ppc.gpr[2] = ppc.tgpr[2];
		ppc.gpr[3] = ppc.tgpr[3];

		ppc.tgpr[0] = temp[0];
		ppc.tgpr[1] = temp[1];
		ppc.tgpr[2] = temp[2];
		ppc.tgpr[3] = temp[3];
	}

	ppc.msr = d;

	if(d & 0x00080000){ // WE

		printf("PPC : waiting ...\n");
		exit(1);
	}
}

INLINE u32 ppc_get_msr(void){

	return(ppc.msr);
}

/*
 * SPR Access
 */

INLINE void ppc_set_spr(u32 n, u32 d)
{
	switch(n)
	{
	case SPR_DEC:

		if((d & 0x80000000) && !(ppc.spr[SPR_DEC] & 0x80000000))
		{
			/* trigger interrupt */
	
			printf("ERROR: set_spr to DEC triggers IRQ\n");
			exit(1);
		}

		ppc.spr[SPR_DEC] = d;
		break;

	case SPR_PVR:
		return;

	case SPR_PIT:
		if(d){
			printf("PIT = %i\n", d);
			exit(1);
		}
		ppc.spr[n] = d;
		ppc.pit_reload = d;
		return;

	case SPR_TSR:
		ppc.spr[n] &= ~d; // 1 clears, 0 does nothing
		return;

	case SPR_TBL_W:
	case SPR_TBL_R: // special 603e case
		ppc.tb &= 0xFFFFFFFF00000000;
		ppc.tb |= (u64)(d);
		return;

	case SPR_TBU_R:
	case SPR_TBU_W: // special 603e case
		printf("ERROR: set_spr - TBU_W = %08X\n", d);
		exit(1);

	}

	ppc.spr[n] = d;
}

INLINE u32 ppc_get_spr(u32 n)
{
	switch(n)
	{
	case SPR_TBL_R:
		printf("ERROR: get_spr - TBL_R\n");
		exit(1);

	case SPR_TBU_R:
		printf("ERROR: get_spr - TBU_R\n");
		exit(1);

	case SPR_TBL_W:
		printf("ERROR: invalid get_spr - TBL_W\n");
		exit(1);

	case SPR_TBU_W:
		printf("ERROR: invalid get_spr - TBU_W\n");
		exit(1);
	}

	return(ppc.spr[n]);
}

/*
 * Memory Access
 */

#define ITLB_ENABLED	(MSR & 0x20)
#define DTLB_ENABLED	(MSR & 0x10)

INLINE u32 ppc_fetch(u32 a)
{
#ifdef KHEPERIX_TEST
	return(ppc.read_op(a));
#else
    return ppc.read_32(a);
#endif
}

INLINE u32 ppc_read_8(u32 a)
{
	return(ppc.read_8(a));
}

INLINE u32 ppc_read_16(u32 a)
{
	return(ppc.read_16(a));
}

INLINE u32 ppc_read_32(u32 a)
{
	return(ppc.read_32(a));
}

INLINE u64 ppc_read_64(u32 a)
{
	return(ppc.read_64(a));
}

INLINE void ppc_write_8(u32 a, u32 d)
{
	ppc.write_8(a, d);
}

INLINE void ppc_write_16(u32 a, u32 d)
{
	ppc.write_16(a, d);
}

INLINE void ppc_write_32(u32 a, u32 d)
{
	ppc.write_32(a, d);
}

INLINE void ppc_write_64(u32 a, u64 d)
{
	ppc.write_64(a, d);
}

INLINE void ppc_test_irq(void)
{
	if(ppc.msr & 0x8000){

		/* external irq and decrementer enabled */

		if(ppc.irq_state){

			/* accept external interrupt */

			ppc.spr[SPR_SRR0] = ppc.pc;
			ppc.spr[SPR_SRR1] = ppc.msr;

			ppc.pc = (ppc.msr & 0x40) ? 0xFFF00500 : 0x00000500;
			ppc.msr = (ppc.msr & 0x11040) | ((ppc.msr >> 16) & 1);

			/* notify to the interrupt manager */

			if(ppc.irq_callback != NULL)
				ppc.irq_state = ppc.irq_callback();
            else
                ppc.irq_state = 0;

			ppc_update_pc();
			
#ifdef RECORD_BB_STATS			
			bb_in_exception = 1;
#endif			
		}else
		if(ppc.dec_expired){

			/* accept decrementer exception */

			ppc.spr[SPR_SRR0] = ppc.pc;
			ppc.spr[SPR_SRR1] = ppc.msr;

			ppc.pc = (ppc.msr & 0x40) ? 0xFFF00900 : 0x00000900;
			ppc.msr = (ppc.msr & 0x11040) | ((ppc.msr >> 16) & 1);

			/* clear pending decrementer exception */

			ppc.dec_expired = 0;

			ppc_update_pc();
			
#ifdef RECORD_BB_STATS			
			bb_in_exception = 1;
#endif						
		}
	}
}
