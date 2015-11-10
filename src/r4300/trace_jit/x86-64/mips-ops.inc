/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - mips-ops.inc                                            *
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

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "../mips-interp.h"
#include "../../cp0_private.h"
#include "../../../main/main.h"
#include "../../../memory/memory.h"

/* Represents the offset of a numbered MIPS register from the address of $16,
 * which is from -128 to +120. */
#define REG_OFFSET(nr) ((int8_t) (((uint8_t*) &g_state.regs.gpr[nr]) - ((uint8_t*) &g_state.regs.gpr[16])))

static enum TJEmitTraceResult x86_64_emit_nop(struct x86_64_state* state)
{
	if (TraceJITSettings.InterpretNOP) {
		return x86_64_emit_interpret(state, &TJ_NOP, 0);
	} else {
		x86_64_next_opcode(state);
		return TJ_SUCCESS;
	}
}

static enum TJEmitTraceResult x86_64_emit_ni(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_NI, TJ_READS_PC);
}

static enum TJEmitTraceResult x86_64_emit_reserved(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_RESERVED, TJ_READS_PC);
}

static enum TJEmitTraceResult x86_64_emit_sll(struct x86_64_state* state)
{
	if (TraceJITSettings.InterpretSLL) {
		return x86_64_emit_interpret(state, &TJ_SLL, 0);
	} else {
		uint8_t rt = RT_OF(state->ops[0]), sa = SA_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
		x86_64_ensure_RDX_has_regs(state);
		x86_64_mov_r64_m64rosi8(state, RAX, RDX, REG_OFFSET(rt));
		x86_64_shl_r32_i5(state, EAX, sa);
		x86_64_movsx_r64_r32(state, RAX, EAX);
		x86_64_mov_m64rosi8_r64(state, RDX, REG_OFFSET(rd), RAX);
		x86_64_next_opcode(state);
		return TJ_SUCCESS;
	}
}

static enum TJEmitTraceResult x86_64_emit_srl(struct x86_64_state* state)
{
	if (TraceJITSettings.InterpretSRA) {
		return x86_64_emit_interpret(state, &TJ_SRA, 0);
	} else {
		uint8_t rt = RT_OF(state->ops[0]), sa = SA_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
		x86_64_ensure_RDX_has_regs(state);
		x86_64_mov_r64_m64rosi8(state, RAX, RDX, REG_OFFSET(rt));
		/* CONSIDER THE FOLLOWING:
		 * The amd64 ISA zero-extends 32-bit results to 64-bit. The MIPS ISA
		 * instead sign-extends 32-bit results to 64-bit. However, since SRL
		 * is going to leave at least bit 31 cleared, amd64's zero-extension
		 * will also fulfill MIPS's sign-extension without needing MOVSX. */
		x86_64_shr_r32_i5(state, EAX, sa);
		x86_64_mov_m64rosi8_r64(state, RDX, REG_OFFSET(rd), RAX);
		x86_64_next_opcode(state);
		return TJ_SUCCESS;
	}
}

static enum TJEmitTraceResult x86_64_emit_sra(struct x86_64_state* state)
{
	if (TraceJITSettings.InterpretSRA) {
		return x86_64_emit_interpret(state, &TJ_SRA, 0);
	} else {
		uint8_t rt = RT_OF(state->ops[0]), sa = SA_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
		x86_64_ensure_RDX_has_regs(state);
		x86_64_mov_r64_m64rosi8(state, RAX, RDX, REG_OFFSET(rt));
		x86_64_sar_r32_i5(state, EAX, sa);
		x86_64_movsx_r64_r32(state, RAX, EAX);
		x86_64_mov_m64rosi8_r64(state, RDX, REG_OFFSET(rd), RAX);
		x86_64_next_opcode(state);
		return TJ_SUCCESS;
	}
}

static enum TJEmitTraceResult x86_64_emit_sllv(struct x86_64_state* state)
{
	if (TraceJITSettings.InterpretSLLV) {
		return x86_64_emit_interpret(state, &TJ_SLLV, 0);
	} else {
		uint8_t rt = RT_OF(state->ops[0]), rs = RS_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
		x86_64_ensure_RDX_has_regs(state);
		x86_64_mov_r64_m64rosi8(state, RAX, RDX, REG_OFFSET(rt));
		x86_64_mov_r64_m64rosi8(state, RCX, RDX, REG_OFFSET(rs));
		x86_64_shl_r32_CL(state, EAX);
		x86_64_movsx_r64_r32(state, RAX, EAX);
		x86_64_mov_m64rosi8_r64(state, RDX, REG_OFFSET(rd), RAX);
		x86_64_next_opcode(state);
		return TJ_SUCCESS;
	}
}

static enum TJEmitTraceResult x86_64_emit_srlv(struct x86_64_state* state)
{
	if (TraceJITSettings.InterpretSRLV) {
		return x86_64_emit_interpret(state, &TJ_SRLV, 0);
	} else {
		uint8_t rt = RT_OF(state->ops[0]), rs = RS_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
		x86_64_ensure_RDX_has_regs(state);
		x86_64_mov_r64_m64rosi8(state, RAX, RDX, REG_OFFSET(rt));
		x86_64_mov_r64_m64rosi8(state, RCX, RDX, REG_OFFSET(rs));
		x86_64_shr_r32_CL(state, EAX);
		x86_64_movsx_r64_r32(state, RAX, EAX);
		x86_64_mov_m64rosi8_r64(state, RDX, REG_OFFSET(rd), RAX);
		x86_64_next_opcode(state);
		return TJ_SUCCESS;
	}
}

static enum TJEmitTraceResult x86_64_emit_srav(struct x86_64_state* state)
{
	if (TraceJITSettings.InterpretSRAV) {
		return x86_64_emit_interpret(state, &TJ_SRAV, 0);
	} else {
		uint8_t rt = RT_OF(state->ops[0]), rs = RS_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
		x86_64_ensure_RDX_has_regs(state);
		x86_64_mov_r64_m64rosi8(state, RAX, RDX, REG_OFFSET(rt));
		x86_64_mov_r64_m64rosi8(state, RCX, RDX, REG_OFFSET(rs));
		x86_64_sar_r32_CL(state, EAX);
		x86_64_movsx_r64_r32(state, RAX, EAX);
		x86_64_mov_m64rosi8_r64(state, RDX, REG_OFFSET(rd), RAX);
		x86_64_next_opcode(state);
		return TJ_SUCCESS;
	}
}

