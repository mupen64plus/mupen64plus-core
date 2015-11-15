/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - Common MIPS (native) code for N64 instructions          *
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

#include "mips-common.h"
#include "mips-emit.h"
#include "native-endian.h"
#include "native-ops.h"
#include "native-regcache.h"
#include "native-utils.h"

#include "../mips-interp.h"
#include "../native-config.h"
#include "../native-tracecache.h"

#include "../../r4300.h"

void mips32_ensure_rounding(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (!state->rounding_set) {
		mips32_start_opcode(state, cache, "[rounding mode upload]");
		uint8_t nfcr_31 = mips32_alloc_int_temp(state, cache),
		        nt1 = mips32_alloc_int_temp(state, cache),
		        efcr_31 = mips32_alloc_int_in_32(state, cache, &g_state.regs.fcr_31);
		mips32_cfc1(state, nfcr_31, 31);
		/* Merge the two low bits of the Nintendo 64 FCR31 into the native FCR31:
		 *    (native & ~3) | (N64 & 3)
		 * == native ^ ((native ^ N64) & 3)
		 * <http://graphics.stanford.edu/~seander/bithacks.html#MaskedMerge> */
		mips32_xor(state, nt1, efcr_31, nfcr_31);
		mips32_andi(state, nt1, nt1, 0x3);
		mips32_xor(state, nfcr_31, nt1, nfcr_31);
		mips32_ctc1(state, nfcr_31, 31);
		state->rounding_set = true;
		mips32_end_opcode(state, cache);
	}
}

void mips32_add_to_count(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	uint32_t Cycles = ((state->pc - state->last_count_update_pc) / 4 + 2) * count_per_op;
	state->last_count_update_pc = state->pc;

	mips32_start_opcode(state, cache, "[cycle count update]");
	uint8_t ncount = mips32_alloc_int_in_32(state, cache, &g_state.regs.cp0[CP0_COUNT_REG]);
	mips32_alloc_int_out_32(state, cache, &g_state.regs.cp0[CP0_COUNT_REG]);
	mips32_addiu(state, ncount, ncount, Cycles);
	mips32_end_opcode(state, cache);
}

void mips32_jump(struct mips32_state* state, uint32_t source, uint32_t target)
{
	if (state->code == NULL)
		return;

	if ((source & ~UINT32_C(0xFFF)) == (target & ~UINT32_C(0xFFF))) {
		void* native_target = (target == state->original_pc)
			? state->after_setup
			: GetTraceAt(target);

		mips32_i32(state, 8, target);
		mips32_sw_abs(state, 8, 9, &TJ_PC.addr);

		if (state->code == NULL)
			return;

		if (native_target == NOT_CODE || native_target == FORMERLY_CODE) {
			/* We don't know where to go yet, and we're still emitting code.
			 * Emit a call to the dynamic linker instead, which will sort it
			 * out when we are no longer emitting. */
			void* start = state->code;
			void* end;
			mips32_i32(state, REG_ARG1, source);
			mips32_addiu(state, REG_ARG2, REG_ARG1, target - source);
			mips32_i32(state, REG_ARG3, (uintptr_t) start);
			/* Save code space if possible by emitting a J to the linker. */
			if (state->code && mips32_can_jump26(state->code, mips32_linker_entry)) {
				mips32_j(state, mips32_linker_entry);
			} else {
				mips32_i32(state, REG_PIC_CALL, (uintptr_t) mips32_linker_entry);
				mips32_jr(state, REG_PIC_CALL);
			}
			end = state->code;
			if (end != NULL) {
				end = (uint32_t*) end + 1;
				mips32_ori(state, REG_ARG4, 0, (uint8_t*) end - (uint8_t*) start);
			}
		} else {
			/* Try BGEZ $0. */
			if (mips32_can_jump16(state->code, native_target)) {
				mips32_bgez(state, 0, mips32_get_jump16(state->code, native_target));
			}
			/* Next, try J. */
			else if (mips32_can_jump26(state->code, native_target)) {
				mips32_j(state, native_target);
			}
			/* Finally, there's always LUI ORI JR. */
			else {
				mips32_i32(state, REG_EMERGENCY, (uintptr_t) native_target);
				mips32_jr(state, REG_EMERGENCY);
			}
			mips32_nop(state);
		}
	} else {
		mips32_i32(state, REG_PIC_CALL, (uintptr_t) mips32_indirect_jump_entry);
		mips32_i32(state, REG_ARG1, target);
		mips32_jr(state, REG_PIC_CALL);
		mips32_borrow_delay(state);
	}
}

