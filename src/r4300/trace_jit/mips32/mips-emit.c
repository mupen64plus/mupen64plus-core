/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - MIPS (Nintendo 64) emission flags and functions         *
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
#include <string.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "mips-alu.h"
#include "mips-branch.h"
#include "mips-cp0.h"
#include "mips-cp1.h"
#include "mips-emit.h"
#include "mips-except.h"
#include "mips-mdu.h"
#include "mips-memory.h"
#include "native-endian.h"
#include "native-ops.h"
#include "native-regcache.h"
#include "native-utils.h"

#include "../mips-interp.h"
#include "../mips-jit.h"
#include "../mips-parse.h"
#include "../native-tracecache.h"

#include "../../r4300.h"

/* Determines whether a relative jump in a 16-bit immediate goes back to the
 * same instruction without doing any work in its delay slot. The jump is
 * relative to the instruction in the delay slot, so 1 instruction backwards
 * (-1) goes back to the jump.
 * The delay slot MUST also be included in the trace. */
#define IS_RELATIVE_IDLE_LOOP(state) \
	(IMM16S_OF((state)->ops[0]) == -1 \
	 && (state)->op_count >= 2 \
	 && (state)->ops[1] == 0)

/* Determines whether an absolute jump in a 26-bit immediate goes back to the
 * same instruction without doing any work in its delay slot. The jump is
 * in the same 256 MiB segment as the delay slot, so if the jump instruction
 * is at the last address in its segment, it does not jump back to itself.
 * The delay slot MUST also be included in the trace. */
#define IS_ABSOLUTE_IDLE_LOOP(state) \
	(JUMP_OF((state)->ops[0]) == ((state)->pc & UINT32_C(0x0FFFFFFF)) >> 2 \
	 && ((state)->pc & UINT32_C(0x0FFFFFFF)) != UINT32_C(0x0FFFFFFC) \
	 && (state)->op_count >= 2 \
	 && (state)->ops[1] == 0)

enum TJEmitTraceResult mips32_add_except_reloc(struct mips32_state* state)
{
	void** new_except_relocs = realloc(state->except_relocs, (state->except_reloc_count + 1) * sizeof(void*));
	if (new_except_relocs) {
		state->except_relocs = new_except_relocs;
		new_except_relocs[state->except_reloc_count++] = (uint32_t*) state->code - 1;
		return TJ_SUCCESS;
	} else {
		DebugMessage(M64MSG_ERROR, "There is insufficient memory for auxiliary data structures required to emit native code.");
		return TJ_FAILURE;
	}
}

enum TJEmitTraceResult mips32_next_opcode(struct mips32_state* state)
{
	if (state->op_count > 0) {
		UpdateConstAnalysis(&state->consts, state->ops[0]);
		UpdateWidthAnalysis(&state->widths, state->ops[0]);
		state->pc += N64_INSN_SIZE;
		state->ops++;
		state->op_count--;
	}
	return (state->code) ? TJ_SUCCESS : TJ_MEMORY_ERROR;
}

enum TJEmitTraceResult mips32_emit_interpret(struct mips32_state* state, struct mips32_reg_cache* cache, void (*func) (uint32_t), int flags)
{
	uint32_t op = state->ops[0];

	mips32_free_all(state, cache);

	/* If the current PC is not synced anymore with TJ_PC.addr, and the
	 * interpreter function reads it... */
	if (state->stored_pc != state->pc && (flags & TJ_READS_PC)) {
		/* Sync it. */
		mips32_i32(state, 8, state->pc);
		mips32_sw_abs(state, 8, 9, &TJ_PC.addr);
		state->stored_pc = state->pc;
	}

	/* Store the op to the first parameter register. */
	/* NOP does not really require any valid opcode. */
	if (op != 0) {
		mips32_i32(state, REG_ARG1, op);
	}
	mips32_pic_call(state, func);

	state->stored_pc += N64_INSN_SIZE;
	FAIL_AS(mips32_next_opcode(state));

