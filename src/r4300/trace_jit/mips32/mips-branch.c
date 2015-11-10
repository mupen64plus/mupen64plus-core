/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - MIPS (Nintendo 64) branch and jump instructions         *
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
#include <stdbool.h>
#include <stdint.h>

#include "mips-branch.h"

#include "mips-common.h"
#include "mips-emit.h"
#include "native-ops.h"
#include "native-utils.h"

#include "../mips-interp.h"
#include "../mips-parse.h"

#include "../../fpu.h"
#include "../../r4300.h"

/* - - - HELPER FUNCTIONS - - - */

static bool CanRaiseExceptions(uint32_t op)
{
	return CanRaiseTLBRefill(op) || CanRaiseCop1Unusable(op);
}

static enum TJEmitTraceResult emit_delay_slot(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	state->in_delay_slot = true;
	FAIL_AS(mips32_next_opcode(state));
	if (!CanRaiseExceptions(state->ops[0])) {
		FAIL_AS(mips32_emit_opcode(state, cache));
	} else {
		uint8_t n1 = mips32_alloc_int_const(state, cache, 1);
		mips32_sw(state, n1, STATE_OFFSET(delay_slot), REG_STATE);
		FAIL_AS(mips32_emit_opcode(state, cache));
		mips32_sw(state, 0, STATE_OFFSET(delay_slot), REG_STATE);
	}
	state->in_delay_slot = false;
	return TJ_SUCCESS;
}

/* Allocates native registers for a 2-register conditional branch.
 *
 * Correctly handles the case where the branch must use the original value of
 * one or both of the registers if the branch's delay slot rewrites them.
 *
 * In:
 *   rs: First Nintendo 64 register in the conditional branch.
 *   rt: Second Nintendo 64 register in the conditional branch.
 * Out:
 *   ns_hi, ns_lo, nt_hi, nt_lo: Native registers allocated to the parts of
 *      the register values used for the branch. ns_* and nt_* refer to rs and
 *      rt, respectively. *_hi and *_lo refer to the upper and lower 32 bits,
 *      respectively.
 *   width_32: Updated to true if only the lower 32 bits are used.
 *   rs_dep, rt_dep: Updated to true if the delay slot writes to rs or rt,
 *      respectively, or false if the delay slot does not write to them.
 */
static void get_branch_rs_rt(struct mips32_state* state, struct mips32_reg_cache* cache, uint8_t rs, uint8_t rt, uint8_t* ns_hi, uint8_t* ns_lo, uint8_t* nt_hi, uint8_t* nt_lo, bool* width_32, bool* rs_dep, bool* rt_dep)
{
	*width_32 = IntGetWidth(&state->widths, rs) <= 32 && IntGetWidth(&state->widths, rt) <= 32;
	*ns_hi = *ns_lo = *nt_hi = *nt_lo = 0;
	*rs_dep = (IntGetReads(state->ops[0]) & IntGetWrites(state->ops[1]) & BIT(rs)) != 0;
	*rt_dep = (IntGetReads(state->ops[0]) & IntGetWrites(state->ops[1]) & BIT(rt)) != 0;

	assert(!(*rs_dep && *rt_dep));

	/* If ns* / nt* are allocated first, they may be given the branch save
	 * registers. Allocate our temporary registers first instead. */
	if (*rs_dep || *rt_dep) {
		mips32_alloc_specific_int_temp(state, cache, REG_BRANCH_SAVE_START);
		if (!*width_32) {
			mips32_alloc_specific_int_temp(state, cache, REG_BRANCH_SAVE_START + 1);
		}
	}

	if (!*width_32) {
		*ns_hi = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rs]);
		*nt_hi = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rt]);
	}
	*ns_lo = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]);
	*nt_lo = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]);

	if (*rs_dep) {
		mips32_or(state, REG_BRANCH_SAVE_START, *ns_lo, 0);
		*ns_lo = REG_BRANCH_SAVE_START;
		if (!*width_32) {
			mips32_or(state, REG_BRANCH_SAVE_START + 1, *ns_hi, 0);
			*ns_hi = REG_BRANCH_SAVE_START + 1;
		}
	} else if (*rt_dep) {
		mips32_or(state, REG_BRANCH_SAVE_START, *nt_lo, 0);
		*nt_lo = REG_BRANCH_SAVE_START;
		if (!*width_32) {
			mips32_or(state, REG_BRANCH_SAVE_START + 1, *nt_hi, 0);
			*nt_hi = REG_BRANCH_SAVE_START + 1;
		}
	}
}

/* Ensures native registers for a 2-register conditional branch are allocated.
 *
 * In:
 *   rs: First Nintendo 64 register in the conditional branch.
 *   rt: Second Nintendo 64 register in the conditional branch.
 *   width_32: true if only the lower 32 bits are used.
 *   rs_dep, rt_dep: true if the delay slot writes to rs or rt, respectively,
 *      or false if the delay slot does not write to them.
 * In/Out:
 *   ns_hi, ns_lo, nt_hi, nt_lo: Native registers allocated to the parts of
 *     the register values used for the branch. ns_* and nt_* refer to rs and
 *      rt, respectively. *_hi and *_lo refer to the upper and lower 32 bits,
 *      respectively.
 *      On entry to the function, these are the registers that were expected
 *      to contain the parts before the delay slot was emitted.
 *      On exit to the function, these are the registers that now contain the
 *      correct parts as of the branch (not after the delay slot).
 */
static void reget_branch_rs_rt(struct mips32_state* state, struct mips32_reg_cache* cache, uint8_t rs, uint8_t rt, uint8_t* ns_hi, uint8_t* ns_lo, uint8_t* nt_hi, uint8_t* nt_lo, bool width_32, bool rs_dep, bool rt_dep)
{
	/* Just in case the delay slot did not use the register cache and freed
	 * everything. If they're already allocated, they will stay allocated.
	 * If original values had to be saved due to the delay slot overwriting
	 * them, they will already be in callee-saved registers. */
	/* Allocate temporary registers first, because otherwise the other
	 * (unmodified) branch register could be given the branch save registers.
	 */
	if (rs_dep || rt_dep) {
		mips32_alloc_specific_int_temp(state, cache, REG_BRANCH_SAVE_START);
		if (!width_32) {
			mips32_alloc_specific_int_temp(state, cache, REG_BRANCH_SAVE_START + 1);
		}
	}
	/* Then read those that have not been written in the delay slot. */
	if (!rs_dep) {
		if (!width_32) {
			*ns_hi = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rs]);
		}
		*ns_lo = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]);
	}
	if (!rt_dep) {
		if (!width_32) {
			*nt_hi = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rt]);
		}
		*nt_lo = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]);
	}
}

/* Allocates native registers for a 1-register conditional branch.
 *
 * Correctly handles the case where the branch must use the original value of
 * the register if the branch's delay slot rewrites it.
 *
 * In:
 *   rs: The Nintendo 64 register in the conditional branch.
 * Out:
 *   ns_hi, ns_lo: Native registers allocated to the parts of rs. ns_hi and
 *      ns_lo refer to the upper and lower 32 bits, respectively.
 *   width_32: Updated to true if only the lower 32 bits are used.
 *   rs_dep: Updated to true if the delay slot writes to rs, or false if the
 *      delay slot does not write to rs.
 */
