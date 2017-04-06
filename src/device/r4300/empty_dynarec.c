/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - empty_dynarec.c                                         *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
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

void dyna_jump()
{
}

void dyna_stop()
{
}

/* M64P pseudo instructions */

#ifdef COMPARE_CORE
void gendebug()
{
}
#endif

void genni()
{
}

void gennotcompiled()
{
}

void genlink_subblock()
{
}

void genfin_block()
{
}

/* Reserved */

void genreserved()
{
}

/* Load instructions */

void genlb()
{
}

void genlbu()
{
}

void genlh()
{
}

void genlhu()
{
}

void genll()
{
}

void genlw()
{
}

void genlwu()
{
}

void genlwl()
{
}

void genlwr()
{
}

void genld()
{
}

void genldl()
{
}

void genldr()
{
}

/* Store instructions */

void gensb()
{
}

void gensh()
{
}

void gensc()
{
}

void gensw()
{
}

void genswl()
{
}

void genswr()
{
}

void gensd()
{
}

void gensdl()
{
}

void gensdr()
{
}

/* Computational instructions */

void genadd()
{
}

void genaddu()
{
}

void genaddi()
{
}

void genaddiu()
{
}

void gendadd()
{
}

void gendaddu()
{
}

void gendaddi()
{
}

void gendaddiu()
{
}

void gensub()
{
}

void gensubu()
{
}

void gendsub()
{
}

void gendsubu()
{
}

void genslt()
{
}

void gensltu()
{
}

void genslti()
{
}

void gensltiu()
{
}

void genand()
{
}

void genandi()
{
}

void genor()
{
}

void genori()
{
}

void genxor()
{
}

void genxori()
{
}

void gennor()
{
}

void genlui()
{
}

/* Shift instructions */

void gennop()
{
}

void gensll()
{
}

void gensllv()
{
}

void gendsll()
{
}

void gendsllv()
{
}

void gendsll32()
{
}

void gensrl()
{
}

void gensrlv()
{
}

void gendsrl()
{
}

void gendsrlv()
{
}

void gendsrl32()
{
}

void gensra()
{
}

void gensrav()
{
}

void gendsra()
{
}

void gendsrav()
{
}

void gendsra32()
{
}

/* Multiply / Divide instructions */

void genmult()
{
}

void genmultu()
{
}

void gendmult()
{
}

void gendmultu()
{
}

void gendiv()
{
}

void gendivu()
{
}

void genddiv()
{
}

void genddivu()
{
}

void genmfhi()
{
}

void genmthi()
{
}

void genmflo()
{
}

void genmtlo()
{
}

/* Jump & Branch instructions */

void genj()
{
}

void genj_out()
{
}

void genj_idle()
{
}

void genjal()
{
}

void genjal_out()
{
}

void genjal_idle()
{
}

void genjr()
{
}

void genjalr()
{
}

void genbeq()
{
}

void genbeq_out()
{
}

void genbeq_idle()
{
}

void genbeql()
{
}

void genbeql_out()
{
}

void genbeql_idle()
{
}

void genbne()
{
}

void genbne_out()
{
}

void genbne_idle()
{
}

void genbnel()
{
}

void genbnel_out()
{
}

void genbnel_idle()
{
}

void genblez()
{
}

void genblez_out()
{
}

void genblez_idle()
{
}

void genblezl()
{
}

void genblezl_out()
{
}

void genblezl_idle()
{
}

void genbgtz()
{
}

void genbgtz_out()
{
}

void genbgtz_idle()
{
}

void genbgtzl()
{
}

void genbgtzl_out()
{
}

void genbgtzl_idle()
{
}

void genbltz()
{
}

void genbltz_out()
{
}

void genbltz_idle()
{
}

void genbltzal()
{
}

void genbltzal_out()
{
}

void genbltzal_idle()
{
}

void genbltzl()
{
}

void genbltzl_out()
{
}

void genbltzl_idle()
{
}

void genbltzall()
{
}

void genbltzall_out()
{
}

void genbltzall_idle()
{
}

void genbgez()
{
}

void genbgez_out()
{
}

void genbgez_idle()
{
}

void genbgezal()
{
}