void mips32_check_interrupt_and_jump(struct mips32_state* state, struct mips32_reg_cache* cache, uint32_t source, uint32_t target)
{
	uint8_t ncount = mips32_alloc_int_in_32(state, cache, &g_state.regs.cp0[CP0_COUNT_REG]),
	        ninterrupt = mips32_alloc_int_in_32(state, cache, &g_state.next_interrupt),
	        nt1 = mips32_alloc_int_temp(state, cache);
	mips32_subu(state, nt1, ncount, ninterrupt);
	mips32_free_all_except_ints(state, cache, BIT(nt1));
	/* If Count < next_interrupt, it's not yet time for an interrupt... */
	mips32_bgez(state, nt1, +0);
	void* label_interrupt = mips32_anticipate_label(state);
	mips32_nop(state);

	/* Here, no interrupt needs to be taken. */
	mips32_jump(state, source, target);

	/* Here, an interrupt must be taken. */
	mips32_realize_label(state, label_interrupt);
	mips32_i32(state, 8, target);
	mips32_sw_abs(state, 8, 9, &TJ_PC.addr);
	mips32_sw_abs(state, 8, 9, &last_addr);
	mips32_pic_call(state, &gen_interupt);

	/* On return, we escape the JIT. */
	mips32_jr(state, REG_ESCAPE);
	mips32_nop(state);
}

void mips32_check_interrupt_and_jump_indirect(struct mips32_state* state, struct mips32_reg_cache* cache, uint8_t ns)
{
	mips32_free_all_except_ints(state, cache, BIT(ns));

	if (ns == REG_ARG1) {
		/* If the native register containing the Nintendo 64 address is
		 * REG_ARG1, it may currently contain a SE32, ZE32 or OE32 allocation,
		 * which means that, after mips32_free_all has run, it contains
		 * all-ones or all-zeroes. Store it to the stack and restore it later.
		 */
		mips32_sw(state, REG_ARG1, STACK_OFFSET_TEMP_INT, REG_STACK);
	} else {
		/* Move 'ns' into argument 1 to mips32_indirect_jump_entry.
		 * It also ensures we can use fixed registers later. */
		mips32_or(state, REG_ARG1, ns, 0);
	}

	uint8_t ncount = mips32_alloc_int_in_32(state, cache, &g_state.regs.cp0[CP0_COUNT_REG]),
	        ninterrupt = mips32_alloc_int_in_32(state, cache, &g_state.next_interrupt),
	        nt1 = mips32_alloc_int_temp(state, cache);
	mips32_subu(state, nt1, ncount, ninterrupt);
	mips32_free_all_except_ints(state, cache, BIT(nt1));
	/* If Count < next_interrupt, it's not yet time for an interrupt... */
	mips32_bgez(state, nt1, +0);
	void* label_interrupt = mips32_anticipate_label(state);
	if (ns == REG_ARG1) {
		mips32_lw(state, REG_ARG1, STACK_OFFSET_TEMP_INT, REG_STACK);
	} else {
		mips32_nop(state);
	}

	/* Here, no interrupt needs to be taken. */
	mips32_pic_call(state, mips32_indirect_jump_entry);

	/* Here, an interrupt must be taken. */
	mips32_realize_label(state, label_interrupt);
	mips32_sw_abs(state, REG_ARG1, 9, &TJ_PC.addr);
	mips32_sw_abs(state, REG_ARG1, 9, &last_addr);
	mips32_pic_call(state, &gen_interupt);

	/* On return, we escape the JIT. */
	mips32_jr(state, REG_ESCAPE);
	mips32_nop(state);
}
