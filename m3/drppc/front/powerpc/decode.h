/*
 * front/powerpc/decode.h
 *
 * PowerPC decoder interface.
 */

#ifndef INCLUDED_DECODE_H
#define INCLUDED_DECODE_H

#include "drppc.h"

extern INT D_Addx(UINT32);
extern INT D_Addcx(UINT32);
extern INT D_Addex(UINT32);
extern INT D_Addi(UINT32);
extern INT D_Addic(UINT32);
extern INT D_Addic_(UINT32);
extern INT D_Addis(UINT32);
extern INT D_Addmex(UINT32);
extern INT D_Addzex(UINT32);
extern INT D_Andx(UINT32);
extern INT D_Andcx(UINT32);
extern INT D_Andi_(UINT32);
extern INT D_Andis_(UINT32);
extern INT D_Bx(UINT32);
extern INT D_Bcx(UINT32);
extern INT D_Bcctrx(UINT32);
extern INT D_Bclrx(UINT32);
extern INT D_Cmp(UINT32);
extern INT D_Cmpi(UINT32);
extern INT D_Cmpl(UINT32);
extern INT D_Cmpli(UINT32);
extern INT D_Cntlzwx(UINT32);
extern INT D_Crand(UINT32);
extern INT D_Crandc(UINT32);
extern INT D_Creqv(UINT32);
extern INT D_Crnand(UINT32);
extern INT D_Crnor(UINT32);
extern INT D_Cror(UINT32);
extern INT D_Crorc(UINT32);
extern INT D_Crxor(UINT32);
extern INT D_Dcba(UINT32);
extern INT D_Dcbf(UINT32);
extern INT D_Dcbi(UINT32);
extern INT D_Dcbst(UINT32);
extern INT D_Dcbt(UINT32);
extern INT D_Dcbtst(UINT32);
extern INT D_Dcbz(UINT32);
extern INT D_Divwx(UINT32);
extern INT D_Divwux(UINT32);
extern INT D_Eieio(UINT32);
extern INT D_Eqvx(UINT32);
extern INT D_Extsbx(UINT32);
extern INT D_Extshx(UINT32);
extern INT D_Icbi(UINT32);
extern INT D_Isync(UINT32);
extern INT D_Lbz(UINT32);
extern INT D_Lbzu(UINT32);
extern INT D_Lbzux(UINT32);
extern INT D_Lbzx(UINT32);
extern INT D_Lha(UINT32);
extern INT D_Lhau(UINT32);
extern INT D_Lhaux(UINT32);
extern INT D_Lhax(UINT32);
extern INT D_Lhbrx(UINT32);
extern INT D_Lhz(UINT32);
extern INT D_Lhzu(UINT32);
extern INT D_Lhzux(UINT32);
extern INT D_Lhzx(UINT32);
extern INT D_Lmw(UINT32);
extern INT D_Lswi(UINT32);
extern INT D_Lswx(UINT32);
extern INT D_Lwarx(UINT32);
extern INT D_Lwbrx(UINT32);
extern INT D_Lwz(UINT32);
extern INT D_Lwzu(UINT32);
extern INT D_Lwzux(UINT32);
extern INT D_Lwzx(UINT32);
extern INT D_Mcrf(UINT32);
extern INT D_Mcrxr(UINT32);
extern INT D_Mfcr(UINT32);
extern INT D_Mtmsr(UINT32);
extern INT D_Mtspr(UINT32);
extern INT D_Mfmsr(UINT32);
extern INT D_Mfspr(UINT32);
extern INT D_Mtcrf(UINT32);
extern INT D_Mulhwx(UINT32);
extern INT D_Mulhwux(UINT32);
extern INT D_Mulli(UINT32);
extern INT D_Mullwx(UINT32);
extern INT D_Nandx(UINT32);
extern INT D_Negx(UINT32);
extern INT D_Norx(UINT32);
extern INT D_Orx(UINT32);
extern INT D_Orcx(UINT32);
extern INT D_Ori(UINT32);
extern INT D_Oris(UINT32);
extern INT D_Rfi(UINT32);
extern INT D_Rlwimix(UINT32);
extern INT D_Rlwinmx(UINT32);
extern INT D_Rlwnmx(UINT32);
extern INT D_Sc(UINT32);
extern INT D_Slwx(UINT32);
extern INT D_Srawx(UINT32);
extern INT D_Srawix(UINT32);
extern INT D_Srwx(UINT32);
extern INT D_Stb(UINT32);
extern INT D_Stbu(UINT32);
extern INT D_Stbux(UINT32);
extern INT D_Stbx(UINT32);
extern INT D_Sth(UINT32);
extern INT D_Sthbrx(UINT32);
extern INT D_Sthu(UINT32);
extern INT D_Sthux(UINT32);
extern INT D_Sthx(UINT32);
extern INT D_Stmw(UINT32);
extern INT D_Stswi(UINT32);
extern INT D_Stswx(UINT32);
extern INT D_Stw(UINT32);
extern INT D_Stwbrx(UINT32);
extern INT D_Stwcx_(UINT32);
extern INT D_Stwu(UINT32);
extern INT D_Stwux(UINT32);
extern INT D_Stwx(UINT32);
extern INT D_Subfx(UINT32);
extern INT D_Subfcx(UINT32);
extern INT D_Subfex(UINT32);
extern INT D_Subfic(UINT32);
extern INT D_Subfmex(UINT32);
extern INT D_Subfzex(UINT32);
extern INT D_Sync(UINT32);
extern INT D_Tw(UINT32);
extern INT D_Twi(UINT32);
extern INT D_Xorx(UINT32);
extern INT D_Xori(UINT32);
extern INT D_Xoris(UINT32);
extern INT D_Dccci(UINT32);
extern INT D_Dcread(UINT32);
extern INT D_Icbt(UINT32);
extern INT D_Iccci(UINT32);
extern INT D_Icread(UINT32);
extern INT D_Mfdcr(UINT32);
extern INT D_Mtdcr(UINT32);
extern INT D_Rfci(UINT32);
extern INT D_Wrtee(UINT32);
extern INT D_Wrteei(UINT32);
extern INT D_Eciwx(UINT32);
extern INT D_Ecowx(UINT32);
extern INT D_Fabsx(UINT32);
extern INT D_Faddx(UINT32);
extern INT D_Faddsx(UINT32);
extern INT D_Fcmpo(UINT32);
extern INT D_Fcmpu(UINT32);
extern INT D_Fctiwx(UINT32);
extern INT D_Fctiwzx(UINT32);
extern INT D_Fdivx(UINT32);
extern INT D_Fdivsx(UINT32);
extern INT D_Fmaddx(UINT32);
extern INT D_Fmaddsx(UINT32);
extern INT D_Fmrx(UINT32);
extern INT D_Fmsubx(UINT32);
extern INT D_Fmsubsx(UINT32);
extern INT D_Fmulx(UINT32);
extern INT D_Fmulsx(UINT32);
extern INT D_Fnabsx(UINT32);
extern INT D_Fnegx(UINT32);
extern INT D_Fnmaddx(UINT32);
extern INT D_Fnmaddsx(UINT32);
extern INT D_Fnmsubx(UINT32);
extern INT D_Fnmsubsx(UINT32);
extern INT D_Fresx(UINT32);
extern INT D_Frspx(UINT32);
extern INT D_Frsqrtex(UINT32);
extern INT D_Fselx(UINT32);
extern INT D_Fsqrtx(UINT32);
extern INT D_Fsqrtsx(UINT32);
extern INT D_Fsubx(UINT32);
extern INT D_Fsubsx(UINT32);
extern INT D_Lfd(UINT32);
extern INT D_Lfdu(UINT32);
extern INT D_Lfdux(UINT32);
extern INT D_Lfdx(UINT32);
extern INT D_Lfs(UINT32);
extern INT D_Lfsu(UINT32);
extern INT D_Lfsux(UINT32);
extern INT D_Lfsx(UINT32);
extern INT D_Mcrfs(UINT32);
extern INT D_Mffsx(UINT32);
extern INT D_Mfsr(UINT32);
extern INT D_Mfsrin(UINT32);
extern INT D_Mftb(UINT32);
extern INT D_Mtfsb0x(UINT32);
extern INT D_Mtfsb1x(UINT32);
extern INT D_Mtfsfx(UINT32);
extern INT D_Mtfsfix(UINT32);
extern INT D_Mtsr(UINT32);
extern INT D_Mtsrin(UINT32);
extern INT D_Stfd(UINT32);
extern INT D_Stfdu(UINT32);
extern INT D_Stfdux(UINT32);
extern INT D_Stfdx(UINT32);
extern INT D_Stfiwx(UINT32);
extern INT D_Stfs(UINT32);
extern INT D_Stfsu(UINT32);
extern INT D_Stfsux(UINT32);
extern INT D_Stfsx(UINT32);
extern INT D_Tlbia(UINT32);
extern INT D_Tlbie(UINT32);
extern INT D_Tlbld(UINT32);
extern INT D_Tlbli(UINT32);
extern INT D_Tlbsync(UINT32);
extern INT D_Psq_l(UINT32);
extern INT D_Psq_lu(UINT32);
extern INT D_Psq_st(UINT32);
extern INT D_Psq_stu(UINT32);
extern INT D_Ps_absx(UINT32);
extern INT D_Ps_addx(UINT32);
extern INT D_Ps_cmpu0(UINT32);
extern INT D_Ps_cmpo0(UINT32);
extern INT D_Ps_cmpu1(UINT32);
extern INT D_Ps_cmpo1(UINT32);
extern INT D_Ps_divx(UINT32);
extern INT D_Ps_maddx(UINT32);
extern INT D_Ps_madds0x(UINT32);
extern INT D_Ps_madds1x(UINT32);
extern INT D_Ps_merge00x(UINT32);
extern INT D_Ps_merge01x(UINT32);
extern INT D_Ps_merge10x(UINT32);
extern INT D_Ps_merge11x(UINT32);
extern INT D_Ps_mrx(UINT32);
extern INT D_Ps_msubx(UINT32);
extern INT D_Ps_mulx(UINT32);
extern INT D_Ps_muls0x(UINT32);
extern INT D_Ps_muls1x(UINT32);
extern INT D_Ps_nabsx(UINT32);
extern INT D_Ps_negx(UINT32);
extern INT D_Ps_nmaddx(UINT32);
extern INT D_Ps_nmsubx(UINT32);
extern INT D_Ps_resx(UINT32);
extern INT D_Ps_rsqrtex(UINT32);
extern INT D_Ps_selx(UINT32);
extern INT D_Ps_subx(UINT32);
extern INT D_Ps_sum0x(UINT32);
extern INT D_Ps_sum1x(UINT32);

#endif // INCLUDED_DECODE_H
