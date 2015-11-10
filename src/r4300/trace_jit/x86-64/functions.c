/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - functions.c                                             *
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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../mips-analysis.h"
#include "../mips-interp.h"
#include "../mips-jit.h"
#include "../mips-parse.h"
#include "../native-tracecache.h"
#include "../native-config.h"

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

#define TJ_MAY_RAISE_TLB_REFILL    0x01
#define TJ_MAY_RAISE_COP1_UNUSABLE 0x02
#define TJ_MAY_RAISE_INTERRUPT     0x04
#define TJ_CHECK_STOP              0x08
#define TJ_TRANSFERS_CONTROL       0x10
#define TJ_HAS_DELAY_SLOT          0x20
#define TJ_READS_PC                0x40

#define FAIL_AS(expr) \
	do { \
		enum TJEmitTraceResult result = (expr); \
		if (result != TJ_SUCCESS) \
			return result; \
	} while (0)

struct x86_64_state {
	void* code; /* Current native code pointer */
	size_t avail; /* Number of bytes available to emit code into */
	const uint32_t* ops; /* MIPS III operations. Advanced after emission. */
	size_t op_count; /* Number of operations remaining. */
	uint32_t stored_pc; /* What value the global PC variable has. Advanced
	                       after emitting an interpreter call. */
	uint32_t pc; /* Program Counter during emission. */
	struct ConstAnalysis consts; /* Constants in MIPS III registers. */
	struct WidthAnalysis widths; /* Value widths in MIPS III registers. */
	bool cop1_checked; /* Whether Coprocessor 1 usability checks can be killed */

	size_t end_reloc_count; /* These allow writing jumps towards the stack  */
	void** end_relocs; /*  adjustement code and have them patched at the end of
	                      x86_64_emit_trace */
	bool fallthrough; /* false if the trace's ending jump has been seen */

	bool RDX_has_regs; /* true if RDX is known to contain &regs[16] for easy
	                      offsetting at the current emission point */
};

#include "native-ops.inc"
#include "native-utils.inc"

bool x86_64_trace_jit_init()
{
	const size_t Size = 4 * 1024 * 1024;
	void* Cache = AllocExec(Size);
	if (Cache != NULL) {
		SetCodeCache(Cache, Size);
		return true;
	} else {
		DebugMessage(M64MSG_ERROR, "Failed to allocate %zi bytes of executable memory for the AMD64 dynamic recompiler.", Size);
		return false;
	}
}

void x86_64_trace_jit_exit()
{
	FreeExec(CodeCache, CodeCacheSize);
	SetCodeCache(NULL, 0);
}

static void x86_64_stack_enter(struct x86_64_state* state)
{
#if defined(_WIN64)
	/* The stack on 64-bit Microsoft Windows is aligned to 16 bytes at the
	 * start of a procedure, must be aligned to 16 bytes at the start of a
	 * procedure it calls, and there must be 32 bytes of "shadow space" to
	 * the right of the return address of a CALL where the callee may save
	 * RCX, RDX, R8 and R9. It looks like this (| is aligned):
	 *
	 * |               |               |               |
	 * +0      +8      +16     +24     +32     +40     +48
	 * retaddr [    shadow space, 32 bytes    ] align  <previous procedure>
	 *
	 * The shadow space can be reserved just once and reused across calls.
	 * Below, 0x28 bytes are reserved, of which 0x20 are shadow space, and
	 * 8 bytes are there to align the stack of callees to 16 bytes after a
	 * CALL instruction pushes the 8-byte return address.
	 */
	x86_64_sub_r64_si8(state, RSP, 0x28);
#else
	/* XXX: Not sure if #else is necessarily the System V AMD64 ABI */
	/* In the System V AMD64 ABI, the stack is aligned to 16 bytes at the
	 * start of a procedure, and must be aligned to 16 bytes at the start
	 * of a procedure it calls.
	 *
	 * Below, 8 bytes are reserved to align the stack of callees to 16 bytes
	 * after a CALL instruction pushes the 8-byte return address.
	 */
	x86_64_sub_r64_si8(state, RSP, 0x8);
#endif
}

static void x86_64_stack_leave(struct x86_64_state* state)
{
#if defined(_WIN64)
	x86_64_add_r64_si8(state, RSP, 0x28);
#else
	x86_64_add_r64_si8(state, RSP, 0x8);
#endif
}