static void get_branch_rs(struct mips32_state* state, struct mips32_reg_cache* cache, uint8_t rs, uint8_t* ns_hi, uint8_t* ns_lo, bool* width_32, bool* rs_dep)
{
	*width_32 = IntGetWidth(&state->widths, rs) <= 32;
	*ns_hi = *ns_lo = 0;
	/* If the delay slot writes the register read by the branch, its initial
	 * value must be used. */
	*rs_dep = (IntGetReads(state->ops[0]) & IntGetWrites(state->ops[1])) != 0;

	/* If ns* are allocated first, they may be given the branch save
	 * registers. Allocate our temporary registers first instead. */
	if (*rs_dep) {
		mips32_alloc_specific_int_temp(state, cache, REG_BRANCH_SAVE_START);
		if (!*width_32) {
			mips32_alloc_specific_int_temp(state, cache, REG_BRANCH_SAVE_START + 1);
		}
	}

	if (!*width_32) {
		*ns_hi = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rs]);
	}
	*ns_lo = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]);

	if (*rs_dep) {
		mips32_or(state, REG_BRANCH_SAVE_START, *ns_lo, 0);
		*ns_lo = REG_BRANCH_SAVE_START;
		if (!*width_32) {
			mips32_or(state, REG_BRANCH_SAVE_START + 1, *ns_hi, 0);
			*ns_hi = REG_BRANCH_SAVE_START + 1;
		}
	}
}

/* Ensures native registers for a 1-register conditional branch are allocated.
 *
 * In:
 *   rs: The Nintendo 64 register in the conditional branch.
 *   width_32: true if only the lower 32 bits are used.
 *   rs_dep: true if the delay slot writes to rs, or false if the delay slot
 *      does not write to rs.
 * In/Out:
 *   ns_hi, ns_lo: Native registers allocated to the parts of rs. ns_hi and
 *      ns_lo refer to the upper and lower 32 bits, respectively.
 *      On entry to the function, these are the registers that were expected
 *      to contain the parts before the delay slot was emitted.
 *      On exit to the function, these are the registers that now contain the
 *      correct parts as of the branch (not after the delay slot).
 */
static void reget_branch_rs(struct mips32_state* state, struct mips32_reg_cache* cache, uint8_t rs, uint8_t* ns_hi, uint8_t* ns_lo, bool width_32, bool rs_dep)
{
	/* Just in case the delay slot did not use the register cache and freed
	 * everything. If they're already allocated, they will stay allocated.
	 * If the original value had to be saved due to the delay slot overwriting
	 * it, they will already be in callee-saved registers. */
	if (rs_dep) {
		mips32_alloc_specific_int_temp(state, cache, REG_BRANCH_SAVE_START);
		if (!width_32) {
			mips32_alloc_specific_int_temp(state, cache, REG_BRANCH_SAVE_START + 1);
		}
	} else {
		if (!width_32) {
			*ns_hi = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rs]);
		}
		*ns_lo = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]);
	}
}

/* Allocates a native register for a 1-register conditional branch that uses
 * only the highest 32 meaningful bits (BGEZ, BLTZ and variants).
 *
 * Correctly handles the case where the branch must use the original value of
 * the register if the branch's delay slot rewrites it.
 *
 * In:
 *   rs: The Nintendo 64 register in the conditional branch.
 * Out:
 *   ns: Native register allocated to the highest 32 meaningful bits of rs.
 *   width_32: Updated to true if only the lower 32 bits are used.
 *   rs_dep: Updated to true if the delay slot writes to rs, or false if the
 *      delay slot does not write to rs.
 */
static void get_branch_rs_32(struct mips32_state* state, struct mips32_reg_cache* cache, uint8_t rs, uint8_t* ns, bool* width_32, bool* rs_dep)
{
	*width_32 = IntGetWidth(&state->widths, rs) <= 32;
	*ns = 0;
	/* If the delay slot writes the register read by the branch, its initial
	 * value must be used. */
	*rs_dep = (IntGetReads(state->ops[0]) & IntGetWrites(state->ops[1])) != 0;

	/* If ns is allocated first, it may be given the branch save register.
	 * Allocate this temporary register first instead. */
	if (*rs_dep) {
		mips32_alloc_specific_int_temp(state, cache, REG_BRANCH_SAVE_START);
	}

	if (*width_32) {
		*ns = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]);
	} else {
		*ns = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rs]);
	}

	if (*rs_dep) {
		mips32_or(state, REG_BRANCH_SAVE_START, *ns, 0);
		*ns = REG_BRANCH_SAVE_START;
	}
}

/* Ensures the native register for a 1-register conditional branch that uses
 * only the highest 32 meaningful bits (BGEZ, BLTZ and variants) is allocated.
 *
 * In:
 *   rs: The Nintendo 64 register in the conditional branch.
 *   width_32: true if only the lower 32 bits are used.
 *   rs_dep: true if the delay slot writes to rs, or false if the delay slot
 *      does not write to rs.
 * In/Out:
 *   ns: Native register allocated to the highest 32 meaningful bits of rs.
 *      On entry to the function, this is the register that is expected to
 *      contain the meaningful part of rs before the delay slot was emitted.
 *      On exit to the function, this is the register that now contains the
 *      correct part as of the branch (not after the delay slot).
 */
static void reget_branch_rs_32(struct mips32_state* state, struct mips32_reg_cache* cache, uint8_t rs, uint8_t* ns, bool width_32, bool rs_dep)
{
	/* Just in case the delay slot did not use the register cache and freed
	 * everything. If it's already allocated, it will stay allocated.
	 * If the original value had to be saved due to the delay slot overwriting
	 * it, it will already be in a callee-saved register. */
	if (rs_dep) {
		mips32_alloc_specific_int_temp(state, cache, REG_BRANCH_SAVE_START);
	} else {
		if (width_32) {
			*ns = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]);
		} else {
			*ns = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rs]);
		}
	}
}

/* Allocates a native register containing the most recent Coprocessor 1
 * condition bit.
 *
 * Only the condition bit is brought into the returned register. It may be at
 * bit 23 or at bit 0; code testing the returned register MUST check the value
 * using BEQ or BNE with $0.
 */
static uint8_t get_c1cond(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	/* TODO Use the native FPU condition if it's the most recent */
	mips32_alloc_specific_int_temp(state, cache, REG_BRANCH_SAVE_START);
	uint8_t efcr_31 = mips32_alloc_int_in_32(state, cache, &g_state.regs.fcr_31),
	        nc1cond_bit = mips32_alloc_int_const(state, cache, FCR31_CMP_BIT);
	mips32_and(state, REG_BRANCH_SAVE_START, efcr_31, nc1cond_bit);

	return REG_BRANCH_SAVE_START;
}

#define BRANCH_LIKELY 0x1
#define BRANCH_LINK31 0x2

/* Returns additional integer registers to be preserved (for performance) as a
 * bitmask. Bit #n represents register $n, and bit 0 is the least significant.
 */
static uint32_t prepare_compiled_jump(struct mips32_state* state, struct mips32_reg_cache* cache, uint32_t flags)
{
	uint8_t ncount = mips32_alloc_int_in_32(state, cache, &g_state.regs.cp0[CP0_COUNT_REG]);
	uint32_t result = BIT(ncount);
	mips32_add_to_count(state, cache);
	if (flags & BRANCH_LIKELY) {
		/* Make sure that native registers corresponding to the lower 32 bits
		 * of all integer registers read by the delay slot are going to be
		 * available. */
		uint32_t reads = IntGetReads(state->ops[1]);
		int i;
		for (i = FindFirstSet(reads); i >= 0; reads &= ~BIT(i), i = FindFirstSet(reads)) {
			uint8_t n = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[i]);
			result |= BIT(n);
		}
	}
	if (flags & BRANCH_LINK31) {
		/* Register 31 is not likely to be needed after the link is written.
		 * Don't return it as a register to be preserved. */
		uint8_t n31 = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[31]);
		mips32_i32(state, n31, state->pc + 8);
		mips32_set_int_ex32(cache, &g_state.regs.gpr[31],
			((state->pc + 8) & UINT32_C(0x80000000)) ? MRIT_MEM_OE32 : MRIT_MEM_ZE32);
	}
	return result;
}

static enum TJEmitTraceResult end_compiled_jump(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	mips32_end_opcode(state, cache);
	state->fallthrough = false;
	state->ending_compiled = true;
	return TJ_SUCCESS;
}

