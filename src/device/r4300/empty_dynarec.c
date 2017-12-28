/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - empty_dynarec.c                                         *
 *   Mupen64Plus homepage: https://mupen64plus.org/                        *
 *   Copyright (C) 2002 Richard42, Nmn                                     *
 *   Copyright (C) 2002 Hacktarux                                          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "recomp.h"

/* From assemble.c */

void init_assembler(void *block_jumps_table, int block_jumps_number, void *block_riprel_table, int block_riprel_number)
{
}

void free_assembler(void **block_jumps_table, int *block_jumps_number, void **block_riprel_table, int *block_riprel_number)
{
}

void passe2(struct precomp_instr *dest, int start, int end, struct precomp_block *block)
{
}

/* From regcache.c */

void init_cache(struct precomp_instr* start)
{
}

void free_all_registers()
{
}

/* Dynarec control functions */

void dyna_jump(void)
{
}

void dyna_stop(struct r4300_core* r4300)
{
}

/* M64P pseudo instructions */

#ifdef COMPARE_CORE
void gendebug(struct r4300_core* r4300)
{
}
#endif

void genni(struct r4300_core* r4300)
{
}

void gennotcompiled(struct r4300_core* r4300)
{
}

void genlink_subblock(struct r4300_core* r4300)
{
}

void genfin_block(struct r4300_core* r4300)
{
}

/* Reserved */

void genreserved(struct r4300_core* r4300)
{
}

/* Load instructions */

void genlb(struct r4300_core* r4300)
{
}

void genlbu(struct r4300_core* r4300)
{
}

void genlh(struct r4300_core* r4300)
{
}

void genlhu(struct r4300_core* r4300)
{
}

void genll(struct r4300_core* r4300)
{
}

void genlw(struct r4300_core* r4300)
{
}

void genlwu(struct r4300_core* r4300)
{
}

void genlwl(struct r4300_core* r4300)
{
}

void genlwr(struct r4300_core* r4300)
{
}

void genld(struct r4300_core* r4300)
{
}

void genldl(struct r4300_core* r4300)
{
}

void genldr(struct r4300_core* r4300)
{
}

/* Store instructions */

void gensb(struct r4300_core* r4300)
{
}

void gensh(struct r4300_core* r4300)
{
}

void gensc(struct r4300_core* r4300)
{
}

void gensw(struct r4300_core* r4300)
{
}

void genswl(struct r4300_core* r4300)
{
}

void genswr(struct r4300_core* r4300)
{
}

void gensd(struct r4300_core* r4300)
{
}

void gensdl(struct r4300_core* r4300)
{
}

void gensdr(struct r4300_core* r4300)
{
}

/* Computational instructions */

void genadd(struct r4300_core* r4300)
{
}

void genaddu(struct r4300_core* r4300)
{
}

void genaddi(struct r4300_core* r4300)
{
}

void genaddiu(struct r4300_core* r4300)
{
}

void gendadd(struct r4300_core* r4300)
{
}

void gendaddu(struct r4300_core* r4300)
{
}

void gendaddi(struct r4300_core* r4300)
{
}

void gendaddiu(struct r4300_core* r4300)
{
}

void gensub(struct r4300_core* r4300)
{
}

void gensubu(struct r4300_core* r4300)
{
}

void gendsub(struct r4300_core* r4300)
{
}

void gendsubu(struct r4300_core* r4300)
{
}

void genslt(struct r4300_core* r4300)
{
}

void gensltu(struct r4300_core* r4300)
{
}

void genslti(struct r4300_core* r4300)
{
}

void gensltiu(struct r4300_core* r4300)
{
}

void genand(struct r4300_core* r4300)
{
}

void genandi(struct r4300_core* r4300)
{
}

void genor(struct r4300_core* r4300)
{
}

void genori(struct r4300_core* r4300)
{
}

void genxor(struct r4300_core* r4300)
{
}

void genxori(struct r4300_core* r4300)
{
}

void gennor(struct r4300_core* r4300)
{
}

void genlui(struct r4300_core* r4300)
{
}

/* Shift instructions */

