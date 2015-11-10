/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - mips-interp.h                                           *
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

#ifndef M64P_TRACE_JIT_MIPS_INTERP_H
#define M64P_TRACE_JIT_MIPS_INTERP_H

#include "../recomp.h"
#include <stdbool.h>
#include <stdint.h>

/* The Program Counter of the Trace JIT. Only its 'addr' member is meaningful,
 * but it's a 'struct precomp_instr' due to other code updating 'PC->addr'. */
extern precomp_instr TJ_PC;

/* A trace function that interprets the opcode at TJ_PC.addr whenever it is
 * run. If it has a delay slot, the delay slot is also interpreted. */
extern void TJFallback(void);

extern bool tj_jump_to(uint32_t addr);

/* Initialises the Trace JIT. Calls the architecture-specific initialisation
 * function. */
extern void trace_jit_init(void);

/* The main loop of the Trace JIT. */
extern void trace_jit(void);

/* Interface for invalidate_r4300_cached_code */
extern void invalidate_cached_code_trace_jit(uint32_t start, size_t len);

/* Exits the Trace JIT. Calls the architecture-specific exit function. */
extern void trace_jit_exit(void);

extern void TJ_NI(uint32_t op); /* writes 'stop' */
extern void TJ_RESERVED(uint32_t op); /* writes 'stop' */
extern void TJ_J(uint32_t op); /* reads and may modify PC; executes its delay slot */
extern void TJ_J_IDLE(uint32_t op); /* reads and may modify PC */
extern void TJ_JAL(uint32_t op); /* reads and may modify PC; executes its delay slot */
extern void TJ_JAL_IDLE(uint32_t op); /* reads and may modify PC */
extern void TJ_BEQ(uint32_t op); /* reads and may modify PC; executes its delay slot */
extern void TJ_BEQ_IDLE(uint32_t op); /* reads and may modify PC */
extern void TJ_BNE(uint32_t op); /* reads and may modify PC; executes its delay slot */
extern void TJ_BNE_IDLE(uint32_t op); /* reads and may modify PC */
extern void TJ_BLEZ(uint32_t op); /* reads and may modify PC; executes its delay slot */
extern void TJ_BLEZ_IDLE(uint32_t op); /* reads and may modify PC */
extern void TJ_BGTZ(uint32_t op); /* reads and may modify PC; executes its delay slot */
extern void TJ_BGTZ_IDLE(uint32_t op); /* reads and may modify PC */
extern void TJ_ADDI(uint32_t op);
extern void TJ_ADDIU(uint32_t op);
extern void TJ_SLTI(uint32_t op);
extern void TJ_SLTIU(uint32_t op);
extern void TJ_ANDI(uint32_t op);
extern void TJ_ORI(uint32_t op);
extern void TJ_XORI(uint32_t op);
extern void TJ_LUI(uint32_t op);
extern void TJ_BEQL(uint32_t op); /* reads and may modify PC; executes its delay slot */
extern void TJ_BEQL_IDLE(uint32_t op); /* reads and may modify PC */
extern void TJ_BNEL(uint32_t op); /* reads and may modify PC; executes its delay slot */
extern void TJ_BNEL_IDLE(uint32_t op); /* reads and may modify PC */
extern void TJ_BLEZL(uint32_t op); /* reads and may modify PC; executes its delay slot */
extern void TJ_BLEZL_IDLE(uint32_t op); /* reads and may modify PC */
extern void TJ_BGTZL(uint32_t op); /* reads and may modify PC; executes its delay slot */
extern void TJ_BGTZL_IDLE(uint32_t op); /* reads and may modify PC */
extern void TJ_DADDI(uint32_t op);
extern void TJ_DADDIU(uint32_t op);
extern void TJ_LDL(uint32_t op); /* may raise exceptions */
extern void TJ_LDR(uint32_t op); /* may raise exceptions */
extern void TJ_LB(uint32_t op); /* may raise exceptions */
extern void TJ_LH(uint32_t op); /* may raise exceptions */
extern void TJ_LWL(uint32_t op); /* may raise exceptions */
extern void TJ_LW(uint32_t op); /* may raise exceptions */
extern void TJ_LBU(uint32_t op); /* may raise exceptions */
extern void TJ_LHU(uint32_t op); /* may raise exceptions */
extern void TJ_LWR(uint32_t op); /* may raise exceptions */
extern void TJ_LWU(uint32_t op); /* may raise exceptions */
extern void TJ_SB(uint32_t op); /* may raise exceptions */
extern void TJ_SH(uint32_t op); /* may raise exceptions */
extern void TJ_SWL(uint32_t op); /* may raise exceptions */
extern void TJ_SW(uint32_t op); /* may raise exceptions */
extern void TJ_SDL(uint32_t op); /* may raise exceptions */
extern void TJ_SDR(uint32_t op); /* may raise exceptions */
extern void TJ_SWR(uint32_t op); /* may raise exceptions */
extern void TJ_CACHE(uint32_t op);
extern void TJ_LL(uint32_t op); /* may raise exceptions */
extern void TJ_LWC1(uint32_t op); /* may raise exceptions */
extern void TJ_LDC1(uint32_t op); /* may raise exceptions */
extern void TJ_LD(uint32_t op); /* may raise exceptions */
extern void TJ_SC(uint32_t op); /* may raise exceptions */
extern void TJ_SWC1(uint32_t op); /* may raise exceptions */
extern void TJ_SDC1(uint32_t op); /* may raise exceptions */
extern void TJ_SD(uint32_t op); /* may raise exceptions */
extern void TJ_MFC0(uint32_t op); /* may write 'stop'; may read PC */
extern void TJ_MTC0(uint32_t op); /* may write 'stop'; may raise exceptions; may read and modify PC */
extern void TJ_BC1F(uint32_t op); /* reads and may modify PC; executes its delay slot */
extern void TJ_BC1F_IDLE(uint32_t op); /* reads and may modify PC */
extern void TJ_BC1T(uint32_t op); /* reads and may modify PC; executes its delay slot */
extern void TJ_BC1T_IDLE(uint32_t op); /* reads and may modify PC */
extern void TJ_BC1FL(uint32_t op); /* reads and may modify PC; may execute its delay slot */
extern void TJ_BC1FL_IDLE(uint32_t op); /* reads and may modify PC */
extern void TJ_BC1TL(uint32_t op); /* reads and may modify PC; may execute its delay slot */
extern void TJ_BC1TL_IDLE(uint32_t op); /* reads and may modify PC */
extern void TJ_MFC1(uint32_t op); /* may raise exceptions */
extern void TJ_DMFC1(uint32_t op); /* may raise exceptions */
extern void TJ_CFC1(uint32_t op); /* may raise exceptions */
extern void TJ_MTC1(uint32_t op); /* may raise exceptions */
extern void TJ_DMTC1(uint32_t op); /* may raise exceptions */
extern void TJ_CTC1(uint32_t op); /* may raise exceptions */
extern void TJ_ADD_D(uint32_t op); /* may raise exceptions */
extern void TJ_SUB_D(uint32_t op); /* may raise exceptions */
extern void TJ_MUL_D(uint32_t op); /* may raise exceptions */
extern void TJ_DIV_D(uint32_t op); /* may raise exceptions */
extern void TJ_SQRT_D(uint32_t op); /* may raise exceptions */
extern void TJ_ABS_D(uint32_t op); /* may raise exceptions */
extern void TJ_MOV_D(uint32_t op); /* may raise exceptions */
extern void TJ_NEG_D(uint32_t op); /* may raise exceptions */
extern void TJ_ROUND_L_D(uint32_t op); /* may raise exceptions */
extern void TJ_TRUNC_L_D(uint32_t op); /* may raise exceptions */
extern void TJ_CEIL_L_D(uint32_t op); /* may raise exceptions */
extern void TJ_FLOOR_L_D(uint32_t op); /* may raise exceptions */
extern void TJ_ROUND_W_D(uint32_t op); /* may raise exceptions */
extern void TJ_TRUNC_W_D(uint32_t op); /* may raise exceptions */
extern void TJ_CEIL_W_D(uint32_t op); /* may raise exceptions */
extern void TJ_FLOOR_W_D(uint32_t op); /* may raise exceptions */
extern void TJ_CVT_S_D(uint32_t op); /* may raise exceptions */
extern void TJ_CVT_W_D(uint32_t op); /* may raise exceptions */
extern void TJ_CVT_L_D(uint32_t op); /* may raise exceptions */
extern void TJ_C_F_D(uint32_t op); /* may raise exceptions */
extern void TJ_C_UN_D(uint32_t op); /* may raise exceptions */
extern void TJ_C_EQ_D(uint32_t op); /* may raise exceptions */
extern void TJ_C_UEQ_D(uint32_t op); /* may raise exceptions */
extern void TJ_C_OLT_D(uint32_t op); /* may raise exceptions */
extern void TJ_C_ULT_D(uint32_t op); /* may raise exceptions */
extern void TJ_C_OLE_D(uint32_t op); /* may raise exceptions */
extern void TJ_C_ULE_D(uint32_t op); /* may raise exceptions */
extern void TJ_C_SF_D(uint32_t op); /* may raise exceptions */
extern void TJ_C_NGLE_D(uint32_t op); /* may raise exceptions */
extern void TJ_C_SEQ_D(uint32_t op); /* may raise exceptions */
extern void TJ_C_NGL_D(uint32_t op); /* may raise exceptions */
extern void TJ_C_LT_D(uint32_t op); /* may raise exceptions */
extern void TJ_C_NGE_D(uint32_t op); /* may raise exceptions */
extern void TJ_C_LE_D(uint32_t op); /* may raise exceptions */
extern void TJ_C_NGT_D(uint32_t op); /* may raise exceptions */
extern void TJ_CVT_S_L(uint32_t op); /* may raise exceptions */
extern void TJ_CVT_D_L(uint32_t op); /* may raise exceptions */
extern void TJ_ADD_S(uint32_t op); /* may raise exceptions */
extern void TJ_SUB_S(uint32_t op); /* may raise exceptions */
extern void TJ_MUL_S(uint32_t op); /* may raise exceptions */
extern void TJ_DIV_S(uint32_t op); /* may raise exceptions */
extern void TJ_SQRT_S(uint32_t op); /* may raise exceptions */
extern void TJ_ABS_S(uint32_t op); /* may raise exceptions */
extern void TJ_MOV_S(uint32_t op); /* may raise exceptions */
extern void TJ_NEG_S(uint32_t op); /* may raise exceptions */
extern void TJ_ROUND_L_S(uint32_t op); /* may raise exceptions */
extern void TJ_TRUNC_L_S(uint32_t op); /* may raise exceptions */
extern void TJ_CEIL_L_S(uint32_t op); /* may raise exceptions */
extern void TJ_FLOOR_L_S(uint32_t op); /* may raise exceptions */
extern void TJ_ROUND_W_S(uint32_t op); /* may raise exceptions */
extern void TJ_TRUNC_W_S(uint32_t op); /* may raise exceptions */
extern void TJ_CEIL_W_S(uint32_t op); /* may raise exceptions */
extern void TJ_FLOOR_W_S(uint32_t op); /* may raise exceptions */
extern void TJ_CVT_D_S(uint32_t op); /* may raise exceptions */
extern void TJ_CVT_W_S(uint32_t op); /* may raise exceptions */
extern void TJ_CVT_L_S(uint32_t op); /* may raise exceptions */
extern void TJ_C_F_S(uint32_t op); /* may raise exceptions */
extern void TJ_C_UN_S(uint32_t op); /* may raise exceptions */
extern void TJ_C_EQ_S(uint32_t op); /* may raise exceptions */
extern void TJ_C_UEQ_S(uint32_t op); /* may raise exceptions */
extern void TJ_C_OLT_S(uint32_t op); /* may raise exceptions */
extern void TJ_C_ULT_S(uint32_t op); /* may raise exceptions */
extern void TJ_C_OLE_S(uint32_t op); /* may raise exceptions */
extern void TJ_C_ULE_S(uint32_t op); /* may raise exceptions */
extern void TJ_C_SF_S(uint32_t op); /* may raise exceptions */
extern void TJ_C_NGLE_S(uint32_t op); /* may raise exceptions */
extern void TJ_C_SEQ_S(uint32_t op); /* may raise exceptions */
extern void TJ_C_NGL_S(uint32_t op); /* may raise exceptions */
extern void TJ_C_LT_S(uint32_t op); /* may raise exceptions */
extern void TJ_C_NGE_S(uint32_t op); /* may raise exceptions */
extern void TJ_C_LE_S(uint32_t op); /* may raise exceptions */
extern void TJ_C_NGT_S(uint32_t op); /* may raise exceptions */
extern void TJ_CVT_S_W(uint32_t op); /* may raise exceptions */
extern void TJ_CVT_D_W(uint32_t op); /* may raise exceptions */
extern void TJ_BLTZ(uint32_t op); /* reads and may modify PC; executes its delay slot */
extern void TJ_BLTZ_IDLE(uint32_t op); /* reads and may modify PC */
extern void TJ_BGEZ(uint32_t op); /* reads and may modify PC; executes its delay slot */
extern void TJ_BGEZ_IDLE(uint32_t op); /* reads and may modify PC */
extern void TJ_BLTZL(uint32_t op); /* reads and may modify PC; may execute its delay slot */
extern void TJ_BLTZL_IDLE(uint32_t op); /* reads and may modify PC */
extern void TJ_BGEZL(uint32_t op); /* reads and may modify PC; may execute its delay slot */
extern void TJ_BGEZL_IDLE(uint32_t op); /* reads and may modify PC */
extern void TJ_BLTZAL(uint32_t op); /* reads and may modify PC; executes its delay slot */
extern void TJ_BLTZAL_IDLE(uint32_t op); /* reads and may modify PC */
extern void TJ_BGEZAL(uint32_t op); /* reads and may modify PC; executes its delay slot */
extern void TJ_BGEZAL_IDLE(uint32_t op); /* reads and may modify PC */
extern void TJ_BLTZALL(uint32_t op); /* reads and may modify PC; may execute its delay slot */
extern void TJ_BLTZALL_IDLE(uint32_t op); /* reads and may modify PC */
extern void TJ_BGEZALL(uint32_t op); /* reads and may modify PC; may execute its delay slot */
extern void TJ_BGEZALL_IDLE(uint32_t op); /* reads and may modify PC */
extern void TJ_NOP(uint32_t op);
extern void TJ_SLL(uint32_t op);
extern void TJ_SRL(uint32_t op);
extern void TJ_SRA(uint32_t op);
extern void TJ_SLLV(uint32_t op);
extern void TJ_SRLV(uint32_t op);
extern void TJ_SRAV(uint32_t op);
extern void TJ_JR(uint32_t op); /* modifies PC; executes its delay slot */
extern void TJ_JALR(uint32_t op); /* reads and modifies PC; executes its delay slot */
extern void TJ_SYSCALL(uint32_t op); /* reads PC; raises exceptions */
extern void TJ_SYNC(uint32_t op);
extern void TJ_MFHI(uint32_t op);
extern void TJ_MTHI(uint32_t op);
extern void TJ_MFLO(uint32_t op);
extern void TJ_MTLO(uint32_t op);
extern void TJ_DSLLV(uint32_t op);
extern void TJ_DSRLV(uint32_t op);
extern void TJ_DSRAV(uint32_t op);
extern void TJ_MULT(uint32_t op);
extern void TJ_MULTU(uint32_t op);
extern void TJ_DIV(uint32_t op);
extern void TJ_DIVU(uint32_t op);
extern void TJ_DMULT(uint32_t op);
extern void TJ_DMULTU(uint32_t op);
extern void TJ_DDIV(uint32_t op);
extern void TJ_DDIVU(uint32_t op);
extern void TJ_ADD(uint32_t op);
extern void TJ_ADDU(uint32_t op);
extern void TJ_SUB(uint32_t op);
extern void TJ_SUBU(uint32_t op);
extern void TJ_AND(uint32_t op);
extern void TJ_OR(uint32_t op);
extern void TJ_XOR(uint32_t op);
extern void TJ_NOR(uint32_t op);
extern void TJ_SLT(uint32_t op);
extern void TJ_SLTU(uint32_t op);
extern void TJ_DADD(uint32_t op);
extern void TJ_DADDU(uint32_t op);
extern void TJ_DSUB(uint32_t op);
extern void TJ_DSUBU(uint32_t op);
extern void TJ_TEQ(uint32_t op); /* may write 'stop' */
extern void TJ_DSLL(uint32_t op);
extern void TJ_DSRL(uint32_t op);
extern void TJ_DSRA(uint32_t op);
extern void TJ_DSLL32(uint32_t op);
extern void TJ_DSRL32(uint32_t op);
extern void TJ_DSRA32(uint32_t op);
extern void TJ_TLBR(uint32_t op);
extern void TJ_TLBWI(uint32_t op);
extern void TJ_TLBWR(uint32_t op);
extern void TJ_TLBP(uint32_t op);
extern void TJ_ERET(uint32_t op); /* may write 'stop'; reads and modifies PC */

#endif /* !M64P_TRACE_JIT_MIPS_INTERP_H */