static enum TJEmitTraceResult x86_64_emit_jr(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_JR, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_jalr(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_JALR, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_syscall(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_SYSCALL, TJ_READS_PC | TJ_TRANSFERS_CONTROL);
}

static enum TJEmitTraceResult x86_64_emit_sync(struct x86_64_state* state)
{
	if (TraceJITSettings.InterpretSYNC || TraceJITSettings.InterpretNOP) {
		return x86_64_emit_interpret(state, &TJ_SYNC, 0);
	} else {
		return x86_64_emit_nop(state);
	}
}

static enum TJEmitTraceResult x86_64_emit_mfhi(struct x86_64_state* state)
{
	if (TraceJITSettings.InterpretMFHI) {
		return x86_64_emit_interpret(state, &TJ_MFHI, 0);
	} else {
		uint8_t rd = RD_OF(state->ops[0]);
		x86_64_ensure_RDX_has_regs(state);
		x86_64_mov_r64_ia(state, RCX, &g_state.regs.hi);
		x86_64_mov_r64_m64r(state, RAX, RCX);
		x86_64_mov_m64rosi8_r64(state, RDX, REG_OFFSET(rd), RAX);
		x86_64_next_opcode(state);
		return TJ_SUCCESS;
	}
}

static enum TJEmitTraceResult x86_64_emit_mthi(struct x86_64_state* state)
{
	if (TraceJITSettings.InterpretMTHI) {
		return x86_64_emit_interpret(state, &TJ_MTHI, 0);
	} else {
		uint8_t rs = RS_OF(state->ops[0]);
		x86_64_ensure_RDX_has_regs(state);
		x86_64_mov_r64_ia(state, RCX, &g_state.regs.hi);
		x86_64_mov_r64_m64rosi8(state, RAX, RDX, REG_OFFSET(rs));
		x86_64_mov_m64r_r64(state, RCX, RAX);
		x86_64_next_opcode(state);
		return TJ_SUCCESS;
	}
}

static enum TJEmitTraceResult x86_64_emit_mflo(struct x86_64_state* state)
{
	if (TraceJITSettings.InterpretMFLO) {
		return x86_64_emit_interpret(state, &TJ_MFLO, 0);
	} else {
		uint8_t rd = RD_OF(state->ops[0]);
		x86_64_ensure_RDX_has_regs(state);
		x86_64_mov_r64_ia(state, RCX, &g_state.regs.lo);
		x86_64_mov_r64_m64r(state, RAX, RCX);
		x86_64_mov_m64rosi8_r64(state, RDX, REG_OFFSET(rd), RAX);
		x86_64_next_opcode(state);
		return TJ_SUCCESS;
	}
}

static enum TJEmitTraceResult x86_64_emit_mtlo(struct x86_64_state* state)
{
	if (TraceJITSettings.InterpretMTLO) {
		return x86_64_emit_interpret(state, &TJ_MTLO, 0);
	} else {
		uint8_t rs = RS_OF(state->ops[0]);
		x86_64_ensure_RDX_has_regs(state);
		x86_64_mov_r64_ia(state, RCX, &g_state.regs.lo);
		x86_64_mov_r64_m64rosi8(state, RAX, RDX, REG_OFFSET(rs));
		x86_64_mov_m64r_r64(state, RCX, RAX);
		x86_64_next_opcode(state);
		return TJ_SUCCESS;
	}
}

static enum TJEmitTraceResult x86_64_emit_dsllv(struct x86_64_state* state)
{
	if (TraceJITSettings.InterpretDSLLV) {
		return x86_64_emit_interpret(state, &TJ_DSLLV, 0);
	} else {
		uint8_t rt = RT_OF(state->ops[0]), rs = RS_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
		x86_64_ensure_RDX_has_regs(state);
		x86_64_mov_r64_m64rosi8(state, RAX, RDX, REG_OFFSET(rt));
		x86_64_mov_r64_m64rosi8(state, RCX, RDX, REG_OFFSET(rs));
		x86_64_shl_r64_CL(state, EAX);
		x86_64_mov_m64rosi8_r64(state, RDX, REG_OFFSET(rd), RAX);
		x86_64_next_opcode(state);
		return TJ_SUCCESS;
	}
}

static enum TJEmitTraceResult x86_64_emit_dsrlv(struct x86_64_state* state)
{
	if (TraceJITSettings.InterpretDSRLV) {
		return x86_64_emit_interpret(state, &TJ_DSRLV, 0);
	} else {
		uint8_t rt = RT_OF(state->ops[0]), rs = RS_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
		x86_64_ensure_RDX_has_regs(state);
		x86_64_mov_r64_m64rosi8(state, RAX, RDX, REG_OFFSET(rt));
		x86_64_mov_r64_m64rosi8(state, RCX, RDX, REG_OFFSET(rs));
		x86_64_shr_r64_CL(state, EAX);
		x86_64_mov_m64rosi8_r64(state, RDX, REG_OFFSET(rd), RAX);
		x86_64_next_opcode(state);
		return TJ_SUCCESS;
	}
}

static enum TJEmitTraceResult x86_64_emit_dsrav(struct x86_64_state* state)
{
	if (TraceJITSettings.InterpretDSRAV) {
		return x86_64_emit_interpret(state, &TJ_DSRAV, 0);
	} else {
		uint8_t rt = RT_OF(state->ops[0]), rs = RS_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
		x86_64_ensure_RDX_has_regs(state);
		x86_64_mov_r64_m64rosi8(state, RAX, RDX, REG_OFFSET(rt));
		x86_64_mov_r64_m64rosi8(state, RCX, RDX, REG_OFFSET(rs));
		x86_64_sar_r64_CL(state, EAX);
		x86_64_mov_m64rosi8_r64(state, RDX, REG_OFFSET(rd), RAX);
		x86_64_next_opcode(state);
		return TJ_SUCCESS;
	}
}

static enum TJEmitTraceResult x86_64_emit_mult(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_MULT, 0);
}

static enum TJEmitTraceResult x86_64_emit_multu(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_MULTU, 0);
}

static enum TJEmitTraceResult x86_64_emit_div(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_DIV, 0);
}

static enum TJEmitTraceResult x86_64_emit_divu(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_DIVU, 0);
}

static enum TJEmitTraceResult x86_64_emit_dmult(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_DMULT, 0);
}

static enum TJEmitTraceResult x86_64_emit_dmultu(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_DMULTU, 0);
}

static enum TJEmitTraceResult x86_64_emit_ddiv(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_DDIV, 0);
}

static enum TJEmitTraceResult x86_64_emit_ddivu(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_DDIVU, 0);
}

static enum TJEmitTraceResult x86_64_emit_addu(struct x86_64_state* state)
{
	if (TraceJITSettings.InterpretADDU) {
		return x86_64_emit_interpret(state, &TJ_ADDU, 0);
	} else {
		uint8_t rs = RS_OF(state->ops[0]), rt = RT_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
		x86_64_ensure_RDX_has_regs(state);
		if (rt == 0) { /* ADDU rd, rs, $0 sign-extends rs into rd */
			x86_64_mov_r64_m64rosi8(state, RAX, RDX, REG_OFFSET(rs));
		} else if (rs == rt) {
			x86_64_mov_r64_m64rosi8(state, RAX, RDX, REG_OFFSET(rs));
			x86_64_add_r64_r64(state, RAX, RAX);
		} else {
			x86_64_mov_r64_m64rosi8(state, RAX, RDX, REG_OFFSET(rs));
			x86_64_mov_r64_m64rosi8(state, RCX, RDX, REG_OFFSET(rt));
			x86_64_add_r64_r64(state, RAX, RCX);
		}
		x86_64_movsx_r64_r32(state, RAX, EAX);
		x86_64_mov_m64rosi8_r64(state, RDX, REG_OFFSET(rd), RAX);
		x86_64_next_opcode(state);
		return TJ_SUCCESS;
	}
}

static enum TJEmitTraceResult x86_64_emit_add(struct x86_64_state* state)
{
	if (TraceJITSettings.InterpretADD || TraceJITSettings.InterpretADDU) {
		return x86_64_emit_interpret(state, &TJ_ADD, 0);
	} else {
		return x86_64_emit_addu(state);
	}
}

static enum TJEmitTraceResult x86_64_emit_subu(struct x86_64_state* state)
{
	if (TraceJITSettings.InterpretSUBU) {
		return x86_64_emit_interpret(state, &TJ_SUBU, 0);
	} else {
		uint8_t rs = RS_OF(state->ops[0]), rt = RT_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
		x86_64_ensure_RDX_has_regs(state);
		if (rs == 0) { /* SUBU rd, $0, rt negates rt into rd */
			x86_64_mov_r64_m64rosi8(state, RAX, RDX, REG_OFFSET(rt));
			x86_64_neg_r64(state, RAX);
		} else {
			x86_64_mov_r64_m64rosi8(state, RAX, RDX, REG_OFFSET(rs));
			x86_64_mov_r64_m64rosi8(state, RCX, RDX, REG_OFFSET(rt));
			x86_64_sub_r64_r64(state, RAX, RCX);
		}
		x86_64_movsx_r64_r32(state, RAX, EAX);
		x86_64_mov_m64rosi8_r64(state, RDX, REG_OFFSET(rd), RAX);
		x86_64_next_opcode(state);
		return TJ_SUCCESS;
	}
}