void gennop(struct r4300_core* r4300)
{
}

void gensll(struct r4300_core* r4300)
{
}

void gensllv(struct r4300_core* r4300)
{
}

void gendsll(struct r4300_core* r4300)
{
}

void gendsllv(struct r4300_core* r4300)
{
}

void gendsll32(struct r4300_core* r4300)
{
}

void gensrl(struct r4300_core* r4300)
{
}

void gensrlv(struct r4300_core* r4300)
{
}

void gendsrl(struct r4300_core* r4300)
{
}

void gendsrlv(struct r4300_core* r4300)
{
}

void gendsrl32(struct r4300_core* r4300)
{
}

void gensra(struct r4300_core* r4300)
{
}

void gensrav(struct r4300_core* r4300)
{
}

void gendsra(struct r4300_core* r4300)
{
}

void gendsrav(struct r4300_core* r4300)
{
}

void gendsra32(struct r4300_core* r4300)
{
}

/* Multiply / Divide instructions */

void genmult(struct r4300_core* r4300)
{
}

void genmultu(struct r4300_core* r4300)
{
}

void gendmult(struct r4300_core* r4300)
{
}

void gendmultu(struct r4300_core* r4300)
{
}

void gendiv(struct r4300_core* r4300)
{
}

void gendivu(struct r4300_core* r4300)
{
}

void genddiv(struct r4300_core* r4300)
{
}

void genddivu(struct r4300_core* r4300)
{
}

void genmfhi(struct r4300_core* r4300)
{
}

void genmthi(struct r4300_core* r4300)
{
}

void genmflo(struct r4300_core* r4300)
{
}

void genmtlo(struct r4300_core* r4300)
{
}

/* Jump & Branch instructions */

void genj(struct r4300_core* r4300)
{
}

void genj_out(struct r4300_core* r4300)
{
}

void genj_idle(struct r4300_core* r4300)
{
}

void genjal(struct r4300_core* r4300)
{
}

void genjal_out(struct r4300_core* r4300)
{
}

void genjal_idle(struct r4300_core* r4300)
{
}

void genjr(struct r4300_core* r4300)
{
}

void genjalr(struct r4300_core* r4300)
{
}

void genbeq(struct r4300_core* r4300)
{
}

void genbeq_out(struct r4300_core* r4300)
{
}

void genbeq_idle(struct r4300_core* r4300)
{
}

void genbeql(struct r4300_core* r4300)
{
}

void genbeql_out(struct r4300_core* r4300)
{
}

void genbeql_idle(struct r4300_core* r4300)
{
}

void genbne(struct r4300_core* r4300)
{
}

void genbne_out(struct r4300_core* r4300)
{
}

void genbne_idle(struct r4300_core* r4300)
{
}

void genbnel(struct r4300_core* r4300)
{
}

void genbnel_out(struct r4300_core* r4300)
{
}

void genbnel_idle(struct r4300_core* r4300)
{
}

void genblez(struct r4300_core* r4300)
{
}

void genblez_out(struct r4300_core* r4300)
{
}

void genblez_idle(struct r4300_core* r4300)
{
}

void genblezl(struct r4300_core* r4300)
{
}

void genblezl_out(struct r4300_core* r4300)
{
}

void genblezl_idle(struct r4300_core* r4300)
{
}

void genbgtz(struct r4300_core* r4300)
{
}

void genbgtz_out(struct r4300_core* r4300)
{
}

void genbgtz_idle(struct r4300_core* r4300)
{
}

void genbgtzl(struct r4300_core* r4300)
{
}

void genbgtzl_out(struct r4300_core* r4300)
{
}

void genbgtzl_idle(struct r4300_core* r4300)
{
}

void genbltz(struct r4300_core* r4300)
{
}

void genbltz_out(struct r4300_core* r4300)
{
}

void genbltz_idle(struct r4300_core* r4300)
{
}

void genbltzal(struct r4300_core* r4300)
{
}

void genbltzal_out(struct r4300_core* r4300)
{
}

void genbltzal_idle(struct r4300_core* r4300)
{
}

void genbltzl(struct r4300_core* r4300)
{
}

