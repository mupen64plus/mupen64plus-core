/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - MIPS (native) instruction emission                      *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2015 Nebuleon <nebuleon.fumika@gmail.com>               *
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

#ifndef M64P_TRACE_JIT_MIPS32_NATIVE_OPS_H
#define M64P_TRACE_JIT_MIPS32_NATIVE_OPS_H

#include <stdint.h>

#include "state.h"

#define FP_FORMAT_S 16
#define FP_FORMAT_D 17
#define FP_FORMAT_W 20
#define FP_FORMAT_L 21

#define FP_PREDICATE_F    0 /**< false */
#define FP_PREDICATE_UN   1 /**< unordered */
#define FP_PREDICATE_EQ   2 /**< equal */
#define FP_PREDICATE_LT   4 /**< less than */
#define FP_EXCEPTION_QNAN 8 /**< raise an exception if a quiet NaN is present */

extern void mips32_dword(struct mips32_state* state, uint32_t data);

#define mips32_nop(state) mips32_dword(state, UINT32_C(0))

/* reg_dst = reg_a + reg_b; */
extern void mips32_addu(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_a, uint_fast8_t reg_b);

/* reg_dst = reg_src + imm; */
extern void mips32_addiu(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_src, int16_t imm);

/* reg_dst = reg_a & reg_b; */
extern void mips32_and(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_a, uint_fast8_t reg_b);

/* reg_dst = reg_src & imm; */
extern void mips32_andi(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_src, uint16_t imm);

/* PC: if (!FPUCondition) goto (PC + 4 + off * 4);
 * 'off' is in instructions, so -32768..+32767; that's -131068..+131076 bytes. */
extern void mips32_bc1f(struct mips32_state* state, int16_t off);

/* PC: if (FPUCondition) goto (PC + 4 + off * 4);
 * 'off' is in instructions, so -32768..+32767; that's -131068..+131076 bytes. */
extern void mips32_bc1t(struct mips32_state* state, int16_t off);

/* PC: if (reg_a == reg_b) goto (PC + 4 + off * 4);
 * 'off' is in instructions, so -32768..+32767; that's -131068..+131076 bytes. */
extern void mips32_beq(struct mips32_state* state, uint_fast8_t reg_a, uint_fast8_t reg_b, int16_t off);

/* PC: if ((int) reg_src >= 0) goto (PC + 4 + off * 4);
 * 'off' is in instructions, so -32768..+32767; that's -131068..+131076 bytes. */
extern void mips32_bgez(struct mips32_state* state, uint_fast8_t reg_src, int16_t off);

/* PC: $31 = PC + 8; if ((int) reg_src >= 0) goto (PC + 4 + off * 4);
 * 'off' is in instructions, so -32768..+32767; that's -131068..+131076 bytes. */
extern void mips32_bgezal(struct mips32_state* state, uint_fast8_t reg_src, int16_t off);

/* PC: if ((int) reg_src > 0) goto (PC + 4 + off * 4);
 * 'off' is in instructions, so -32768..+32767; that's -131068..+131076 bytes. */
extern void mips32_bgtz(struct mips32_state* state, uint_fast8_t reg_src, int16_t off);

/* PC: if ((int) reg_src <= 0) goto (PC + 4 + off * 4);
 * 'off' is in instructions, so -32768..+32767; that's -131068..+131076 bytes. */
extern void mips32_blez(struct mips32_state* state, uint_fast8_t reg_src, int16_t off);

/* PC: if ((int) reg_src < 0) goto (PC + 4 + off * 4);
 * 'off' is in instructions, so -32768..+32767; that's -131068..+131076 bytes. */
extern void mips32_bltz(struct mips32_state* state, uint_fast8_t reg_src, int16_t off);

/* PC: $31 = PC + 8; if ((int) reg_src < 0) goto (PC + 4 + off * 4);
 * 'off' is in instructions, so -32768..+32767; that's -131068..+131076 bytes. */
extern void mips32_bltzal(struct mips32_state* state, uint_fast8_t reg_src, int16_t off);

/* PC: if (reg_a != reg_b) goto (PC + 4 + off * 4);
 * 'off' is in instructions, so -32768..+32767; that's -131068..+131076 bytes. */
extern void mips32_bne(struct mips32_state* state, uint_fast8_t reg_a, uint_fast8_t reg_b, int16_t off);

/* reg_dst = FPUControl reg_c1c; */
extern void mips32_cfc1(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_c1c);

