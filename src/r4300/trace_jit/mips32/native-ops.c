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

#include <assert.h>

#include "native-ops.h"

#define _OP_SLL		0x00
#define _OP_SRL		0x02
#define _OP_SRA		0x03
#define _OP_SLLV		0x04
#define _OP_SRLV		0x06
#define _OP_SRAV		0x07
#define _OP_JR		0x08
#define _OP_JALR		0x09
#define _OP_MULT		0x18
#define _OP_MULTU	0x19
#define _OP_DIV		0x1a
#define _OP_DIVU		0x1b
#define _OP_MFHI		0x10
#define _OP_MTHI		0x11
#define _OP_ADDU		0x21
#define _OP_MFLO		0x12
#define _OP_MTLO		0x13
#define _OP_SUBU		0x23
#define _OP_AND		0x24
#define _OP_OR		0x25
#define _OP_XOR		0x26
#define _OP_NOR		0x27
#define _OP_SLT		0x2a
#define _OP_SLTU		0x2b
#define _OP_MOVZ		0x0a
#define _OP_MOVN		0x0b
#define _OP_BLTZ		0x04000000
#define _OP_BGEZ		0x04010000
#define _OP_BLTZAL	0x04100000
#define _OP_BGEZAL	0x04110000
#define _OP_J		0x08000000
#define _OP_JAL		0x0c000000
#define _OP_BEQ		0x10000000
#define _OP_BNE		0x14000000
#define _OP_BLEZ		0x18000000
#define _OP_BGTZ		0x1c000000
#define _OP_ADDIU	0x24000000
#define _OP_SLTI		0x28000000
#define _OP_SLTIU	0x2c000000
#define _OP_ANDI		0x30000000
#define _OP_ORI		0x34000000
#define _OP_XORI		0x38000000
#define _OP_LUI		0x3c000000
#define _OP_LB		0x80000000
#define _OP_LH		0x84000000
#define _OP_LW		0x8c000000
#define _OP_LBU		0x90000000
#define _OP_LHU		0x94000000
#define _OP_SB		0xa0000000
#define _OP_SH		0xa4000000
#define _OP_SW		0xac000000
#define _OP_CFC1	0x44400000
#define _OP_CTC1	0x44c00000
#define _OP_FP_ADD	0x44000000
#define _OP_FP_SUB	0x44000001
#define _OP_FP_MUL	0x44000002
#define _OP_FP_DIV	0x44000003
#define _OP_FP_SQRT	0x44000004
#define _OP_FP_ABS	0x44000005
#define _OP_FP_MOV	0x44000006
#define _OP_FP_NEG	0x44000007
#define _OP_FP_ROUND_W	0x4400000c
#define _OP_FP_ROUND_L	0x44000008
#define _OP_FP_TRUNC_W	0x4400000d
#define _OP_FP_TRUNC_L	0x44000009
#define _OP_FP_CEIL_W	0x4400000e
#define _OP_FP_CEIL_L	0x4400000a
#define _OP_FP_FLOOR_W	0x4400000f
#define _OP_FP_FLOOR_L	0x4400000b
#define _OP_FP_CVT_S	0x44000020
#define _OP_FP_CVT_D	0x44000021
#define _OP_FP_CVT_W	0x44000024
#define _OP_FP_CVT_L	0x44000025
#define _OP_FP_C	0x44000030
#define _OP_MFC1	0x44000000
#define _OP_MTC1	0x44800000
#define _OP_LWC1	0xc4000000
#define _OP_LDC1	0xd4000000
#define _OP_SWC1	0xe4000000
#define _OP_SDC1	0xf4000000
#define _OP_BC1F	0x45000000
#define _OP_BC1T	0x45010000

#ifdef _MIPS_ARCH_MIPS32R2
#define _OP_EXT		0x7c000000
#define _OP_INS		0x7c000004
#define _OP_SEB		0x7c000420
#define _OP_SEH		0x7c000620
#endif


#define __OP_R(rs1, rs2, rd, imm, op) \
  (((rs1) << 21) | ((rs2) << 16) | ((rd) << 11) | ((imm) << 6) | (op))