void genbltzl_out(struct r4300_core* r4300)
{
}

void genbltzl_idle(struct r4300_core* r4300)
{
}

void genbltzall(struct r4300_core* r4300)
{
}

void genbltzall_out(struct r4300_core* r4300)
{
}

void genbltzall_idle(struct r4300_core* r4300)
{
}

void genbgez(struct r4300_core* r4300)
{
}

void genbgez_out(struct r4300_core* r4300)
{
}

void genbgez_idle(struct r4300_core* r4300)
{
}

void genbgezal(struct r4300_core* r4300)
{
}

void genbgezal_out(struct r4300_core* r4300)
{
}

void genbgezal_idle(struct r4300_core* r4300)
{
}

void genbgezl(struct r4300_core* r4300)
{
}

void genbgezl_out(struct r4300_core* r4300)
{
}

void genbgezl_idle(struct r4300_core* r4300)
{
}

void genbgezall(struct r4300_core* r4300)
{
}

void genbgezall_out(struct r4300_core* r4300)
{
}

void genbgezall_idle(struct r4300_core* r4300)
{
}

void genbc1f(struct r4300_core* r4300)
{
}

void genbc1f_out(struct r4300_core* r4300)
{
}

void genbc1f_idle(struct r4300_core* r4300)
{
}

void genbc1fl(struct r4300_core* r4300)
{
}

void genbc1fl_out(struct r4300_core* r4300)
{
}

void genbc1fl_idle(struct r4300_core* r4300)
{
}

void genbc1t(struct r4300_core* r4300)
{
}

void genbc1t_out(struct r4300_core* r4300)
{
}

void genbc1t_idle(struct r4300_core* r4300)
{
}

void genbc1tl(struct r4300_core* r4300)
{
}

void genbc1tl_out(struct r4300_core* r4300)
{
}

void genbc1tl_idle(struct r4300_core* r4300)
{
}

/* Special instructions */

void gencache(struct r4300_core* r4300)
{
}

void generet(struct r4300_core* r4300)
{
}

void gensync(struct r4300_core* r4300)
{
}

void gensyscall(struct r4300_core* r4300)
{
}

/* Exception instructions */

void genteq(struct r4300_core* r4300)
{
}

/* TLB instructions */

void gentlbp(struct r4300_core* r4300)
{
}

void gentlbr(struct r4300_core* r4300)
{
}

void gentlbwr(struct r4300_core* r4300)
{
}

void gentlbwi(struct r4300_core* r4300)
{
}

/* CP0 load/store instructions */

void genmfc0(struct r4300_core* r4300)
{
}

void genmtc0(struct r4300_core* r4300)
{
}

/* CP1 load/store instructions */

void genlwc1(struct r4300_core* r4300)
{
}

void genldc1(struct r4300_core* r4300)
{
}

void genswc1(struct r4300_core* r4300)
{
}

void gensdc1(struct r4300_core* r4300)
{
}

void genmfc1(struct r4300_core* r4300)
{
}

void gendmfc1(struct r4300_core* r4300)
{
}

void gencfc1(struct r4300_core* r4300)
{
}

void genmtc1(struct r4300_core* r4300)
{
}

void gendmtc1(struct r4300_core* r4300)
{
}

void genctc1(struct r4300_core* r4300)
{
}

/* CP1 computational instructions */

void genabs_s(struct r4300_core* r4300)
{
}

void genabs_d(struct r4300_core* r4300)
{
}

void genadd_s(struct r4300_core* r4300)
{
}

void genadd_d(struct r4300_core* r4300)
{
}

void gendiv_s(struct r4300_core* r4300)
{
}

void gendiv_d(struct r4300_core* r4300)
{
}

void genmov_s(struct r4300_core* r4300)
{
}

void genmov_d(struct r4300_core* r4300)
{
}

void genmul_s(struct r4300_core* r4300)
{
}

void genmul_d(struct r4300_core* r4300)
{
}

void genneg_s(struct r4300_core* r4300)
{
}

void genneg_d(struct r4300_core* r4300)
{
}

void gensqrt_s(struct r4300_core* r4300)
{
}