	/* If required, check for unexpected PC changes. */
	if (flags & (TJ_MAY_RAISE_TLB_REFILL | TJ_MAY_RAISE_INTERRUPT)) {
		/* Jump to the end of the block if an exception occurred, which is
		 * defined here as TJ_PC.addr not matching what we expect. */
		mips32_lw_abs(state, 9, 9, &TJ_PC.addr);
		mips32_i32(state, 8, state->pc);
		mips32_bne(state, 8, 9, +0);
		FAIL_AS(mips32_add_except_reloc(state));
		mips32_nop(state);
	}
	if (flags & TJ_CHECK_STOP) {
		/* Jump to the end of the block if stop became non-zero. */
		mips32_lw_abs(state, 9, 9, &stop);
		mips32_bne(state, 9, 0, +0);
		FAIL_AS(mips32_add_except_reloc(state));
		mips32_nop(state);
	}
	if (flags & TJ_HAS_DELAY_SLOT) {
		state->stored_pc += N64_INSN_SIZE;
		FAIL_AS(mips32_next_opcode(state));
	}
	if (flags & TJ_TRANSFERS_CONTROL) {
		state->fallthrough = false;
	}

	return TJ_SUCCESS;
}

enum TJEmitTraceResult mips32_emit_opcode(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	uint32_t op = state->ops[0];
	switch ((op >> 26) & 0x3F) {
	case 0: /* SPECIAL prefix */
		switch (op & 0x3F) {
		case 0: /* SPECIAL opcode 0: SLL */
			/* SLL is a special case: it must check whether it's writing to
			 * $0, because the encoding for NOP is an SLL. */
			if (RD_OF(op) != 0) return mips32_emit_sll(state, cache);
			else                return mips32_emit_nop(state, cache);
		case 2: return mips32_emit_srl(state, cache);
		case 3: return mips32_emit_sra(state, cache);
		case 4: return mips32_emit_sllv(state, cache);
		case 6: return mips32_emit_srlv(state, cache);
		case 7: return mips32_emit_srav(state, cache);
		case 8: return mips32_emit_jr(state, cache);
		case 9: return mips32_emit_jalr(state, cache);
		case 12: return mips32_emit_syscall(state, cache);
		case 13: /* SPECIAL opcode 13: BREAK (Not implemented) */
			return mips32_emit_ni(state, cache);
		case 15: return mips32_emit_sync(state, cache);
		case 16: return mips32_emit_mfhi(state, cache);
		case 17: return mips32_emit_mthi(state, cache);
		case 18: return mips32_emit_mflo(state, cache);
		case 19: return mips32_emit_mtlo(state, cache);
		case 20: return mips32_emit_dsllv(state, cache);
		case 22: return mips32_emit_dsrlv(state, cache);
		case 23: return mips32_emit_dsrav(state, cache);
		case 24: return mips32_emit_mult(state, cache);
		case 25: return mips32_emit_multu(state, cache);
		case 26: return mips32_emit_div(state, cache);
		case 27: return mips32_emit_divu(state, cache);
		case 28: return mips32_emit_dmult(state, cache);
		case 29: return mips32_emit_dmultu(state, cache);
		case 30: return mips32_emit_ddiv(state, cache);
		case 31: return mips32_emit_ddivu(state, cache);
		case 32: return mips32_emit_add(state, cache);
		case 33: return mips32_emit_addu(state, cache);
		case 34: return mips32_emit_sub(state, cache);
		case 35: return mips32_emit_subu(state, cache);
		case 36: return mips32_emit_and(state, cache);
		case 37: return mips32_emit_or(state, cache);
		case 38: return mips32_emit_xor(state, cache);
		case 39: return mips32_emit_nor(state, cache);
		case 42: return mips32_emit_slt(state, cache);
		case 43: return mips32_emit_sltu(state, cache);
		case 44: return mips32_emit_dadd(state, cache);
		case 45: return mips32_emit_daddu(state, cache);
		case 46: return mips32_emit_dsub(state, cache);
		case 47: return mips32_emit_dsubu(state, cache);
		case 48: /* SPECIAL opcode 48: TGE (Not implemented) */
		case 49: /* SPECIAL opcode 49: TGEU (Not implemented) */
		case 50: /* SPECIAL opcode 50: TLT (Not implemented) */
		case 51: /* SPECIAL opcode 51: TLTU (Not implemented) */
			return mips32_emit_ni(state, cache);
		case 52: return mips32_emit_teq(state, cache);
		case 54: /* SPECIAL opcode 54: TNE (Not implemented) */
			return mips32_emit_ni(state, cache);
		case 56: return mips32_emit_dsll(state, cache);
		case 58: return mips32_emit_dsrl(state, cache);
		case 59: return mips32_emit_dsra(state, cache);
		case 60: return mips32_emit_dsll32(state, cache);
		case 62: return mips32_emit_dsrl32(state, cache);
		case 63: return mips32_emit_dsra32(state, cache);
		default: /* SPECIAL opcodes 1, 5, 10, 11, 14, 21, 40, 41, 53, 55, 57,
		            61: Reserved Instructions */
			return mips32_emit_reserved(state, cache);
		} /* switch (op & 0x3F) for the SPECIAL prefix */
	case 1: /* REGIMM prefix */
		switch ((op >> 16) & 0x1F) {
		case 0: /* REGIMM opcode 0: BLTZ */
			if (IS_RELATIVE_IDLE_LOOP(state)) return mips32_emit_bltz_idle(state, cache);
			else                              return mips32_emit_bltz(state, cache);
		case 1: /* REGIMM opcode 1: BGEZ */
			if (IS_RELATIVE_IDLE_LOOP(state)) return mips32_emit_bgez_idle(state, cache);
			else                              return mips32_emit_bgez(state, cache);
		case 2: /* REGIMM opcode 2: BLTZL */
			if (IS_RELATIVE_IDLE_LOOP(state)) return mips32_emit_bltzl_idle(state, cache);
			else                              return mips32_emit_bltzl(state, cache);
		case 3: /* REGIMM opcode 3: BGEZL */
			if (IS_RELATIVE_IDLE_LOOP(state)) return mips32_emit_bgezl_idle(state, cache);
			else                              return mips32_emit_bgezl(state, cache);
		case 8: /* REGIMM opcode 8: TGEI (Not implemented) */
		case 9: /* REGIMM opcode 9: TGEIU (Not implemented) */
		case 10: /* REGIMM opcode 10: TLTI (Not implemented) */
		case 11: /* REGIMM opcode 11: TLTIU (Not implemented) */
		case 12: /* REGIMM opcode 12: TEQI (Not implemented) */
		case 14: /* REGIMM opcode 14: TNEI (Not implemented) */
			return mips32_emit_ni(state, cache);
		case 16: /* REGIMM opcode 16: BLTZAL */
			if (IS_RELATIVE_IDLE_LOOP(state)) return mips32_emit_bltzal_idle(state, cache);
			else                              return mips32_emit_bltzal(state, cache);
		case 17: /* REGIMM opcode 17: BGEZAL */
			if (IS_RELATIVE_IDLE_LOOP(state)) return mips32_emit_bgezal_idle(state, cache);
			else                              return mips32_emit_bgezal(state, cache);
		case 18: /* REGIMM opcode 18: BLTZALL */
			if (IS_RELATIVE_IDLE_LOOP(state)) return mips32_emit_bltzall_idle(state, cache);
			else                              return mips32_emit_bltzall(state, cache);
		case 19: /* REGIMM opcode 19: BGEZALL */
			if (IS_RELATIVE_IDLE_LOOP(state)) return mips32_emit_bgezall_idle(state, cache);
			else                              return mips32_emit_bgezall(state, cache);
		default: /* REGIMM opcodes 4..7, 13, 15, 20..31:
		            Reserved Instructions */
			return mips32_emit_reserved(state, cache);
		} /* switch ((op >> 16) & 0x1F) for the REGIMM prefix */
	case 2: /* Major opcode 2: J */
		if (IS_ABSOLUTE_IDLE_LOOP(state)) return mips32_emit_j_idle(state, cache);
		else                              return mips32_emit_j(state, cache);
	case 3: /* Major opcode 3: JAL */
		if (IS_ABSOLUTE_IDLE_LOOP(state)) return mips32_emit_jal_idle(state, cache);
		else                              return mips32_emit_jal(state, cache);
	case 4: /* Major opcode 4: BEQ */
		if (IS_RELATIVE_IDLE_LOOP(state)) return mips32_emit_beq_idle(state, cache);
		else                              return mips32_emit_beq(state, cache);
	case 5: /* Major opcode 5: BNE */
		if (IS_RELATIVE_IDLE_LOOP(state)) return mips32_emit_bne_idle(state, cache);
		else                              return mips32_emit_bne(state, cache);
	case 6: /* Major opcode 6: BLEZ */
		if (IS_RELATIVE_IDLE_LOOP(state)) return mips32_emit_blez_idle(state, cache);
		else                              return mips32_emit_blez(state, cache);
	case 7: /* Major opcode 7: BGTZ */
		if (IS_RELATIVE_IDLE_LOOP(state)) return mips32_emit_bgtz_idle(state, cache);
		else                              return mips32_emit_bgtz(state, cache);
	case 8: return mips32_emit_addi(state, cache);
	case 9: return mips32_emit_addiu(state, cache);
	case 10: return mips32_emit_slti(state, cache);
	case 11: return mips32_emit_sltiu(state, cache);
	case 12: return mips32_emit_andi(state, cache);
	case 13: return mips32_emit_ori(state, cache);
	case 14: return mips32_emit_xori(state, cache);
	case 15: return mips32_emit_lui(state, cache);
	case 16: /* Coprocessor 0 prefix */
		switch ((op >> 21) & 0x1F) {
		case 0: return mips32_emit_mfc0(state, cache);
		case 4: return mips32_emit_mtc0(state, cache);
		case 16: /* Coprocessor 0 opcode 16: TLB */
			switch (op & 0x3F) {
			case 1: return mips32_emit_tlbr(state, cache);
			case 2: return mips32_emit_tlbwi(state, cache);
			case 6: return mips32_emit_tlbwr(state, cache);
			case 8: return mips32_emit_tlbp(state, cache);
			case 24: return mips32_emit_eret(state, cache);
			default: /* TLB sub-opcodes 0, 3..5, 7, 9..23, 25..63:
			            Reserved Instructions */
				return mips32_emit_reserved(state, cache);
			} /* switch (op & 0x3F) for Coprocessor 0 TLB opcodes */
		default: /* Coprocessor 0 opcodes 1..3, 4..15, 17..31:
		            Reserved Instructions */
			return mips32_emit_reserved(state, cache);
		} /* switch ((op >> 21) & 0x1F) for the Coprocessor 0 prefix */
	case 17: /* Coprocessor 1 prefix */
		switch ((op >> 21) & 0x1F) {
		case 0: return mips32_emit_mfc1(state, cache);
		case 1: return mips32_emit_dmfc1(state, cache);
		case 2: return mips32_emit_cfc1(state, cache);
		case 4: return mips32_emit_mtc1(state, cache);
		case 5: return mips32_emit_dmtc1(state, cache);
		case 6: return mips32_emit_ctc1(state, cache);
		case 8: /* Coprocessor 1 opcode 8: Branch on C1 condition... */
			switch ((op >> 16) & 0x3) {
			case 0: /* opcode 0: BC1F */
				if (IS_RELATIVE_IDLE_LOOP(state)) return mips32_emit_bc1f_idle(state, cache);
				else                              return mips32_emit_bc1f(state, cache);
			case 1: /* opcode 1: BC1T */
				if (IS_RELATIVE_IDLE_LOOP(state)) return mips32_emit_bc1t_idle(state, cache);
				else                              return mips32_emit_bc1t(state, cache);
			case 2: /* opcode 2: BC1FL */
				if (IS_RELATIVE_IDLE_LOOP(state)) return mips32_emit_bc1fl_idle(state, cache);
				else                              return mips32_emit_bc1fl(state, cache);
			case 3: /* opcode 3: BC1TL */
				if (IS_RELATIVE_IDLE_LOOP(state)) return mips32_emit_bc1tl_idle(state, cache);
				else                              return mips32_emit_bc1tl(state, cache);
			} /* switch ((op >> 16) & 0x3) for branches on C1 condition */
		case 16: /* Coprocessor 1 S-format opcodes */
			switch (op & 0x3F) {
			case 0: return mips32_emit_add_s(state, cache);
			case 1: return mips32_emit_sub_s(state, cache);
			case 2: return mips32_emit_mul_s(state, cache);
			case 3: return mips32_emit_div_s(state, cache);
			case 4: return mips32_emit_sqrt_s(state, cache);
			case 5: return mips32_emit_abs_s(state, cache);
			case 6: return mips32_emit_mov_s(state, cache);
			case 7: return mips32_emit_neg_s(state, cache);
			case 8: return mips32_emit_round_l_s(state, cache);
			case 9: return mips32_emit_trunc_l_s(state, cache);
			case 10: return mips32_emit_ceil_l_s(state, cache);
			case 11: return mips32_emit_floor_l_s(state, cache);
			case 12: return mips32_emit_round_w_s(state, cache);
			case 13: return mips32_emit_trunc_w_s(state, cache);
			case 14: return mips32_emit_ceil_w_s(state, cache);
			case 15: return mips32_emit_floor_w_s(state, cache);
			case 33: return mips32_emit_cvt_d_s(state, cache);
			case 36: return mips32_emit_cvt_w_s(state, cache);
			case 37: return mips32_emit_cvt_l_s(state, cache);
			case 48: return mips32_emit_c_f_s(state, cache);
			case 49: return mips32_emit_c_un_s(state, cache);
			case 50: return mips32_emit_c_eq_s(state, cache);
			case 51: return mips32_emit_c_ueq_s(state, cache);
			case 52: return mips32_emit_c_olt_s(state, cache);
			case 53: return mips32_emit_c_ult_s(state, cache);
			case 54: return mips32_emit_c_ole_s(state, cache);
			case 55: return mips32_emit_c_ule_s(state, cache);
			case 56: return mips32_emit_c_sf_s(state, cache);
			case 57: return mips32_emit_c_ngle_s(state, cache);
			case 58: return mips32_emit_c_seq_s(state, cache);
			case 59: return mips32_emit_c_ngl_s(state, cache);
			case 60: return mips32_emit_c_lt_s(state, cache);
			case 61: return mips32_emit_c_nge_s(state, cache);
			case 62: return mips32_emit_c_le_s(state, cache);
			case 63: return mips32_emit_c_ngt_s(state, cache);
			default: /* Coprocessor 1 S-format opcodes 16..32, 34..35, 38..47:
			            Reserved Instructions */
				return mips32_emit_reserved(state, cache);
			} /* switch (op & 0x3F) for Coprocessor 1 S-format opcodes */
		case 17: /* Coprocessor 1 D-format opcodes */
			switch (op & 0x3F) {
			case 0: return mips32_emit_add_d(state, cache);
			case 1: return mips32_emit_sub_d(state, cache);
			case 2: return mips32_emit_mul_d(state, cache);
			case 3: return mips32_emit_div_d(state, cache);
			case 4: return mips32_emit_sqrt_d(state, cache);
			case 5: return mips32_emit_abs_d(state, cache);
			case 6: return mips32_emit_mov_d(state, cache);
			case 7: return mips32_emit_neg_d(state, cache);
			case 8: return mips32_emit_round_l_d(state, cache);
			case 9: return mips32_emit_trunc_l_d(state, cache);
			case 10: return mips32_emit_ceil_l_d(state, cache);
			case 11: return mips32_emit_floor_l_d(state, cache);
			case 12: return mips32_emit_round_w_d(state, cache);
			case 13: return mips32_emit_trunc_w_d(state, cache);
			case 14: return mips32_emit_ceil_w_d(state, cache);
			case 15: return mips32_emit_floor_w_d(state, cache);
			case 32: return mips32_emit_cvt_s_d(state, cache);
			case 36: return mips32_emit_cvt_w_d(state, cache);
			case 37: return mips32_emit_cvt_l_d(state, cache);
			case 48: return mips32_emit_c_f_d(state, cache);
			case 49: return mips32_emit_c_un_d(state, cache);
			case 50: return mips32_emit_c_eq_d(state, cache);
			case 51: return mips32_emit_c_ueq_d(state, cache);
			case 52: return mips32_emit_c_olt_d(state, cache);
			case 53: return mips32_emit_c_ult_d(state, cache);
			case 54: return mips32_emit_c_ole_d(state, cache);
			case 55: return mips32_emit_c_ule_d(state, cache);
			case 56: return mips32_emit_c_sf_d(state, cache);
			case 57: return mips32_emit_c_ngle_d(state, cache);
			case 58: return mips32_emit_c_seq_d(state, cache);
			case 59: return mips32_emit_c_ngl_d(state, cache);
			case 60: return mips32_emit_c_lt_d(state, cache);
			case 61: return mips32_emit_c_nge_d(state, cache);
			case 62: return mips32_emit_c_le_d(state, cache);
			case 63: return mips32_emit_c_ngt_d(state, cache);
			default: /* Coprocessor 1 D-format opcodes 16..31, 33..35, 38..47:
			            Reserved Instructions */
				return mips32_emit_reserved(state, cache);
			} /* switch (op & 0x3F) for Coprocessor 1 D-format opcodes */
		case 20: /* Coprocessor 1 W-format opcodes */
			switch (op & 0x3F) {
			case 32: return mips32_emit_cvt_s_w(state, cache);
			case 33: return mips32_emit_cvt_d_w(state, cache);
			default: /* Coprocessor 1 W-format opcodes 0..31, 34..63:
			            Reserved Instructions */
				return mips32_emit_reserved(state, cache);
			}
		case 21: /* Coprocessor 1 L-format opcodes */
			switch (op & 0x3F) {
			case 32: return mips32_emit_cvt_s_l(state, cache);
			case 33: return mips32_emit_cvt_d_l(state, cache);
			default: /* Coprocessor 1 L-format opcodes 0..31, 34..63:
			            Reserved Instructions */
				return mips32_emit_reserved(state, cache);
			}
		default: /* Coprocessor 1 opcodes 3, 7, 9..15, 18..19, 22..31:
		            Reserved Instructions */
			return mips32_emit_reserved(state, cache);
		} /* switch ((op >> 21) & 0x1F) for the Coprocessor 1 prefix */
	case 20: /* Major opcode 20: BEQL */
		if (IS_RELATIVE_IDLE_LOOP(state)) return mips32_emit_beql_idle(state, cache);
		else                              return mips32_emit_beql(state, cache);
	case 21: /* Major opcode 21: BNEL */
		if (IS_RELATIVE_IDLE_LOOP(state)) return mips32_emit_bnel_idle(state, cache);
		else                              return mips32_emit_bnel(state, cache);
	case 22: /* Major opcode 22: BLEZL */
		if (IS_RELATIVE_IDLE_LOOP(state)) return mips32_emit_blezl_idle(state, cache);
		else                              return mips32_emit_blezl(state, cache);
	case 23: /* Major opcode 23: BGTZL */
		if (IS_RELATIVE_IDLE_LOOP(state)) return mips32_emit_bgtzl_idle(state, cache);
		else                              return mips32_emit_bgtzl(state, cache);
	case 24: return mips32_emit_daddi(state, cache);
	case 25: return mips32_emit_daddiu(state, cache);
	case 26: return mips32_emit_ldl(state, cache);
	case 27: return mips32_emit_ldr(state, cache);
	case 32: return mips32_emit_lb(state, cache);
	case 33: return mips32_emit_lh(state, cache);
	case 34: return mips32_emit_lwl(state, cache);
	case 35: return mips32_emit_lw(state, cache);
	case 36: return mips32_emit_lbu(state, cache);
	case 37: return mips32_emit_lhu(state, cache);
	case 38: return mips32_emit_lwr(state, cache);
	case 39: return mips32_emit_lwu(state, cache);
	case 40: return mips32_emit_sb(state, cache);
	case 41: return mips32_emit_sh(state, cache);
	case 42: return mips32_emit_swl(state, cache);
	case 43: return mips32_emit_sw(state, cache);
	case 44: return mips32_emit_sdl(state, cache);
	case 45: return mips32_emit_sdr(state, cache);
	case 46: return mips32_emit_swr(state, cache);
	case 47: return mips32_emit_cache(state, cache);
	case 48: return mips32_emit_ll(state, cache);
	case 49: return mips32_emit_lwc1(state, cache);
	case 52: /* Major opcode 52: LLD (Not implemented) */
		return mips32_emit_ni(state, cache);
	case 53: return mips32_emit_ldc1(state, cache);
	case 55: return mips32_emit_ld(state, cache);
	case 56: return mips32_emit_sc(state, cache);
	case 57: return mips32_emit_swc1(state, cache);
	case 60: /* Major opcode 60: SCD (Not implemented) */
		return mips32_emit_ni(state, cache);
	case 61: return mips32_emit_sdc1(state, cache);
	case 63: return mips32_emit_sd(state, cache);
	default: /* Major opcodes 18..19, 28..31, 50..51, 54, 58..59, 62:
	            Reserved Instructions */
		return mips32_emit_reserved(state, cache);
	} /* switch ((op >> 26) & 0x3F) */
}