static enum TJEmitTraceResult x86_64_add_end_reloc(struct x86_64_state* state)
{
	void** new_end_relocs = realloc(state->end_relocs, (state->end_reloc_count + 1) * sizeof(void*));
	if (new_end_relocs) {
		state->end_relocs = new_end_relocs;
		new_end_relocs[state->end_reloc_count++] = (uint8_t*) state->code - 4;
		return TJ_SUCCESS;
	} else {
		return TJ_FAILURE;
	}
}

static void x86_64_next_opcode(struct x86_64_state* state)
{
	if (state->op_count > 0) {
		UpdateConstAnalysis(&state->consts, state->ops[0]);
		UpdateWidthAnalysis(&state->widths, state->ops[0]);
		state->pc += N64_INSN_SIZE;
		state->ops++;
		state->op_count--;
	}
}

static enum TJEmitTraceResult x86_64_emit_interpret(struct x86_64_state* state, void (*func) (uint32_t), int flags)
{
	uint32_t op = state->ops[0];

	/* If checked for, the Coprocessor Unusable exception may not be raised
	 * again during the trace. It may be raised again only if an interrupt
	 * handler runs and sets Coprocessor 1 to be unusable, and only branch
	 * and jump instructions allow interrupt handlers to run in mupen64plus.
	 */
	if (flags & TJ_MAY_RAISE_COP1_UNUSABLE) {
		if (state->cop1_checked)
			flags &= ~TJ_MAY_RAISE_COP1_UNUSABLE;
		else {
			state->cop1_checked = true;
			flags |= TJ_READS_PC;
		}
	}

	/* If the current PC is not synced anymore with TJ_PC.addr, and the
	 * interpreter function reads it... */
	if (state->stored_pc != state->pc && (flags & TJ_READS_PC)) {
		/* Sync it. */
		x86_64_mov_r64_ia(state, RCX, &TJ_PC.addr);
		x86_64_mov_r64_zi32(state, RAX, state->pc);
		x86_64_mov_m32r_r32(state, RCX, EAX);
		state->stored_pc = state->pc;
	}

	/* Store the op to the first parameter register. */
	/* NOP does not really require any valid opcode. */
	if (func != &TJ_NOP) {
		x86_64_mov_r64_zi32(state, REG_INT_ARG1, op);
	}
	/* Call the interpreter function for the opcode. */
	x86_64_call_ia(state, func, RAX);
	/* After it returns, RDX's value is overwritten. */
	state->RDX_has_regs = false;

	state->stored_pc += N64_INSN_SIZE;
	x86_64_next_opcode(state);

	/* If required, check for unexpected PC changes. */
	if (flags & (TJ_MAY_RAISE_TLB_REFILL | TJ_MAY_RAISE_COP1_UNUSABLE
	           | TJ_MAY_RAISE_INTERRUPT)) {
		/* Jump to the end of the block (where the stack frame is torn down)
		 * if an exception occurred, which is defined here as TJ_PC.addr not
		 * matching what we expect, at run time. This uses a forward branch,
		 * which is statically predicted not taken, according to the manuals
		 * for recent Intel cores. */
		x86_64_mov_r64_ia(state, RAX, &TJ_PC.addr);
		x86_64_cmp_m32r_i32(state, RAX, state->stored_pc);
		x86_64_jne_si32(state, state->code);
		FAIL_AS(x86_64_add_end_reloc(state));
	}
	if (flags & TJ_CHECK_STOP) {
		/* Jump to the end of the block (where the stack frame is torn down)
		 * if stop became non-zero, at run time. This uses a forward branch,
		 * which is statically predicted not taken, according to the manuals
		 * for recent Intel cores. */
		x86_64_mov_r64_ia(state, RAX, &stop);
		x86_64_cmp_m32r_si8(state, RAX, 0);
		x86_64_jne_si32(state, state->code);
		FAIL_AS(x86_64_add_end_reloc(state));
	}
	if (flags & TJ_HAS_DELAY_SLOT) {
		state->stored_pc += N64_INSN_SIZE;
		x86_64_next_opcode(state);
	}
	if (flags & TJ_TRANSFERS_CONTROL) {
		state->fallthrough = false;
	}

	return TJ_SUCCESS;
}

#include "mips-utils.inc"
#include "mips-ops.inc"

