/*
 * decode.c
 *
 * PowerPC instruction decoders. These functions translate native PowerPC
 * opcodes into intermediate representation (IR). Pretty naked ATM.
 */

#include "drppc.h"
#include "internal.h"

extern INT decoded_bb_timing;

INT D_Addx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Addcx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Addex(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Addi(UINT32 op)
{
	// 0xFFF00104

	return DRPPC_OKAY;
}

INT D_Addic(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Addic_(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Addis(UINT32 op)
{
	// 0xFFF00100

	return DRPPC_OKAY;
}

INT D_Addmex(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Addzex(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Andx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Andcx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Andi_(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Andis_(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Bx(UINT32 op)
{
	return DRPPC_TERMINATOR;
}

INT D_Bcx(UINT32 op)
{
	return DRPPC_TERMINATOR;
}

INT D_Bcctrx(UINT32 op)
{
	return DRPPC_TERMINATOR;
}

INT D_Bclrx(UINT32 op)
{
	return DRPPC_TERMINATOR;
}

INT D_Cmp(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Cmpi(UINT32 op)
{
	// 0xFFF00150

	return DRPPC_OKAY;
}

INT D_Cmpl(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Cmpli(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Cntlzwx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Crand(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Crandc(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Creqv(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Crnand(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Crnor(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Cror(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Crorc(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Crxor(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Dcba(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Dcbf(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Dcbi(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Dcbst(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Dcbt(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Dcbtst(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Dcbz(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Divwx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Divwux(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Eieio(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Eqvx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Extsbx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Extshx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Icbi(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Isync(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Lbz(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Lbzu(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Lbzux(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Lbzx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Lha(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Lhau(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Lhaux(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Lhax(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Lhbrx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Lhz(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Lhzu(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Lhzux(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Lhzx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Lmw(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Lswi(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Lswx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Lwarx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Lwbrx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Lwz(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Lwzu(UINT32 op)
{
	// 0xFFF0014C

	return DRPPC_OKAY;
}

INT D_Lwzux(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Lwzx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Mcrf(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Mcrxr(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Mfcr(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Mtmsr(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Mtspr(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Mfmsr(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Mfspr(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Mtcrf(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Mulhwx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Mulhwux(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Mulli(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Mullwx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Nandx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Negx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Norx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Orx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Orcx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Ori(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Oris(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Rfi(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Rlwimix(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Rlwinmx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Rlwnmx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Sc(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Slwx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Srawx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Srawix(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Srwx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Stb(UINT32 op)
{
	// 0xFFF00118

	return DRPPC_OKAY;
}

INT D_Stbu(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Stbux(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Stbx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Sth(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Sthbrx(UINT32 op)
{
	// 0xFFF00108

	return DRPPC_OKAY;
}

INT D_Sthu(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Sthux(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Sthx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Stmw(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Stswi(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Stswx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Stw(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Stwbrx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Stwcx_(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Stwu(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Stwux(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Stwx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Subfx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Subfcx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Subfex(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Subfic(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Subfmex(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Subfzex(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Sync(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Tw(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Twi(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Xorx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Xori(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Xoris(UINT32 op)
{
	return DRPPC_ERROR;
}

/*
 * PowerPC 4xx
 */

INT D_Dccci(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Dcread(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Icbt(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Iccci(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Icread(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Mfdcr(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Mtdcr(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Rfci(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Wrtee(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Wrteei(UINT32 op)
{
	return DRPPC_ERROR;
}

/*
 * PowerPC 6xx
 */

INT D_Eciwx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Ecowx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Fabsx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Faddx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Faddsx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Fcmpo(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Fcmpu(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Fctiwx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Fctiwzx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Fdivx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Fdivsx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Fmaddx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Fmaddsx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Fmrx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Fmsubx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Fmsubsx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Fmulx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Fmulsx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Fnabsx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Fnegx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Fnmaddx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Fnmaddsx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Fnmsubx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Fnmsubsx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Fresx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Frspx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Frsqrtex(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Fselx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Fsqrtx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Fsqrtsx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Fsubx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Fsubsx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Lfd(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Lfdu(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Lfdux(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Lfdx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Lfs(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Lfsu(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Lfsux(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Lfsx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Mcrfs(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Mffsx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Mfsr(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Mfsrin(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Mftb(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Mtfsb0x(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Mtfsb1x(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Mtfsfx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Mtfsfix(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Mtsr(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Mtsrin(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Stfd(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Stfdu(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Stfdux(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Stfdx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Stfiwx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Stfs(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Stfsu(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Stfsux(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Stfsx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Tlbia(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Tlbie(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Tlbld(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Tlbli(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Tlbsync(UINT32 op)
{
	return DRPPC_ERROR;
}

/*
 * PowerPC Gekko
 */

INT D_Psq_l(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Psq_lu(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Psq_st(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Psq_stu(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Ps_absx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Ps_addx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Ps_cmpu0(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Ps_cmpo0(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Ps_cmpu1(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Ps_cmpo1(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Ps_divx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Ps_maddx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Ps_madds0x(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Ps_madds1x(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Ps_merge00x(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Ps_merge01x(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Ps_merge10x(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Ps_merge11x(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Ps_mrx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Ps_msubx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Ps_mulx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Ps_muls0x(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Ps_muls1x(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Ps_nabsx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Ps_negx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Ps_nmaddx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Ps_nmsubx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Ps_resx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Ps_rsqrtex(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Ps_selx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Ps_subx(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Ps_sum0x(UINT32 op)
{
	return DRPPC_ERROR;
}

INT D_Ps_sum1x(UINT32 op)
{
	return DRPPC_ERROR;
}
