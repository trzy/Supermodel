/*
 * front/powerpc/interp.h
 *
 * Interface to the PowerPC interpreter.
 */

#ifndef INCLUDED_INTERP_H
#define INCLUDED_INTERP_H

#include "drppc.h"

extern INT I_Addx(UINT32);
extern INT I_Addcx(UINT32);
extern INT I_Addex(UINT32);
extern INT I_Addi(UINT32);
extern INT I_Addic(UINT32);
extern INT I_Addic_(UINT32);
extern INT I_Addis(UINT32);
extern INT I_Addmex(UINT32);
extern INT I_Addzex(UINT32);
extern INT I_Andx(UINT32);
extern INT I_Andcx(UINT32);
extern INT I_Andi_(UINT32);
extern INT I_Andis_(UINT32);
extern INT I_Bx(UINT32);
extern INT I_Bcx(UINT32);
extern INT I_Bcctrx(UINT32);
extern INT I_Bclrx(UINT32);
extern INT I_Cmp(UINT32);
extern INT I_Cmpi(UINT32);
extern INT I_Cmpl(UINT32);
extern INT I_Cmpli(UINT32);
extern INT I_Cntlzwx(UINT32);
extern INT I_Crand(UINT32);
extern INT I_Crandc(UINT32);
extern INT I_Creqv(UINT32);
extern INT I_Crnand(UINT32);
extern INT I_Crnor(UINT32);
extern INT I_Cror(UINT32);
extern INT I_Crorc(UINT32);
extern INT I_Crxor(UINT32);
extern INT I_Dcba(UINT32);
extern INT I_Dcbf(UINT32);
extern INT I_Dcbi(UINT32);
extern INT I_Dcbst(UINT32);
extern INT I_Dcbt(UINT32);
extern INT I_Dcbtst(UINT32);
extern INT I_Dcbz(UINT32);
extern INT I_Divwx(UINT32);
extern INT I_Divwux(UINT32);
extern INT I_Eieio(UINT32);
extern INT I_Eqvx(UINT32);
extern INT I_Extsbx(UINT32);
extern INT I_Extshx(UINT32);
extern INT I_Icbi(UINT32);
extern INT I_Isync(UINT32);
extern INT I_Lbz(UINT32);
extern INT I_Lbzu(UINT32);
extern INT I_Lbzux(UINT32);
extern INT I_Lbzx(UINT32);
extern INT I_Lha(UINT32);
extern INT I_Lhau(UINT32);
extern INT I_Lhaux(UINT32);
extern INT I_Lhax(UINT32);
extern INT I_Lhbrx(UINT32);
extern INT I_Lhz(UINT32);
extern INT I_Lhzu(UINT32);
extern INT I_Lhzux(UINT32);
extern INT I_Lhzx(UINT32);
extern INT I_Lmw(UINT32);
extern INT I_Lswi(UINT32);
extern INT I_Lswx(UINT32);
extern INT I_Lwarx(UINT32);
extern INT I_Lwbrx(UINT32);
extern INT I_Lwz(UINT32);
extern INT I_Lwzu(UINT32);
extern INT I_Lwzux(UINT32);
extern INT I_Lwzx(UINT32);
extern INT I_Mcrf(UINT32);
extern INT I_Mcrxr(UINT32);
extern INT I_Mfcr(UINT32);
extern INT I_Mtmsr(UINT32);
extern INT I_Mtspr(UINT32);
extern INT I_Mfmsr(UINT32);
extern INT I_Mfspr(UINT32);
extern INT I_Mtcrf(UINT32);
extern INT I_Mulhwx(UINT32);
extern INT I_Mulhwux(UINT32);
extern INT I_Mulli(UINT32);
extern INT I_Mullwx(UINT32);
extern INT I_Nandx(UINT32);
extern INT I_Negx(UINT32);
extern INT I_Norx(UINT32);
extern INT I_Orx(UINT32);
extern INT I_Orcx(UINT32);
extern INT I_Ori(UINT32);
extern INT I_Oris(UINT32);
extern INT I_Rfi(UINT32);
extern INT I_Rlwimix(UINT32);
extern INT I_Rlwinmx(UINT32);
extern INT I_Rlwnmx(UINT32);
extern INT I_Sc(UINT32);
extern INT I_Slwx(UINT32);
extern INT I_Srawx(UINT32);
extern INT I_Srawix(UINT32);
extern INT I_Srwx(UINT32);
extern INT I_Stb(UINT32);
extern INT I_Stbu(UINT32);
extern INT I_Stbux(UINT32);
extern INT I_Stbx(UINT32);
extern INT I_Sth(UINT32);
extern INT I_Sthbrx(UINT32);
extern INT I_Sthu(UINT32);
extern INT I_Sthux(UINT32);
extern INT I_Sthx(UINT32);
extern INT I_Stmw(UINT32);
extern INT I_Stswi(UINT32);
extern INT I_Stswx(UINT32);
extern INT I_Stw(UINT32);
extern INT I_Stwbrx(UINT32);
extern INT I_Stwcx_(UINT32);
extern INT I_Stwu(UINT32);
extern INT I_Stwux(UINT32);
extern INT I_Stwx(UINT32);
extern INT I_Subfx(UINT32);
extern INT I_Subfcx(UINT32);
extern INT I_Subfex(UINT32);
extern INT I_Subfic(UINT32);
extern INT I_Subfmex(UINT32);
extern INT I_Subfzex(UINT32);
extern INT I_Sync(UINT32);
extern INT I_Tw(UINT32);
extern INT I_Twi(UINT32);
extern INT I_Xorx(UINT32);
extern INT I_Xori(UINT32);
extern INT I_Xoris(UINT32);
extern INT I_Dccci(UINT32);
extern INT I_Dcread(UINT32);
extern INT I_Icbt(UINT32);
extern INT I_Iccci(UINT32);
extern INT I_Icread(UINT32);
extern INT I_Mfdcr(UINT32);
extern INT I_Mtdcr(UINT32);
extern INT I_Rfci(UINT32);
extern INT I_Wrtee(UINT32);
extern INT I_Wrteei(UINT32);
extern INT I_Eciwx(UINT32);
extern INT I_Ecowx(UINT32);
extern INT I_Fabsx(UINT32);
extern INT I_Faddx(UINT32);
extern INT I_Faddsx(UINT32);
extern INT I_Fcmpo(UINT32);
extern INT I_Fcmpu(UINT32);
extern INT I_Fctiwx(UINT32);
extern INT I_Fctiwzx(UINT32);
extern INT I_Fdivx(UINT32);
extern INT I_Fdivsx(UINT32);
extern INT I_Fmaddx(UINT32);
extern INT I_Fmaddsx(UINT32);
extern INT I_Fmrx(UINT32);
extern INT I_Fmsubx(UINT32);
extern INT I_Fmsubsx(UINT32);
extern INT I_Fmulx(UINT32);
extern INT I_Fmulsx(UINT32);
extern INT I_Fnabsx(UINT32);
extern INT I_Fnegx(UINT32);
extern INT I_Fnmaddx(UINT32);
extern INT I_Fnmaddsx(UINT32);
extern INT I_Fnmsubx(UINT32);
extern INT I_Fnmsubsx(UINT32);
extern INT I_Fresx(UINT32);
extern INT I_Frspx(UINT32);
extern INT I_Frsqrtex(UINT32);
extern INT I_Fselx(UINT32);
extern INT I_Fsqrtx(UINT32);
extern INT I_Fsqrtsx(UINT32);
extern INT I_Fsubx(UINT32);
extern INT I_Fsubsx(UINT32);
extern INT I_Lfd(UINT32);
extern INT I_Lfdu(UINT32);
extern INT I_Lfdux(UINT32);
extern INT I_Lfdx(UINT32);
extern INT I_Lfs(UINT32);
extern INT I_Lfsu(UINT32);
extern INT I_Lfsux(UINT32);
extern INT I_Lfsx(UINT32);
extern INT I_Mcrfs(UINT32);
extern INT I_Mffsx(UINT32);
extern INT I_Mfsr(UINT32);
extern INT I_Mfsrin(UINT32);
extern INT I_Mftb(UINT32);
extern INT I_Mtfsb0x(UINT32);
extern INT I_Mtfsb1x(UINT32);
extern INT I_Mtfsfx(UINT32);
extern INT I_Mtfsfix(UINT32);
extern INT I_Mtsr(UINT32);
extern INT I_Mtsrin(UINT32);
extern INT I_Stfd(UINT32);
extern INT I_Stfdu(UINT32);
extern INT I_Stfdux(UINT32);
extern INT I_Stfdx(UINT32);
extern INT I_Stfiwx(UINT32);
extern INT I_Stfs(UINT32);
extern INT I_Stfsu(UINT32);
extern INT I_Stfsux(UINT32);
extern INT I_Stfsx(UINT32);
extern INT I_Tlbia(UINT32);
extern INT I_Tlbie(UINT32);
extern INT I_Tlbld(UINT32);
extern INT I_Tlbli(UINT32);
extern INT I_Tlbsync(UINT32);
extern INT I_Psq_l(UINT32);
extern INT I_Psq_lu(UINT32);
extern INT I_Psq_st(UINT32);
extern INT I_Psq_stu(UINT32);
extern INT I_Ps_absx(UINT32);
extern INT I_Ps_addx(UINT32);
extern INT I_Ps_cmpu0(UINT32);
extern INT I_Ps_cmpo0(UINT32);
extern INT I_Ps_cmpu1(UINT32);
extern INT I_Ps_cmpo1(UINT32);
extern INT I_Ps_divx(UINT32);
extern INT I_Ps_maddx(UINT32);
extern INT I_Ps_madds0x(UINT32);
extern INT I_Ps_madds1x(UINT32);
extern INT I_Ps_merge00x(UINT32);
extern INT I_Ps_merge01x(UINT32);
extern INT I_Ps_merge10x(UINT32);
extern INT I_Ps_merge11x(UINT32);
extern INT I_Ps_mrx(UINT32);
extern INT I_Ps_msubx(UINT32);
extern INT I_Ps_mulx(UINT32);
extern INT I_Ps_muls0x(UINT32);
extern INT I_Ps_muls1x(UINT32);
extern INT I_Ps_nabsx(UINT32);
extern INT I_Ps_negx(UINT32);
extern INT I_Ps_nmaddx(UINT32);
extern INT I_Ps_nmsubx(UINT32);
extern INT I_Ps_resx(UINT32);
extern INT I_Ps_rsqrtex(UINT32);
extern INT I_Ps_selx(UINT32);
extern INT I_Ps_subx(UINT32);
extern INT I_Ps_sum0x(UINT32);
extern INT I_Ps_sum1x(UINT32);

#endif // INCLUDED_INTERP_H