static enum TJEmitTraceResult x86_64_emit_opcode(struct x86_64_state* state)
{
	uint32_t op = state->ops[0];
	switch ((op >> 26) & 0x3F) {
	case 0: /* SPECIAL prefix */
		switch (op & 0x3F) {
		case 0: /* SPECIAL opcode 0: SLL */
			/* SLL is a special case: it must check whether it's writing to
			 * $0, because the encoding for NOP is an SLL. */
			if (RD_OF(op) != 0) return x86_64_emit_sll(state);
			else                return x86_64_emit_nop(state);
		case 2: return x86_64_emit_srl(state);
		case 3: return x86_64_emit_sra(state);
		case 4: return x86_64_emit_sllv(state);
		case 6: return x86_64_emit_srlv(state);
		case 7: return x86_64_emit_srav(state);
		case 8: return x86_64_emit_jr(state);
		case 9: return x86_64_emit_jalr(state);
		case 12: return x86_64_emit_syscall(state);
		case 13: /* SPECIAL opcode 13: BREAK (Not implemented) */
			return x86_64_emit_ni(state);
		case 15: return x86_64_emit_sync(state);
		case 16: return x86_64_emit_mfhi(state);
		case 17: return x86_64_emit_mthi(state);
		case 18: return x86_64_emit_mflo(state);
		case 19: return x86_64_emit_mtlo(state);
		case 20: return x86_64_emit_dsllv(state);
		case 22: return x86_64_emit_dsrlv(state);
		case 23: return x86_64_emit_dsrav(state);
		case 24: return x86_64_emit_mult(state);
		case 25: return x86_64_emit_multu(state);
		case 26: return x86_64_emit_div(state);
		case 27: return x86_64_emit_divu(state);
		case 28: return x86_64_emit_dmult(state);
		case 29: return x86_64_emit_dmultu(state);
		case 30: return x86_64_emit_ddiv(state);
		case 31: return x86_64_emit_ddivu(state);
		case 32: return x86_64_emit_add(state);
		case 33: return x86_64_emit_addu(state);
		case 34: return x86_64_emit_sub(state);
		case 35: return x86_64_emit_subu(state);
		case 36: return x86_64_emit_and(state);
		case 37: return x86_64_emit_or(state);
		case 38: return x86_64_emit_xor(state);
		case 39: return x86_64_emit_nor(state);
		case 42: return x86_64_emit_slt(state);
		case 43: return x86_64_emit_sltu(state);
		case 44: return x86_64_emit_dadd(state);
		case 45: return x86_64_emit_daddu(state);
		case 46: return x86_64_emit_dsub(state);
		case 47: return x86_64_emit_dsubu(state);
		case 48: /* SPECIAL opcode 48: TGE (Not implemented) */
		case 49: /* SPECIAL opcode 49: TGEU (Not implemented) */
		case 50: /* SPECIAL opcode 50: TLT (Not implemented) */
		case 51: /* SPECIAL opcode 51: TLTU (Not implemented) */
			return x86_64_emit_ni(state);
		case 52: return x86_64_emit_teq(state);
		case 54: /* SPECIAL opcode 54: TNE (Not implemented) */
			return x86_64_emit_ni(state);
		case 56: return x86_64_emit_dsll(state);
		case 58: return x86_64_emit_dsrl(state);
		case 59: return x86_64_emit_dsra(state);
		case 60: return x86_64_emit_dsll32(state);
		case 62: return x86_64_emit_dsrl32(state);
		case 63: return x86_64_emit_dsra32(state);
		default: /* SPECIAL opcodes 1, 5, 10, 11, 14, 21, 40, 41, 53, 55, 57,
		            61: Reserved Instructions */
			return x86_64_emit_reserved(state);
		} /* switch (op & 0x3F) for the SPECIAL prefix */
	case 1: /* REGIMM prefix */
		switch ((op >> 16) & 0x1F) {
		case 0: /* REGIMM opcode 0: BLTZ */
			if (IS_RELATIVE_IDLE_LOOP(state)) return x86_64_emit_bltz_idle(state);
			else                              return x86_64_emit_bltz(state);
		case 1: /* REGIMM opcode 1: BGEZ */
			if (IS_RELATIVE_IDLE_LOOP(state)) return x86_64_emit_bgez_idle(state);
			else                              return x86_64_emit_bgez(state);
		case 2: /* REGIMM opcode 2: BLTZL */
			if (IS_RELATIVE_IDLE_LOOP(state)) return x86_64_emit_bltzl_idle(state);
			else                              return x86_64_emit_bltzl(state);
		case 3: /* REGIMM opcode 3: BGEZL */
			if (IS_RELATIVE_IDLE_LOOP(state)) return x86_64_emit_bgezl_idle(state);
			else                              return x86_64_emit_bgezl(state);
		case 8: /* REGIMM opcode 8: TGEI (Not implemented) */
		case 9: /* REGIMM opcode 9: TGEIU (Not implemented) */
		case 10: /* REGIMM opcode 10: TLTI (Not implemented) */
		case 11: /* REGIMM opcode 11: TLTIU (Not implemented) */
		case 12: /* REGIMM opcode 12: TEQI (Not implemented) */
		case 14: /* REGIMM opcode 14: TNEI (Not implemented) */
			return x86_64_emit_ni(state);
		case 16: /* REGIMM opcode 16: BLTZAL */
			if (IS_RELATIVE_IDLE_LOOP(state)) return x86_64_emit_bltzal_idle(state);
			else                              return x86_64_emit_bltzal(state);
		case 17: /* REGIMM opcode 17: BGEZAL */
			if (IS_RELATIVE_IDLE_LOOP(state)) return x86_64_emit_bgezal_idle(state);
			else                              return x86_64_emit_bgezal(state);
		case 18: /* REGIMM opcode 18: BLTZALL */
			if (IS_RELATIVE_IDLE_LOOP(state)) return x86_64_emit_bltzall_idle(state);
			else                              return x86_64_emit_bltzall(state);
		case 19: /* REGIMM opcode 19: BGEZALL */
			if (IS_RELATIVE_IDLE_LOOP(state)) return x86_64_emit_bgezall_idle(state);
			else                              return x86_64_emit_bgezall(state);
		default: /* REGIMM opcodes 4..7, 13, 15, 20..31:
		            Reserved Instructions */
			return x86_64_emit_reserved(state);
		} /* switch ((op >> 16) & 0x1F) for the REGIMM prefix */
	case 2: /* Major opcode 2: J */
		if (IS_ABSOLUTE_IDLE_LOOP(state)) return x86_64_emit_j_idle(state);
		else                              return x86_64_emit_j(state);
	case 3: /* Major opcode 3: JAL */
		if (IS_ABSOLUTE_IDLE_LOOP(state)) return x86_64_emit_jal_idle(state);
		else                              return x86_64_emit_jal(state);
	case 4: /* Major opcode 4: BEQ */
		if (IS_RELATIVE_IDLE_LOOP(state)) return x86_64_emit_beq_idle(state);
		else                              return x86_64_emit_beq(state);
	case 5: /* Major opcode 5: BNE */
		if (IS_RELATIVE_IDLE_LOOP(state)) return x86_64_emit_bne_idle(state);
		else                              return x86_64_emit_bne(state);
	case 6: /* Major opcode 6: BLEZ */
		if (IS_RELATIVE_IDLE_LOOP(state)) return x86_64_emit_blez_idle(state);
		else                              return x86_64_emit_blez(state);
	case 7: /* Major opcode 7: BGTZ */
		if (IS_RELATIVE_IDLE_LOOP(state)) return x86_64_emit_bgtz_idle(state);
		else                              return x86_64_emit_bgtz(state);
	case 8: return x86_64_emit_addi(state);
	case 9: return x86_64_emit_addiu(state);
	case 10: return x86_64_emit_slti(state);
	case 11: return x86_64_emit_sltiu(state);
	case 12: return x86_64_emit_andi(state);
	case 13: return x86_64_emit_ori(state);
	case 14: return x86_64_emit_xori(state);
	case 15: return x86_64_emit_lui(state);
	case 16: /* Coprocessor 0 prefix */
		switch ((op >> 21) & 0x1F) {
		case 0: return x86_64_emit_mfc0(state);
		case 4: return x86_64_emit_mtc0(state);
		case 16: /* Coprocessor 0 opcode 16: TLB */
			switch (op & 0x3F) {
			case 1: return x86_64_emit_tlbr(state);
			case 2: return x86_64_emit_tlbwi(state);
			case 6: return x86_64_emit_tlbwr(state);
			case 8: return x86_64_emit_tlbp(state);
			case 24: return x86_64_emit_eret(state);
			default: /* TLB sub-opcodes 0, 3..5, 7, 9..23, 25..63:
			            Reserved Instructions */
				return x86_64_emit_reserved(state);
			} /* switch (op & 0x3F) for Coprocessor 0 TLB opcodes */
		default: /* Coprocessor 0 opcodes 1..3, 4..15, 17..31:
		            Reserved Instructions */
			return x86_64_emit_reserved(state);
		} /* switch ((op >> 21) & 0x1F) for the Coprocessor 0 prefix */
	case 17: /* Coprocessor 1 prefix */
		switch ((op >> 21) & 0x1F) {
		case 0: return x86_64_emit_mfc1(state);
		case 1: return x86_64_emit_dmfc1(state);
		case 2: return x86_64_emit_cfc1(state);
		case 4: return x86_64_emit_mtc1(state);
		case 5: return x86_64_emit_dmtc1(state);
		case 6: return x86_64_emit_ctc1(state);
		case 8: /* Coprocessor 1 opcode 8: Branch on C1 condition... */
			switch ((op >> 16) & 0x3) {
			case 0: /* opcode 0: BC1F */
				if (IS_RELATIVE_IDLE_LOOP(state)) return x86_64_emit_bc1f_idle(state);
				else                              return x86_64_emit_bc1f(state);
			case 1: /* opcode 1: BC1T */
				if (IS_RELATIVE_IDLE_LOOP(state)) return x86_64_emit_bc1t_idle(state);
				else                              return x86_64_emit_bc1t(state);
			case 2: /* opcode 2: BC1FL */
				if (IS_RELATIVE_IDLE_LOOP(state)) return x86_64_emit_bc1fl_idle(state);
				else                              return x86_64_emit_bc1fl(state);
			case 3: /* opcode 3: BC1TL */
				if (IS_RELATIVE_IDLE_LOOP(state)) return x86_64_emit_bc1tl_idle(state);
				else                              return x86_64_emit_bc1tl(state);
			} /* switch ((op >> 16) & 0x3) for branches on C1 condition */
		case 16: /* Coprocessor 1 S-format opcodes */
			switch (op & 0x3F) {
			case 0: return x86_64_emit_add_s(state);
			case 1: return x86_64_emit_sub_s(state);
			case 2: return x86_64_emit_mul_s(state);
			case 3: return x86_64_emit_div_s(state);
			case 4: return x86_64_emit_sqrt_s(state);
			case 5: return x86_64_emit_abs_s(state);
			case 6: return x86_64_emit_mov_s(state);
			case 7: return x86_64_emit_neg_s(state);
			case 8: return x86_64_emit_round_l_s(state);
			case 9: return x86_64_emit_trunc_l_s(state);
			case 10: return x86_64_emit_ceil_l_s(state);
			case 11: return x86_64_emit_floor_l_s(state);
			case 12: return x86_64_emit_round_w_s(state);
			case 13: return x86_64_emit_trunc_w_s(state);
			case 14: return x86_64_emit_ceil_w_s(state);
			case 15: return x86_64_emit_floor_w_s(state);
			case 33: return x86_64_emit_cvt_d_s(state);
			case 36: return x86_64_emit_cvt_w_s(state);
			case 37: return x86_64_emit_cvt_l_s(state);
			case 48: return x86_64_emit_c_f_s(state);
			case 49: return x86_64_emit_c_un_s(state);
			case 50: return x86_64_emit_c_eq_s(state);
			case 51: return x86_64_emit_c_ueq_s(state);
			case 52: return x86_64_emit_c_olt_s(state);
			case 53: return x86_64_emit_c_ult_s(state);
			case 54: return x86_64_emit_c_ole_s(state);
			case 55: return x86_64_emit_c_ule_s(state);
			case 56: return x86_64_emit_c_sf_s(state);
			case 57: return x86_64_emit_c_ngle_s(state);
			case 58: return x86_64_emit_c_seq_s(state);
			case 59: return x86_64_emit_c_ngl_s(state);
			case 60: return x86_64_emit_c_lt_s(state);
			case 61: return x86_64_emit_c_nge_s(state);
			case 62: return x86_64_emit_c_le_s(state);
			case 63: return x86_64_emit_c_ngt_s(state);
			default: /* Coprocessor 1 S-format opcodes 16..32, 34..35, 38..47:
			            Reserved Instructions */
				return x86_64_emit_reserved(state);
			} /* switch (op & 0x3F) for Coprocessor 1 S-format opcodes */
		case 17: /* Coprocessor 1 D-format opcodes */
			switch (op & 0x3F) {
			case 0: return x86_64_emit_add_d(state);
			case 1: return x86_64_emit_sub_d(state);
			case 2: return x86_64_emit_mul_d(state);
			case 3: return x86_64_emit_div_d(state);
			case 4: return x86_64_emit_sqrt_d(state);
			case 5: return x86_64_emit_abs_d(state);
			case 6: return x86_64_emit_mov_d(state);
			case 7: return x86_64_emit_neg_d(state);
			case 8: return x86_64_emit_round_l_d(state);
			case 9: return x86_64_emit_trunc_l_d(state);
			case 10: return x86_64_emit_ceil_l_d(state);
			case 11: return x86_64_emit_floor_l_d(state);
			case 12: return x86_64_emit_round_w_d(state);
			case 13: return x86_64_emit_trunc_w_d(state);
			case 14: return x86_64_emit_ceil_w_d(state);
			case 15: return x86_64_emit_floor_w_d(state);
			case 32: return x86_64_emit_cvt_s_d(state);
			case 36: return x86_64_emit_cvt_w_d(state);
			case 37: return x86_64_emit_cvt_l_d(state);
			case 48: return x86_64_emit_c_f_d(state);
			case 49: return x86_64_emit_c_un_d(state);
			case 50: return x86_64_emit_c_eq_d(state);
			case 51: return x86_64_emit_c_ueq_d(state);
			case 52: return x86_64_emit_c_olt_d(state);
			case 53: return x86_64_emit_c_ult_d(state);
			case 54: return x86_64_emit_c_ole_d(state);
			case 55: return x86_64_emit_c_ule_d(state);
			case 56: return x86_64_emit_c_sf_d(state);
			case 57: return x86_64_emit_c_ngle_d(state);
			case 58: return x86_64_emit_c_seq_d(state);
			case 59: return x86_64_emit_c_ngl_d(state);
			case 60: return x86_64_emit_c_lt_d(state);
			case 61: return x86_64_emit_c_nge_d(state);
			case 62: return x86_64_emit_c_le_d(state);
			case 63: return x86_64_emit_c_ngt_d(state);
			default: /* Coprocessor 1 D-format opcodes 16..31, 33..35, 38..47:
			            Reserved Instructions */
				return x86_64_emit_reserved(state);
			} /* switch (op & 0x3F) for Coprocessor 1 D-format opcodes */
		case 20: /* Coprocessor 1 W-format opcodes */
			switch (op & 0x3F) {
			case 32: return x86_64_emit_cvt_s_w(state);
			case 33: return x86_64_emit_cvt_d_w(state);
			default: /* Coprocessor 1 W-format opcodes 0..31, 34..63:
			            Reserved Instructions */
				return x86_64_emit_reserved(state);
			}
		case 21: /* Coprocessor 1 L-format opcodes */
			switch (op & 0x3F) {
			case 32: return x86_64_emit_cvt_s_l(state);
			case 33: return x86_64_emit_cvt_d_l(state);
			default: /* Coprocessor 1 L-format opcodes 0..31, 34..63:
			            Reserved Instructions */
				return x86_64_emit_reserved(state);
			}
		default: /* Coprocessor 1 opcodes 3, 7, 9..15, 18..19, 22..31:
		            Reserved Instructions */
			return x86_64_emit_reserved(state);
		} /* switch ((op >> 21) & 0x1F) for the Coprocessor 1 prefix */
	case 20: /* Major opcode 20: BEQL */
		if (IS_RELATIVE_IDLE_LOOP(state)) return x86_64_emit_beql_idle(state);
		else                              return x86_64_emit_beql(state);
	case 21: /* Major opcode 21: BNEL */
		if (IS_RELATIVE_IDLE_LOOP(state)) return x86_64_emit_bnel_idle(state);
		else                              return x86_64_emit_bnel(state);
	case 22: /* Major opcode 22: BLEZL */
		if (IS_RELATIVE_IDLE_LOOP(state)) return x86_64_emit_blezl_idle(state);
		else                              return x86_64_emit_blezl(state);
	case 23: /* Major opcode 23: BGTZL */
		if (IS_RELATIVE_IDLE_LOOP(state)) return x86_64_emit_bgtzl_idle(state);
		else                              return x86_64_emit_bgtzl(state);
	case 24: return x86_64_emit_daddi(state);
	case 25: return x86_64_emit_daddiu(state);
	case 26: return x86_64_emit_ldl(state);
	case 27: return x86_64_emit_ldr(state);
	case 32: return x86_64_emit_lb(state);
	case 33: return x86_64_emit_lh(state);
	case 34: return x86_64_emit_lwl(state);
	case 35: return x86_64_emit_lw(state);
	case 36: return x86_64_emit_lbu(state);
	case 37: return x86_64_emit_lhu(state);
	case 38: return x86_64_emit_lwr(state);
	case 39: return x86_64_emit_lwu(state);
	case 40: return x86_64_emit_sb(state);
	case 41: return x86_64_emit_sh(state);
	case 42: return x86_64_emit_swl(state);
	case 43: return x86_64_emit_sw(state);
	case 44: return x86_64_emit_sdl(state);
	case 45: return x86_64_emit_sdr(state);
	case 46: return x86_64_emit_swr(state);
	case 47: return x86_64_emit_cache(state);
	case 48: return x86_64_emit_ll(state);
	case 49: return x86_64_emit_lwc1(state);
	case 52: /* Major opcode 52: LLD (Not implemented) */
		return x86_64_emit_ni(state);
	case 53: return x86_64_emit_ldc1(state);
	case 55: return x86_64_emit_ld(state);
	case 56: return x86_64_emit_sc(state);
	case 57: return x86_64_emit_swc1(state);
	case 60: /* Major opcode 60: SCD (Not implemented) */
		return x86_64_emit_ni(state);
	case 61: return x86_64_emit_sdc1(state);
	case 63: return x86_64_emit_sd(state);
	default: /* Major opcodes 18..19, 28..31, 50..51, 54, 58..59, 62:
	            Reserved Instructions */
		return x86_64_emit_reserved(state);
	} /* switch ((op >> 26) & 0x3F) */
}

