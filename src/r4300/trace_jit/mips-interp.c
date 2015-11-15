/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - mips-interp.c                                           *
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

#include <string.h>
#include <stdint.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "api/m64p_types.h"
#include "api/callbacks.h"
#include "api/debugger.h"
#include "memory/memory.h"
#include "main/main.h"
#include "osal/preproc.h"

/* TLBWrite requires invalid_code and blocks from cached_interp.h; however,
 * the Trace JIT's interpreter fallback uses neither. */
#include "../cached_interp.h"
#include "../r4300.h"
#include "../cp0_private.h"
#include "../cp1_private.h"
#include "../exception.h"
#include "../interupt.h"
#include "../tlb.h"

#include "mips-interp.h"
#include "mips-jit.h"
#include "mips-parse.h"
#include "native-config.h"
#include "native-tracecache.h"

#ifdef DBG
#include "../debugger/dbg_types.h"
#include "../debugger/debugger.h"
#endif

precomp_instr TJ_PC;

#define PCADDR TJ_PC.addr
#define ADD_TO_PC(x) TJ_PC.addr += (x) * 4
#define DECLARE_INSTRUCTION(name) void TJ_##name(uint32_t op)
#define DECLARE_JUMP(name, destination, condition, link, likely, cop1) \
	void TJ_##name(uint32_t op) \
	{ \
		const int take_jump = (condition); \
		const uint32_t jump_target = (destination); \
		int64_t *link_register = (link); \
		if (cop1 && check_cop1_unusable()) return; \
		if (link_register != &g_state.regs.gpr[0]) \
		{ \
			*link_register = SE32(TJ_PC.addr + 8); \
		} \
		if (!likely || take_jump) \
		{ \
			TJ_PC.addr += 4; \
			g_state.delay_slot = 1; \
			TJFallback(); \
			if (skip_jump) \
			{ \
				skip_jump = 0; \
				return; \
			} \
			cp0_update_count(); \
			g_state.delay_slot = 0; \
			if (take_jump && !skip_jump) \
			{ \
				tj_jump_to(jump_target); \
			} \
			else \
			{ \
				tj_jump_to(TJ_PC.addr); /* revalidate code where we end up */ \
			} \
		} \
		else \
		{ \
			tj_jump_to(TJ_PC.addr + 8); \
			cp0_update_count(); \
		} \
		last_addr = TJ_PC.addr; \
		if (g_state.next_interrupt <= g_state.regs.cp0[CP0_COUNT_REG]) gen_interupt(); \
	} \
	void TJ_##name##_IDLE(uint32_t op) \
	{ \
		const int take_jump = (condition); \
		int skip; \
		if (cop1 && check_cop1_unusable()) return; \
		if (take_jump) \
		{ \
			cp0_update_count(); \
			skip = g_state.next_interrupt - g_state.regs.cp0[CP0_COUNT_REG]; \
			if (skip > 3) g_state.regs.cp0[CP0_COUNT_REG] += (skip & 0xFFFFFFFC); \
			else TJ_##name(op); \
		} \
		else TJ_##name(op); \
	}

#if defined(ARCH_EMIT_TRACE)
#  define CHECK_MEMORY() \
	do { \
		if (GetTraceAt(g_state.access_addr) != NOT_CODE) \
			invalid_code[g_state.access_addr >> 12] = 1; \
	} while (0)
#else
#  define CHECK_MEMORY()
#endif

#define SE8(a) ((int64_t) ((int8_t) (a)))
#define SE16(a) ((int64_t) ((int16_t) (a)))
#define SE32(a) ((int64_t) ((int32_t) (a)))

/* These macros are like those in macros.h, but they parse opcode fields. */
#define rrt g_state.regs.gpr[RT_OF(op)]
#define rrd g_state.regs.gpr[RD_OF(op)]
#define rfs FS_OF(op)
#define rrs g_state.regs.gpr[RS_OF(op)]
#define rsa SA_OF(op)
#define irt g_state.regs.gpr[RT_OF(op)]
#define ioffset IMM16S_OF(op)
#define iimmediate IMM16S_OF(op)
#define irs g_state.regs.gpr[RS_OF(op)]
#define ibase g_state.regs.gpr[RS_OF(op)]
#define jinst_index JUMP_OF(op)
#define lfbase RS_OF(op)
#define lfft FT_OF(op)
#define lfoffset IMM16S_OF(op)
#define cfft FT_OF(op)
#define cffs FS_OF(op)
#define cffd FD_OF(op)