/* - - - PUBLIC FUNCTIONS - - - */

enum TJEmitTraceResult mips32_emit_jr(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretJR || state->op_count != 2) {
		return mips32_emit_interpret(state, cache, &TJ_JR, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
	} else {
		uint8_t rs = RS_OF(state->ops[0]);
		/* If the delay slot writes the register used by the jump, its initial
		 * value must be saved. */
		bool reorderable = (IntGetReads(state->ops[0]) & IntGetWrites(state->ops[1])) == 0;

		mips32_start_opcode(state, cache, "JR");

		if (!reorderable) {
			/* Must be allocated before 'ns', otherwise 'ns' may be the
			 * branch save register. */
			mips32_alloc_specific_int_temp(state, cache, REG_BRANCH_SAVE_START);
		}

		uint8_t ns = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]);

		if (!reorderable) {
			mips32_or(state, REG_BRANCH_SAVE_START, ns, 0);
			ns = REG_BRANCH_SAVE_START;
		}

		prepare_compiled_jump(state, cache, 0);
		FAIL_AS(emit_delay_slot(state, cache));

		/* Here, the delay slot did not raise an exception. */
		if (!reorderable) {
			mips32_alloc_specific_int_temp(state, cache, REG_BRANCH_SAVE_START);
		} else {
			/* Just in case the delay slot did not use the register cache. */
			ns = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]);
		}
		mips32_check_interrupt_and_jump_indirect(state, cache, ns);

		return end_compiled_jump(state, cache);
	}
}

enum TJEmitTraceResult mips32_emit_jalr(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretJALR || state->op_count != 2) {
		return mips32_emit_interpret(state, cache, &TJ_JALR, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
	} else {
		uint8_t rs = RS_OF(state->ops[0]), rd = RD_OF(state->ops[0]);
		/* If the delay slot writes the register used by the jump, its initial
		 * value must be saved. */
		bool reorderable = (IntGetReads(state->ops[0]) & IntGetWrites(state->ops[1])) == 0;

		mips32_start_opcode(state, cache, "JALR");

		if (!reorderable) {
			/* Must be allocated before 'ns', otherwise 'ns' may be the
			 * branch save register. */
			mips32_alloc_specific_int_temp(state, cache, REG_BRANCH_SAVE_START);
		}

		uint8_t ns = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
		        nd = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rd]);

		mips32_i32(state, nd, state->pc + 8);
		mips32_set_int_ex32(cache, &g_state.regs.gpr[rd],
			((state->pc + 8) & UINT32_C(0x80000000)) ? MRIT_MEM_OE32 : MRIT_MEM_ZE32);

		if (!reorderable) {
			mips32_or(state, REG_BRANCH_SAVE_START, ns, 0);
			ns = REG_BRANCH_SAVE_START;
		}

		prepare_compiled_jump(state, cache, 0);
		FAIL_AS(emit_delay_slot(state, cache));

		/* Here, the delay slot did not raise an exception. */
		if (!reorderable) {
			mips32_alloc_specific_int_temp(state, cache, REG_BRANCH_SAVE_START);
		} else {
			/* Just in case the delay slot did not use the register cache. */
			ns = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]);
		}
		mips32_check_interrupt_and_jump_indirect(state, cache, ns);

		return end_compiled_jump(state, cache);
	}
}

enum TJEmitTraceResult mips32_emit_bltz_idle(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_BLTZ_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

enum TJEmitTraceResult mips32_emit_bltz(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretBLTZ || state->op_count != 2) {
		return mips32_emit_interpret(state, cache, &TJ_BLTZ, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
	} else {
		uint8_t rs = RS_OF(state->ops[0]);
		uint32_t target = RELATIVE_TARGET(state->pc, IMM16S_OF(state->ops[0]));

		mips32_start_opcode(state, cache, "BLTZ");
		uint8_t ns;
		bool width_32, rs_dep;
		get_branch_rs_32(state, cache, rs, &ns, &width_32, &rs_dep);

		uint32_t extras = prepare_compiled_jump(state, cache, 0);
		FAIL_AS(emit_delay_slot(state, cache));

		/* Here, the delay slot did not raise an exception. */
		reget_branch_rs_32(state, cache, rs, &ns, width_32, rs_dep);

		/* Reduce the number of SWs that must be done in each path, while
		 * keeping some registers for the branch decision. */
		mips32_free_all_except_ints(state, cache, BIT(ns) | extras);

		/* Now we get to decide whether the branch is taken. */
		void* label_untaken = NULL;
		mips32_bgez(state, ns, +0);
		label_untaken = mips32_anticipate_label(state);
		mips32_nop(state);

		/* Here, the branch is taken. */
		/* The taken path will issue free_all. The untaken path needs to know
		 * what to write. Write into a copy. */
		struct mips32_reg_cache taken_cache;
		mips32_copy_reg_cache(&taken_cache, cache);
		mips32_check_interrupt_and_jump(state, &taken_cache, state->pc - 8, target);

		/* Here, the branch is not taken. */
		mips32_realize_label(state, label_untaken);
		mips32_free_all(state, cache);
		mips32_jump(state, state->pc - 8, state->pc);

		return end_compiled_jump(state, cache);
	}
}

enum TJEmitTraceResult mips32_emit_bgez_idle(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_BGEZ_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

enum TJEmitTraceResult mips32_emit_bgez(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	uint8_t rs = RS_OF(state->ops[0]);
	if (TraceJITSettings.InterpretBGEZ || state->op_count != 2) {
		return mips32_emit_interpret(state, cache, &TJ_BGEZ, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
	} else if (rs == 0) {
		uint32_t target = RELATIVE_TARGET(state->pc, IMM16S_OF(state->ops[0]));

		mips32_start_opcode(state, cache, "B");

		prepare_compiled_jump(state, cache, 0);
		FAIL_AS(emit_delay_slot(state, cache));

		/* Here, the delay slot did not raise an exception. */
		mips32_check_interrupt_and_jump(state, cache, state->pc - 8, target);

		return end_compiled_jump(state, cache);
	} else {
		uint32_t target = RELATIVE_TARGET(state->pc, IMM16S_OF(state->ops[0]));

		mips32_start_opcode(state, cache, "BGEZ");
		uint8_t ns;
		bool width_32, rs_dep;
		get_branch_rs_32(state, cache, rs, &ns, &width_32, &rs_dep);

		uint32_t extras = prepare_compiled_jump(state, cache, 0);
		FAIL_AS(emit_delay_slot(state, cache));

		/* Here, the delay slot did not raise an exception. */
		reget_branch_rs_32(state, cache, rs, &ns, width_32, rs_dep);

		/* Reduce the number of SWs that must be done in each path, while
		 * keeping some registers for the branch decision. */
		mips32_free_all_except_ints(state, cache, BIT(ns) | extras);

		/* Now we get to decide whether the branch is taken. */
		void* label_untaken = NULL;
		mips32_bltz(state, ns, +0);
		label_untaken = mips32_anticipate_label(state);
		mips32_nop(state);

		/* Here, the branch is taken. */
		/* The taken path will issue free_all. The untaken path needs to know
		 * what to write. Write into a copy. */
		struct mips32_reg_cache taken_cache;
		mips32_copy_reg_cache(&taken_cache, cache);
		mips32_check_interrupt_and_jump(state, &taken_cache, state->pc - 8, target);

		/* Here, the branch is not taken. */
		mips32_realize_label(state, label_untaken);
		mips32_free_all(state, cache);
		mips32_jump(state, state->pc - 8, state->pc);

		return end_compiled_jump(state, cache);
	}
}

enum TJEmitTraceResult mips32_emit_bltzl_idle(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_BLTZL_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

enum TJEmitTraceResult mips32_emit_bltzl(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretBLTZL || state->op_count != 2) {
		return mips32_emit_interpret(state, cache, &TJ_BLTZL, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
	} else {
		uint8_t rs = RS_OF(state->ops[0]);
		uint32_t target = RELATIVE_TARGET(state->pc, IMM16S_OF(state->ops[0]));
		bool width_32 = IntGetWidth(&state->widths, rs);

		mips32_start_opcode(state, cache, "BLTZL");

		uint8_t ns;
		if (width_32) {
			ns = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]);
		} else {
			ns = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rs]);
		}

		uint32_t extras = prepare_compiled_jump(state, cache, BRANCH_LIKELY);
		/* Reduce the number of SWs that must be done in each path, while
		 * keeping some registers for the branch decision and performance. */
		mips32_free_all_except_ints(state, cache, BIT(ns) | extras);

		/* Now we get to decide whether the branch is taken. */
		void* label_untaken = NULL;
		mips32_bgez(state, ns, +0);
		label_untaken = mips32_anticipate_label(state);
		mips32_nop(state);

		/* Here, the branch is taken. */
		/* The taken path will issue free_all. The untaken path needs to know
		 * what to write. Write into a copy. */
		struct mips32_reg_cache taken_cache;
		mips32_copy_reg_cache(&taken_cache, cache);
		FAIL_AS(emit_delay_slot(state, &taken_cache));

		/* Here, the delay slot did not raise an exception. */
		mips32_check_interrupt_and_jump(state, &taken_cache, state->pc - 8, target);

		/* Here, the branch is not taken. */
		mips32_realize_label(state, label_untaken);
		mips32_free_all(state, cache);
		mips32_jump(state, state->pc - 8, state->pc);

		return end_compiled_jump(state, cache);
	}
}