void genbgezal_out()
{
}

void genbgezal_idle()
{
}

void genbgezl()
{
}

void genbgezl_out()
{
}

void genbgezl_idle()
{
}

void genbgezall()
{
}

void genbgezall_out()
{
}

void genbgezall_idle()
{
}

void genbc1f()
{
}

void genbc1f_out()
{
}

void genbc1f_idle()
{
}

void genbc1fl()
{
}

void genbc1fl_out()
{
}

void genbc1fl_idle()
{
}

void genbc1t()
{
}

void genbc1t_out()
{
}

void genbc1t_idle()
{
}

void genbc1tl()
{
}

void genbc1tl_out()
{
}

void genbc1tl_idle()
{
}

/* Special instructions */

void gencache()
{
}

void generet()
{
}

void gensync()
{
}

void gensyscall()
{
}

/* Exception instructions */

void genteq()
{
}

/* TLB instructions */

void gentlbp()
{
}

void gentlbr()
{
}

void gentlbwr()
{
}

void gentlbwi()
{
}

/* CP0 load/store instructions */

void genmfc0()
{
}

void genmtc0()
{
}

/* CP1 load/store instructions */

void genlwc1()
{
}

void genldc1()
{
}

void genswc1()
{
}

void gensdc1()
{
}

void genmfc1()
{
}

void gendmfc1()
{
}

void gencfc1()
{
}

void genmtc1()
{
}

void gendmtc1()
{
}

void genctc1()
{
}

/* CP1 computational instructions */

void genabs_s()
{
}

void genabs_d()
{
}

void genadd_s()
{
}

void genadd_d()
{
}

void gendiv_s()
{
}

void gendiv_d()
{
}

void genmov_s()
{
}

void genmov_d()
{
}

void genmul_s()
{
}

void genmul_d()
{
}

void genneg_s()
{
}

void genneg_d()
{
}

void gensqrt_s()
{
}

void gensqrt_d()
{
}

void gensub_s()
{
}

void gensub_d()
{
}

void gentrunc_w_s()
{
}

void gentrunc_w_d()
{
}

void gentrunc_l_s()
{
}

void gentrunc_l_d()
{
}

void genround_w_s()
{
}

void genround_w_d()
{
}

void genround_l_s()
{
}

void genround_l_d()
{
}

void genceil_w_s()
{
}

void genceil_w_d()
{
}

void genceil_l_s()
{
}

void genceil_l_d()
{
}

void genfloor_w_s()
{
}

void genfloor_w_d()
{
}

void genfloor_l_s()
{
}

void genfloor_l_d()
{
}

void gencvt_s_d()
{
}

void gencvt_s_w()
{
}

void gencvt_s_l()
{
}

void gencvt_d_s()
{
}

void gencvt_d_w()
{
}

void gencvt_d_l()
{
}

void gencvt_w_s()
{
}

void gencvt_w_d()
{
}

void gencvt_l_s()
{
}

void gencvt_l_d()
{
}

/* CP1 relational instructions */

void genc_f_s()
{
}

void genc_f_d()
{
}

void genc_un_s()
{
}

void genc_un_d()
{
}

void genc_eq_s()
{
}

void genc_eq_d()
{
}

void genc_ueq_s()
{
}

void genc_ueq_d()
{
}

void genc_olt_s()
{
}

void genc_olt_d()
{
}

void genc_ult_s()
{
}

void genc_ult_d()
{
}

void genc_ole_s()
{
}

void genc_ole_d()
{
}

void genc_ule_s()
{
}

void genc_ule_d()
{
}

void genc_sf_s()
{
}

void genc_sf_d()
{
}

void genc_ngle_s()
{
}

void genc_ngle_d()
{
}

void genc_seq_s()
{
}

void genc_seq_d()
{
}

void genc_ngl_s()
{
}

void genc_ngl_d()
{
}

void genc_lt_s()
{
}

void genc_lt_d()
{
}

void genc_nge_s()
{
}

void genc_nge_d()
{
}

void genc_le_s()
{
}

void genc_le_d()
{
}

void genc_ngt_s()
{
}

void genc_ngt_d()
{
}
