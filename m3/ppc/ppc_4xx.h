/*
 * ppc_4xx.h
 *
 * PowerPC 4xx Emulationn. To be included only by ppc.c
 */

/*
 * Serial Port
 */

static const char * spu_id[16] = {
	"SPLS",		"-",		"SPHS",		"-",
	"BRDH",		"BRDL",		"SPCTL",	"SPRC",
	"SPTC",		"SPXB",		"-",		"-",
	"-",		"-",		"-",		"-",
};

INLINE void spu_wb(u32 a, u8 d){

	switch(a & 15){

	case 0: ppc.spu.spls &= ~(d & 0xF6); return;
	case 2: ppc.spu.sphs &= ~(d & 0xC0); return;
	case 4: ppc.spu.brdh = d; return;
	case 5: ppc.spu.brdl = d; return;
	case 6: ppc.spu.spctl = d; return;

	case 7:
	ppc.spu.sprc = d;
		return;

	case 8: ppc.spu.sptc = d; return;
	case 9: ppc.spu.sptb = d; return;

	}

	printf("%08X : wb %08X = %02X\n", CIA, a, d);
	exit(1);
}

INLINE u8 spu_rb(u32 a){

	switch(a & 15){

	case 0: return(ppc.spu.spls);
	case 2: return(ppc.spu.sphs);
	case 4: return(ppc.spu.brdh);
	case 5: return(ppc.spu.brdl);
	case 6: return(ppc.spu.spctl);
	case 7: return(ppc.spu.sprc);
	case 8: return(ppc.spu.sptc);
	case 9: return(ppc.spu.sprb);

	}

	printf("%08X : rb %08X\n", CIA, a);
	exit(1);
}

/*
 * DMA
 */

#define DMA_CR(ch)	ppc.dcr[0xC0 + (ch << 3) + 0]
#define DMA_CT(ch)	ppc.dcr[0xC0 + (ch << 3) + 1]
#define DMA_DA(ch)	ppc.dcr[0xC0 + (ch << 3) + 2]
#define DMA_SA(ch)	ppc.dcr[0xC0 + (ch << 3) + 3]
#define DMA_CC(ch)	ppc.dcr[0xC0 + (ch << 3) + 4]
#define DMA_SR		ppc.dcr[0xE0]

INLINE  void ppc_dma(u32 ch){

	if(DMA_CR(ch) & 0x000000F0){

		printf("ERROR: PPC chained DMA %u\n", ch);
		exit(1);
	}

	switch((DMA_CR(ch) >> 26) & 3){

	case 0: // 1 byte

		// internal transfer (prolly *SPU*)

		while(DMA_CT(ch)){

			//d = ppc_r_b(DMA_SA(ch));
			if(DMA_CR(ch) & 0x01000000) DMA_SA(ch)++;
			//ppc_w_b(DMA_DA(ch), d);
			if(DMA_CR(ch) & 0x02000000) DMA_DA(ch)++;
			DMA_CT(ch)--;
		}

		break;

	case 1: // 2 bytes
		printf("%08X : DMA%u %08X -> %08X * %08X (2-bytes)\n", CIA, ch, DMA_SA(ch), DMA_DA(ch), DMA_CT(ch));
		exit(1);

	case 2: // 4 bytes
		printf("%08X : DMA%u %08X -> %08X * %08X (4-bytes)\n", CIA, ch, DMA_SA(ch), DMA_DA(ch), DMA_CT(ch));
		exit(1);

	default: // 16 bytes
		printf("%08X : DMA%u %08X -> %08X * %08X (16-bytes)\n", CIA, ch, DMA_SA(ch), DMA_DA(ch), DMA_CT(ch));
		exit(1);
	}

	if(DMA_CR(ch) & 0x40000000){ // DIE

		ppc.dcr[DCR_EXISR] |= (0x00800000 >> ch);
		ppc.irq_state = 1;
	}
}

/*
 * DCR Access
 */

INLINE void ppc_set_dcr(u32 n, u32 d){

	switch(n){

	case DCR_BEAR: ppc.dcr[n] = d; break;
	case DCR_BESR: ppc.dcr[n] = d; break;

	case DCR_DMACC0:
	case DCR_DMACC1:
	case DCR_DMACC2:
	case DCR_DMACC3:
		if(d){
			printf("chained dma\n");
			exit(1);
		}
		break;

	case DCR_DMACR0:
		ppc.dcr[n] = d;
		if(d & 0x80000000)
			ppc_dma(0);
		return;

	case DCR_DMACR1:
		ppc.dcr[n] = d;
		if(d & 0x80000000)
			ppc_dma(1);
		return;

	case DCR_DMACR2:
		ppc.dcr[n] = d;
		if(d & 0x80000000)
			ppc_dma(2);
		return;

	case DCR_DMACR3:
		ppc.dcr[n] = d;
		if(d & 0x80000000)
			ppc_dma(3);
		return;

	case DCR_DMASR:
		ppc.dcr[n] &= ~d;
		return;

	case DCR_EXIER:
		ppc.dcr[n] = d;
		break;

	case DCR_EXISR:
		ppc.dcr[n] = d;
		break;

	case DCR_IOCR:
		ppc.dcr[n] = d;
		break;
	}

	ppc.dcr[n] = d;
}

INLINE u32 ppc_get_dcr(u32 n){

	return(ppc.dcr[n]);
}

/*
 * MSR Access
 */

INLINE void ppc_set_msr(u32 d){

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

INLINE void ppc_set_spr(u32 n, u32 d){

	switch(n){

	case SPR_PVR:
		return;

	case SPR_PIT:
		if(d){
			printf("PIT = %i\n", d);
			exit(1);
		}
		ppc.spr[n] = d;
		ppc.pit_reload = d;
		break;

	case SPR_TSR:
		ppc.spr[n] &= ~d; // 1 clears, 0 does nothing
		return;
	}

	ppc.spr[n] = d;
}

INLINE u32 ppc_get_spr(u32 n){

	if(n == SPR_TBLO)
		return((u32)ppc.tb);
	else
	if(n == SPR_TBHI)
		return((u32)(ppc.tb >> 32));

	return(ppc.spr[n]);
}

/*
 * Memory Access
 */

INLINE u32 ppc_fetch(u32 a)
{
	return(ppc.rd(a & 0x0FFFFFFF));
}

INLINE u32 ppc_read_8(u32 a)
{
	if((a >> 28) == 4)
		return(spu_rb(a));

	return(ppc.rb(a & 0x0FFFFFFF));
}

INLINE u32 ppc_read_16(u32 a)
{
	return(ppc.rw(a & 0x0FFFFFFE));
}

INLINE u32 ppc_read_32(u32 a)
{
	return(ppc.rd(a & 0x0FFFFFFC));
}

INLINE u64 ppc_read_64(u32 a)
{
	return(0);
}

INLINE void ppc_write_8(u32 a, u32 d)
{
	if((a >> 28) == 4)
		spu_wb(a,d);
	else
		ppc.wb(a & 0x0FFFFFFF, d);
}

INLINE void ppc_write_16(u32 a, u32 d)
{
	ppc.ww(a & 0x0FFFFFFE, d);
}

INLINE void ppc_write_32(u32 a, u32 d)
{
	ppc.wd(a & 0x0FFFFFFC, d);
}

INLINE void ppc_write_64(u32 a, u64 d)
{
}

INLINE void ppc_test_irq(void){
}
