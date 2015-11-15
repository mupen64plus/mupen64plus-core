/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - Main code file for the MIPS32 Trace JIT                 *
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
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "mips-common.h"
#include "mips-except.h"
#include "mips-emit.h"
#include "native-endian.h"
#include "native-ops.h"
#include "native-utils.h"
#include "state.h"

#include "../mips-interp.h"
#include "../mips-jit.h"
#include "../native-tracecache.h"

#include "../../cp0_private.h"
#include "../../r4300.h"

void (*mips32_jit_entry) (void*);

void (*mips32_linker_entry) (uint32_t pc, uint32_t target, void* start, size_t length);

void (*mips32_indirect_jump_entry) (uint32_t target);

static void* AuxiliaryCode;

#define AUXILIARY_CODE_SIZE ((size_t) 4096)

bool mips32_trace_jit_init()
{
	const size_t Size = 16 * 1024 * 1024;
	void* Cache;
#if defined(M64P_BIG_ENDIAN)
#  if defined(_MIPS_ARCH_MIPS32R2)
	DebugMessage(M64MSG_INFO, "Architecture: MIPS32r2 big-endian");
#  else
	DebugMessage(M64MSG_INFO, "Architecture: MIPS32 big-endian");
#  endif
#else
#  if defined(_MIPS_ARCH_MIPS32R2)
	DebugMessage(M64MSG_INFO, "Architecture: MIPS32r2 little-endian");
#  else
	DebugMessage(M64MSG_INFO, "Architecture: MIPS32 little-endian");
#  endif
#endif

#if 0 /* Misaligned accesses exist in the video plugins... */
	DebugMessage(M64MSG_INFO, "Disabling fixups of Address Error exceptions");
	sysmips(MIPS_FIXADE, 0, 0, 0);
#endif

	Cache = AllocExec(Size);
	AuxiliaryCode = AllocExec(AUXILIARY_CODE_SIZE);
	if (Cache != NULL && AuxiliaryCode != NULL) {
		/* Because creating functions that properly grab the address of a
		 * global variable in GCC inline assembly is too complicated, the
		 * contents of some functions follow. */
		struct mips32_state state;
		mips32_jit_entry = state.code = AuxiliaryCode;
		state.avail = AUXILIARY_CODE_SIZE;
		/* - - - mips32_jit_entry follows - - - */
		/* This stack adjustment includes:
		 * - 16 bytes for callee arguments, reused over and over;
		 * - 88 bytes to preserve the values of 22 callee-saved registers
		 *      (10 integer, 12 floating-point);
		 * -  4 bytes for the return address, because traces may call C;
		 * -  4 bytes to align the stack frame to 8 bytes;
		 * -  8 bytes that serve as a scratch area for emitted memory reads
		 *      (g_state.read_dest = 112($29));
		 * -148 bytes that allow emitted code to preserve caller-saved
		 *      registers before calls:
		 *      120($29) .. 176($29): $1 .. $15
		 *      180($29) .. 184($29): $24 .. $25
		 *      188($29): HI
		 *      192($29): LO
		 *      200($29): $f0
		 *      208($29): $f2
		 *       . . .
		 *      272($29): $f18
		 *      280($29): $f20 (on some ABIs)
		 *      288($29): $f22 (on some ABIs)
		 *      (This is only to describe how much space could be needed, and
		 *       to aid in documenting further expansion of the space.
		 *       It is not meant to prescribe an exact stack mapping.) */
		mips32_addiu(&state, REG_STACK, REG_STACK, -296);
		/* Save $16..$23, $28, $30, $31. */
		mips32_sw(&state, 31, 104, REG_STACK);
		mips32_sw(&state, 16, 16, REG_STACK);
		mips32_sw(&state, 17, 20, REG_STACK);
		mips32_sw(&state, 18, 24, REG_STACK);
		mips32_sw(&state, 19, 28, REG_STACK);
		mips32_sw(&state, 20, 32, REG_STACK);
		mips32_sw(&state, 21, 36, REG_STACK);
		mips32_sw(&state, 22, 40, REG_STACK);
		mips32_sw(&state, 23, 44, REG_STACK);
		mips32_sw(&state, 28, 48, REG_STACK);
		mips32_sw(&state, 30, 52, REG_STACK);
		mips32_sdc1(&state, 20, 56, REG_STACK);
		mips32_sdc1(&state, 22, 64, REG_STACK);
		mips32_sdc1(&state, 24, 72, REG_STACK);
		mips32_sdc1(&state, 26, 80, REG_STACK);
		mips32_sdc1(&state, 28, 88, REG_STACK);
		mips32_sdc1(&state, 30, 96, REG_STACK);
		/* Stick the address of g_state into REG_STATE. */
		mips32_i32(&state, REG_STATE, (uintptr_t) &g_state);
		/* Jump to the trace given in argument 1. Leave the address of the
		 * rest of mips32_jit_entry into REG_ESCAPE. That will be the escape
		 * mechanism to return to C. */
		mips32_jalr_to(&state, REG_ESCAPE, REG_ARG1);
		mips32_borrow_delay(&state);

		/* On return... */
		/* Restore $16..$23, $28, $30, $31. */
		mips32_lw(&state, 31, 104, REG_STACK);
		mips32_lw(&state, 16, 16, REG_STACK);
		mips32_lw(&state, 17, 20, REG_STACK);
		mips32_lw(&state, 18, 24, REG_STACK);
		mips32_lw(&state, 19, 28, REG_STACK);
		mips32_lw(&state, 20, 32, REG_STACK);
		mips32_lw(&state, 21, 36, REG_STACK);
		mips32_lw(&state, 22, 40, REG_STACK);
		mips32_lw(&state, 23, 44, REG_STACK);
		mips32_lw(&state, 28, 48, REG_STACK);
		mips32_lw(&state, 30, 52, REG_STACK);
		mips32_ldc1(&state, 20, 56, REG_STACK);
		mips32_ldc1(&state, 22, 64, REG_STACK);
		mips32_ldc1(&state, 24, 72, REG_STACK);
		mips32_ldc1(&state, 26, 80, REG_STACK);
		mips32_ldc1(&state, 28, 88, REG_STACK);
		mips32_ldc1(&state, 30, 96, REG_STACK);
		/* Return to the caller. */
		mips32_jr(&state, 31);
		/* And get rid of our stack frame in the delay slot of the JR. */
		mips32_addiu(&state, REG_STACK, REG_STACK, +296);

		mips32_linker_entry = state.code;
		/* - - - mips32_linker_entry follows - - - */
		/* All arguments are forwarded as-is. The callee argument space is
		 * transferred as-is on the stack, too. */
		mips32_pic_call(&state, &mips32_dynamic_linker);

		/* On return... */
		mips32_jr(&state, REG_RETVAL);
		mips32_nop(&state);

		mips32_indirect_jump_entry = state.code;
		/* - - - mips32_indirect_jump_entry follows - - - */
		/* No stack adjustment is needed. We are under the JIT entry code here
		 * and it allows the use of $17..$22 and provides the callee argument
		 * space. */
		mips32_i32(&state, REG_PIC_CALL, (uintptr_t) &tj_jump_to);
		/* All arguments are forwarded as-is. This is a PIC call. */
		mips32_jalr(&state, REG_PIC_CALL);
		/* Stash argument 1 into $17 (callee-saved) as it is used twice.
		 * (Delay slot.) */
		mips32_or(&state, 17, REG_ARG1, 0);

		/* On return... */
		/* If tj_jump_to returned false, we have a PC change on our hands.
		 * In that case, regrab the PC before calling GetOrMakeTraceAt.
		 * Since this is very rare, we don't want to pollute the instruction
		 * cache, so we jump to a small part beyond the JR $2. */
		mips32_beq(&state, REG_RETVAL, 0, +0);
		void* label_regrab_pc = mips32_anticipate_label(&state);
		/* The more likely option is that we can just reuse the address we got
		 * as an argument. Get argument 1 back from our stash. (Delay slot.) */
		mips32_or(&state, REG_ARG1, 17, 0);
		
		void* label_ready_for_jump = state.code;
		/* And here, we don't (or we fell through). */
		mips32_pic_call(&state, &GetOrMakeTraceAt);

		/* On return... */
		mips32_jr(&state, REG_RETVAL);
		mips32_nop(&state);

		mips32_realize_label(&state, label_regrab_pc);
		/* Here, we need to regrab PC and jump back to ready_for_jump.
		 * This is still for mips32_indirect_jump_entry. */
		mips32_lw_abs(&state, REG_ARG1, 9, &TJ_PC.addr);
		mips32_bgez(&state, 0, (uint32_t*) label_ready_for_jump - (uint32_t*) state.code);
		mips32_borrow_delay(&state);

		mips32_flush_cache((void*) AuxiliaryCode, (uint8_t*) state.code - (uint8_t*) AuxiliaryCode);

		SetCodeCache(Cache, Size);

		return true;
	} else {
		if (Cache == NULL) {
			DebugMessage(M64MSG_ERROR, "Failed to allocate %zi bytes of executable memory for the MIPS dynamic recompiler.", Size);
		}
		if (AuxiliaryCode == NULL) {
			DebugMessage(M64MSG_ERROR, "Failed to allocate %zi bytes for auxiliary code for the MIPS dynamic recompiler.", AUXILIARY_CODE_SIZE);
		}
		return false;
	}
}