// 32 bits macros
#ifndef M64P_BIG_ENDIAN
#define rrt32 *((int32_t*) &g_state.regs.gpr[RT_OF(op)])
#define rrd32 *((int32_t*) &g_state.regs.gpr[RD_OF(op)])
#define rrs32 *((int32_t*) &g_state.regs.gpr[RS_OF(op)])
#define irs32 *((int32_t*) &g_state.regs.gpr[RS_OF(op)])
#define irt32 *((int32_t*) &g_state.regs.gpr[RT_OF(op)])
#else
#define rrt32 *((int32_t*) &g_state.regs.gpr[RT_OF(op)] + 1)
#define rrd32 *((int32_t*) &g_state.regs.gpr[RD_OF(op)] + 1)
#define rrs32 *((int32_t*) &g_state.regs.gpr[RS_OF(op)] + 1)
#define irs32 *((int32_t*) &g_state.regs.gpr[RS_OF(op)] + 1)
#define irt32 *((int32_t*) &g_state.regs.gpr[RT_OF(op)] + 1)
#endif

/* Determines whether a relative jump in a 16-bit immediate goes back to the
 * same instruction without doing any work in its delay slot. The jump is
 * relative to the instruction in the delay slot, so 1 instruction backwards
 * (-1) goes back to the jump. */
#define IS_RELATIVE_IDLE_LOOP(op, addr) \
	(IMM16S_OF(op) == -1 && *fast_mem_access((addr) + 4) == 0)

/* Determines whether an absolute jump in a 26-bit immediate goes back to the
 * same instruction without doing any work in its delay slot. The jump is
 * in the same 256 MiB segment as the delay slot, so if the jump instruction
 * is at the last address in its segment, it does not jump back to itself. */
#define IS_ABSOLUTE_IDLE_LOOP(op, addr) \
	(JUMP_OF(op) == ((addr) & UINT32_C(0x0FFFFFFF)) >> 2 \
	 && ((addr) & UINT32_C(0x0FFFFFFF)) != UINT32_C(0x0FFFFFFC) \
	 && *fast_mem_access((addr) + 4) == 0)

#include "../interpreter.def"