#define OP_ADDU(rs1, rs2, rd) __OP_R(rs1, rs2, rd, 0, _OP_ADDU)
#define OP_SUBU(rs1, rs2, rd) __OP_R(rs1, rs2, rd, 0, _OP_SUBU)
#define OP_AND(rs1, rs2, rd)  __OP_R(rs1, rs2, rd, 0, _OP_AND)
#define OP_NOR(rs1, rs2, rd)  __OP_R(rs1, rs2, rd, 0, _OP_NOR)
#define OP_OR(rs1, rs2, rd)   __OP_R(rs1, rs2, rd, 0, _OP_OR)
#define OP_SLT(rs1, rs2, rd)  __OP_R(rs1, rs2, rd, 0, _OP_SLT)
#define OP_SLTU(rs1, rs2, rd) __OP_R(rs1, rs2, rd, 0, _OP_SLTU)
#define OP_XOR(rs1, rs2, rd)  __OP_R(rs1, rs2, rd, 0, _OP_XOR)
#define OP_SLLV(rs1, rs2, rd) __OP_R(rs1, rs2, rd, 0, _OP_SLLV)
#define OP_SRAV(rs1, rs2, rd) __OP_R(rs1, rs2, rd, 0, _OP_SRAV)
#define OP_SRLV(rs1, rs2, rd) __OP_R(rs1, rs2, rd, 0, _OP_SRLV)
#define OP_DIV(rs1, rs2)      __OP_R(rs1, rs2, 0,  0, _OP_DIV)
#define OP_DIVU(rs1, rs2)     __OP_R(rs1, rs2, 0,  0, _OP_DIVU)
#define OP_MULT(rs1, rs2)     __OP_R(rs1, rs2, 0,  0, _OP_MULT)
#define OP_MULTU(rs1, rs2)    __OP_R(rs1, rs2, 0,  0, _OP_MULTU)
#define OP_MOVZ(rs, rc, rd)   __OP_R(rs,  rc,  rd, 0, _OP_MOVZ)
#define OP_MOVN(rs, rc, rd)   __OP_R(rs,  rc,  rd, 0, _OP_MOVN)
#define OP_MFHI(rd)           __OP_R(0,   0,   rd, 0, _OP_MFHI)
#define OP_MFLO(rd)           __OP_R(0,   0,   rd, 0, _OP_MFLO)
#define OP_MTHI(rs)           __OP_R(rs,  0,   0,  0, _OP_MTHI)
#define OP_MTLO(rs)           __OP_R(rs,  0,   0,  0, _OP_MTLO)

#define OP_SLL(rs, rd, imm)   __OP_R(0,   rs,  rd, imm, _OP_SLL)
#define OP_SRA(rs, rd, imm)   __OP_R(0,   rs,  rd, imm, _OP_SRA)
#define OP_SRL(rs, rd, imm)   __OP_R(0,   rs,  rd, imm, _OP_SRL)

#define OP_JALR(rs, rd)       __OP_R(rs,  0,   rd, 0, _OP_JALR)
#define OP_JR(rs)             __OP_R(rs,  0,   0,  0, _OP_JR)

#define OP_CFC1(rt, fs)       __OP_R(0,   rt,  fs, 0, _OP_CFC1)
#define OP_CTC1(rt, fs)       __OP_R(0,   rt,  fs, 0, _OP_CTC1)
#define OP_FP_ADD(fmt, ft, fs, fd)  __OP_R(fmt, ft, fs, fd, _OP_FP_ADD)
#define OP_FP_SUB(fmt, ft, fs, fd)  __OP_R(fmt, ft, fs, fd, _OP_FP_SUB)
#define OP_FP_MUL(fmt, ft, fs, fd)  __OP_R(fmt, ft, fs, fd, _OP_FP_MUL)
#define OP_FP_DIV(fmt, ft, fs, fd)  __OP_R(fmt, ft, fs, fd, _OP_FP_DIV)
#define OP_FP_SQRT(fmt, fs, fd)     __OP_R(fmt, 0,  fs, fd, _OP_FP_SQRT)
#define OP_FP_ABS(fmt, fs, fd)      __OP_R(fmt, 0,  fs, fd, _OP_FP_ABS)
#define OP_FP_MOV(fmt, fs, fd)      __OP_R(fmt, 0,  fs, fd, _OP_FP_MOV)
#define OP_FP_NEG(fmt, fs, fd)      __OP_R(fmt, 0,  fs, fd, _OP_FP_NEG)
#define OP_FP_ROUND_W(fmt, fs, fd)  __OP_R(fmt, 0,  fs, fd, _OP_FP_ROUND_W)
#define OP_FP_ROUND_L(fmt, fs, fd)  __OP_R(fmt, 0,  fs, fd, _OP_FP_ROUND_L)
#define OP_FP_TRUNC_W(fmt, fs, fd)  __OP_R(fmt, 0,  fs, fd, _OP_FP_TRUNC_W)
#define OP_FP_TRUNC_L(fmt, fs, fd)  __OP_R(fmt, 0,  fs, fd, _OP_FP_TRUNC_L)
#define OP_FP_CEIL_W(fmt, fs, fd)   __OP_R(fmt, 0,  fs, fd, _OP_FP_CEIL_W)
#define OP_FP_CEIL_L(fmt, fs, fd)   __OP_R(fmt, 0,  fs, fd, _OP_FP_CEIL_L)
#define OP_FP_FLOOR_W(fmt, fs, fd)  __OP_R(fmt, 0,  fs, fd, _OP_FP_FLOOR_W)
#define OP_FP_FLOOR_L(fmt, fs, fd)  __OP_R(fmt, 0,  fs, fd, _OP_FP_FLOOR_L)
#define OP_FP_CVT_S(fmt, fs, fd)    __OP_R(fmt, 0,  fs, fd, _OP_FP_CVT_S)
#define OP_FP_CVT_D(fmt, fs, fd)    __OP_R(fmt, 0,  fs, fd, _OP_FP_CVT_D)
#define OP_FP_CVT_W(fmt, fs, fd)    __OP_R(fmt, 0,  fs, fd, _OP_FP_CVT_W)
#define OP_FP_CVT_L(fmt, fs, fd)    __OP_R(fmt, 0,  fs, fd, _OP_FP_CVT_L)