enum TJEmitTraceResult x86_64_emit_trace(void** code_ptr, size_t avail, uint32_t pc, const uint32_t* ops, uint32_t op_count)
{
	enum TJEmitTraceResult result = TJ_SUCCESS;
	struct x86_64_state state;
	void* code_end;
	size_t i, misalignment;

	state.code = *code_ptr;
	state.avail = avail;
	state.ops = ops;
	state.op_count = op_count;
	state.stored_pc = state.pc = pc;
	InitConstAnalysis(&state.consts);
	InitWidthAnalysis(&state.widths);
	state.cop1_checked = false;
	state.fallthrough = true;

	state.end_reloc_count = 0;
	state.end_relocs = NULL;

	state.RDX_has_regs = false;

	x86_64_stack_enter(&state);
	if (!state.code) {
		result = TJ_MEMORY_ERROR;
		goto end;
	}

	while (state.op_count > 0) {
		if (x86_64_emit_opcode(&state) == TJ_FAILURE) {
			result = TJ_FAILURE;
			goto end;
		}

		if (!state.code) {
			result = TJ_MEMORY_ERROR;
			goto end;
		}
	}

	if (state.fallthrough) {
		/* Store the PC to the first parameter register. */
		x86_64_mov_r64_zi32(&state, REG_INT_ARG1, state.pc);
		/* Call tj_jump_to. */
		x86_64_call_ia(&state, &tj_jump_to, RAX);
	}

	code_end = state.code;

	x86_64_stack_leave(&state);
	x86_64_ret(&state);

	if (!state.code) {
		result = TJ_MEMORY_ERROR;
		goto end;
	}

	/* Apply relocations in all the jumps that wanted to go to the end. */
	for (i = 0; i < state.end_reloc_count; i++) {
		void* origin = state.end_relocs[i];
		*(uint32_t*) origin = (uint32_t) (int32_t)
			((uint8_t*) code_end - ((uint8_t*) origin + 4));
	}

	/* Align the next header and trace to 16 bytes. The padding is opcode 0x90
	 * (NOP), which is recommended by the Intel manuals instead of leaving the
	 * previous contents there or padding with 0x00. Padding with 0x00 creates
	 * ADD [RAX], AL instructions, which may be executed speculatively and are
	 * costly due to memory access. There is necessarily enough space for this
	 * padding. */
	misalignment = (0x10 - ((uintptr_t) state.code & 0x0F)) & 0x0F;
	if (misalignment) {
		memset(state.code, 0x90, misalignment);
		state.code += misalignment;
	}

	*code_ptr = state.code;
end:
	free(state.end_relocs);
	return result;
}