void TJFallback()
{
	uint32_t op = *fast_mem_access(TJ_PC.addr);
	switch ((op >> 26) & 0x3F) {
	case 0: /* SPECIAL prefix */
		switch (op & 0x3F) {
		case 0: /* SPECIAL opcode 0: SLL */
			if (RD_OF(op) != 0) TJ_SLL(op);
			else                TJ_NOP(0);
			break;
		case 2: /* SPECIAL opcode 2: SRL */
			if (RD_OF(op) != 0) TJ_SRL(op);
			else                TJ_NOP(0);
			break;
		case 3: /* SPECIAL opcode 3: SRA */
			if (RD_OF(op) != 0) TJ_SRA(op);
			else                TJ_NOP(0);
			break;
		case 4: /* SPECIAL opcode 4: SLLV */
			if (RD_OF(op) != 0) TJ_SLLV(op);
			else                TJ_NOP(0);
			break;
		case 6: /* SPECIAL opcode 6: SRLV */
			if (RD_OF(op) != 0) TJ_SRLV(op);
			else                TJ_NOP(0);
			break;
		case 7: /* SPECIAL opcode 7: SRAV */
			if (RD_OF(op) != 0) TJ_SRAV(op);
			else                TJ_NOP(0);
			break;
		case 8: TJ_JR(op); break;
		case 9: /* SPECIAL opcode 9: JALR */
			/* Note: This can omit the check for Rd == 0 because the JALR
			 * function checks for link_register != &reg[0]. If you're
			 * using this as a reference for a JIT, do check Rd == 0 in it. */
			TJ_JALR(op);
			break;
		case 12: TJ_SYSCALL(op); break;
		case 13: /* SPECIAL opcode 13: BREAK (Not implemented) */
			TJ_NI(op);
			break;
		case 15: TJ_SYNC(op); break;
		case 16: /* SPECIAL opcode 16: MFHI */
			if (RD_OF(op) != 0) TJ_MFHI(op);
			else                TJ_NOP(0);
			break;
		case 17: TJ_MTHI(op); break;
		case 18: /* SPECIAL opcode 18: MFLO */
			if (RD_OF(op) != 0) TJ_MFLO(op);
			else                TJ_NOP(0);
			break;
		case 19: TJ_MTLO(op); break;
		case 20: /* SPECIAL opcode 20: DSLLV */
			if (RD_OF(op) != 0) TJ_DSLLV(op);
			else                TJ_NOP(0);
			break;
		case 22: /* SPECIAL opcode 22: DSRLV */
			if (RD_OF(op) != 0) TJ_DSRLV(op);
			else                TJ_NOP(0);
			break;
		case 23: /* SPECIAL opcode 23: DSRAV */
			if (RD_OF(op) != 0) TJ_DSRAV(op);
			else                TJ_NOP(0);
			break;
		case 24: TJ_MULT(op); break;
		case 25: TJ_MULTU(op); break;
		case 26: TJ_DIV(op); break;
		case 27: TJ_DIVU(op); break;
		case 28: TJ_DMULT(op); break;
		case 29: TJ_DMULTU(op); break;
		case 30: TJ_DDIV(op); break;
		case 31: TJ_DDIVU(op); break;
		case 32: /* SPECIAL opcode 32: ADD */
			if (RD_OF(op) != 0) TJ_ADD(op);
			else                TJ_NOP(0);
			break;
		case 33: /* SPECIAL opcode 33: ADDU */
			if (RD_OF(op) != 0) TJ_ADDU(op);
			else                TJ_NOP(0);
			break;
		case 34: /* SPECIAL opcode 34: SUB */
			if (RD_OF(op) != 0) TJ_SUB(op);
			else                TJ_NOP(0);
			break;
		case 35: /* SPECIAL opcode 35: SUBU */
			if (RD_OF(op) != 0) TJ_SUBU(op);
			else                TJ_NOP(0);
			break;
		case 36: /* SPECIAL opcode 36: AND */
			if (RD_OF(op) != 0) TJ_AND(op);
			else                TJ_NOP(0);
			break;
		case 37: /* SPECIAL opcode 37: OR */
			if (RD_OF(op) != 0) TJ_OR(op);
			else                TJ_NOP(0);
			break;
		case 38: /* SPECIAL opcode 38: XOR */
			if (RD_OF(op) != 0) TJ_XOR(op);
			else                TJ_NOP(0);
			break;
		case 39: /* SPECIAL opcode 39: NOR */
			if (RD_OF(op) != 0) TJ_NOR(op);
			else                TJ_NOP(0);
			break;
		case 42: /* SPECIAL opcode 42: SLT */
			if (RD_OF(op) != 0) TJ_SLT(op);
			else                TJ_NOP(0);
			break;
		case 43: /* SPECIAL opcode 43: SLTU */
			if (RD_OF(op) != 0) TJ_SLTU(op);
			else                TJ_NOP(0);
			break;
		case 44: /* SPECIAL opcode 44: DADD */
			if (RD_OF(op) != 0) TJ_DADD(op);
			else                TJ_NOP(0);
			break;
		case 45: /* SPECIAL opcode 45: DADDU */
			if (RD_OF(op) != 0) TJ_DADDU(op);
			else                TJ_NOP(0);
			break;
		case 46: /* SPECIAL opcode 46: DSUB */
			if (RD_OF(op) != 0) TJ_DSUB(op);
			else                TJ_NOP(0);
			break;
		case 47: /* SPECIAL opcode 47: DSUBU */
			if (RD_OF(op) != 0) TJ_DSUBU(op);
			else                TJ_NOP(0);
			break;
		case 48: /* SPECIAL opcode 48: TGE (Not implemented) */
		case 49: /* SPECIAL opcode 49: TGEU (Not implemented) */
		case 50: /* SPECIAL opcode 50: TLT (Not implemented) */
		case 51: /* SPECIAL opcode 51: TLTU (Not implemented) */
			TJ_NI(op);
			break;
		case 52: TJ_TEQ(op); break;
		case 54: /* SPECIAL opcode 54: TNE (Not implemented) */
			TJ_NI(op);
			break;
		case 56: /* SPECIAL opcode 56: DSLL */
			if (RD_OF(op) != 0) TJ_DSLL(op);
			else                TJ_NOP(0);
			break;
		case 58: /* SPECIAL opcode 58: DSRL */
			if (RD_OF(op) != 0) TJ_DSRL(op);
			else                TJ_NOP(0);
			break;
		case 59: /* SPECIAL opcode 59: DSRA */
			if (RD_OF(op) != 0) TJ_DSRA(op);
			else                TJ_NOP(0);
			break;
		case 60: /* SPECIAL opcode 60: DSLL32 */
			if (RD_OF(op) != 0) TJ_DSLL32(op);
			else                TJ_NOP(0);
			break;
		case 62: /* SPECIAL opcode 62: DSRL32 */
			if (RD_OF(op) != 0) TJ_DSRL32(op);
			else                TJ_NOP(0);
			break;
		case 63: /* SPECIAL opcode 63: DSRA32 */
			if (RD_OF(op) != 0) TJ_DSRA32(op);
			else                TJ_NOP(0);
			break;
		default: /* SPECIAL opcodes 1, 5, 10, 11, 14, 21, 40, 41, 53, 55, 57,
		            61: Reserved Instructions */
			TJ_RESERVED(op);
			break;
		} /* switch (op & 0x3F) for the SPECIAL prefix */
		break;
	case 1: /* REGIMM prefix */
		switch ((op >> 16) & 0x1F) {
		case 0: /* REGIMM opcode 0: BLTZ */
			if (IS_RELATIVE_IDLE_LOOP(op, TJ_PC.addr)) TJ_BLTZ_IDLE(op);
			else                                       TJ_BLTZ(op);
			break;
		case 1: /* REGIMM opcode 1: BGEZ */
			if (IS_RELATIVE_IDLE_LOOP(op, TJ_PC.addr)) TJ_BGEZ_IDLE(op);
			else                                       TJ_BGEZ(op);
			break;
		case 2: /* REGIMM opcode 2: BLTZL */
			if (IS_RELATIVE_IDLE_LOOP(op, TJ_PC.addr)) TJ_BLTZL_IDLE(op);
			else                                       TJ_BLTZL(op);
			break;
		case 3: /* REGIMM opcode 3: BGEZL */
			if (IS_RELATIVE_IDLE_LOOP(op, TJ_PC.addr)) TJ_BGEZL_IDLE(op);
			else                                       TJ_BGEZL(op);
			break;
		case 8: /* REGIMM opcode 8: TGEI (Not implemented) */
		case 9: /* REGIMM opcode 9: TGEIU (Not implemented) */
		case 10: /* REGIMM opcode 10: TLTI (Not implemented) */
		case 11: /* REGIMM opcode 11: TLTIU (Not implemented) */
		case 12: /* REGIMM opcode 12: TEQI (Not implemented) */
		case 14: /* REGIMM opcode 14: TNEI (Not implemented) */
			TJ_NI(op);
			break;
		case 16: /* REGIMM opcode 16: BLTZAL */
			if (IS_RELATIVE_IDLE_LOOP(op, TJ_PC.addr)) TJ_BLTZAL_IDLE(op);
			else                                       TJ_BLTZAL(op);
			break;
		case 17: /* REGIMM opcode 17: BGEZAL */
			if (IS_RELATIVE_IDLE_LOOP(op, TJ_PC.addr)) TJ_BGEZAL_IDLE(op);
			else                                       TJ_BGEZAL(op);
			break;
		case 18: /* REGIMM opcode 18: BLTZALL */
			if (IS_RELATIVE_IDLE_LOOP(op, TJ_PC.addr)) TJ_BLTZALL_IDLE(op);
			else                                       TJ_BLTZALL(op);
			break;
		case 19: /* REGIMM opcode 19: BGEZALL */
			if (IS_RELATIVE_IDLE_LOOP(op, TJ_PC.addr)) TJ_BGEZALL_IDLE(op);
			else                                       TJ_BGEZALL(op);
			break;
		default: /* REGIMM opcodes 4..7, 13, 15, 20..31:
		            Reserved Instructions */
			TJ_RESERVED(op);
			break;
		} /* switch ((op >> 16) & 0x1F) for the REGIMM prefix */
		break;
	case 2: /* Major opcode 2: J */
		if (IS_ABSOLUTE_IDLE_LOOP(op, TJ_PC.addr)) TJ_J_IDLE(op);
		else                                       TJ_J(op);
		break;
	case 3: /* Major opcode 3: JAL */
		if (IS_ABSOLUTE_IDLE_LOOP(op, TJ_PC.addr)) TJ_JAL_IDLE(op);
		else                                       TJ_JAL(op);
		break;
	case 4: /* Major opcode 4: BEQ */
		if (IS_RELATIVE_IDLE_LOOP(op, TJ_PC.addr)) TJ_BEQ_IDLE(op);
		else                                       TJ_BEQ(op);
		break;
	case 5: /* Major opcode 5: BNE */
		if (IS_RELATIVE_IDLE_LOOP(op, TJ_PC.addr)) TJ_BNE_IDLE(op);
		else                                       TJ_BNE(op);
		break;
	case 6: /* Major opcode 6: BLEZ */
		if (IS_RELATIVE_IDLE_LOOP(op, TJ_PC.addr)) TJ_BLEZ_IDLE(op);
		else                                       TJ_BLEZ(op);
		break;
	case 7: /* Major opcode 7: BGTZ */
		if (IS_RELATIVE_IDLE_LOOP(op, TJ_PC.addr)) TJ_BGTZ_IDLE(op);
		else                                       TJ_BGTZ(op);
		break;
	case 8: /* Major opcode 8: ADDI */
		if (RT_OF(op) != 0) TJ_ADDI(op);
		else                TJ_NOP(0);
		break;
	case 9: /* Major opcode 9: ADDIU */
		if (RT_OF(op) != 0) TJ_ADDIU(op);
		else                TJ_NOP(0);
		break;
	case 10: /* Major opcode 10: SLTI */
		if (RT_OF(op) != 0) TJ_SLTI(op);
		else                TJ_NOP(0);
		break;
	case 11: /* Major opcode 11: SLTIU */
		if (RT_OF(op) != 0) TJ_SLTIU(op);
		else                TJ_NOP(0);
		break;
	case 12: /* Major opcode 12: ANDI */
		if (RT_OF(op) != 0) TJ_ANDI(op);
		else                TJ_NOP(0);
		break;
	case 13: /* Major opcode 13: ORI */
		if (RT_OF(op) != 0) TJ_ORI(op);
		else                TJ_NOP(0);
		break;
	case 14: /* Major opcode 14: XORI */
		if (RT_OF(op) != 0) TJ_XORI(op);
		else                TJ_NOP(0);
		break;
	case 15: /* Major opcode 15: LUI */
		if (RT_OF(op) != 0) TJ_LUI(op);
		else                TJ_NOP(0);
		break;
	case 16: /* Coprocessor 0 prefix */
		switch ((op >> 21) & 0x1F) {
		case 0: /* Coprocessor 0 opcode 0: MFC0 */
			if (RT_OF(op) != 0) TJ_MFC0(op);
			else                TJ_NOP(0);
			break;
		case 4: TJ_MTC0(op); break;
		case 16: /* Coprocessor 0 opcode 16: TLB */
			switch (op & 0x3F) {
			case 1: TJ_TLBR(op); break;
			case 2: TJ_TLBWI(op); break;
			case 6: TJ_TLBWR(op); break;
			case 8: TJ_TLBP(op); break;
			case 24: TJ_ERET(op); break;
			default: /* TLB sub-opcodes 0, 3..5, 7, 9..23, 25..63:
			            Reserved Instructions */
				TJ_RESERVED(op);
				break;
			} /* switch (op & 0x3F) for Coprocessor 0 TLB opcodes */
			break;
		default: /* Coprocessor 0 opcodes 1..3, 4..15, 17..31:
		            Reserved Instructions */
			TJ_RESERVED(op);
			break;
		} /* switch ((op >> 21) & 0x1F) for the Coprocessor 0 prefix */
		break;
	case 17: /* Coprocessor 1 prefix */
		switch ((op >> 21) & 0x1F) {
		case 0: /* Coprocessor 1 opcode 0: MFC1 */
			if (RT_OF(op) != 0) TJ_MFC1(op);
			else                TJ_NOP(0);
			break;
		case 1: /* Coprocessor 1 opcode 1: DMFC1 */
			if (RT_OF(op) != 0) TJ_DMFC1(op);
			else                TJ_NOP(0);
			break;
		case 2: /* Coprocessor 1 opcode 2: CFC1 */
			if (RT_OF(op) != 0) TJ_CFC1(op);
			else                TJ_NOP(0);
			break;
		case 4: TJ_MTC1(op); break;
		case 5: TJ_DMTC1(op); break;
		case 6: TJ_CTC1(op); break;
		case 8: /* Coprocessor 1 opcode 8: Branch on C1 condition... */
			switch ((op >> 16) & 0x3) {
			case 0: /* opcode 0: BC1F */
				if (IS_RELATIVE_IDLE_LOOP(op, TJ_PC.addr)) TJ_BC1F_IDLE(op);
				else                                       TJ_BC1F(op);
				break;
			case 1: /* opcode 1: BC1T */
				if (IS_RELATIVE_IDLE_LOOP(op, TJ_PC.addr)) TJ_BC1T_IDLE(op);
				else                                       TJ_BC1T(op);
				break;
			case 2: /* opcode 2: BC1FL */
				if (IS_RELATIVE_IDLE_LOOP(op, TJ_PC.addr)) TJ_BC1FL_IDLE(op);
				else                                       TJ_BC1FL(op);
				break;
			case 3: /* opcode 3: BC1TL */
				if (IS_RELATIVE_IDLE_LOOP(op, TJ_PC.addr)) TJ_BC1TL_IDLE(op);
				else                                       TJ_BC1TL(op);
				break;
			} /* switch ((op >> 16) & 0x3) for branches on C1 condition */
			break;
		case 16: /* Coprocessor 1 S-format opcodes */
			switch (op & 0x3F) {
			case 0: TJ_ADD_S(op); break;
			case 1: TJ_SUB_S(op); break;
			case 2: TJ_MUL_S(op); break;
			case 3: TJ_DIV_S(op); break;
			case 4: TJ_SQRT_S(op); break;
			case 5: TJ_ABS_S(op); break;
			case 6: TJ_MOV_S(op); break;
			case 7: TJ_NEG_S(op); break;
			case 8: TJ_ROUND_L_S(op); break;
			case 9: TJ_TRUNC_L_S(op); break;
			case 10: TJ_CEIL_L_S(op); break;
			case 11: TJ_FLOOR_L_S(op); break;
			case 12: TJ_ROUND_W_S(op); break;
			case 13: TJ_TRUNC_W_S(op); break;
			case 14: TJ_CEIL_W_S(op); break;
			case 15: TJ_FLOOR_W_S(op); break;
			case 33: TJ_CVT_D_S(op); break;
			case 36: TJ_CVT_W_S(op); break;
			case 37: TJ_CVT_L_S(op); break;
			case 48: TJ_C_F_S(op); break;
			case 49: TJ_C_UN_S(op); break;
			case 50: TJ_C_EQ_S(op); break;
			case 51: TJ_C_UEQ_S(op); break;
			case 52: TJ_C_OLT_S(op); break;
			case 53: TJ_C_ULT_S(op); break;
			case 54: TJ_C_OLE_S(op); break;
			case 55: TJ_C_ULE_S(op); break;
			case 56: TJ_C_SF_S(op); break;
			case 57: TJ_C_NGLE_S(op); break;
			case 58: TJ_C_SEQ_S(op); break;
			case 59: TJ_C_NGL_S(op); break;
			case 60: TJ_C_LT_S(op); break;
			case 61: TJ_C_NGE_S(op); break;
			case 62: TJ_C_LE_S(op); break;
			case 63: TJ_C_NGT_S(op); break;
			default: /* Coprocessor 1 S-format opcodes 16..32, 34..35, 38..47:
			            Reserved Instructions */
				TJ_RESERVED(op);
				break;
			} /* switch (op & 0x3F) for Coprocessor 1 S-format opcodes */
			break;
		case 17: /* Coprocessor 1 D-format opcodes */
			switch (op & 0x3F) {
			case 0: TJ_ADD_D(op); break;
			case 1: TJ_SUB_D(op); break;
			case 2: TJ_MUL_D(op); break;
			case 3: TJ_DIV_D(op); break;
			case 4: TJ_SQRT_D(op); break;
			case 5: TJ_ABS_D(op); break;
			case 6: TJ_MOV_D(op); break;
			case 7: TJ_NEG_D(op); break;
			case 8: TJ_ROUND_L_D(op); break;
			case 9: TJ_TRUNC_L_D(op); break;
			case 10: TJ_CEIL_L_D(op); break;
			case 11: TJ_FLOOR_L_D(op); break;
			case 12: TJ_ROUND_W_D(op); break;
			case 13: TJ_TRUNC_W_D(op); break;
			case 14: TJ_CEIL_W_D(op); break;
			case 15: TJ_FLOOR_W_D(op); break;
			case 32: TJ_CVT_S_D(op); break;
			case 36: TJ_CVT_W_D(op); break;
			case 37: TJ_CVT_L_D(op); break;
			case 48: TJ_C_F_D(op); break;
			case 49: TJ_C_UN_D(op); break;
			case 50: TJ_C_EQ_D(op); break;
			case 51: TJ_C_UEQ_D(op); break;
			case 52: TJ_C_OLT_D(op); break;
			case 53: TJ_C_ULT_D(op); break;
			case 54: TJ_C_OLE_D(op); break;
			case 55: TJ_C_ULE_D(op); break;
			case 56: TJ_C_SF_D(op); break;
			case 57: TJ_C_NGLE_D(op); break;
			case 58: TJ_C_SEQ_D(op); break;
			case 59: TJ_C_NGL_D(op); break;
			case 60: TJ_C_LT_D(op); break;
			case 61: TJ_C_NGE_D(op); break;
			case 62: TJ_C_LE_D(op); break;
			case 63: TJ_C_NGT_D(op); break;
			default: /* Coprocessor 1 D-format opcodes 16..31, 33..35, 38..47:
			            Reserved Instructions */
				TJ_RESERVED(op);
				break;
			} /* switch (op & 0x3F) for Coprocessor 1 D-format opcodes */
			break;
		case 20: /* Coprocessor 1 W-format opcodes */
			switch (op & 0x3F) {
			case 32: TJ_CVT_S_W(op); break;
			case 33: TJ_CVT_D_W(op); break;
			default: /* Coprocessor 1 W-format opcodes 0..31, 34..63:
			            Reserved Instructions */
				TJ_RESERVED(op);
				break;
			}
			break;
		case 21: /* Coprocessor 1 L-format opcodes */
			switch (op & 0x3F) {
			case 32: TJ_CVT_S_L(op); break;
			case 33: TJ_CVT_D_L(op); break;
			default: /* Coprocessor 1 L-format opcodes 0..31, 34..63:
			            Reserved Instructions */
				TJ_RESERVED(op);
				break;
			}
			break;
		default: /* Coprocessor 1 opcodes 3, 7, 9..15, 18..19, 22..31:
		            Reserved Instructions */
			TJ_RESERVED(op);
			break;
		} /* switch ((op >> 21) & 0x1F) for the Coprocessor 1 prefix */
		break;
	case 20: /* Major opcode 20: BEQL */
		if (IS_RELATIVE_IDLE_LOOP(op, TJ_PC.addr)) TJ_BEQL_IDLE(op);
		else                                       TJ_BEQL(op);
		break;
	case 21: /* Major opcode 21: BNEL */
		if (IS_RELATIVE_IDLE_LOOP(op, TJ_PC.addr)) TJ_BNEL_IDLE(op);
		else                                       TJ_BNEL(op);
		break;
	case 22: /* Major opcode 22: BLEZL */
		if (IS_RELATIVE_IDLE_LOOP(op, TJ_PC.addr)) TJ_BLEZL_IDLE(op);
		else                                       TJ_BLEZL(op);
		break;
	case 23: /* Major opcode 23: BGTZL */
		if (IS_RELATIVE_IDLE_LOOP(op, TJ_PC.addr)) TJ_BGTZL_IDLE(op);
		else                                       TJ_BGTZL(op);
		break;
	case 24: /* Major opcode 24: DADDI */
		if (RT_OF(op) != 0) TJ_DADDI(op);
		else                TJ_NOP(0);
		break;
	case 25: /* Major opcode 25: DADDIU */
		if (RT_OF(op) != 0) TJ_DADDIU(op);
		else                TJ_NOP(0);
		break;
	case 26: /* Major opcode 26: LDL */
		if (RT_OF(op) != 0) TJ_LDL(op);
		else                TJ_NOP(0);
		break;
	case 27: /* Major opcode 27: LDR */
		if (RT_OF(op) != 0) TJ_LDR(op);
		else                TJ_NOP(0);
		break;
	case 32: /* Major opcode 32: LB */
		if (RT_OF(op) != 0) TJ_LB(op);
		else                TJ_NOP(0);
		break;
	case 33: /* Major opcode 33: LH */
		if (RT_OF(op) != 0) TJ_LH(op);
		else                TJ_NOP(0);
		break;
	case 34: /* Major opcode 34: LWL */
		if (RT_OF(op) != 0) TJ_LWL(op);
		else                TJ_NOP(0);
		break;
	case 35: /* Major opcode 35: LW */
		if (RT_OF(op) != 0) TJ_LW(op);
		else                TJ_NOP(0);
		break;
	case 36: /* Major opcode 36: LBU */
		if (RT_OF(op) != 0) TJ_LBU(op);
		else                TJ_NOP(0);
		break;
	case 37: /* Major opcode 37: LHU */
		if (RT_OF(op) != 0) TJ_LHU(op);
		else                TJ_NOP(0);
		break;
	case 38: /* Major opcode 38: LWR */
		if (RT_OF(op) != 0) TJ_LWR(op);
		else                TJ_NOP(0);
		break;
	case 39: /* Major opcode 39: LWU */
		if (RT_OF(op) != 0) TJ_LWU(op);
		else                TJ_NOP(0);
		break;
	case 40: TJ_SB(op); break;
	case 41: TJ_SH(op); break;
	case 42: TJ_SWL(op); break;
	case 43: TJ_SW(op); break;
	case 44: TJ_SDL(op); break;
	case 45: TJ_SDR(op); break;
	case 46: TJ_SWR(op); break;
	case 47: TJ_CACHE(op); break;
	case 48: /* Major opcode 48: LL */
		if (RT_OF(op) != 0) TJ_LL(op);
		else                TJ_NOP(0);
		break;
	case 49: TJ_LWC1(op); break;
	case 52: /* Major opcode 52: LLD (Not implemented) */
		TJ_NI(op);
		break;
	case 53: TJ_LDC1(op); break;
	case 55: /* Major opcode 55: LD */
		if (RT_OF(op) != 0) TJ_LD(op);
		else                TJ_NOP(0);
		break;
	case 56: /* Major opcode 56: SC */
		if (RT_OF(op) != 0) TJ_SC(op);
		else                TJ_NOP(0);
		break;
	case 57: TJ_SWC1(op); break;
	case 60: /* Major opcode 60: SCD (Not implemented) */
		TJ_NI(op);
		break;
	case 61: TJ_SDC1(op); break;
	case 63: TJ_SD(op); break;
	default: /* Major opcodes 18..19, 28..31, 50..51, 54, 58..59, 62:
	            Reserved Instructions */
		TJ_RESERVED(op);
		break;
	} /* switch ((op >> 26) & 0x3F) */
}