#define OP_MFC1(rt, fs)             __OP_R(0,   rt, fs, 0,  _OP_MFC1)
#define OP_MTC1(rt, fs)             __OP_R(0,   rt, fs, 0,  _OP_MTC1)

#ifdef _MIPS_ARCH_MIPS32R2
#define OP_EXT(rs, rd, s, p)  __OP_R(rs,  rd,  (s) - 1,  p, _OP_EXT)
#define OP_INS(rs, rd, s, p)  __OP_R(rs,  rd,  (p) + (s) - 1,  p, _OP_INS)
#define OP_SEB(rs, rd)        __OP_R(0,   rs,  rd, 0, _OP_SEB)
#define OP_SEH(rs, rd)        __OP_R(0,   rs,  rd, 0, _OP_SEH)
#endif


#define OP_FP_C(fmt, ft, fs, cond) \
  (((fmt) << 21) | ((ft) << 16) | ((fs) << 11) | (cond) | _OP_FP_C)

#define __OP_I(rs, rd, imm, op) \
  ((op) | ((rs) << 21) | ((rd) << 16) | ((imm) & 0xffff))

#define OP_ADDIU(rs, rd, imm) __OP_I(rs, rd, imm, _OP_ADDIU)
#define OP_ANDI(rs, rd, imm)  __OP_I(rs, rd, imm, _OP_ANDI)
#define OP_ORI(rs, rd, imm)   __OP_I(rs, rd, imm, _OP_ORI)
#define OP_SLTI(rs, rd, imm)  __OP_I(rs, rd, imm, _OP_SLTI)
#define OP_SLTIU(rs, rd, imm) __OP_I(rs, rd, imm, _OP_SLTIU)
#define OP_XORI(rs, rd, imm)  __OP_I(rs, rd, imm, _OP_XORI)
#define OP_LB(rs, rd, off)    __OP_I(rs, rd, off, _OP_LB)
#define OP_LBU(rs, rd, off)   __OP_I(rs, rd, off, _OP_LBU)
#define OP_LH(rs, rd, off)    __OP_I(rs, rd, off, _OP_LH)
#define OP_LHU(rs, rd, off)   __OP_I(rs, rd, off, _OP_LHU)
#define OP_LW(rs, rd, off)    __OP_I(rs, rd, off, _OP_LW)
#define OP_SB(rs, rd, off)    __OP_I(rs, rd, off, _OP_SB)
#define OP_SH(rs, rd, off)    __OP_I(rs, rd, off, _OP_SH)
#define OP_SW(rs, rd, off)    __OP_I(rs, rd, off, _OP_SW)
#define OP_LWC1(rs, fd, off)  __OP_I(rs, fd, off, _OP_LWC1)
#define OP_LDC1(rs, fd, off)  __OP_I(rs, fd, off, _OP_LDC1)
#define OP_SWC1(rs, fd, off)  __OP_I(rs, fd, off, _OP_SWC1)
#define OP_SDC1(rs, fd, off)  __OP_I(rs, fd, off, _OP_SDC1)

#define OP_BEQ(rs1, rs2, off) __OP_I(rs1, rs2, off, _OP_BEQ)
#define OP_BNE(rs1, rs2, off) __OP_I(rs1, rs2, off, _OP_BNE)