static enum TJEmitTraceResult x86_64_emit_sub(struct x86_64_state* state)
{
	if (TraceJITSettings.InterpretSUB || TraceJITSettings.InterpretSUBU) {
		return x86_64_emit_interpret(state, &TJ_SUB, 0);
	} else {
		return x86_64_emit_subu(state);
	}
}

static enum TJEmitTraceResult x86_64_emit_and(struct x86_64_state* state)
{
	if (TraceJITSettings.InterpretAND) {
		return x86_64_emit_interpret(state, &TJ_AND, 0);
	} else {
		uint8_t rs = RS_OF(state->ops[0]), rt = RT_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
		x86_64_ensure_RDX_has_regs(state);
		x86_64_mov_r64_m64rosi8(state, RAX, RDX, REG_OFFSET(rs));
		x86_64_mov_r64_m64rosi8(state, RCX, RDX, REG_OFFSET(rt));
		x86_64_and_r64_r64(state, RAX, RCX);
		x86_64_mov_m64rosi8_r64(state, RDX, REG_OFFSET(rd), RAX);
		x86_64_next_opcode(state);
		return TJ_SUCCESS;
	}
}

static enum TJEmitTraceResult x86_64_emit_or(struct x86_64_state* state)
{
	if (TraceJITSettings.InterpretOR) {
		return x86_64_emit_interpret(state, &TJ_OR, 0);
	} else {
		uint8_t rs = RS_OF(state->ops[0]), rt = RT_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
		x86_64_ensure_RDX_has_regs(state);
		if (rs == 0 && rt == 0) { /* OR rd, $0, $0 loads 0 into rd */
			x86_64_xor_r64_r64(state, RAX, RAX);
		} else if (rt == 0) { /* OR rd, rs, $0 moves rs into rd */
			x86_64_mov_r64_m64rosi8(state, RAX, RDX, REG_OFFSET(rs));
		} else {
			x86_64_mov_r64_m64rosi8(state, RAX, RDX, REG_OFFSET(rs));
			x86_64_mov_r64_m64rosi8(state, RCX, RDX, REG_OFFSET(rt));
			x86_64_or_r64_r64(state, RAX, RCX);
		}
		x86_64_mov_m64rosi8_r64(state, RDX, REG_OFFSET(rd), RAX);
		x86_64_next_opcode(state);
		return TJ_SUCCESS;
	}
}

static enum TJEmitTraceResult x86_64_emit_xor(struct x86_64_state* state)
{
	if (TraceJITSettings.InterpretXOR) {
		return x86_64_emit_interpret(state, &TJ_XOR, 0);
	} else {
		uint8_t rs = RS_OF(state->ops[0]), rt = RT_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
		x86_64_ensure_RDX_has_regs(state);
		x86_64_mov_r64_m64rosi8(state, RAX, RDX, REG_OFFSET(rs));
		x86_64_mov_r64_m64rosi8(state, RCX, RDX, REG_OFFSET(rt));
		x86_64_xor_r64_r64(state, RAX, RCX);
		x86_64_mov_m64rosi8_r64(state, RDX, REG_OFFSET(rd), RAX);
		x86_64_next_opcode(state);
		return TJ_SUCCESS;
	}
}

static enum TJEmitTraceResult x86_64_emit_nor(struct x86_64_state* state)
{
	if (TraceJITSettings.InterpretNOR) {
		return x86_64_emit_interpret(state, &TJ_NOR, 0);
	} else {
		uint8_t rs = RS_OF(state->ops[0]), rt = RT_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
		x86_64_ensure_RDX_has_regs(state);
		if (rs == 0 && rt == 0) { /* NOR rd, $0, $0 loads -1 into rd */
			x86_64_mov_r64_si32(state, RAX, INT32_C(-1));
		} else if (rt == 0) { /* NOR rd, rs, $0 complements rs into rd */
			x86_64_mov_r64_m64rosi8(state, RAX, RDX, REG_OFFSET(rs));
			x86_64_not_r64(state, RAX);
		} else {
			x86_64_mov_r64_m64rosi8(state, RAX, RDX, REG_OFFSET(rs));
			x86_64_mov_r64_m64rosi8(state, RCX, RDX, REG_OFFSET(rt));
			x86_64_or_r64_r64(state, RAX, RCX);
			x86_64_not_r64(state, RAX);
		}
		x86_64_mov_m64rosi8_r64(state, RDX, REG_OFFSET(rd), RAX);
		x86_64_next_opcode(state);
		return TJ_SUCCESS;
	}
}

static enum TJEmitTraceResult x86_64_emit_slt(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_SLT, 0);
}

static enum TJEmitTraceResult x86_64_emit_sltu(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_SLTU, 0);
}

static enum TJEmitTraceResult x86_64_emit_daddu(struct x86_64_state* state)
{
	if (TraceJITSettings.InterpretDADDU) {
		return x86_64_emit_interpret(state, &TJ_DADDU, 0);
	} else {
		uint8_t rs = RS_OF(state->ops[0]), rt = RT_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
		x86_64_ensure_RDX_has_regs(state);
		x86_64_mov_r64_m64rosi8(state, RAX, RDX, REG_OFFSET(rs));
		x86_64_mov_r64_m64rosi8(state, RCX, RDX, REG_OFFSET(rt));
		x86_64_add_r64_r64(state, RAX, RCX);
		x86_64_mov_m64rosi8_r64(state, RDX, REG_OFFSET(rd), RAX);
		x86_64_next_opcode(state);
		return TJ_SUCCESS;
	}
}

static enum TJEmitTraceResult x86_64_emit_dadd(struct x86_64_state* state)
{
	if (TraceJITSettings.InterpretDADD || TraceJITSettings.InterpretDADDU) {
		return x86_64_emit_interpret(state, &TJ_DADD, 0);
	} else {
		return x86_64_emit_daddu(state);
	}
}

static enum TJEmitTraceResult x86_64_emit_dsubu(struct x86_64_state* state)
{
	if (TraceJITSettings.InterpretDSUBU) {
		return x86_64_emit_interpret(state, &TJ_DSUBU, 0);
	} else {
		uint8_t rs = RS_OF(state->ops[0]), rt = RT_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
		x86_64_ensure_RDX_has_regs(state);
		if (rs == 0) { /* DSUBU rd, $0, rt negates rt into rd */
			x86_64_mov_r64_m64rosi8(state, RAX, RDX, REG_OFFSET(rt));
			x86_64_neg_r64(state, RAX);
		} else {
			x86_64_mov_r64_m64rosi8(state, RAX, RDX, REG_OFFSET(rs));
			x86_64_mov_r64_m64rosi8(state, RCX, RDX, REG_OFFSET(rt));
			x86_64_sub_r64_r64(state, RAX, RCX);
		}
		x86_64_mov_m64rosi8_r64(state, RDX, REG_OFFSET(rd), RAX);
		x86_64_next_opcode(state);
		return TJ_SUCCESS;
	}
}

static enum TJEmitTraceResult x86_64_emit_dsub(struct x86_64_state* state)
{
	if (TraceJITSettings.InterpretDSUB || TraceJITSettings.InterpretDSUBU) {
		return x86_64_emit_interpret(state, &TJ_DSUB, 0);
	} else {
		return x86_64_emit_dsubu(state);
	}
}

static enum TJEmitTraceResult x86_64_emit_teq(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_TEQ, TJ_READS_PC | TJ_CHECK_STOP);
}

