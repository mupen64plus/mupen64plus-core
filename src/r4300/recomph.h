/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - recomph.h                                               *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
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

#ifndef RECOMPH_H
#define RECOMPH_H

#include <stdio.h>

#include "recomp.h"

#if defined(COUNT_INSTR)
extern unsigned int instr_count[132];
extern unsigned int instr_type[131];
extern char instr_name[][10];
extern char instr_typename[][20];
#endif

extern int code_length;
extern int max_code_length;
extern unsigned char **inst_pointer;
extern precomp_block* dst_block;
extern int jump_marker;
extern unsigned long *return_address;
extern int fast_memory;
extern int src;   /* opcode of r4300 instruction being recompiled */

#if defined(PROFILE_R4300)
extern FILE *pfProfile;
#endif

void passe2(precomp_instr *dest, int start, int end, precomp_block* block);
void init_assembler(void *block_jumps_table, int block_jumps_number, void *block_riprel_table, int block_riprel_number);
void free_assembler(void **block_jumps_table, int *block_jumps_number, void **block_riprel_table, int *block_riprel_number);
void stop_it();

void gencallinterp(unsigned long addr, int jump);

void genupdate_system(int type);
void genbnel();
void genblezl();
void genlw();
void genlbu();
void genlhu();
void gensb();
void gensh();
void gensw();
void gencache();
void genlwc1();
void genld();
void gensd();
void genbeq();
void genbne();
void genblez();
void genaddi();
void genaddiu();
void genslti();
void gensltiu();
void genandi();
void genori();
void genxori();
void genlui();
void genbeql();
void genmul_s();
void gendiv_s();
void gencvt_d_s();
void genadd_d();
void gentrunc_w_d();
void gencvt_s_w();
void genmfc1();
void gencfc1();
void genmtc1();
void genctc1();
void genj();
void genjal();
void genslt();
void gensltu();
void gendsll32();
void gendsra32();
void genbgez();
void genbgezl();
void genbgezal();
void gentlbwi();
void generet();
void genmfc0();
void genadd_s();
void genmult();
void genmultu();
void genmflo();
void genmtlo();
void gendiv();
void gendmultu();
void genddivu();
void genadd();
void genaddu();
void gensubu();
void genand();
void genor();
void genxor();
void genreserved();
void gennop();
void gensll();
void gensrl();
void gensra();
void gensllv();
void gensrlv();
void genjr();
void genni();
void genmfhi();
void genmthi();
void genmtc0();
void genbltz();
void genlwl();
void genswl();
void gentlbp();
void gentlbr();
void genswr();
void genlwr();
void gensrav();
void genbgtz();
void genlb();
void genswc1();
void genldc1();
void gencvt_d_w();
void genmul_d();
void gensub_d();
void gendiv_d();
void gencvt_s_d();
void genmov_s();
void genc_le_s();
void genbc1t();
void gentrunc_w_s();
void genbc1tl();
void genc_lt_s();
void genbc1fl();
void genneg_s();
void genc_le_d();
void genbgezal_idle();
void genj_idle();
void genbeq_idle();
void genlh();
void genmov_d();
void genc_lt_d();
void genbc1f();
void gennor();
void genneg_d();
void gensub();
void genblez_idle();
void gendivu();
void gencvt_w_s();
void genbltzl();
void gensdc1();
void genc_eq_s();
void genjalr();
void gensub_s();
void gensqrt_s();
void genc_eq_d();
void gencvt_w_d();
void genfin_block();
void genddiv();
void gendaddiu();
void genbgtzl();
void gendsrav();
void gendsllv();
void gencvt_s_l();
void gendmtc1();
void gendsrlv();
void gendsra();
void gendmult();
void gendsll();
void genabs_s();
void gensc();
void gennotcompiled();
void genjal_idle();
void genjal_out();
void gendebug();
void genbeq_out();
void gensyscall();
void gensync();
void gendadd();
void gendaddu();
void gendsub();
void gendsubu();
void genteq();
void gendsrl();
void gendsrl32();
void genbltz_idle();
void genbltz_out();
void genbgez_idle();
void genbgez_out();
void genbltzl_idle();
void genbltzl_out();
void genbgezl_idle();
void genbgezl_out();
void genbltzal_idle();
void genbltzal_out();
void genbltzal();
void genbgezal_out();
void genbltzall_idle();
void genbltzall_out();
void genbltzall();
void genbgezall_idle();
void genbgezall_out();
void genbgezall();
void gentlbwr();
void genbc1f_idle();
void genbc1f_out();
void genbc1t_idle();
void genbc1t_out();
void genbc1fl_idle();
void genbc1fl_out();
void genbc1tl_idle();
void genbc1tl_out();
void genround_l_s();
void gentrunc_l_s();
void genceil_l_s();
void genfloor_l_s();
void genround_w_s();
void genceil_w_s();
void genfloor_w_s();
void gencvt_l_s();
void genc_f_s();
void genc_un_s();
void genc_ueq_s();
void genc_olt_s();
void genc_ult_s();
void genc_ole_s();
void genc_ule_s();
void genc_sf_s();
void genc_ngle_s();
void genc_seq_s();
void genc_ngl_s();
void genc_nge_s();
void genc_ngt_s();
void gensqrt_d();
void genabs_d();
void genround_l_d();
void gentrunc_l_d();
void genceil_l_d();
void genfloor_l_d();
void genround_w_d();
void genceil_w_d();
void genfloor_w_d();
void gencvt_l_d();
void genc_f_d();
void genc_un_d();
void genc_ueq_d();
void genc_olt_d();
void genc_ult_d();
void genc_ole_d();
void genc_ule_d();
void genc_sf_d();
void genc_ngle_d();
void genc_seq_d();
void genc_ngl_d();
void genc_nge_d();
void genc_ngt_d();
void gencvt_d_l();
void gendmfc1();
void genj_out();
void genbne_idle();
void genbne_out();
void genblez_out();
void genbgtz_idle();
void genbgtz_out();
void genbeql_idle();
void genbeql_out();
void genbnel_idle();
void genbnel_out();
void genblezl_idle();
void genblezl_out();
void genbgtzl_idle();
void genbgtzl_out();
void gendaddi();
void genldl();
void genldr();
void genlwu();
void gensdl();
void gensdr();
void genlink_subblock();
void gendelayslot();
void gencheck_interupt_reg();
void gentest();
void gentest_out();
void gentest_idle();
void gentestl();
void gentestl_out();
void gencheck_cop1_unusable();
void genll();

#endif