#define OP_BGEZ(rs, off)      __OP_I(rs,  0,   off, _OP_BGEZ)
#define OP_BGEZAL(rs, off)    __OP_I(rs,  0,   off, _OP_BGEZAL)
#define OP_BGTZ(rs, off)      __OP_I(rs,  0,   off, _OP_BGTZ)
#define OP_BLEZ(rs, off)      __OP_I(rs,  0,   off, _OP_BLEZ)
#define OP_BLTZ(rs, off)      __OP_I(rs,  0,   off, _OP_BLTZ)
#define OP_BLTZAL(rs, off)    __OP_I(rs,  0,   off, _OP_BLTZAL)

#define OP_BC1F(off)          __OP_I(0,   0,   off, _OP_BC1F)
#define OP_BC1T(off)          __OP_I(0,   0,   off, _OP_BC1T)

#define OP_LUI(rd, imm)       __OP_I(0 ,  rd,  imm, _OP_LUI)

#define __OP_J(addr, op) ((op) | ((((uintptr_t) (addr)) & 0x0ffffffc) >> 2))

#define OP_J(addr)            __OP_J(addr, _OP_J)
#define OP_JAL(addr)          __OP_J(addr, _OP_JAL)

#define CHECK_REG(reg) assert((reg) < 32)
#define CHECK_FP_FMT_S_D(fp_fmt) assert((fp_fmt) == FP_FORMAT_S || (fp_fmt) == FP_FORMAT_D)
#define CHECK_FP_PREDICATE(fp_pred) assert((fp_pred) < 16)
#define CHECK_256_MIB_SEGMENT(target, source) assert(((uintptr_t) ((source) + 4) & ~(uintptr_t) 0xFFFFFFF) == (((uintptr_t) (target)) & ~(uintptr_t) 0xFFFFFFF))

void mips32_dword(struct mips32_state* state, uint32_t data)
{
	if (state->code && state->avail >= 4) {
		*(uint32_t*) state->code = data;
		state->avail -= 4;
		state->code = (uint32_t*) state->code + 1;
	}
	else state->code = NULL;
}

void mips32_addu(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_a, uint_fast8_t reg_b)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_a);
	CHECK_REG(reg_b);
	mips32_dword(state, OP_ADDU(reg_a, reg_b, reg_dst));
}

void mips32_addiu(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_src, int16_t imm)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	mips32_dword(state, OP_ADDIU(reg_src, reg_dst, imm));
}

void mips32_and(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_a, uint_fast8_t reg_b)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_a);
	CHECK_REG(reg_b);
	mips32_dword(state, OP_AND(reg_a, reg_b, reg_dst));
}

void mips32_andi(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_src, uint16_t imm)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	mips32_dword(state, OP_ANDI(reg_src, reg_dst, imm));
}

void mips32_bc1f(struct mips32_state* state, int16_t off)
{
	mips32_dword(state, OP_BC1F(off));
}

void mips32_bc1t(struct mips32_state* state, int16_t off)
{
	mips32_dword(state, OP_BC1T(off));
}

void mips32_beq(struct mips32_state* state, uint_fast8_t reg_a, uint_fast8_t reg_b, int16_t off)
{
	CHECK_REG(reg_a);
	CHECK_REG(reg_b);
	mips32_dword(state, OP_BEQ(reg_a, reg_b, off));
}

void mips32_bgez(struct mips32_state* state, uint_fast8_t reg_src, int16_t off)
{
	CHECK_REG(reg_src);
	mips32_dword(state, OP_BGEZ(reg_src, off));
}

void mips32_bgezal(struct mips32_state* state, uint_fast8_t reg_src, int16_t off)
{
	CHECK_REG(reg_src);
	mips32_dword(state, OP_BGEZAL(reg_src, off));
}

void mips32_bgtz(struct mips32_state* state, uint_fast8_t reg_src, int16_t off)
{
	CHECK_REG(reg_src);
	mips32_dword(state, OP_BGTZ(reg_src, off));
}

void mips32_blez(struct mips32_state* state, uint_fast8_t reg_src, int16_t off)
{
	CHECK_REG(reg_src);
	mips32_dword(state, OP_BLEZ(reg_src, off));
}

void mips32_bltz(struct mips32_state* state, uint_fast8_t reg_src, int16_t off)
{
	CHECK_REG(reg_src);
	mips32_dword(state, OP_BLTZ(reg_src, off));
}

void mips32_bltzal(struct mips32_state* state, uint_fast8_t reg_src, int16_t off)
{
	CHECK_REG(reg_src);
	mips32_dword(state, OP_BLTZAL(reg_src, off));
}

void mips32_bne(struct mips32_state* state, uint_fast8_t reg_a, uint_fast8_t reg_b, int16_t off)
{
	CHECK_REG(reg_a);
	CHECK_REG(reg_b);
	mips32_dword(state, OP_BNE(reg_a, reg_b, off));
}