enum TJEmitTraceResult mips32_emit_bgezl_idle(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_BGEZL_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

enum TJEmitTraceResult mips32_emit_bgezl(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretBGEZL || state->op_count != 2) {
		return mips32_emit_interpret(state, cache, &TJ_BGEZL, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
	} else {
		uint8_t rs = RS_OF(state->ops[0]);
		uint32_t target = RELATIVE_TARGET(state->pc, IMM16S_OF(state->ops[0]));
		bool width_32 = IntGetWidth(&state->widths, rs);

		mips32_start_opcode(state, cache, "BGEZL");

		uint8_t ns;
		if (width_32) {
			ns = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]);
		} else {
			ns = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rs]);
		}

		uint32_t extras = prepare_compiled_jump(state, cache, BRANCH_LIKELY);
		/* Reduce the number of SWs that must be done in each path, while
		 * keeping some registers for the branch decision and performance. */
		mips32_free_all_except_ints(state, cache, BIT(ns) | extras);

		/* Now we get to decide whether the branch is taken. */
		void* label_untaken = NULL;
		mips32_bltz(state, ns, +0);
		label_untaken = mips32_anticipate_label(state);
		mips32_nop(state);

		/* Here, the branch is taken. */
		/* The taken path will issue free_all. The untaken path needs to know
		 * what to write. Write into a copy. */
		struct mips32_reg_cache taken_cache;
		mips32_copy_reg_cache(&taken_cache, cache);
		FAIL_AS(emit_delay_slot(state, &taken_cache));

		/* Here, the delay slot did not raise an exception. */
		mips32_check_interrupt_and_jump(state, &taken_cache, state->pc - 8, target);

		/* Here, the branch is not taken. */
		mips32_realize_label(state, label_untaken);
		mips32_free_all(state, cache);
		mips32_jump(state, state->pc - 8, state->pc);

		return end_compiled_jump(state, cache);
	}
}