/* FPUControl reg_c1c = reg_src; */
extern void mips32_ctc1(struct mips32_state* state, uint_fast8_t reg_src, uint_fast8_t reg_c1c);

/* LO = (int) reg_a / (int) reg_b;
 * HI = (int) reg_a % (int) reg_b; */
extern void mips32_div(struct mips32_state* state, uint_fast8_t reg_a, uint_fast8_t reg_b);

/* LO = (unsigned int) reg_a / (unsigned int) reg_b;
 * HI = (unsigned int) reg_a % (unsigned int) reg_b; */
extern void mips32_divu(struct mips32_state* state, uint_fast8_t reg_a, uint_fast8_t reg_b);

/* FPU reg_dst = fabs(FPU reg_src); */
extern void mips32_fp_abs(struct mips32_state* state, uint_fast8_t fp_fmt, uint_fast8_t reg_dst, uint_fast8_t reg_src);

/* FPU reg_dst = FPU reg_a + FPU reg_b; */
extern void mips32_fp_add(struct mips32_state* state, uint_fast8_t fp_fmt, uint_fast8_t reg_dst, uint_fast8_t reg_a, uint_fast8_t reg_b);

/* FPU reg_dst = (int64_t) ceil(FPU reg_src); */
extern void mips32_fp_ceil_l(struct mips32_state* state, uint_fast8_t fp_fmt, uint_fast8_t reg_dst, uint_fast8_t reg_src);

/* FPU reg_dst = (int32_t) ceil(FPU reg_src); */
extern void mips32_fp_ceil_w(struct mips32_state* state, uint_fast8_t fp_fmt, uint_fast8_t reg_dst, uint_fast8_t reg_src);

/* FPUCondition = FPU reg_a <predicate> FPU reg_b; */
extern void mips32_fp_compare(struct mips32_state* state, uint_fast8_t fp_fmt, uint_fast8_t fp_pred, uint_fast8_t reg_a, uint_fast8_t reg_b);

/* FPU reg_dst = (double) FPU reg_src; */
extern void mips32_fp_cvt_d(struct mips32_state* state, uint_fast8_t fp_fmt, uint_fast8_t reg_dst, uint_fast8_t reg_src);

/* FPU reg_dst = (int64_t) FPU reg_src; */
extern void mips32_fp_cvt_l(struct mips32_state* state, uint_fast8_t fp_fmt, uint_fast8_t reg_dst, uint_fast8_t reg_src);

/* FPU reg_dst = (float) FPU reg_src; */
extern void mips32_fp_cvt_s(struct mips32_state* state, uint_fast8_t fp_fmt, uint_fast8_t reg_dst, uint_fast8_t reg_src);

/* FPU reg_dst = (int32_t) FPU reg_src; */
extern void mips32_fp_cvt_w(struct mips32_state* state, uint_fast8_t fp_fmt, uint_fast8_t reg_dst, uint_fast8_t reg_src);

/* FPU reg_dst = FPU reg_a / FPU reg_b; */
extern void mips32_fp_div(struct mips32_state* state, uint_fast8_t fp_fmt, uint_fast8_t reg_dst, uint_fast8_t reg_a, uint_fast8_t reg_b);

/* FPU reg_dst = (int64_t) floor(FPU reg_src); */
extern void mips32_fp_floor_l(struct mips32_state* state, uint_fast8_t fp_fmt, uint_fast8_t reg_dst, uint_fast8_t reg_src);

/* FPU reg_dst = (int32_t) floor(FPU reg_src); */
extern void mips32_fp_floor_w(struct mips32_state* state, uint_fast8_t fp_fmt, uint_fast8_t reg_dst, uint_fast8_t reg_src);

/* FPU reg_dst = FPU reg_src; */
extern void mips32_fp_mov(struct mips32_state* state, uint_fast8_t fp_fmt, uint_fast8_t reg_dst, uint_fast8_t reg_src);

/* FPU reg_dst = FPU reg_a * FPU reg_b; */
extern void mips32_fp_mul(struct mips32_state* state, uint_fast8_t fp_fmt, uint_fast8_t reg_dst, uint_fast8_t reg_a, uint_fast8_t reg_b);

/* FPU reg_dst = -FPU reg_src; */
extern void mips32_fp_neg(struct mips32_state* state, uint_fast8_t fp_fmt, uint_fast8_t reg_dst, uint_fast8_t reg_src);