void mips32_cfc1(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_c1c)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_c1c);
	mips32_dword(state, OP_CFC1(reg_dst, reg_c1c));
}

void mips32_ctc1(struct mips32_state* state, uint_fast8_t reg_src, uint_fast8_t reg_c1c)
{
	CHECK_REG(reg_src);
	CHECK_REG(reg_c1c);
	mips32_dword(state, OP_CTC1(reg_src, reg_c1c));
}

void mips32_div(struct mips32_state* state, uint_fast8_t reg_a, uint_fast8_t reg_b)
{
	CHECK_REG(reg_a);
	CHECK_REG(reg_b);
	mips32_dword(state, OP_DIV(reg_a, reg_b));
}

void mips32_divu(struct mips32_state* state, uint_fast8_t reg_a, uint_fast8_t reg_b)
{
	CHECK_REG(reg_a);
	CHECK_REG(reg_b);
	mips32_dword(state, OP_DIVU(reg_a, reg_b));
}

void mips32_fp_abs(struct mips32_state* state, uint_fast8_t fp_fmt, uint_fast8_t reg_dst, uint_fast8_t reg_src)
{
	CHECK_FP_FMT_S_D(fp_fmt);
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	mips32_dword(state, OP_FP_ABS(fp_fmt, reg_src, reg_dst));
}

void mips32_fp_add(struct mips32_state* state, uint_fast8_t fp_fmt, uint_fast8_t reg_dst, uint_fast8_t reg_a, uint_fast8_t reg_b)
{
	CHECK_FP_FMT_S_D(fp_fmt);
	CHECK_REG(reg_dst);
	CHECK_REG(reg_a);
	CHECK_REG(reg_b);
	mips32_dword(state, OP_FP_ADD(fp_fmt, reg_b, reg_a, reg_dst));
}

void mips32_fp_ceil_l(struct mips32_state* state, uint_fast8_t fp_fmt, uint_fast8_t reg_dst, uint_fast8_t reg_src)
{
	CHECK_FP_FMT_S_D(fp_fmt);
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	mips32_dword(state, OP_FP_CEIL_L(fp_fmt, reg_src, reg_dst));
}

void mips32_fp_ceil_w(struct mips32_state* state, uint_fast8_t fp_fmt, uint_fast8_t reg_dst, uint_fast8_t reg_src)
{
	CHECK_FP_FMT_S_D(fp_fmt);
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	mips32_dword(state, OP_FP_CEIL_W(fp_fmt, reg_src, reg_dst));
}

void mips32_fp_compare(struct mips32_state* state, uint_fast8_t fp_fmt, uint_fast8_t fp_pred, uint_fast8_t reg_a, uint_fast8_t reg_b)
{
	CHECK_FP_FMT_S_D(fp_fmt);
	CHECK_FP_PREDICATE(fp_pred);
	CHECK_REG(reg_a);
	CHECK_REG(reg_b);
	mips32_dword(state, OP_FP_C(fp_fmt, reg_b, reg_a, fp_pred));
}

void mips32_fp_cvt_d(struct mips32_state* state, uint_fast8_t fp_fmt, uint_fast8_t reg_dst, uint_fast8_t reg_src)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	mips32_dword(state, OP_FP_CVT_D(fp_fmt, reg_src, reg_dst));
}

void mips32_fp_cvt_l(struct mips32_state* state, uint_fast8_t fp_fmt, uint_fast8_t reg_dst, uint_fast8_t reg_src)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	mips32_dword(state, OP_FP_CVT_L(fp_fmt, reg_src, reg_dst));
}

void mips32_fp_cvt_s(struct mips32_state* state, uint_fast8_t fp_fmt, uint_fast8_t reg_dst, uint_fast8_t reg_src)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	mips32_dword(state, OP_FP_CVT_S(fp_fmt, reg_src, reg_dst));
}

void mips32_fp_cvt_w(struct mips32_state* state, uint_fast8_t fp_fmt, uint_fast8_t reg_dst, uint_fast8_t reg_src)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	mips32_dword(state, OP_FP_CVT_W(fp_fmt, reg_src, reg_dst));
}

void mips32_fp_div(struct mips32_state* state, uint_fast8_t fp_fmt, uint_fast8_t reg_dst, uint_fast8_t reg_a, uint_fast8_t reg_b)
{
	CHECK_FP_FMT_S_D(fp_fmt);
	CHECK_REG(reg_dst);
	CHECK_REG(reg_a);
	CHECK_REG(reg_b);
	mips32_dword(state, OP_FP_DIV(fp_fmt, reg_b, reg_a, reg_dst));
}