void mips32_trace_jit_exit()
{
	FreeExec(CodeCache, CodeCacheSize);
	SetCodeCache(NULL, 0);
	FreeExec(AuxiliaryCode, AUXILIARY_CODE_SIZE);
}

enum TJEmitTraceResult mips32_emit_trace(void** code_ptr, size_t avail, uint32_t pc, const uint32_t* ops, uint32_t op_count)
{
	enum TJEmitTraceResult result = TJ_SUCCESS;
	struct mips32_state state;
	struct mips32_reg_cache cache;
	void* code_start = *code_ptr;
	void* code_exception = NULL;
	void* code_end;
	size_t i;
	bool cop1_checked = false;

	state.original_code = state.code = code_start;
	state.avail = avail;
	state.original_ops = state.ops = ops;
	state.original_op_count = state.op_count = op_count;
	state.original_pc = state.last_count_update_pc = state.stored_pc = state.pc = pc;
	state.in_delay_slot = false;
	InitConstAnalysis(&state.consts);
	InitWidthAnalysis(&state.widths);
	mips32_init_reg_cache(&cache);
	state.fallthrough = true;
	state.ending_compiled = false;
	state.rounding_set = false;
	state.except_reloc_count = 0;
	state.except_relocs = NULL;
	state.slow_path_count = 0;
	state.slow_paths = NULL;