/* FPU reg_dst = (int64_t) round(FPU reg_src); */
extern void mips32_fp_round_l(struct mips32_state* state, uint_fast8_t fp_fmt, uint_fast8_t reg_dst, uint_fast8_t reg_src);

/* FPU reg_dst = (int32_t) round(FPU reg_src); */
extern void mips32_fp_round_w(struct mips32_state* state, uint_fast8_t fp_fmt, uint_fast8_t reg_dst, uint_fast8_t reg_src);

/* FPU reg_dst = sqrt(FPU reg_src); */
extern void mips32_fp_sqrt(struct mips32_state* state, uint_fast8_t fp_fmt, uint_fast8_t reg_dst, uint_fast8_t reg_src);

/* FPU reg_dst = FPU reg_a - FPU reg_b; */
extern void mips32_fp_sub(struct mips32_state* state, uint_fast8_t fp_fmt, uint_fast8_t reg_dst, uint_fast8_t reg_a, uint_fast8_t reg_b);

/* FPU reg_dst = (int64_t) FPU reg_src; */
extern void mips32_fp_trunc_l(struct mips32_state* state, uint_fast8_t fp_fmt, uint_fast8_t reg_dst, uint_fast8_t reg_src);

/* FPU reg_dst = (int32_t) FPU reg_src; */
extern void mips32_fp_trunc_w(struct mips32_state* state, uint_fast8_t fp_fmt, uint_fast8_t reg_dst, uint_fast8_t reg_src);

/* goto target;
 * The native version is more like:
 * PC: goto ((PC + 4) & ~0x0FFFFFFF) | ((target & 0x03FFFFFF) << 2); */
extern void mips32_j(struct mips32_state* state, void* target);

/* PC: $31 = PC + 8; goto target;
 * The native version is more like:
 * PC: $31 = PC + 8; goto ((PC + 4) & ~0x0FFFFFFF) | ((target & 0x03FFFFFF) << 2); */
extern void mips32_jal(struct mips32_state* state, void* target);

/* PC: $31 = PC + 8; goto reg; */
extern void mips32_jalr(struct mips32_state* state, uint_fast8_t reg);

/* PC: reg_link = PC + 8; goto reg; */
extern void mips32_jalr_to(struct mips32_state* state, uint_fast8_t reg_link, uint_fast8_t reg_target);

/* goto reg; */
extern void mips32_jr(struct mips32_state* state, uint_fast8_t reg);

/* reg = imm << 16; */
extern void mips32_lui(struct mips32_state* state, uint_fast8_t reg, int16_t imm);

/* reg_dst = *(int8_t*) ((uintptr_t) reg_addr + off); */
extern void mips32_lb(struct mips32_state* state, uint_fast8_t reg_dst, int16_t off, uint_fast8_t reg_addr);

/* reg_dst = *(uint8_t*) ((uintptr_t) reg_addr + off); */
extern void mips32_lbu(struct mips32_state* state, uint_fast8_t reg_dst, int16_t off, uint_fast8_t reg_addr);

/* FPU fp_dst = *(double*) ((uintptr_t) reg_addr + off); */
extern void mips32_ldc1(struct mips32_state* state, uint_fast8_t fp_dst, int16_t off, uint_fast8_t reg_addr);

/* reg_dst = *(int16_t*) ((uintptr_t) reg_addr + off); */
extern void mips32_lh(struct mips32_state* state, uint_fast8_t reg_dst, int16_t off, uint_fast8_t reg_addr);

/* reg_dst = *(uint16_t*) ((uintptr_t) reg_addr + off); */
extern void mips32_lhu(struct mips32_state* state, uint_fast8_t reg_dst, int16_t off, uint_fast8_t reg_addr);

/* reg_dst = *(int32_t*) ((uintptr_t) reg_addr + off); */
extern void mips32_lw(struct mips32_state* state, uint_fast8_t reg_dst, int16_t off, uint_fast8_t reg_addr);

/* FPU fp_dst = *(float*) ((uintptr_t) reg_addr + off); */
extern void mips32_lwc1(struct mips32_state* state, uint_fast8_t fp_dst, int16_t off, uint_fast8_t reg_addr);

/* int_dst = FPU fp_src; */
extern void mips32_mfc1(struct mips32_state* state, uint_fast8_t int_dst, uint_fast8_t fp_src);

/* reg_dst = HI; */
extern void mips32_mfhi(struct mips32_state* state, uint_fast8_t reg_dst);

/* reg_dst = LO; */
extern void mips32_mflo(struct mips32_state* state, uint_fast8_t reg_dst);