void mips32_fp_floor_l(struct mips32_state* state, uint_fast8_t fp_fmt, uint_fast8_t reg_dst, uint_fast8_t reg_src)
{
	CHECK_FP_FMT_S_D(fp_fmt);
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	mips32_dword(state, OP_FP_FLOOR_L(fp_fmt, reg_src, reg_dst));
}

void mips32_fp_floor_w(struct mips32_state* state, uint_fast8_t fp_fmt, uint_fast8_t reg_dst, uint_fast8_t reg_src)
{
	CHECK_FP_FMT_S_D(fp_fmt);
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	mips32_dword(state, OP_FP_FLOOR_W(fp_fmt, reg_src, reg_dst));
}

void mips32_fp_mov(struct mips32_state* state, uint_fast8_t fp_fmt, uint_fast8_t reg_dst, uint_fast8_t reg_src)
{
	CHECK_FP_FMT_S_D(fp_fmt);
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	mips32_dword(state, OP_FP_MOV(fp_fmt, reg_src, reg_dst));
}

void mips32_fp_mul(struct mips32_state* state, uint_fast8_t fp_fmt, uint_fast8_t reg_dst, uint_fast8_t reg_a, uint_fast8_t reg_b)
{
	CHECK_FP_FMT_S_D(fp_fmt);
	CHECK_REG(reg_dst);
	CHECK_REG(reg_a);
	CHECK_REG(reg_b);
	mips32_dword(state, OP_FP_MUL(fp_fmt, reg_b, reg_a, reg_dst));
}

void mips32_fp_neg(struct mips32_state* state, uint_fast8_t fp_fmt, uint_fast8_t reg_dst, uint_fast8_t reg_src)
{
	CHECK_FP_FMT_S_D(fp_fmt);
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	mips32_dword(state, OP_FP_NEG(fp_fmt, reg_src, reg_dst));
}

void mips32_fp_round_l(struct mips32_state* state, uint_fast8_t fp_fmt, uint_fast8_t reg_dst, uint_fast8_t reg_src)
{
	CHECK_FP_FMT_S_D(fp_fmt);
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	mips32_dword(state, OP_FP_ROUND_L(fp_fmt, reg_src, reg_dst));
}

void mips32_fp_round_w(struct mips32_state* state, uint_fast8_t fp_fmt, uint_fast8_t reg_dst, uint_fast8_t reg_src)
{
	CHECK_FP_FMT_S_D(fp_fmt);
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	mips32_dword(state, OP_FP_ROUND_W(fp_fmt, reg_src, reg_dst));
}

void mips32_fp_sqrt(struct mips32_state* state, uint_fast8_t fp_fmt, uint_fast8_t reg_dst, uint_fast8_t reg_src)
{
	CHECK_FP_FMT_S_D(fp_fmt);
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	mips32_dword(state, OP_FP_SQRT(fp_fmt, reg_src, reg_dst));
}

void mips32_fp_sub(struct mips32_state* state, uint_fast8_t fp_fmt, uint_fast8_t reg_dst, uint_fast8_t reg_a, uint_fast8_t reg_b)
{
	CHECK_FP_FMT_S_D(fp_fmt);
	CHECK_REG(reg_dst);
	CHECK_REG(reg_a);
	CHECK_REG(reg_b);
	mips32_dword(state, OP_FP_SUB(fp_fmt, reg_b, reg_a, reg_dst));
}

void mips32_fp_trunc_l(struct mips32_state* state, uint_fast8_t fp_fmt, uint_fast8_t reg_dst, uint_fast8_t reg_src)
{
	CHECK_FP_FMT_S_D(fp_fmt);
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	mips32_dword(state, OP_FP_TRUNC_L(fp_fmt, reg_src, reg_dst));
}

void mips32_fp_trunc_w(struct mips32_state* state, uint_fast8_t fp_fmt, uint_fast8_t reg_dst, uint_fast8_t reg_src)
{
	CHECK_FP_FMT_S_D(fp_fmt);
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	mips32_dword(state, OP_FP_TRUNC_W(fp_fmt, reg_src, reg_dst));
}

void mips32_j(struct mips32_state* state, void* target)
{
	CHECK_256_MIB_SEGMENT(target, state->code);
	mips32_dword(state, OP_J(target));
}

void mips32_jal(struct mips32_state* state, void* target)
{
	CHECK_256_MIB_SEGMENT(target, state->code);
	mips32_dword(state, OP_JAL(target));
}

void mips32_jalr(struct mips32_state* state, uint_fast8_t reg)
{
	CHECK_REG(reg);
	mips32_dword(state, OP_JALR(reg, 31));
}