void gensqrt_d(struct r4300_core* r4300)
{
}

void gensub_s(struct r4300_core* r4300)
{
}

void gensub_d(struct r4300_core* r4300)
{
}

void gentrunc_w_s(struct r4300_core* r4300)
{
}

void gentrunc_w_d(struct r4300_core* r4300)
{
}

void gentrunc_l_s(struct r4300_core* r4300)
{
}

void gentrunc_l_d(struct r4300_core* r4300)
{
}

void genround_w_s(struct r4300_core* r4300)
{
}

void genround_w_d(struct r4300_core* r4300)
{
}

void genround_l_s(struct r4300_core* r4300)
{
}

void genround_l_d(struct r4300_core* r4300)
{
}

void genceil_w_s(struct r4300_core* r4300)
{
}

void genceil_w_d(struct r4300_core* r4300)
{
}

void genceil_l_s(struct r4300_core* r4300)
{
}

void genceil_l_d(struct r4300_core* r4300)
{
}

void genfloor_w_s(struct r4300_core* r4300)
{
}

void genfloor_w_d(struct r4300_core* r4300)
{
}

void genfloor_l_s(struct r4300_core* r4300)
{
}

void genfloor_l_d(struct r4300_core* r4300)
{
}

void gencvt_s_d(struct r4300_core* r4300)
{
}

void gencvt_s_w(struct r4300_core* r4300)
{
}

void gencvt_s_l(struct r4300_core* r4300)
{
}

void gencvt_d_s(struct r4300_core* r4300)
{
}

void gencvt_d_w(struct r4300_core* r4300)
{
}

void gencvt_d_l(struct r4300_core* r4300)
{
}

void gencvt_w_s(struct r4300_core* r4300)
{
}

void gencvt_w_d(struct r4300_core* r4300)
{
}

void gencvt_l_s(struct r4300_core* r4300)
{
}

void gencvt_l_d(struct r4300_core* r4300)
{
}

/* CP1 relational instructions */

void genc_f_s(struct r4300_core* r4300)
{
}

void genc_f_d(struct r4300_core* r4300)
{
}

void genc_un_s(struct r4300_core* r4300)
{
}

void genc_un_d(struct r4300_core* r4300)
{
}

void genc_eq_s(struct r4300_core* r4300)
{
}

void genc_eq_d(struct r4300_core* r4300)
{
}

void genc_ueq_s(struct r4300_core* r4300)
{
}

void genc_ueq_d(struct r4300_core* r4300)
{
}

void genc_olt_s(struct r4300_core* r4300)
{
}

void genc_olt_d(struct r4300_core* r4300)
{
}

void genc_ult_s(struct r4300_core* r4300)
{
}

void genc_ult_d(struct r4300_core* r4300)
{
}

void genc_ole_s(struct r4300_core* r4300)
{
}

void genc_ole_d(struct r4300_core* r4300)
{
}

void genc_ule_s(struct r4300_core* r4300)
{
}

void genc_ule_d(struct r4300_core* r4300)
{
}

void genc_sf_s(struct r4300_core* r4300)
{
}

void genc_sf_d(struct r4300_core* r4300)
{
}

void genc_ngle_s(struct r4300_core* r4300)
{
}

void genc_ngle_d(struct r4300_core* r4300)
{
}

void genc_seq_s(struct r4300_core* r4300)
{
}

void genc_seq_d(struct r4300_core* r4300)
{
}

void genc_ngl_s(struct r4300_core* r4300)
{
}

void genc_ngl_d(struct r4300_core* r4300)
{
}

void genc_lt_s(struct r4300_core* r4300)
{
}

void genc_lt_d(struct r4300_core* r4300)
{
}

void genc_nge_s(struct r4300_core* r4300)
{
}

void genc_nge_d(struct r4300_core* r4300)
{
}

void genc_le_s(struct r4300_core* r4300)
{
}

void genc_le_d(struct r4300_core* r4300)
{
}

void genc_ngt_s(struct r4300_core* r4300)
{
}

void genc_ngt_d(struct r4300_core* r4300)
{
}