enum TJEmitTraceResult mips32_emit_bltzal_idle(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_BLTZAL_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

enum TJEmitTraceResult mips32_emit_bltzal(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretBLTZAL || state->op_count != 2) {
		return mips32_emit_interpret(state, cache, &TJ_BLTZAL, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
	} else {
		uint8_t rs = RS_OF(state->ops[0]);
		uint32_t target = RELATIVE_TARGET(state->pc, IMM16S_OF(state->ops[0]));

		mips32_start_opcode(state, cache, "BLTZAL");

		uint8_t ns;
		bool width_32, rs_dep;
		get_branch_rs_32(state, cache, rs, &ns, &width_32, &rs_dep);

		uint32_t extras = prepare_compiled_jump(state, cache, BRANCH_LINK31);
		FAIL_AS(emit_delay_slot(state, cache));

		/* Here, the delay slot did not raise an exception. */
		reget_branch_rs_32(state, cache, rs, &ns, width_32, rs_dep);

		/* Reduce the number of SWs that must be done in each path, while
		 * keeping some registers for the branch decision. */
		mips32_free_all_except_ints(state, cache, BIT(ns) | extras);

		/* Now we get to decide whether the branch is taken. */
		void* label_untaken = NULL;
		mips32_bgez(state, ns, +0);
		label_untaken = mips32_anticipate_label(state);
		mips32_nop(state);

		/* Here, the branch is taken. */
		/* The taken path will issue free_all. The untaken path needs to know
		 * what to write. Write into a copy. */
		struct mips32_reg_cache taken_cache;
		mips32_copy_reg_cache(&taken_cache, cache);
		mips32_check_interrupt_and_jump(state, &taken_cache, state->pc - 8, target);

		/* Here, the branch is not taken. */
		mips32_realize_label(state, label_untaken);
		mips32_free_all(state, cache);
		mips32_jump(state, state->pc - 8, state->pc);

		return end_compiled_jump(state, cache);
	}
}

enum TJEmitTraceResult mips32_emit_bgezal_idle(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_BGEZAL_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

enum TJEmitTraceResult mips32_emit_bgezal(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	uint8_t rs = RS_OF(state->ops[0]);
	if (TraceJITSettings.InterpretBGEZAL || state->op_count != 2) {
		return mips32_emit_interpret(state, cache, &TJ_BGEZAL, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
	} else if (rs == 0) {
		uint32_t target = RELATIVE_TARGET(state->pc, IMM16S_OF(state->ops[0]));

		mips32_start_opcode(state, cache, "BAL");

		prepare_compiled_jump(state, cache, BRANCH_LINK31);
		FAIL_AS(emit_delay_slot(state, cache));

		/* Here, the delay slot did not raise an exception. */
		mips32_check_interrupt_and_jump(state, cache, state->pc - 8, target);

		mips32_end_opcode(state, cache);
		state->fallthrough = false;
		state->ending_compiled = true;
		return TJ_SUCCESS;
	} else {
		uint32_t target = RELATIVE_TARGET(state->pc, IMM16S_OF(state->ops[0]));

		mips32_start_opcode(state, cache, "BGEZAL");

		uint8_t ns;
		bool width_32, rs_dep;
		get_branch_rs_32(state, cache, rs, &ns, &width_32, &rs_dep);

		uint32_t extras = prepare_compiled_jump(state, cache, BRANCH_LINK31);
		FAIL_AS(emit_delay_slot(state, cache));

		/* Here, the delay slot did not raise an exception. */
		reget_branch_rs_32(state, cache, rs, &ns, width_32, rs_dep);

		/* Reduce the number of SWs that must be done in each path, while
		 * keeping some registers for the branch decision. */
		mips32_free_all_except_ints(state, cache, BIT(ns) | extras);

		/* Now we get to decide whether the branch is taken. */
		void* label_untaken = NULL;
		mips32_bltz(state, ns, +0);
		label_untaken = mips32_anticipate_label(state);
		mips32_nop(state);

		/* Here, the branch is taken. */
		/* The taken path will issue free_all. The untaken path needs to know
		 * what to write. Write into a copy. */
		struct mips32_reg_cache taken_cache;
		mips32_copy_reg_cache(&taken_cache, cache);
		mips32_check_interrupt_and_jump(state, &taken_cache, state->pc - 8, target);

		/* Here, the branch is not taken. */
		mips32_realize_label(state, label_untaken);
		mips32_free_all(state, cache);
		mips32_jump(state, state->pc - 8, state->pc);

		return end_compiled_jump(state, cache);
	}
}

enum TJEmitTraceResult mips32_emit_bltzall_idle(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_BLTZALL_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

enum TJEmitTraceResult mips32_emit_bltzall(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_BLTZALL, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

enum TJEmitTraceResult mips32_emit_bgezall_idle(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_BGEZALL_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

enum TJEmitTraceResult mips32_emit_bgezall(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_BGEZALL, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

enum TJEmitTraceResult mips32_emit_j_idle(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_J_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

enum TJEmitTraceResult mips32_emit_j(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretJ || state->op_count != 2) {
		return mips32_emit_interpret(state, cache, &TJ_J, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
	} else {
		uint32_t target = ABSOLUTE_TARGET(state->pc, JUMP_OF(state->ops[0]));

		mips32_start_opcode(state, cache, "J");

		prepare_compiled_jump(state, cache, 0);
		FAIL_AS(emit_delay_slot(state, cache));

		mips32_check_interrupt_and_jump(state, cache, state->pc - 8, target);

		return end_compiled_jump(state, cache);
	}
}

enum TJEmitTraceResult mips32_emit_jal_idle(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_JAL_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

enum TJEmitTraceResult mips32_emit_jal(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretJAL || state->op_count != 2) {
		return mips32_emit_interpret(state, cache, &TJ_JAL, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
	} else {
		uint32_t target = ABSOLUTE_TARGET(state->pc, JUMP_OF(state->ops[0]));

		mips32_start_opcode(state, cache, "JAL");

		prepare_compiled_jump(state, cache, BRANCH_LINK31);
		FAIL_AS(emit_delay_slot(state, cache));

		mips32_check_interrupt_and_jump(state, cache, state->pc - 8, target);

		return end_compiled_jump(state, cache);
	}
}

enum TJEmitTraceResult mips32_emit_beq_idle(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_BEQ_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

enum TJEmitTraceResult mips32_emit_beq(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretBEQ || state->op_count != 2) {
		return mips32_emit_interpret(state, cache, &TJ_BEQ, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
	} else {
		uint8_t rs = RS_OF(state->ops[0]), rt = RT_OF(state->ops[0]);
		uint32_t target = RELATIVE_TARGET(state->pc, IMM16S_OF(state->ops[0]));

		mips32_start_opcode(state, cache, "BEQ");
		uint8_t ns_hi, nt_hi, ns_lo, nt_lo;
		bool rs_dep, rt_dep, width_32;
		get_branch_rs_rt(state, cache, rs, rt, &ns_hi, &ns_lo, &nt_hi, &nt_lo, &width_32, &rs_dep, &rt_dep);

		uint32_t extras = prepare_compiled_jump(state, cache, 0);
		FAIL_AS(emit_delay_slot(state, cache));

		/* Here, the delay slot did not raise an exception. */
		reget_branch_rs_rt(state, cache, rs, rt, &ns_hi, &ns_lo, &nt_hi, &nt_lo, width_32, rs_dep, rt_dep);

		/* Reduce the number of SWs that must be done in each path, while
		 * keeping some registers for the branch decision. */
		mips32_free_all_except_ints(state, cache, BIT(ns_lo) | BIT(nt_lo) | BIT(ns_hi) | BIT(nt_hi) | extras);

		/* Now we get to decide whether the branch is taken. */
		void* label_untaken_lo = NULL;
		void* label_untaken_hi = NULL;
		mips32_bne(state, ns_lo, nt_lo, +0);
		label_untaken_lo = mips32_anticipate_label(state);
		mips32_nop(state);
		if (!width_32) {
			mips32_bne(state, ns_hi, nt_hi, +0);
			label_untaken_hi = mips32_anticipate_label(state);
			mips32_nop(state);
		}

		/* Here, the branch is taken. */
		/* The taken path will issue free_all. The untaken path needs to know
		 * what to write. Write into a copy. */
		struct mips32_reg_cache taken_cache;
		mips32_copy_reg_cache(&taken_cache, cache);
		mips32_check_interrupt_and_jump(state, &taken_cache, state->pc - 8, target);

		/* Here, the branch is not taken. */
		mips32_realize_label(state, label_untaken_lo);
		mips32_realize_label(state, label_untaken_hi);
		mips32_free_all(state, cache);
		mips32_jump(state, state->pc - 8, state->pc);

		return end_compiled_jump(state, cache);
	}
}

enum TJEmitTraceResult mips32_emit_bne_idle(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_BNE_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

enum TJEmitTraceResult mips32_emit_bne(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretBNE || state->op_count != 2) {
		return mips32_emit_interpret(state, cache, &TJ_BNE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
	} else {
		uint8_t rs = RS_OF(state->ops[0]), rt = RT_OF(state->ops[0]);
		uint32_t target = RELATIVE_TARGET(state->pc, IMM16S_OF(state->ops[0]));

		mips32_start_opcode(state, cache, "BNE");
		uint8_t ns_hi, nt_hi, ns_lo, nt_lo;
		bool rs_dep, rt_dep, width_32;
		get_branch_rs_rt(state, cache, rs, rt, &ns_hi, &ns_lo, &nt_hi, &nt_lo, &width_32, &rs_dep, &rt_dep);

		uint32_t extras = prepare_compiled_jump(state, cache, 0);
		FAIL_AS(emit_delay_slot(state, cache));

		/* Here, the delay slot did not raise an exception. */
		reget_branch_rs_rt(state, cache, rs, rt, &ns_hi, &ns_lo, &nt_hi, &nt_lo, width_32, rs_dep, rt_dep);

		/* Reduce the number of SWs that must be done in each path, while
		 * keeping some registers for the branch decision. */
		mips32_free_all_except_ints(state, cache, BIT(ns_lo) | BIT(nt_lo) | BIT(ns_hi) | BIT(nt_hi) | extras);

		/* Now we get to decide whether the branch is taken. */
		void* label_untaken = NULL;
		if (width_32) {
			mips32_beq(state, ns_lo, nt_lo, +0);
			label_untaken = mips32_anticipate_label(state);
			mips32_nop(state);
		} else if (rt == 0) {
			uint8_t nt1 = mips32_alloc_int_temp(state, cache);
			mips32_or(state, nt1, ns_lo, ns_hi);
			mips32_beq(state, nt1, 0, +0);
			label_untaken = mips32_anticipate_label(state);
			mips32_nop(state);
		} else if (rs == 0) {
			uint8_t nt1 = mips32_alloc_int_temp(state, cache);
			mips32_or(state, nt1, nt_lo, nt_hi);
			mips32_beq(state, nt1, 0, +0);
			label_untaken = mips32_anticipate_label(state);
			mips32_nop(state);
		} else {
			uint8_t nt1 = mips32_alloc_int_temp(state, cache),
			        nt2 = mips32_alloc_int_temp(state, cache);
			mips32_xor(state, nt1, ns_lo, nt_lo);
			mips32_xor(state, nt2, ns_hi, nt_hi);
			mips32_or(state, nt1, nt1, nt2);
			mips32_beq(state, nt1, 0, +0);
			label_untaken = mips32_anticipate_label(state);
			mips32_nop(state);
		}

		/* Here, the branch is taken. */
		/* The taken path will issue free_all. The untaken path needs to know
		 * what to write. Write into a copy. */
		struct mips32_reg_cache taken_cache;
		mips32_copy_reg_cache(&taken_cache, cache);
		mips32_check_interrupt_and_jump(state, &taken_cache, state->pc - 8, target);

		/* Here, the branch is not taken. */
		mips32_realize_label(state, label_untaken);
		mips32_free_all(state, cache);
		mips32_jump(state, state->pc - 8, state->pc);

		return end_compiled_jump(state, cache);
	}
}

enum TJEmitTraceResult mips32_emit_blez_idle(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_BLEZ_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

enum TJEmitTraceResult mips32_emit_blez(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretBLEZ || state->op_count != 2) {
		return mips32_emit_interpret(state, cache, &TJ_BLEZ, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
	} else {
		uint8_t rs = RS_OF(state->ops[0]);
		uint32_t target = RELATIVE_TARGET(state->pc, IMM16S_OF(state->ops[0]));

		mips32_start_opcode(state, cache, "BLEZ");
		uint8_t ns_hi, ns_lo;
		bool width_32, rs_dep;
		get_branch_rs(state, cache, rs, &ns_hi, &ns_lo, &width_32, &rs_dep);

		uint32_t extras = prepare_compiled_jump(state, cache, 0);
		FAIL_AS(emit_delay_slot(state, cache));

		/* Here, the delay slot did not raise an exception. */
		reget_branch_rs(state, cache, rs, &ns_hi, &ns_lo, width_32, rs_dep);

		/* Reduce the number of SWs that must be done in each path, while
		 * keeping some registers for the branch decision. */
		mips32_free_all_except_ints(state, cache, BIT(ns_lo) | BIT(ns_hi) | extras);

		/* Now we get to decide whether the branch is taken. */
		void* label_taken = NULL;
		void* label_untaken = NULL;
		if (width_32) {
			mips32_bgtz(state, ns_lo, +0);
			label_untaken = mips32_anticipate_label(state);
			mips32_nop(state);
		} else {
			/* The jump is taken if rs <= 0. So if rs_hi | rs_lo == 0,
			 * it's taken. Then if rs >= 0, it's not taken. */
			uint8_t nt1 = mips32_alloc_int_temp(state, cache);
			mips32_or(state, nt1, ns_hi, ns_lo);
			mips32_beq(state, nt1, 0, +0);
			label_taken = mips32_anticipate_label(state);
			mips32_nop(state);
			mips32_bgez(state, ns_hi, +0);
			label_untaken = mips32_anticipate_label(state);
			mips32_nop(state);
		}

		/* Here, the branch is taken. */
		mips32_realize_label(state, label_taken);
		/* The taken path will issue free_all. The untaken path needs to know
		 * what to write. Write into a copy. */
		struct mips32_reg_cache taken_cache;
		mips32_copy_reg_cache(&taken_cache, cache);
		mips32_check_interrupt_and_jump(state, &taken_cache, state->pc - 8, target);

		/* Here, the branch is not taken. */
		mips32_realize_label(state, label_untaken);
		mips32_free_all(state, cache);
		mips32_jump(state, state->pc - 8, state->pc);

		return end_compiled_jump(state, cache);
	}
}

enum TJEmitTraceResult mips32_emit_bgtz_idle(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_BGTZ_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

enum TJEmitTraceResult mips32_emit_bgtz(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretBGTZ || state->op_count != 2) {
		return mips32_emit_interpret(state, cache, &TJ_BGTZ, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
	} else {
		uint8_t rs = RS_OF(state->ops[0]);
		uint32_t target = RELATIVE_TARGET(state->pc, IMM16S_OF(state->ops[0]));

		mips32_start_opcode(state, cache, "BGTZ");
		uint8_t ns_hi, ns_lo;
		bool width_32, rs_dep;
		get_branch_rs(state, cache, rs, &ns_hi, &ns_lo, &width_32, &rs_dep);

		uint32_t extras = prepare_compiled_jump(state, cache, 0);
		FAIL_AS(emit_delay_slot(state, cache));

		/* Here, the delay slot did not raise an exception. */
		reget_branch_rs(state, cache, rs, &ns_hi, &ns_lo, width_32, rs_dep);

		/* Reduce the number of SWs that must be done in each path, while
		 * keeping some registers for the branch decision. */
		mips32_free_all_except_ints(state, cache, BIT(ns_lo) | BIT(ns_hi) | extras);

		/* Now we get to decide whether the branch is taken. */
		void* label_untaken_lo = NULL;
		void* label_untaken_hi = NULL;
		if (width_32) {
			mips32_blez(state, ns_lo, +0);
			label_untaken_lo = mips32_anticipate_label(state);
			mips32_nop(state);
		} else {
			/* The jump is taken if rs > 0. So it's not taken if
			 * rs < 0, or if rs_hi | rs_lo == 0. */
			uint8_t nt1 = mips32_alloc_int_temp(state, cache);
			mips32_bltz(state, ns_hi, +0);
			label_untaken_hi = mips32_anticipate_label(state);
			mips32_or(state, nt1, ns_hi, ns_lo);
			mips32_beq(state, nt1, 0, +0);
			label_untaken_lo = mips32_anticipate_label(state);
			mips32_nop(state);
		}

		/* Here, the branch is taken. */
		/* The taken path will issue free_all. The untaken path needs to know
		 * what to write. Write into a copy. */
		struct mips32_reg_cache taken_cache;
		mips32_copy_reg_cache(&taken_cache, cache);
		mips32_check_interrupt_and_jump(state, &taken_cache, state->pc - 8, target);

		/* Here, the branch is not taken. */
		mips32_realize_label(state, label_untaken_lo);
		mips32_realize_label(state, label_untaken_hi);
		mips32_free_all(state, cache);
		mips32_jump(state, state->pc - 8, state->pc);

		return end_compiled_jump(state, cache);
	}
}

enum TJEmitTraceResult mips32_emit_bc1f_idle(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_BC1F_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

enum TJEmitTraceResult mips32_emit_bc1f(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretBC1F || state->op_count != 2) {
		return mips32_emit_interpret(state, cache, &TJ_BC1F, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
	} else {
		uint32_t target = RELATIVE_TARGET(state->pc, IMM16S_OF(state->ops[0]));

		mips32_start_opcode(state, cache, "BC1F");
		uint8_t nc1_cond = get_c1cond(state, cache);

		uint32_t extras = prepare_compiled_jump(state, cache, 0);
		FAIL_AS(emit_delay_slot(state, cache));

		/* Here, the delay slot did not raise an exception. */

		/* Reduce the number of SWs that must be done in each path, while
		 * keeping some registers for the branch decision. */
		mips32_free_all_except_ints(state, cache, BIT(nc1_cond) | extras);

		/* Now we get to decide whether the branch is taken. */
		mips32_bne(state, nc1_cond, 0, +0);
		void* label_untaken = mips32_anticipate_label(state);
		mips32_nop(state);

		/* Here, the branch is taken. */
		/* The taken path will issue free_all. The untaken path needs to know
		 * what to write. Write into a copy. */
		struct mips32_reg_cache taken_cache;
		mips32_copy_reg_cache(&taken_cache, cache);
		mips32_check_interrupt_and_jump(state, &taken_cache, state->pc - 8, target);

		/* Here, the branch is not taken. */
		mips32_realize_label(state, label_untaken);
		mips32_free_all(state, cache);
		mips32_jump(state, state->pc - 8, state->pc);

		return end_compiled_jump(state, cache);
	}
}

enum TJEmitTraceResult mips32_emit_bc1t_idle(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_BC1T_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

enum TJEmitTraceResult mips32_emit_bc1t(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretBC1T || state->op_count != 2) {
		return mips32_emit_interpret(state, cache, &TJ_BC1T, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
	} else {
		uint32_t target = RELATIVE_TARGET(state->pc, IMM16S_OF(state->ops[0]));

		mips32_start_opcode(state, cache, "BC1T");
		uint8_t nc1_cond = get_c1cond(state, cache);

		uint32_t extras = prepare_compiled_jump(state, cache, 0);
		FAIL_AS(emit_delay_slot(state, cache));

		/* Here, the delay slot did not raise an exception. */

		/* Reduce the number of SWs that must be done in each path, while
		 * keeping some registers for the branch decision. */
		mips32_free_all_except_ints(state, cache, BIT(nc1_cond) | extras);

		/* Now we get to decide whether the branch is taken. */
		mips32_beq(state, nc1_cond, 0, +0);
		void* label_untaken = mips32_anticipate_label(state);
		mips32_nop(state);

		/* Here, the branch is taken. */
		/* The taken path will issue free_all. The untaken path needs to know
		 * what to write. Write into a copy. */
		struct mips32_reg_cache taken_cache;
		mips32_copy_reg_cache(&taken_cache, cache);
		mips32_check_interrupt_and_jump(state, &taken_cache, state->pc - 8, target);

		/* Here, the branch is not taken. */
		mips32_realize_label(state, label_untaken);
		mips32_free_all(state, cache);
		mips32_jump(state, state->pc - 8, state->pc);

		return end_compiled_jump(state, cache);
	}
}

enum TJEmitTraceResult mips32_emit_bc1fl_idle(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_BC1FL_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

enum TJEmitTraceResult mips32_emit_bc1fl(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretBC1FL || state->op_count != 2) {
		return mips32_emit_interpret(state, cache, &TJ_BC1FL, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
	} else {
		uint32_t target = RELATIVE_TARGET(state->pc, IMM16S_OF(state->ops[0]));

		mips32_start_opcode(state, cache, "BC1FL");

		uint8_t nc1_cond = get_c1cond(state, cache);

		uint32_t extras = prepare_compiled_jump(state, cache, BRANCH_LIKELY);
		/* Reduce the number of SWs that must be done in each path, while
		 * keeping some registers for the branch decision and performance. */
		mips32_free_all_except_ints(state, cache, BIT(nc1_cond) | extras);

		/* Now we get to decide whether the branch is taken. */
		mips32_bne(state, nc1_cond, 0, +0);
		void* label_untaken = mips32_anticipate_label(state);
		mips32_nop(state);

		/* Here, the branch is taken. */
		/* The taken path will issue free_all. The untaken path needs to know
		 * what to write. Write into a copy. */
		struct mips32_reg_cache taken_cache;
		mips32_copy_reg_cache(&taken_cache, cache);
		FAIL_AS(emit_delay_slot(state, &taken_cache));

		/* Here, the delay slot did not raise an exception. */
		mips32_check_interrupt_and_jump(state, &taken_cache, state->pc - 8, target);

		/* Here, the branch is not taken. */
		mips32_realize_label(state, label_untaken);
		mips32_free_all(state, cache);
		mips32_jump(state, state->pc - 8, state->pc);

		return end_compiled_jump(state, cache);
	}
}

enum TJEmitTraceResult mips32_emit_bc1tl_idle(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_BC1TL_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

enum TJEmitTraceResult mips32_emit_bc1tl(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretBC1TL || state->op_count != 2) {
		return mips32_emit_interpret(state, cache, &TJ_BC1TL, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
	} else {
		uint32_t target = RELATIVE_TARGET(state->pc, IMM16S_OF(state->ops[0]));

		mips32_start_opcode(state, cache, "BC1TL");

		uint8_t nc1_cond = get_c1cond(state, cache);

		uint32_t extras = prepare_compiled_jump(state, cache, BRANCH_LIKELY);
		/* Reduce the number of SWs that must be done in each path, while
		 * keeping some registers for the branch decision and performance. */
		mips32_free_all_except_ints(state, cache, BIT(nc1_cond) | extras);

		/* Now we get to decide whether the branch is taken. */
		mips32_beq(state, nc1_cond, 0, +0);
		void* label_untaken = mips32_anticipate_label(state);
		mips32_nop(state);

		/* Here, the branch is taken. */
		/* The taken path will issue free_all. The untaken path needs to know
		 * what to write. Write into a copy. */
		struct mips32_reg_cache taken_cache;
		mips32_copy_reg_cache(&taken_cache, cache);
		FAIL_AS(emit_delay_slot(state, &taken_cache));

		/* Here, the delay slot did not raise an exception. */
		mips32_check_interrupt_and_jump(state, &taken_cache, state->pc - 8, target);

		/* Here, the branch is not taken. */
		mips32_realize_label(state, label_untaken);
		mips32_free_all(state, cache);
		mips32_jump(state, state->pc - 8, state->pc);

		return end_compiled_jump(state, cache);
	}
}

enum TJEmitTraceResult mips32_emit_beql_idle(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_BEQL_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

enum TJEmitTraceResult mips32_emit_beql(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretBEQL || state->op_count != 2) {
		return mips32_emit_interpret(state, cache, &TJ_BEQL, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
	} else {
		uint8_t rs = RS_OF(state->ops[0]), rt = RT_OF(state->ops[0]);
		bool width_32 = IntGetWidth(&state->widths, rs) <= 32
		             && IntGetWidth(&state->widths, rt) <= 32;
		uint32_t target = RELATIVE_TARGET(state->pc, IMM16S_OF(state->ops[0]));

		mips32_start_opcode(state, cache, "BEQL");

		uint8_t ns_hi = 0, nt_hi = 0, ns_lo, nt_lo;
		if (!width_32) {
			ns_hi = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rs]);
			nt_hi = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rt]);
		}
		ns_lo = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]);
		nt_lo = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]);

		uint32_t extras = prepare_compiled_jump(state, cache, BRANCH_LIKELY);
		/* Reduce the number of SWs that must be done in each path, while
		 * keeping some registers for the branch decision and performance. */
		mips32_free_all_except_ints(state, cache, BIT(ns_lo) | BIT(nt_lo) | BIT(ns_hi) | BIT(nt_hi) | extras);

		/* Now we get to decide whether the branch is taken. */
		void* label_untaken_lo = NULL;
		void* label_untaken_hi = NULL;
		mips32_bne(state, ns_lo, nt_lo, +0);
		label_untaken_lo = mips32_anticipate_label(state);
		mips32_nop(state);
		if (!width_32) {
			mips32_bne(state, ns_hi, nt_hi, +0);
			label_untaken_hi = mips32_anticipate_label(state);
			mips32_nop(state);
		}

		/* Here, the branch is taken. */
		/* The taken path will issue free_all. The untaken path needs to know
		 * what to write. Write into a copy. */
		struct mips32_reg_cache taken_cache;
		mips32_copy_reg_cache(&taken_cache, cache);
		FAIL_AS(emit_delay_slot(state, &taken_cache));

		mips32_check_interrupt_and_jump(state, &taken_cache, state->pc - 8, target);

		/* Here, the branch is not taken. */
		mips32_realize_label(state, label_untaken_lo);
		mips32_realize_label(state, label_untaken_hi);
		mips32_free_all(state, cache);
		mips32_jump(state, state->pc - 8, state->pc);

		return end_compiled_jump(state, cache);
	}
}

enum TJEmitTraceResult mips32_emit_bnel_idle(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_BNEL_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

enum TJEmitTraceResult mips32_emit_bnel(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretBNEL || state->op_count != 2) {
		return mips32_emit_interpret(state, cache, &TJ_BNEL, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
	} else {
		uint8_t rs = RS_OF(state->ops[0]), rt = RT_OF(state->ops[0]);
		bool width_32 = IntGetWidth(&state->widths, rs) <= 32
		             && IntGetWidth(&state->widths, rt) <= 32;
		uint32_t target = RELATIVE_TARGET(state->pc, IMM16S_OF(state->ops[0]));

		mips32_start_opcode(state, cache, "BNEL");

		uint8_t ns_hi = 0, nt_hi = 0, ns_lo, nt_lo;
		if (!width_32) {
			ns_hi = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rs]);
			nt_hi = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rt]);
		}
		ns_lo = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]);
		nt_lo = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]);

		uint32_t extras = prepare_compiled_jump(state, cache, BRANCH_LIKELY);
		/* Reduce the number of SWs that must be done in each path, while
		 * keeping some registers for the branch decision and performance. */
		mips32_free_all_except_ints(state, cache, BIT(ns_lo) | BIT(nt_lo) | BIT(ns_hi) | BIT(nt_hi) | extras);

		/* Now we get to decide whether the branch is taken. */
		void* label_untaken = NULL;
		if (width_32) {
			mips32_beq(state, ns_lo, nt_lo, +0);
			label_untaken = mips32_anticipate_label(state);
			mips32_nop(state);
		} else if (rt == 0) {
			uint8_t nt1 = mips32_alloc_int_temp(state, cache);
			mips32_or(state, nt1, ns_lo, ns_hi);
			mips32_beq(state, nt1, 0, +0);
			label_untaken = mips32_anticipate_label(state);
			mips32_nop(state);
		} else if (rs == 0) {
			uint8_t nt1 = mips32_alloc_int_temp(state, cache);
			mips32_or(state, nt1, nt_lo, nt_hi);
			mips32_beq(state, nt1, 0, +0);
			label_untaken = mips32_anticipate_label(state);
			mips32_nop(state);
		} else {
			uint8_t nt1 = mips32_alloc_int_temp(state, cache),
			        nt2 = mips32_alloc_int_temp(state, cache);
			mips32_xor(state, nt1, ns_lo, nt_lo);
			mips32_xor(state, nt2, ns_hi, nt_hi);
			mips32_or(state, nt1, nt1, nt2);
			mips32_beq(state, nt1, 0, +0);
			label_untaken = mips32_anticipate_label(state);
			mips32_nop(state);
		}

		/* Here, the branch is taken. */
		/* The taken path will issue free_all. The untaken path needs to know
		 * what to write. Write into a copy. */
		struct mips32_reg_cache taken_cache;
		mips32_copy_reg_cache(&taken_cache, cache);
		FAIL_AS(emit_delay_slot(state, &taken_cache));

		mips32_check_interrupt_and_jump(state, &taken_cache, state->pc - 8, target);

		/* Here, the branch is not taken. */
		mips32_realize_label(state, label_untaken);
		mips32_free_all(state, cache);
		mips32_jump(state, state->pc - 8, state->pc);

		return end_compiled_jump(state, cache);
	}
}

enum TJEmitTraceResult mips32_emit_blezl_idle(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_BLEZL_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

enum TJEmitTraceResult mips32_emit_blezl(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretBLEZL || state->op_count != 2) {
		return mips32_emit_interpret(state, cache, &TJ_BLEZL, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
	} else {
		uint8_t rs = RS_OF(state->ops[0]);
		bool width_32 = IntGetWidth(&state->widths, rs) <= 32;
		uint32_t target = RELATIVE_TARGET(state->pc, IMM16S_OF(state->ops[0]));

		mips32_start_opcode(state, cache, "BLEZL");

		uint8_t ns_hi = 0, ns_lo;
		if (!width_32) {
			ns_hi = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rs]);
		}
		ns_lo = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]);

		uint32_t extras = prepare_compiled_jump(state, cache, BRANCH_LIKELY);
		/* Reduce the number of SWs that must be done in each path, while
		 * keeping some registers for the branch decision and performance. */
		mips32_free_all_except_ints(state, cache, BIT(ns_lo) | BIT(ns_hi) | extras);

		/* Now we get to decide whether the branch is taken. */
		void* label_taken = NULL;
		void* label_untaken = NULL;
		if (width_32) {
			mips32_bgtz(state, ns_lo, +0);
			label_untaken = mips32_anticipate_label(state);
			mips32_nop(state);
		} else {
			/* The jump is taken if rs <= 0. So if rs_hi | rs_lo == 0,
			 * it's taken. Then if rs >= 0, it's not taken. */
			uint8_t nt1 = mips32_alloc_int_temp(state, cache);
			mips32_or(state, nt1, ns_hi, ns_lo);
			mips32_beq(state, nt1, 0, +0);
			label_taken = mips32_anticipate_label(state);
			mips32_nop(state);
			mips32_bgez(state, ns_hi, +0);
			label_untaken = mips32_anticipate_label(state);
			mips32_nop(state);
		}

		/* Here, the branch is taken. */
		mips32_realize_label(state, label_taken);
		/* The taken path will issue free_all. The untaken path needs to know
		 * what to write. Write into a copy. */
		struct mips32_reg_cache taken_cache;
		mips32_copy_reg_cache(&taken_cache, cache);
		FAIL_AS(emit_delay_slot(state, &taken_cache));

		/* Here, the delay slot did not raise an exception. */
		mips32_check_interrupt_and_jump(state, &taken_cache, state->pc - 8, target);

		/* Here, the branch is not taken. */
		mips32_realize_label(state, label_untaken);
		mips32_free_all(state, cache);
		mips32_jump(state, state->pc - 8, state->pc);

		return end_compiled_jump(state, cache);
	}
}