	/* In case of exceptions, set last_addr. It's used, along with TJ_PC.addr,
	 * in order to correctly update Count. */
	mips32_i32(&state, 8, state.pc);
	mips32_sw_abs(&state, 8, 9, &last_addr);

	/* If this trace contains a floating-point instruction, ensure Coprocessor
	 * 1 is usable (at run time). If it isn't, raise the Coprocessor Unusable
	 * exception. */
	for (i = 0; i < op_count; i++) {
		if (!cop1_checked && CanRaiseCop1Unusable(ops[i])) {
			uint8_t n0x2000 = mips32_alloc_int_const(&state, &cache, UINT32_C(0x20000000)),
			        nstatus = mips32_alloc_int_in_32(&state, &cache, &g_state.regs.cp0[CP0_STATUS_REG]),
			        nt1 = mips32_alloc_int_temp(&state, &cache);
			/* Check whether bit 29 of Status is set. */
			mips32_and(&state, nt1, nstatus, n0x2000);
			/* If it's not (i.e. nt1 is now 0), go raise the Coprocessor
			 * Unusable exception. */
			mips32_beq(&state, nt1, 0, +0);
			FAIL_AS(mips32_add_slow_path(&state, &cache, &mips32_raise_cop1_unusable, state.code,
				NULL /* no usual path */, 0 /* no userdata */));
			mips32_nop(&state);

			if (!state.code) {
				result = TJ_MEMORY_ERROR;
				goto end;
			}

			cop1_checked = true;
		}
		if (UsesRoundingMode(ops[i]) == RMS_N64) {
			mips32_ensure_rounding(&state, &cache);
			/* An optimisation: Since any instruction that uses rounding
			 * necessarily uses Coprocessor 1, exit the loop here. */
			break;
		}
	}

	/* And all of this needs to run only once, even in loops that go back to
	 * the first instruction. */
	state.after_setup = state.code;