/* if (reg_check != 0) reg_dst = reg_src; */
extern void mips32_movn(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_src, uint_fast8_t reg_check);

/* if (reg_check == 0) reg_dst = reg_src; */
extern void mips32_movz(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_src, uint_fast8_t reg_check);

/* FPU fp_dst = int_src; */
extern void mips32_mtc1(struct mips32_state* state, uint_fast8_t int_src, uint_fast8_t fp_dst);

/* HI = reg_src; */
extern void mips32_mthi(struct mips32_state* state, uint_fast8_t reg_src);

/* LO = reg_src; */
extern void mips32_mtlo(struct mips32_state* state, uint_fast8_t reg_src);

/* HI:LO = (int) reg_a * (int) reg_b; */
extern void mips32_mult(struct mips32_state* state, uint_fast8_t reg_a, uint_fast8_t reg_b);

/* HI:LO = (unsigned int) reg_a * (unsigned int) reg_b; */
extern void mips32_multu(struct mips32_state* state, uint_fast8_t reg_a, uint_fast8_t reg_b);

/* reg_dst = ~(reg_a | reg_b); */
extern void mips32_nor(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_a, uint_fast8_t reg_b);

/* reg_dst = reg_a | reg_b; */
extern void mips32_or(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_a, uint_fast8_t reg_b);

/* reg_dst = reg_a | imm; */
extern void mips32_ori(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_src, uint16_t imm);

/* *(int8_t*) ((uintptr_t) reg_addr + off) = reg_src; */
extern void mips32_sb(struct mips32_state* state, uint_fast8_t reg_src, int16_t off, uint_fast8_t reg_addr);

/* *(double*) ((uintptr_t) reg_addr + off) = FPU fp_src; */
extern void mips32_sdc1(struct mips32_state* state, uint_fast8_t fp_src, int16_t off, uint_fast8_t reg_addr);

/* *(int16_t*) ((uintptr_t) reg_addr + off) = reg_src; */
extern void mips32_sh(struct mips32_state* state, uint_fast8_t reg_src, int16_t off, uint_fast8_t reg_addr);

/* reg_dst = reg_src << amt; */
extern void mips32_sll(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_src, uint_fast8_t amt);

/* reg_dst = reg_src << reg_amt; */
extern void mips32_sllv(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_src, uint_fast8_t reg_amt);

/* reg_dst = ((int) reg_a < (int) reg_b) ? 1 : 0; */
extern void mips32_slt(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_a, uint_fast8_t reg_b);

/* reg_dst = ((unsigned int) reg_a < (unsigned int) reg_b) ? 1 : 0; */
extern void mips32_sltu(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_a, uint_fast8_t reg_b);

/* reg_dst = ((int) reg_src < imm) ? 1 : 0; */
extern void mips32_slti(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_src, int16_t imm);

/* reg_dst = ((unsigned int) reg_src < (unsigned int) (int) imm) ? 1 : 0; */
extern void mips32_sltiu(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_src, int16_t imm);

/* reg_dst = (int) reg_src >> amt; */
extern void mips32_sra(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_src, uint_fast8_t amt);

/* reg_dst = (int) reg_src >> reg_amt; */
extern void mips32_srav(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_src, uint_fast8_t reg_amt);

/* reg_dst = (unsigned int) reg_src >> amt; */
extern void mips32_srl(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_src, uint_fast8_t amt);

/* reg_dst = (unsigned int) reg_src >> reg_amt; */
extern void mips32_srlv(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_src, uint_fast8_t reg_amt);

/* reg_dst = reg_a - reg_b; */
extern void mips32_subu(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_a, uint_fast8_t reg_b);

/* *(int32_t*) ((uintptr_t) reg_addr + off) = reg_src; */
extern void mips32_sw(struct mips32_state* state, uint_fast8_t reg_src, int16_t off, uint_fast8_t reg_addr);

/* *(float*) ((uintptr_t) reg_addr + off) = FPU fp_src; */
extern void mips32_swc1(struct mips32_state* state, uint_fast8_t fp_src, int16_t off, uint_fast8_t reg_addr);

/* reg_dst = reg_a ^ reg_b; */
extern void mips32_xor(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_a, uint_fast8_t reg_b);

/* reg_dst = reg_a ^ imm; */
extern void mips32_xori(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_src, uint16_t imm);

#endif /* !M64P_TRACE_JIT_MIPS32_NATIVE_OPS_H */