enum TJEmitTraceResult mips32_add_slow_path(struct mips32_state* state, const struct mips32_reg_cache* cache, mips32_slow_path_handler handler, void* source, void* usual_path, uint32_t userdata)
{
	if (!state->code) {
		return TJ_MEMORY_ERROR;
	}

	struct mips32_slow_path* new_slow_paths = realloc(state->slow_paths, (state->slow_path_count + 1) * sizeof(struct mips32_slow_path));
	if (new_slow_paths) {
		struct mips32_slow_path* new_slow_path = &new_slow_paths[state->slow_path_count++];

		state->slow_paths = new_slow_paths;

		new_slow_path->cache = malloc(sizeof(struct mips32_reg_cache));
		if (!new_slow_path->cache) {
			DebugMessage(M64MSG_ERROR, "There is insufficient memory for auxiliary data structures required to emit native code.");
			return TJ_FAILURE;
		}
		mips32_copy_reg_cache(new_slow_path->cache, cache);
		new_slow_path->handler = handler;
		new_slow_path->pc = state->pc;
		new_slow_path->ops = state->ops;
		new_slow_path->op_count = state->op_count;
		new_slow_path->in_delay_slot = state->in_delay_slot;
		/* Assume we're in the delay slot of the branch. Back up to the
		 * branch. */
		new_slow_path->source = (uint32_t*) source - 1;
		new_slow_path->usual_path = usual_path;
		new_slow_path->userdata = userdata;
		return TJ_SUCCESS;
	} else {
		DebugMessage(M64MSG_ERROR, "There is insufficient memory for auxiliary data structures required to emit native code.");
		return TJ_FAILURE;
	}
}