static enum TJEmitTraceResult x86_64_emit_dsll(struct x86_64_state* state)
{
	if (TraceJITSettings.InterpretDSLL) {
		return x86_64_emit_interpret(state, &TJ_DSLL, 0);
	} else {
		uint8_t rt = RT_OF(state->ops[0]), sa = SA_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
		x86_64_ensure_RDX_has_regs(state);
		x86_64_mov_r64_m64rosi8(state, RAX, RDX, REG_OFFSET(rt));
		x86_64_shl_r64_i6(state, RAX, sa);
		x86_64_mov_m64rosi8_r64(state, RDX, REG_OFFSET(rd), RAX);
		x86_64_next_opcode(state);
		return TJ_SUCCESS;
	}
}

static enum TJEmitTraceResult x86_64_emit_dsrl(struct x86_64_state* state)
{
	if (TraceJITSettings.InterpretDSRL) {
		return x86_64_emit_interpret(state, &TJ_DSRL, 0);
	} else {
		uint8_t rt = RT_OF(state->ops[0]), sa = SA_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
		x86_64_ensure_RDX_has_regs(state);
		x86_64_mov_r64_m64rosi8(state, RAX, RDX, REG_OFFSET(rt));
		x86_64_shr_r64_i6(state, RAX, sa);
		x86_64_mov_m64rosi8_r64(state, RDX, REG_OFFSET(rd), RAX);
		x86_64_next_opcode(state);
		return TJ_SUCCESS;
	}
}

static enum TJEmitTraceResult x86_64_emit_dsra(struct x86_64_state* state)
{
	if (TraceJITSettings.InterpretDSRA) {
		return x86_64_emit_interpret(state, &TJ_DSRA, 0);
	} else {
		uint8_t rt = RT_OF(state->ops[0]), sa = SA_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
		x86_64_ensure_RDX_has_regs(state);
		x86_64_mov_r64_m64rosi8(state, RAX, RDX, REG_OFFSET(rt));
		x86_64_sar_r64_i6(state, RAX, sa);
		x86_64_mov_m64rosi8_r64(state, RDX, REG_OFFSET(rd), RAX);
		x86_64_next_opcode(state);
		return TJ_SUCCESS;
	}
}

static enum TJEmitTraceResult x86_64_emit_dsll32(struct x86_64_state* state)
{
	if (TraceJITSettings.InterpretDSLL32) {
		return x86_64_emit_interpret(state, &TJ_DSLL32, 0);
	} else {
		uint8_t rt = RT_OF(state->ops[0]), sa = SA_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
		x86_64_ensure_RDX_has_regs(state);
		x86_64_mov_r64_m64rosi8(state, RAX, RDX, REG_OFFSET(rt));
		x86_64_shl_r64_i6(state, RAX, 32 + sa);
		x86_64_mov_m64rosi8_r64(state, RDX, REG_OFFSET(rd), RAX);
		x86_64_next_opcode(state);
		return TJ_SUCCESS;
	}
}

static enum TJEmitTraceResult x86_64_emit_dsrl32(struct x86_64_state* state)
{
	if (TraceJITSettings.InterpretDSRL32) {
		return x86_64_emit_interpret(state, &TJ_DSRL32, 0);
	} else {
		uint8_t rt = RT_OF(state->ops[0]), sa = SA_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
		x86_64_ensure_RDX_has_regs(state);
		x86_64_mov_r64_m64rosi8(state, RAX, RDX, REG_OFFSET(rt));
		x86_64_shr_r64_i6(state, RAX, 32 + sa);
		x86_64_mov_m64rosi8_r64(state, RDX, REG_OFFSET(rd), RAX);
		x86_64_next_opcode(state);
		return TJ_SUCCESS;
	}
}

static enum TJEmitTraceResult x86_64_emit_dsra32(struct x86_64_state* state)
{
	if (TraceJITSettings.InterpretDSRA32) {
		return x86_64_emit_interpret(state, &TJ_DSRA32, 0);
	} else {
		uint8_t rt = RT_OF(state->ops[0]), sa = SA_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
		x86_64_ensure_RDX_has_regs(state);
		x86_64_mov_r64_m64rosi8(state, RAX, RDX, REG_OFFSET(rt));
		x86_64_sar_r64_i6(state, RAX, 32 + sa);
		x86_64_mov_m64rosi8_r64(state, RDX, REG_OFFSET(rd), RAX);
		x86_64_next_opcode(state);
		return TJ_SUCCESS;
	}
}