static uint32_t validate_alternates(uint32_t addr)
{
#if defined(ARCH_EMIT_TRACE)
	if ((addr & UINT32_C(0xC0000000)) == UINT32_C(0x80000000)) {
		/* A physical address. Its alternate is 0x20000000 bytes away:
		 * below if the address is uncached, and above if the address is
		 * cached. */
		uint32_t alt_addr = addr ^ UINT32_C(0x20000000);
		if (invalid_code[addr >> 12] | invalid_code[alt_addr >> 12]) {
			invalid_code[addr >> 12] = invalid_code[alt_addr >> 12] = 0;
			ClearTracePage(addr);
			ClearTracePage(alt_addr);
		}
		return addr;
	} else {
		/* A virtual address. Its alternates are the cached and uncached
		 * versions of its physical address. The call below may raise a
		 * TLB Refill exception. */
		uint32_t alt_addr1 = virtual_to_physical_address(addr, 2), alt_addr2;
		if (alt_addr1 != 0) {
			alt_addr2 = alt_addr1 ^ UINT32_C(0x20000000);
			if (invalid_code[addr >> 12] | invalid_code[alt_addr1 >> 12]
			  | invalid_code[alt_addr2 >> 12]) {
				invalid_code[addr >> 12] = invalid_code[alt_addr1 >> 12]
					= invalid_code[alt_addr2 >> 12] = 0;
				ClearTracePage(addr);
				ClearTracePage(alt_addr1);
				ClearTracePage(alt_addr2);
			}
		}
		return alt_addr1;
	}
#else
	return addr;
#endif
}