void mips32_jalr_to(struct mips32_state* state, uint_fast8_t reg_link, uint_fast8_t reg_target)
{
	CHECK_REG(reg_link);
	CHECK_REG(reg_target);
	mips32_dword(state, OP_JALR(reg_target, reg_link));
}

void mips32_jr(struct mips32_state* state, uint_fast8_t reg)
{
	CHECK_REG(reg);
	mips32_dword(state, OP_JR(reg));
}

void mips32_lui(struct mips32_state* state, uint_fast8_t reg, int16_t imm)
{
	CHECK_REG(reg);
	mips32_dword(state, OP_LUI(reg, imm));
}

void mips32_lb(struct mips32_state* state, uint_fast8_t reg_dst, int16_t off, uint_fast8_t reg_addr)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_addr);
	mips32_dword(state, OP_LB(reg_addr, reg_dst, off));
}

void mips32_lbu(struct mips32_state* state, uint_fast8_t reg_dst, int16_t off, uint_fast8_t reg_addr)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_addr);
	mips32_dword(state, OP_LBU(reg_addr, reg_dst, off));
}

void mips32_ldc1(struct mips32_state* state, uint_fast8_t fp_dst, int16_t off, uint_fast8_t reg_addr)
{
	CHECK_REG(fp_dst);
	CHECK_REG(reg_addr);
	mips32_dword(state, OP_LDC1(reg_addr, fp_dst, off));
}

void mips32_lh(struct mips32_state* state, uint_fast8_t reg_dst, int16_t off, uint_fast8_t reg_addr)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_addr);
	mips32_dword(state, OP_LH(reg_addr, reg_dst, off));
}

void mips32_lhu(struct mips32_state* state, uint_fast8_t reg_dst, int16_t off, uint_fast8_t reg_addr)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_addr);
	mips32_dword(state, OP_LHU(reg_addr, reg_dst, off));
}

void mips32_lw(struct mips32_state* state, uint_fast8_t reg_dst, int16_t off, uint_fast8_t reg_addr)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_addr);
	mips32_dword(state, OP_LW(reg_addr, reg_dst, off));
}

void mips32_lwc1(struct mips32_state* state, uint_fast8_t fp_dst, int16_t off, uint_fast8_t reg_addr)
{
	CHECK_REG(fp_dst);
	CHECK_REG(reg_addr);
	mips32_dword(state, OP_LWC1(reg_addr, fp_dst, off));
}

void mips32_mfc1(struct mips32_state* state, uint_fast8_t int_dst, uint_fast8_t fp_src)
{
	CHECK_REG(int_dst);
	CHECK_REG(fp_src);
	mips32_dword(state, OP_MFC1(int_dst, fp_src));
}

void mips32_mfhi(struct mips32_state* state, uint_fast8_t reg_dst)
{
	CHECK_REG(reg_dst);
	mips32_dword(state, OP_MFHI(reg_dst));
}

void mips32_mflo(struct mips32_state* state, uint_fast8_t reg_dst)
{
	CHECK_REG(reg_dst);
	mips32_dword(state, OP_MFLO(reg_dst));
}

void mips32_movn(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_src, uint_fast8_t reg_check)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	CHECK_REG(reg_check);
	mips32_dword(state, OP_MOVN(reg_src, reg_check, reg_dst));
}

void mips32_movz(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_src, uint_fast8_t reg_check)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	CHECK_REG(reg_check);
	mips32_dword(state, OP_MOVZ(reg_src, reg_check, reg_dst));
}

void mips32_mtc1(struct mips32_state* state, uint_fast8_t int_src, uint_fast8_t fp_dst)
{
	CHECK_REG(int_src);
	CHECK_REG(fp_dst);
	mips32_dword(state, OP_MTC1(int_src, fp_dst));
}

void mips32_mthi(struct mips32_state* state, uint_fast8_t reg_src)
{
	CHECK_REG(reg_src);
	mips32_dword(state, OP_MTHI(reg_src));
}

void mips32_mtlo(struct mips32_state* state, uint_fast8_t reg_src)
{
	CHECK_REG(reg_src);
	mips32_dword(state, OP_MTLO(reg_src));
}

void mips32_mult(struct mips32_state* state, uint_fast8_t reg_a, uint_fast8_t reg_b)
{
	CHECK_REG(reg_a);
	CHECK_REG(reg_b);
	mips32_dword(state, OP_MULT(reg_a, reg_b));
}

void mips32_multu(struct mips32_state* state, uint_fast8_t reg_a, uint_fast8_t reg_b)
{
	CHECK_REG(reg_a);
	CHECK_REG(reg_b);
	mips32_dword(state, OP_MULTU(reg_a, reg_b));
}