enum TJEmitTraceResult mips32_emit_bgtzl_idle(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_BGTZL_IDLE, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
}

enum TJEmitTraceResult mips32_emit_bgtzl(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretBGTZL || state->op_count != 2) {
		return mips32_emit_interpret(state, cache, &TJ_BGTZL, TJ_READS_PC | TJ_TRANSFERS_CONTROL | TJ_HAS_DELAY_SLOT);
	} else {
		uint8_t rs = RS_OF(state->ops[0]);
		bool width_32 = IntGetWidth(&state->widths, rs) <= 32;
		uint32_t target = RELATIVE_TARGET(state->pc, IMM16S_OF(state->ops[0]));

		mips32_start_opcode(state, cache, "BGTZL");

		uint8_t ns_hi = 0, ns_lo;
		if (!width_32) {
			ns_hi = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rs]);
		}
		ns_lo = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]);

		uint32_t extras = prepare_compiled_jump(state, cache, BRANCH_LIKELY);
		/* Reduce the number of SWs that must be done in each path, while
		 * keeping some registers for the branch decision and performance. */
		mips32_free_all_except_ints(state, cache, BIT(ns_lo) | BIT(ns_hi) | extras);

		/* Now we get to decide whether the branch is taken. */
		void* label_untaken_lo = NULL;
		void* label_untaken_hi = NULL;
		if (width_32) {
			mips32_blez(state, ns_lo, +0);
			label_untaken_lo = mips32_anticipate_label(state);
			mips32_nop(state);
		} else {
			/* The jump is taken if rs > 0. So it's not taken if
			 * rs < 0, or if rs_hi | rs_lo == 0. */
			uint8_t nt1 = mips32_alloc_int_temp(state, cache);
			mips32_bltz(state, ns_hi, +0);
			label_untaken_hi = mips32_anticipate_label(state);
			mips32_or(state, nt1, ns_hi, ns_lo);
			mips32_beq(state, nt1, 0, +0);
			label_untaken_lo = mips32_anticipate_label(state);
			mips32_nop(state);
		}

		/* Here, the branch is taken. */
		/* The taken path will issue free_all. The untaken path needs to know
		 * what to write. Write into a copy. */
		struct mips32_reg_cache taken_cache;
		mips32_copy_reg_cache(&taken_cache, cache);
		FAIL_AS(emit_delay_slot(state, &taken_cache));

		/* Here, the delay slot did not raise an exception. */
		mips32_check_interrupt_and_jump(state, &taken_cache, state->pc - 8, target);

		/* Here, the branch is not taken. */
		mips32_realize_label(state, label_untaken_lo);
		mips32_realize_label(state, label_untaken_hi);
		mips32_free_all(state, cache);
		mips32_jump(state, state->pc - 8, state->pc);

		return end_compiled_jump(state, cache);
	}
}