bool tj_jump_to(uint32_t addr)
{
	/* The skip_jump check avoids the jump if an exception is pending.
	 * validate_alternates may raise TLB Refill for the target address.
	 * In either case, don't jump. */
	if (!skip_jump && validate_alternates(addr) != 0) {
		TJ_PC.addr = addr;
		return true;
	} else {
		return false;
	}
}

void trace_jit_init()
{
	DebugMessage(M64MSG_INFO, "Starting R4300 emulator: Trace JIT");
	/* SB is used to modify code by at least Donkey Kong 64. Play it safe. */
	TraceJITSettings.SBCanModifyCode = true;
	/* SD is used to modify code by at least Banjo-Tooie. Play it safe. */
	TraceJITSettings.SDCanModifyCode = true;
	ARCH_TRACE_JIT_INIT();

	stop = 0;
	PC = &TJ_PC;
	TJ_PC.addr = last_addr = UINT32_C(0xa4000040);

	/* Initially, all code is valid, because none has been written. */
	memset(invalid_code, 0, sizeof(invalid_code));
}

void trace_jit()
{
	while (!stop) {
#ifdef COMPARE_CORE
		CoreCompareCallback();
#endif
#ifdef DBG
		if (g_DebuggerActive) update_debugger(TJ_PC.addr);
#endif
#if defined(ARCH_EMIT_TRACE)
#  if defined(ARCH_JIT_ENTRY)
		ARCH_JIT_ENTRY(GetOrMakeTraceAt(TJ_PC.addr));
#  else
		((TraceFunc) GetTraceAt(TJ_PC.addr)) ();
#  endif
#else
		TJFallback();
#endif
	}
}

void invalidate_cached_code_trace_jit(uint32_t start, size_t len)
{
#if defined(ARCH_EMIT_TRACE)
	if (len == 0) {
		memset(invalid_code, 0, sizeof(invalid_code));
		FreeTraceCache();
	} else {
		uint32_t end = start + len, addr;
		start &= ~(N64_PAGE_SIZE - 1);
		for (addr = start; addr < end; addr += N64_PAGE_SIZE)
			invalid_code[addr >> 12] = 1;
	}
#endif
}

void trace_jit_exit()
{
	ARCH_TRACE_JIT_EXIT();
}