void mips32_nor(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_a, uint_fast8_t reg_b)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_a);
	CHECK_REG(reg_b);
	mips32_dword(state, OP_NOR(reg_a, reg_b, reg_dst));
}

void mips32_or(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_a, uint_fast8_t reg_b)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_a);
	CHECK_REG(reg_b);
	mips32_dword(state, OP_OR(reg_a, reg_b, reg_dst));
}

void mips32_ori(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_src, uint16_t imm)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	mips32_dword(state, OP_ORI(reg_src, reg_dst, imm));
}

void mips32_sb(struct mips32_state* state, uint_fast8_t reg_src, int16_t off, uint_fast8_t reg_addr)
{
	CHECK_REG(reg_src);
	CHECK_REG(reg_addr);
	mips32_dword(state, OP_SB(reg_addr, reg_src, off));
}

void mips32_sdc1(struct mips32_state* state, uint_fast8_t fp_src, int16_t off, uint_fast8_t reg_addr)
{
	CHECK_REG(fp_src);
	CHECK_REG(reg_addr);
	mips32_dword(state, OP_SDC1(reg_addr, fp_src, off));
}

void mips32_sh(struct mips32_state* state, uint_fast8_t reg_src, int16_t off, uint_fast8_t reg_addr)
{
	CHECK_REG(reg_src);
	CHECK_REG(reg_addr);
	mips32_dword(state, OP_SH(reg_addr, reg_src, off));
}

void mips32_sll(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_src, uint_fast8_t amt)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	assert(amt != 0 && amt < 32);
	mips32_dword(state, OP_SLL(reg_src, reg_dst, amt));
}

void mips32_sllv(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_src, uint_fast8_t reg_amt)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	CHECK_REG(reg_amt);
	mips32_dword(state, OP_SLLV(reg_amt, reg_src, reg_dst));
}

void mips32_slt(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_a, uint_fast8_t reg_b)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_a);
	CHECK_REG(reg_b);
	mips32_dword(state, OP_SLT(reg_a, reg_b, reg_dst));
}

void mips32_sltu(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_a, uint_fast8_t reg_b)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_a);
	CHECK_REG(reg_b);
	mips32_dword(state, OP_SLTU(reg_a, reg_b, reg_dst));
}

void mips32_slti(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_src, int16_t imm)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	mips32_dword(state, OP_SLTI(reg_src, reg_dst, imm));
}

void mips32_sltiu(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_src, int16_t imm)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	mips32_dword(state, OP_SLTIU(reg_src, reg_dst, imm));
}

void mips32_sra(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_src, uint_fast8_t amt)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	assert(amt != 0 && amt < 32);
	mips32_dword(state, OP_SRA(reg_src, reg_dst, amt));
}

void mips32_srav(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_src, uint_fast8_t reg_amt)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	CHECK_REG(reg_amt);
	mips32_dword(state, OP_SRAV(reg_amt, reg_src, reg_dst));
}

void mips32_srl(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_src, uint_fast8_t amt)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	assert(amt != 0 && amt < 32);
	mips32_dword(state, OP_SRL(reg_src, reg_dst, amt));
}

void mips32_srlv(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_src, uint_fast8_t reg_amt)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	CHECK_REG(reg_amt);
	mips32_dword(state, OP_SRLV(reg_amt, reg_src, reg_dst));
}

void mips32_subu(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_a, uint_fast8_t reg_b)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_a);
	CHECK_REG(reg_b);
	mips32_dword(state, OP_SUBU(reg_a, reg_b, reg_dst));
}

void mips32_sw(struct mips32_state* state, uint_fast8_t reg_src, int16_t off, uint_fast8_t reg_addr)
{
	CHECK_REG(reg_src);
	CHECK_REG(reg_addr);
	mips32_dword(state, OP_SW(reg_addr, reg_src, off));
}

void mips32_swc1(struct mips32_state* state, uint_fast8_t fp_src, int16_t off, uint_fast8_t reg_addr)
{
	CHECK_REG(fp_src);
	CHECK_REG(reg_addr);
	mips32_dword(state, OP_SWC1(reg_addr, fp_src, off));
}

void mips32_xor(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_a, uint_fast8_t reg_b)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_a);
	CHECK_REG(reg_b);
	mips32_dword(state, OP_XOR(reg_a, reg_b, reg_dst));
}

void mips32_xori(struct mips32_state* state, uint_fast8_t reg_dst, uint_fast8_t reg_src, uint16_t imm)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	mips32_dword(state, OP_XORI(reg_src, reg_dst, imm));
}