enum TJEmitTraceResult mips32_return_to_usual_path(struct mips32_state* state, const struct mips32_reg_cache* cache, void* usual_path)
{
	void* origin;

	if (!state->code) {
		return TJ_MEMORY_ERROR;
	}

	origin = (uint32_t*) state->code - 1;
	/* Does the offset fit in 16 bits? */
	if (mips32_can_jump16(origin, usual_path)) {
		*LO16_32(origin) = mips32_get_jump16(origin, usual_path);
	} else {
		DebugMessage(M64MSG_ERROR, "Failed to create a block: Unconditional branch from the slow path of opcode %s to the next opcode too far (%zi bytes; maximum allowed: -131068 bytes)", cache->op_names[0], (size_t) ((uintptr_t) (usual_path) - (uintptr_t) (origin) - 4));
		return TJ_FAILURE;
	}

	return TJ_SUCCESS;
}

void* mips32_dynamic_linker(uint32_t pc, uint32_t target, void* start, size_t length)
{
	assert((pc & ~UINT32_C(0xFFF)) == (target & ~UINT32_C(0xFFF)));
	assert(start != NULL);
	assert(length >= 16);

	void* Trace = GetOrMakeTraceAt(target);
	if (Trace != NULL) {
		/* Make sure the code calling this function is still set up to call
		 * this function. */
		if (GetFollowingTrace(Trace) <= start
		 || (uint8_t*) Trace >= ((uint8_t*) start + length)) {
			struct mips32_state state;
			state.code = start;
			state.avail = length;
			/* Try BGEZ $0. */
			if (mips32_can_jump16(start, Trace)) {
				mips32_bgez(&state, 0, mips32_get_jump16(start, Trace));
				DebugMessage(M64MSG_VERBOSE, "Linked N64 jump %08" PRIX32 " -> %08" PRIX32 " at native %" PRIXPTR " -> %" PRIXPTR " as BGEZ", pc, target, start, Trace);
			}
			/* Next, try J. */
			else if (mips32_can_jump26(start, Trace)) {
				mips32_j(&state, Trace);
				DebugMessage(M64MSG_VERBOSE, "Linked N64 jump %08" PRIX32 " -> %08" PRIX32 " at native %" PRIXPTR " -> %" PRIXPTR " as J", pc, target, start, Trace);
			}
			/* Finally, there's always LUI ORI JR. */
			else {
				mips32_i32(&state, REG_EMERGENCY, (uintptr_t) Trace);
				mips32_jr(&state, REG_EMERGENCY);
				DebugMessage(M64MSG_VERBOSE, "Linked N64 jump %08" PRIX32 " -> %08" PRIX32 " at native %" PRIXPTR " -> %" PRIXPTR " as LUI ORI JR", pc, target, start, Trace);
			}
			while (state.code != NULL) {
				mips32_nop(&state);
			}
			mips32_flush_cache(start, length);
		}
		return Trace;
	} else {
		DebugMessage(M64MSG_ERROR, "Failed to write a direct jump for a Nintendo 64 jump.");
		DebugMessage(M64MSG_ERROR, "Nintendo 64: %08" PRIX32 " -> %08" PRIX32, pc, target);
		DebugMessage(M64MSG_ERROR, "Native: %" PRIXPTR " -> ?", start);
		return NULL;
	}
}