static enum TJEmitTraceResult x86_64_emit_bltz_idle(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_BLTZ_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_bltz(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_BLTZ, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_bgez_idle(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_BGEZ_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_bgez(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_BGEZ, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_bltzl_idle(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_BLTZL_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_bltzl(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_BLTZL, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_bgezl_idle(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_BGEZL_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_bgezl(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_BGEZL, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_bltzal_idle(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_BLTZAL_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_bltzal(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_BLTZAL, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_bgezal_idle(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_BGEZAL_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_bgezal(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_BGEZAL, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_bltzall_idle(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_BLTZALL_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_bltzall(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_BLTZALL, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_bgezall_idle(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_BGEZALL_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_bgezall(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_BGEZALL, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_j_idle(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_J_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_j(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_J, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_jal_idle(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_JAL_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_jal(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_JAL, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_beq_idle(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_BEQ_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_beq(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_BEQ, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_bne_idle(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_BNE_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_bne(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_BNE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_blez_idle(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_BLEZ_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_blez(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_BLEZ, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_bgtz_idle(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_BGTZ_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_bgtz(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_BGTZ, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_addiu(struct x86_64_state* state)
{
	if (TraceJITSettings.InterpretADDIU) {
		return x86_64_emit_interpret(state, &TJ_ADDIU, 0);
	} else {
		uint8_t rs = RS_OF(state->ops[0]), rt = RT_OF(state->ops[0]);
		int16_t imm = IMM16S_OF(state->ops[0]);
		x86_64_ensure_RDX_has_regs(state);
		x86_64_mov_r64_m64rosi8(state, RAX, RDX, REG_OFFSET(rs));
		if (imm == (int16_t) (int8_t) imm) {
			x86_64_add_r64_si8(state, RAX, (int8_t) imm);
		} else {
			x86_64_add_r64_si32(state, RAX, (int32_t) imm);
		}
		x86_64_movsx_r64_r32(state, RAX, EAX);
		x86_64_mov_m64rosi8_r64(state, RDX, REG_OFFSET(rt), RAX);
		x86_64_next_opcode(state);
		return TJ_SUCCESS;
	}
}

static enum TJEmitTraceResult x86_64_emit_addi(struct x86_64_state* state)
{
	if (TraceJITSettings.InterpretADDI || TraceJITSettings.InterpretADDIU) {
		return x86_64_emit_interpret(state, &TJ_ADDI, 0);
	} else {
		return x86_64_emit_addiu(state);
	}
}

static enum TJEmitTraceResult x86_64_emit_slti(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_SLTI, 0);
}

static enum TJEmitTraceResult x86_64_emit_sltiu(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_SLTIU, 0);
}

static enum TJEmitTraceResult x86_64_emit_andi(struct x86_64_state* state)
{
	if (TraceJITSettings.InterpretANDI) {
		return x86_64_emit_interpret(state, &TJ_ANDI, 0);
	} else {
		uint8_t rs = RS_OF(state->ops[0]), rt = RT_OF(state->ops[0]);
		uint16_t imm = IMM16U_OF(state->ops[0]);
		x86_64_ensure_RDX_has_regs(state);
		x86_64_mov_r64_m64rosi8(state, RAX, RDX, REG_OFFSET(rs));
		if (imm < UINT16_C(0x80)) {
			x86_64_and_r64_si8(state, RAX, (int8_t) imm);
		} else {
			x86_64_and_r64_si32(state, RAX, (int32_t) imm);
		}
		x86_64_mov_m64rosi8_r64(state, RDX, REG_OFFSET(rt), RAX);
		x86_64_next_opcode(state);
		return TJ_SUCCESS;
	}
}

static enum TJEmitTraceResult x86_64_emit_ori(struct x86_64_state* state)
{
	if (TraceJITSettings.InterpretORI) {
		return x86_64_emit_interpret(state, &TJ_ORI, 0);
	} else {
		uint8_t rs = RS_OF(state->ops[0]), rt = RT_OF(state->ops[0]);
		uint16_t imm = IMM16U_OF(state->ops[0]);
		x86_64_ensure_RDX_has_regs(state);
		x86_64_mov_r64_m64rosi8(state, RAX, RDX, REG_OFFSET(rs));
		if (imm < UINT16_C(0x80)) {
			x86_64_or_r64_si8(state, RAX, (int8_t) imm);
		} else {
			x86_64_or_r64_si32(state, RAX, (int32_t) imm);
		}
		x86_64_mov_m64rosi8_r64(state, RDX, REG_OFFSET(rt), RAX);
		x86_64_next_opcode(state);
		return TJ_SUCCESS;
	}
}

static enum TJEmitTraceResult x86_64_emit_xori(struct x86_64_state* state)
{
	if (TraceJITSettings.InterpretXORI) {
		return x86_64_emit_interpret(state, &TJ_XORI, 0);
	} else {
		uint8_t rs = RS_OF(state->ops[0]), rt = RT_OF(state->ops[0]);
		uint16_t imm = IMM16U_OF(state->ops[0]);
		x86_64_ensure_RDX_has_regs(state);
		x86_64_mov_r64_m64rosi8(state, RAX, RDX, REG_OFFSET(rs));
		if (imm < UINT16_C(0x80)) {
			x86_64_xor_r64_si8(state, RAX, (int8_t) imm);
		} else {
			x86_64_xor_r64_si32(state, RAX, (int32_t) imm);
		}
		x86_64_mov_m64rosi8_r64(state, RDX, REG_OFFSET(rt), RAX);
		x86_64_next_opcode(state);
		return TJ_SUCCESS;
	}
}

static enum TJEmitTraceResult x86_64_emit_lui(struct x86_64_state* state)
{
	if (TraceJITSettings.InterpretLUI) {
		return x86_64_emit_interpret(state, &TJ_LUI, 0);
	} else {
		uint8_t rt = RT_OF(state->ops[0]);
		int16_t imm = IMM16S_OF(state->ops[0]);
		x86_64_ensure_RDX_has_regs(state);
		x86_64_mov_r64_si32(state, RAX, (int32_t) imm << 16);
		x86_64_mov_m64rosi8_r64(state, RDX, REG_OFFSET(rt), RAX);
		x86_64_next_opcode(state);
		return TJ_SUCCESS;
	}
}

static enum TJEmitTraceResult x86_64_emit_mfc0(struct x86_64_state* state)
{
	switch (RD_OF(state->ops[0])) {
	case CP0_RANDOM_REG:
	case CP0_COUNT_REG:
		return x86_64_emit_interpret(state, &TJ_MFC0, TJ_READS_PC);
	default:
		if (TraceJITSettings.InterpretMFC0) {
			return x86_64_emit_interpret(state, &TJ_MFC0, 0);
		} else {
			uint8_t rd = RD_OF(state->ops[0]), rt = RT_OF(state->ops[0]);
			x86_64_ensure_RDX_has_regs(state);
			x86_64_mov_r64_ia(state, RAX, &g_state.regs.cp0[rd]);
			x86_64_mov_r32_m32r(state, EAX, RAX);
			x86_64_movsx_r64_r32(state, RAX, EAX);
			x86_64_mov_m64rosi8_r64(state, RDX, REG_OFFSET(rt), RAX);
			x86_64_next_opcode(state);
			return TJ_SUCCESS;
		}
	}
}

static enum TJEmitTraceResult x86_64_emit_mtc0(struct x86_64_state* state)
{
	switch (RD_OF(state->ops[0])) {
	case CP0_COUNT_REG:
	case CP0_COMPARE_REG:
	case CP0_STATUS_REG:
		return x86_64_emit_interpret(state, &TJ_MTC0, TJ_READS_PC | TJ_MAY_RAISE_INTERRUPT);
	case CP0_INDEX_REG:
	case CP0_CAUSE_REG:
		if (RT_OF(state->ops[0]) != 0) {
			return x86_64_emit_interpret(state, &TJ_MTC0, TJ_CHECK_STOP);
		} else {
			return x86_64_emit_interpret(state, &TJ_MTC0, 0);
		}
	default:
		return x86_64_emit_interpret(state, &TJ_MTC0, 0);
	}
}

static enum TJEmitTraceResult x86_64_emit_tlbr(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_TLBR, 0);
}

static enum TJEmitTraceResult x86_64_emit_tlbwi(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_TLBWI, 0);
}

static enum TJEmitTraceResult x86_64_emit_tlbwr(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_TLBWR, TJ_READS_PC);
}

static enum TJEmitTraceResult x86_64_emit_tlbp(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_TLBP, 0);
}

static enum TJEmitTraceResult x86_64_emit_eret(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_ERET, TJ_TRANSFERS_CONTROL);
}

static enum TJEmitTraceResult x86_64_emit_mfc1(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_MFC1, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_dmfc1(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_DMFC1, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_cfc1(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_CFC1, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_mtc1(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_MTC1, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_dmtc1(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_DMTC1, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_ctc1(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_CTC1, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_bc1f_idle(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_BC1F_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_bc1f(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_BC1F, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_bc1t_idle(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_BC1T_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_bc1t(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_BC1T, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_bc1fl_idle(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_BC1FL_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_bc1fl(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_BC1FL, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_bc1tl_idle(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_BC1TL_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_bc1tl(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_BC1TL, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_add_s(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_ADD_S, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_sub_s(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_SUB_S, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_mul_s(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_MUL_S, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_div_s(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_DIV_S, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_sqrt_s(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_SQRT_S, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_abs_s(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_ABS_S, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_mov_s(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_MOV_S, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_neg_s(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_NEG_S, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_round_l_s(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_ROUND_L_S, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_trunc_l_s(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_TRUNC_L_S, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_ceil_l_s(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_CEIL_L_S, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_floor_l_s(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_FLOOR_L_S, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_round_w_s(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_ROUND_W_S, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_trunc_w_s(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_TRUNC_W_S, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_ceil_w_s(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_CEIL_W_S, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_floor_w_s(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_FLOOR_W_S, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_cvt_d_s(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_CVT_D_S, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_cvt_w_s(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_CVT_W_S, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_cvt_l_s(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_CVT_L_S, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_c_f_s(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_C_F_S, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_c_un_s(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_C_UN_S, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_c_eq_s(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_C_EQ_S, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_c_ueq_s(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_C_UEQ_S, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_c_olt_s(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_C_OLT_S, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_c_ult_s(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_C_ULT_S, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_c_ole_s(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_C_OLE_S, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_c_ule_s(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_C_ULE_S, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_c_sf_s(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_C_SF_S, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_c_ngle_s(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_C_NGLE_S, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_c_seq_s(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_C_SEQ_S, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_c_ngl_s(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_C_NGL_S, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_c_lt_s(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_C_LT_S, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_c_nge_s(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_C_NGE_S, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_c_le_s(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_C_LE_S, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_c_ngt_s(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_C_NGT_S, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_add_d(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_ADD_D, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_sub_d(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_SUB_D, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_mul_d(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_MUL_D, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_div_d(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_DIV_D, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_sqrt_d(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_SQRT_D, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_abs_d(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_ABS_D, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_mov_d(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_MOV_D, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_neg_d(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_NEG_D, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_round_l_d(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_ROUND_L_D, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_trunc_l_d(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_TRUNC_L_D, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_ceil_l_d(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_CEIL_L_D, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_floor_l_d(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_FLOOR_L_D, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_round_w_d(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_ROUND_W_D, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_trunc_w_d(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_TRUNC_W_D, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_ceil_w_d(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_CEIL_W_D, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_floor_w_d(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_FLOOR_W_D, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_cvt_s_d(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_CVT_S_D, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_cvt_w_d(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_CVT_W_D, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_cvt_l_d(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_CVT_L_D, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_c_f_d(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_C_F_D, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_c_un_d(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_C_UN_D, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_c_eq_d(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_C_EQ_D, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_c_ueq_d(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_C_UEQ_D, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_c_olt_d(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_C_OLT_D, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_c_ult_d(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_C_ULT_D, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_c_ole_d(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_C_OLE_D, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_c_ule_d(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_C_ULE_D, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_c_sf_d(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_C_SF_D, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_c_ngle_d(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_C_NGLE_D, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_c_seq_d(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_C_SEQ_D, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_c_ngl_d(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_C_NGL_D, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_c_lt_d(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_C_LT_D, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_c_nge_d(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_C_NGE_D, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_c_le_d(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_C_LE_D, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_c_ngt_d(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_C_NGT_D, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_cvt_s_w(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_CVT_S_W, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_cvt_d_w(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_CVT_D_W, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_cvt_s_l(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_CVT_S_L, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_cvt_d_l(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_CVT_D_L, TJ_MAY_RAISE_COP1_UNUSABLE);
}

static enum TJEmitTraceResult x86_64_emit_beql_idle(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_BEQL_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_beql(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_BEQL, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_bnel_idle(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_BNEL_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_bnel(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_BNEL, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_blezl_idle(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_BLEZL_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_blezl(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_BLEZL, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_bgtzl_idle(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_BGTZL_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_bgtzl(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_BGTZL, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

static enum TJEmitTraceResult x86_64_emit_daddiu(struct x86_64_state* state)
{
	if (TraceJITSettings.InterpretDADDIU) {
		return x86_64_emit_interpret(state, &TJ_DADDIU, 0);
	} else {
		uint8_t rs = RS_OF(state->ops[0]), rt = RT_OF(state->ops[0]);
		int16_t imm = IMM16S_OF(state->ops[0]);
		x86_64_ensure_RDX_has_regs(state);
		x86_64_mov_r64_m64rosi8(state, RAX, RDX, REG_OFFSET(rs));
		if (imm == (int16_t) (int8_t) imm) {
			x86_64_add_r64_si8(state, RAX, (int8_t) imm);
		} else {
			x86_64_add_r64_si32(state, RAX, (int32_t) imm);
		}
		x86_64_mov_m64rosi8_r64(state, RDX, REG_OFFSET(rt), RAX);
		x86_64_next_opcode(state);
		return TJ_SUCCESS;
	}
}

static enum TJEmitTraceResult x86_64_emit_daddi(struct x86_64_state* state)
{
	if (TraceJITSettings.InterpretDADDI || TraceJITSettings.InterpretDADDIU) {
		return x86_64_emit_interpret(state, &TJ_DADDI, 0);
	} else {
		return x86_64_emit_daddiu(state);
	}
}

static enum TJEmitTraceResult x86_64_emit_ldl(struct x86_64_state* state)
{
	uint32_t addr;
	if (IntIsKnown(&state->consts, RS_OF(state->ops[0]))) {
		addr = IntGetKnownBits(&state->consts, RS_OF(state->ops[0])) + IMM16S_OF(state->ops[0]);
		if ((addr & UINT32_C(0xDF800000)) == UINT32_C(0x80000000)) {
			return x86_64_emit_interpret(state, &TJ_LDL, 0);
		} else {
			return x86_64_emit_interpret(state, &TJ_LDL, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
		}
	} else {
		return x86_64_emit_interpret(state, &TJ_LDL, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
	}
}

static enum TJEmitTraceResult x86_64_emit_ldr(struct x86_64_state* state)
{
	uint32_t addr;
	if (IntIsKnown(&state->consts, RS_OF(state->ops[0]))) {
		addr = IntGetKnownBits(&state->consts, RS_OF(state->ops[0])) + IMM16S_OF(state->ops[0]);
		if ((addr & UINT32_C(0xDF800000)) == UINT32_C(0x80000000)) {
			return x86_64_emit_interpret(state, &TJ_LDR, 0);
		} else {
			return x86_64_emit_interpret(state, &TJ_LDR, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
		}
	} else {
		return x86_64_emit_interpret(state, &TJ_LDR, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
	}
}

static enum TJEmitTraceResult x86_64_emit_lb(struct x86_64_state* state)
{
	uint32_t addr;
	if (IntIsKnown(&state->consts, RS_OF(state->ops[0]))) {
		addr = IntGetKnownBits(&state->consts, RS_OF(state->ops[0])) + IMM16S_OF(state->ops[0]);
		if ((addr & UINT32_C(0xDF800000)) == UINT32_C(0x80000000)) {
			if (TraceJITSettings.InterpretLB || !fast_memory) {
				return x86_64_emit_interpret(state, &TJ_LB, 0);
			} else {
				uint8_t rt = RT_OF(state->ops[0]);
				x86_64_ensure_RDX_has_regs(state);
				x86_64_mov_r64_ia(state, RAX, (uint8_t*) g_rdram + ((addr & UINT32_C(0x7FFFFF)) ^ S8));
				x86_64_mov_r8_m8r(state, AL, RAX);
				x86_64_movsx_r64_r8(state, RAX, AL);
				x86_64_mov_m64rosi8_r64(state, RDX, REG_OFFSET(rt), RAX);
				x86_64_next_opcode(state);
				return TJ_SUCCESS;
			}
		} else {
			return x86_64_emit_interpret(state, &TJ_LB, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
		}
	} else {
		return x86_64_emit_interpret(state, &TJ_LB, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
	}
}

static enum TJEmitTraceResult x86_64_emit_lh(struct x86_64_state* state)
{
	uint32_t addr;
	if (IntIsKnown(&state->consts, RS_OF(state->ops[0]))) {
		addr = IntGetKnownBits(&state->consts, RS_OF(state->ops[0])) + IMM16S_OF(state->ops[0]);
		if ((addr & UINT32_C(0xDF800000)) == UINT32_C(0x80000000)) {
			if (TraceJITSettings.InterpretLH || !fast_memory) {
				return x86_64_emit_interpret(state, &TJ_LH, 0);
			} else {
				uint8_t rt = RT_OF(state->ops[0]);
				x86_64_ensure_RDX_has_regs(state);
				x86_64_mov_r64_ia(state, RAX, (uint8_t*) g_rdram + ((addr & UINT32_C(0x7FFFFE)) ^ S16));
				x86_64_mov_r16_m16r(state, AX, RAX);
				x86_64_movsx_r64_r16(state, RAX, AX);
				x86_64_mov_m64rosi8_r64(state, RDX, REG_OFFSET(rt), RAX);
				x86_64_next_opcode(state);
				return TJ_SUCCESS;
			}
		} else {
			return x86_64_emit_interpret(state, &TJ_LH, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
		}
	} else {
		return x86_64_emit_interpret(state, &TJ_LH, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
	}
}

static enum TJEmitTraceResult x86_64_emit_lwl(struct x86_64_state* state)
{
	uint32_t addr;
	if (IntIsKnown(&state->consts, RS_OF(state->ops[0]))) {
		addr = IntGetKnownBits(&state->consts, RS_OF(state->ops[0])) + IMM16S_OF(state->ops[0]);
		if ((addr & UINT32_C(0xDF800000)) == UINT32_C(0x80000000)) {
			return x86_64_emit_interpret(state, &TJ_LWL, 0);
		} else {
			return x86_64_emit_interpret(state, &TJ_LWL, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
		}
	} else {
		return x86_64_emit_interpret(state, &TJ_LWL, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
	}
}

static enum TJEmitTraceResult x86_64_emit_lw(struct x86_64_state* state)
{
	uint32_t addr;
	if (IntIsKnown(&state->consts, RS_OF(state->ops[0]))) {
		addr = IntGetKnownBits(&state->consts, RS_OF(state->ops[0])) + IMM16S_OF(state->ops[0]);
		if ((addr & UINT32_C(0xDF800000)) == UINT32_C(0x80000000)) {
			if (TraceJITSettings.InterpretLW || !fast_memory) {
				return x86_64_emit_interpret(state, &TJ_LW, 0);
			} else {
				uint8_t rt = RT_OF(state->ops[0]);
				x86_64_ensure_RDX_has_regs(state);
				x86_64_mov_r64_ia(state, RAX, (uint8_t*) g_rdram + (addr & UINT32_C(0x7FFFFC)));
				x86_64_mov_r32_m32r(state, EAX, RAX);
				x86_64_movsx_r64_r32(state, RAX, EAX);
				x86_64_mov_m64rosi8_r64(state, RDX, REG_OFFSET(rt), RAX);
				x86_64_next_opcode(state);
				return TJ_SUCCESS;
			}
		} else {
			return x86_64_emit_interpret(state, &TJ_LW, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
		}
	} else {
		return x86_64_emit_interpret(state, &TJ_LW, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
	}
}

static enum TJEmitTraceResult x86_64_emit_lbu(struct x86_64_state* state)
{
	uint32_t addr;
	if (IntIsKnown(&state->consts, RS_OF(state->ops[0]))) {
		addr = IntGetKnownBits(&state->consts, RS_OF(state->ops[0])) + IMM16S_OF(state->ops[0]);
		if ((addr & UINT32_C(0xDF800000)) == UINT32_C(0x80000000)) {
			if (TraceJITSettings.InterpretLBU || !fast_memory) {
				return x86_64_emit_interpret(state, &TJ_LBU, 0);
			} else {
				uint8_t rt = RT_OF(state->ops[0]);
				x86_64_ensure_RDX_has_regs(state);
				x86_64_mov_r64_ia(state, RAX, (uint8_t*) g_rdram + ((addr & UINT32_C(0x7FFFFF)) ^ S8));
				x86_64_mov_r8_m8r(state, AL, RAX);
				x86_64_movzx_r64_r8(state, RAX, AL);
				x86_64_mov_m64rosi8_r64(state, RDX, REG_OFFSET(rt), RAX);
				x86_64_next_opcode(state);
				return TJ_SUCCESS;
			}
		} else {
			return x86_64_emit_interpret(state, &TJ_LBU, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
		}
	} else {
		return x86_64_emit_interpret(state, &TJ_LBU, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
	}
}

static enum TJEmitTraceResult x86_64_emit_lhu(struct x86_64_state* state)
{
	uint32_t addr;
	if (IntIsKnown(&state->consts, RS_OF(state->ops[0]))) {
		addr = IntGetKnownBits(&state->consts, RS_OF(state->ops[0])) + IMM16S_OF(state->ops[0]);
		if ((addr & UINT32_C(0xDF800000)) == UINT32_C(0x80000000)) {
			if (TraceJITSettings.InterpretLHU || !fast_memory) {
				return x86_64_emit_interpret(state, &TJ_LHU, 0);
			} else {
				uint8_t rt = RT_OF(state->ops[0]);
				x86_64_ensure_RDX_has_regs(state);
				x86_64_mov_r64_ia(state, RAX, (uint8_t*) g_rdram + ((addr & UINT32_C(0x7FFFFE)) ^ S16));
				x86_64_mov_r16_m16r(state, AX, RAX);
				x86_64_movzx_r64_r16(state, RAX, AX);
				x86_64_mov_m64rosi8_r64(state, RDX, REG_OFFSET(rt), RAX);
				x86_64_next_opcode(state);
				return TJ_SUCCESS;
			}
		} else {
			return x86_64_emit_interpret(state, &TJ_LHU, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
		}
	} else {
		return x86_64_emit_interpret(state, &TJ_LHU, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
	}
}

static enum TJEmitTraceResult x86_64_emit_lwr(struct x86_64_state* state)
{
	uint32_t addr;
	if (IntIsKnown(&state->consts, RS_OF(state->ops[0]))) {
		addr = IntGetKnownBits(&state->consts, RS_OF(state->ops[0])) + IMM16S_OF(state->ops[0]);
		if ((addr & UINT32_C(0xDF800000)) == UINT32_C(0x80000000)) {
			return x86_64_emit_interpret(state, &TJ_LWR, 0);
		} else {
			return x86_64_emit_interpret(state, &TJ_LWR, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
		}
	} else {
		return x86_64_emit_interpret(state, &TJ_LWR, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
	}
}

static enum TJEmitTraceResult x86_64_emit_lwu(struct x86_64_state* state)
{
	uint32_t addr;
	if (IntIsKnown(&state->consts, RS_OF(state->ops[0]))) {
		addr = IntGetKnownBits(&state->consts, RS_OF(state->ops[0])) + IMM16S_OF(state->ops[0]);
		if ((addr & UINT32_C(0xDF800000)) == UINT32_C(0x80000000)) {
			if (TraceJITSettings.InterpretLWU || !fast_memory) {
				return x86_64_emit_interpret(state, &TJ_LWU, 0);
			} else {
				uint8_t rt = RT_OF(state->ops[0]);
				x86_64_ensure_RDX_has_regs(state);
				x86_64_mov_r64_ia(state, RAX, (uint8_t*) g_rdram + (addr & UINT32_C(0x7FFFFC)));
				x86_64_mov_r32_m32r(state, EAX, RAX);
				x86_64_mov_m64rosi8_r64(state, RDX, REG_OFFSET(rt), RAX);
				x86_64_next_opcode(state);
				return TJ_SUCCESS;
			}
		} else {
			return x86_64_emit_interpret(state, &TJ_LWU, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
		}
	} else {
		return x86_64_emit_interpret(state, &TJ_LWU, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
	}
}

static enum TJEmitTraceResult x86_64_emit_sb(struct x86_64_state* state)
{
	uint32_t addr;
	if (IntIsKnown(&state->consts, RS_OF(state->ops[0]))) {
		addr = IntGetKnownBits(&state->consts, RS_OF(state->ops[0])) + IMM16S_OF(state->ops[0]);
		if ((addr & UINT32_C(0xDF800000)) == UINT32_C(0x80000000)) {
			return x86_64_emit_interpret(state, &TJ_SB, 0);
		} else {
			return x86_64_emit_interpret(state, &TJ_SB, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
		}
	} else {
		return x86_64_emit_interpret(state, &TJ_SB, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
	}
}

static enum TJEmitTraceResult x86_64_emit_sh(struct x86_64_state* state)
{
	uint32_t addr;
	if (IntIsKnown(&state->consts, RS_OF(state->ops[0]))) {
		addr = IntGetKnownBits(&state->consts, RS_OF(state->ops[0])) + IMM16S_OF(state->ops[0]);
		if ((addr & UINT32_C(0xDF800000)) == UINT32_C(0x80000000)) {
			return x86_64_emit_interpret(state, &TJ_SH, 0);
		} else {
			return x86_64_emit_interpret(state, &TJ_SH, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
		}
	} else {
		return x86_64_emit_interpret(state, &TJ_SH, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
	}
}

static enum TJEmitTraceResult x86_64_emit_swl(struct x86_64_state* state)
{
	uint32_t addr;
	if (IntIsKnown(&state->consts, RS_OF(state->ops[0]))) {
		addr = IntGetKnownBits(&state->consts, RS_OF(state->ops[0])) + IMM16S_OF(state->ops[0]);
		if ((addr & UINT32_C(0xDF800000)) == UINT32_C(0x80000000)) {
			return x86_64_emit_interpret(state, &TJ_SWL, 0);
		} else {
			return x86_64_emit_interpret(state, &TJ_SWL, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
		}
	} else {
		return x86_64_emit_interpret(state, &TJ_SWL, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
	}
}

static enum TJEmitTraceResult x86_64_emit_sw(struct x86_64_state* state)
{
	uint32_t addr;
	if (IntIsKnown(&state->consts, RS_OF(state->ops[0]))) {
		addr = IntGetKnownBits(&state->consts, RS_OF(state->ops[0])) + IMM16S_OF(state->ops[0]);
		if ((addr & UINT32_C(0xDF800000)) == UINT32_C(0x80000000)) {
			return x86_64_emit_interpret(state, &TJ_SW, 0);
		} else {
			return x86_64_emit_interpret(state, &TJ_SW, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
		}
	} else {
		return x86_64_emit_interpret(state, &TJ_SW, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
	}
}

static enum TJEmitTraceResult x86_64_emit_sdl(struct x86_64_state* state)
{
	uint32_t addr;
	if (IntIsKnown(&state->consts, RS_OF(state->ops[0]))) {
		addr = IntGetKnownBits(&state->consts, RS_OF(state->ops[0])) + IMM16S_OF(state->ops[0]);
		if ((addr & UINT32_C(0xDF800000)) == UINT32_C(0x80000000)) {
			return x86_64_emit_interpret(state, &TJ_SDL, 0);
		} else {
			return x86_64_emit_interpret(state, &TJ_SDL, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
		}
	} else {
		return x86_64_emit_interpret(state, &TJ_SDL, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
	}
}

static enum TJEmitTraceResult x86_64_emit_sdr(struct x86_64_state* state)
{
	uint32_t addr;
	if (IntIsKnown(&state->consts, RS_OF(state->ops[0]))) {
		addr = IntGetKnownBits(&state->consts, RS_OF(state->ops[0])) + IMM16S_OF(state->ops[0]);
		if ((addr & UINT32_C(0xDF800000)) == UINT32_C(0x80000000)) {
			return x86_64_emit_interpret(state, &TJ_SDR, 0);
		} else {
			return x86_64_emit_interpret(state, &TJ_SDR, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
		}
	} else {
		return x86_64_emit_interpret(state, &TJ_SDR, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
	}
}

static enum TJEmitTraceResult x86_64_emit_swr(struct x86_64_state* state)
{
	uint32_t addr;
	if (IntIsKnown(&state->consts, RS_OF(state->ops[0]))) {
		addr = IntGetKnownBits(&state->consts, RS_OF(state->ops[0])) + IMM16S_OF(state->ops[0]);
		if ((addr & UINT32_C(0xDF800000)) == UINT32_C(0x80000000)) {
			return x86_64_emit_interpret(state, &TJ_SWR, 0);
		} else {
			return x86_64_emit_interpret(state, &TJ_SWR, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
		}
	} else {
		return x86_64_emit_interpret(state, &TJ_SWR, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
	}
}

static enum TJEmitTraceResult x86_64_emit_cache(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_CACHE, 0);
}

static enum TJEmitTraceResult x86_64_emit_ll(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_LL, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
}

static enum TJEmitTraceResult x86_64_emit_lwc1(struct x86_64_state* state)
{
	uint32_t addr;
	if (IntIsKnown(&state->consts, RS_OF(state->ops[0]))) {
		addr = IntGetKnownBits(&state->consts, RS_OF(state->ops[0])) + IMM16S_OF(state->ops[0]);
		if ((addr & UINT32_C(0xDF800000)) == UINT32_C(0x80000000)) {
			return x86_64_emit_interpret(state, &TJ_LWC1, TJ_MAY_RAISE_COP1_UNUSABLE);
		} else {
			return x86_64_emit_interpret(state, &TJ_LWC1, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL | TJ_MAY_RAISE_COP1_UNUSABLE);
		}
	} else {
		return x86_64_emit_interpret(state, &TJ_LWC1, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL | TJ_MAY_RAISE_COP1_UNUSABLE);
	}
}

static enum TJEmitTraceResult x86_64_emit_ldc1(struct x86_64_state* state)
{
	uint32_t addr;
	if (IntIsKnown(&state->consts, RS_OF(state->ops[0]))) {
		addr = IntGetKnownBits(&state->consts, RS_OF(state->ops[0])) + IMM16S_OF(state->ops[0]);
		if ((addr & UINT32_C(0xDF800000)) == UINT32_C(0x80000000)) {
			return x86_64_emit_interpret(state, &TJ_LDC1, TJ_MAY_RAISE_COP1_UNUSABLE);
		} else {
			return x86_64_emit_interpret(state, &TJ_LDC1, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL | TJ_MAY_RAISE_COP1_UNUSABLE);
		}
	} else {
		return x86_64_emit_interpret(state, &TJ_LDC1, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL | TJ_MAY_RAISE_COP1_UNUSABLE);
	}
}

static enum TJEmitTraceResult x86_64_emit_ld(struct x86_64_state* state)
{
	uint32_t addr;
	if (IntIsKnown(&state->consts, RS_OF(state->ops[0]))) {
		addr = IntGetKnownBits(&state->consts, RS_OF(state->ops[0])) + IMM16S_OF(state->ops[0]);
		if ((addr & UINT32_C(0xDF800000)) == UINT32_C(0x80000000)) {
			return x86_64_emit_interpret(state, &TJ_LD, 0);
		} else {
			return x86_64_emit_interpret(state, &TJ_LD, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
		}
	} else {
		return x86_64_emit_interpret(state, &TJ_LD, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
	}
}

static enum TJEmitTraceResult x86_64_emit_sc(struct x86_64_state* state)
{
	return x86_64_emit_interpret(state, &TJ_SC, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
}

static enum TJEmitTraceResult x86_64_emit_swc1(struct x86_64_state* state)
{
	uint32_t addr;
	if (IntIsKnown(&state->consts, RS_OF(state->ops[0]))) {
		addr = IntGetKnownBits(&state->consts, RS_OF(state->ops[0])) + IMM16S_OF(state->ops[0]);
		if ((addr & UINT32_C(0xDF800000)) == UINT32_C(0x80000000)) {
			return x86_64_emit_interpret(state, &TJ_SWC1, TJ_MAY_RAISE_COP1_UNUSABLE);
		} else {
			return x86_64_emit_interpret(state, &TJ_SWC1, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL | TJ_MAY_RAISE_COP1_UNUSABLE);
		}
	} else {
		return x86_64_emit_interpret(state, &TJ_SWC1, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL | TJ_MAY_RAISE_COP1_UNUSABLE);
	}
}

static enum TJEmitTraceResult x86_64_emit_sdc1(struct x86_64_state* state)
{
	uint32_t addr;
	if (IntIsKnown(&state->consts, RS_OF(state->ops[0]))) {
		addr = IntGetKnownBits(&state->consts, RS_OF(state->ops[0])) + IMM16S_OF(state->ops[0]);
		if ((addr & UINT32_C(0xDF800000)) == UINT32_C(0x80000000)) {
			return x86_64_emit_interpret(state, &TJ_SDC1, TJ_MAY_RAISE_COP1_UNUSABLE);
		} else {
			return x86_64_emit_interpret(state, &TJ_SDC1, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL | TJ_MAY_RAISE_COP1_UNUSABLE);
		}
	} else {
		return x86_64_emit_interpret(state, &TJ_SDC1, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL | TJ_MAY_RAISE_COP1_UNUSABLE);
	}
}

static enum TJEmitTraceResult x86_64_emit_sd(struct x86_64_state* state)
{
	uint32_t addr;
	if (IntIsKnown(&state->consts, RS_OF(state->ops[0]))) {
		addr = IntGetKnownBits(&state->consts, RS_OF(state->ops[0])) + IMM16S_OF(state->ops[0]);
		if ((addr & UINT32_C(0xDF800000)) == UINT32_C(0x80000000)) {
			return x86_64_emit_interpret(state, &TJ_SD, 0);
		} else {
			return x86_64_emit_interpret(state, &TJ_SD, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
		}
	} else {
		return x86_64_emit_interpret(state, &TJ_SD, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
	}
}