	while (state.op_count > 0) {
		if ((result = mips32_emit_opcode(&state, &cache)) != TJ_SUCCESS) {
			goto end;
		}

		if (!state.code) {
			result = TJ_MEMORY_ERROR;
			goto end;
		}
	}

	assert(cache.opcode_level == 0);
	mips32_free_all(&state, &cache);

	if (state.fallthrough) {
		/* The trace doesn't end with a jump. Let's update the Count register
		 * and last_addr so that the next trace can simply add its own cycles
		 * to it... */
		mips32_add_to_count(&state, &cache);
		mips32_free_all(&state, &cache);
		mips32_i32(&state, REG_ARG1, state.pc);
		mips32_sw_abs(&state, REG_ARG1, 9, &last_addr);
		/* Then jump to the very next instruction to revalidate it. */
		/* Call mips32_indirect_jump_entry using 1 argument. */
		mips32_pic_call(&state, mips32_indirect_jump_entry);
	} else if (!state.ending_compiled) {
		/* If the ending jump was not compiled, it will have updated
		 * TJ_PC.addr, checked for interrupts, etc. Just escape the JIT. */
		mips32_jr(&state, REG_ESCAPE);
		mips32_nop(&state);
	}

	if (!state.code) {
		result = TJ_MEMORY_ERROR;
		goto end;
	}

	if (state.except_reloc_count > 0) {
		code_exception = state.code;

		/* Set skip_jump to 0 to indicate we're handling this exception. */
		mips32_sw_abs(&state, 0, 9, &skip_jump);
		/* Jump to the JIT escape code ($23). */
		mips32_jr(&state, REG_ESCAPE);
		mips32_borrow_delay(&state);

		if (!state.code) {
			result = TJ_MEMORY_ERROR;
			goto end;
		}
	}

	/* Apply relocations in all the jumps that wanted to go to the exception
	 * handler. */
	for (i = 0; i < state.except_reloc_count; i++) {
		void* origin = state.except_relocs[i];
		/* Does the offset fit in 16 bits? */
		if (mips32_can_jump16(origin, code_exception)) {
			*LO16_32(origin) = mips32_get_jump16(origin, code_exception);
		} else {
			DebugMessage(M64MSG_ERROR, "Failed to create a block: Conditional branch to exception handler too far (%zi bytes; maximum allowed: 131072 bytes)", (size_t) ((uintptr_t) (code_exception) - (uintptr_t) (origin) - 4));
			result = TJ_FAILURE;
			goto end;
		}
	}

	/* Apply relocations to slow paths, and have the handler functions emit
	 * them. Since slow paths may need slow paths of their own, the value of
	 * state.slow_path_count may increase. Reget it from memory every time. */
	for (i = 0; i < state.slow_path_count; i++) {
		struct mips32_slow_path* slow_path = &state.slow_paths[i];

		void* origin = slow_path->source;
		/* Does the offset fit in 16 bits? */
		if (mips32_can_jump16(origin, state.code)) {
			*LO16_32(origin) = mips32_get_jump16(origin, state.code);
		} else {
			DebugMessage(M64MSG_ERROR, "Failed to create a block: Conditional branch to the slow path of opcode %s too far (%zi bytes; maximum allowed: 131072 bytes)", slow_path->cache->op_names[0], (size_t) ((uintptr_t) (state.code) - (uintptr_t) (origin) - 4));
			result = TJ_FAILURE;
			goto end;
		}

		/* Restore enough of the environment so that the handler may parse the
		 * opcode. */
		state.pc = slow_path->pc;
		state.ops = slow_path->ops;
		state.op_count = slow_path->op_count;
		state.in_delay_slot = slow_path->in_delay_slot;
		(*(slow_path->handler)) (&state, slow_path->cache, slow_path->usual_path, slow_path->userdata);

		if (!state.code) {
			result = TJ_MEMORY_ERROR;
			goto end;
		}
	}

	code_end = state.code;

	mips32_flush_cache(code_start, code_end - code_start);

	*code_ptr = code_end;
end:
	free(state.except_relocs);

	for (i = 0; i < state.slow_path_count; i++) {
		free(state.slow_paths[i].cache);
	}
	free(state.slow_paths);

	return result;
}
