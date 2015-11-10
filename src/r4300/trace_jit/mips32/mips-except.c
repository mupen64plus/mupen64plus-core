/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - MIPS (Nintendo 64) exception-causing instructions       *
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

#include <stdint.h>

#include "mips-except.h"

#include "mips-emit.h"
#include "native-ops.h"
#include "native-utils.h"

#include "../mips-interp.h"
#include "../mips-parse.h"

#include "../../exception.h"
#include "../../r4300.h"

enum TJEmitTraceResult mips32_emit_ni(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_NI, TJ_READS_PC);
}

enum TJEmitTraceResult mips32_emit_reserved(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_RESERVED, TJ_READS_PC);
}

enum TJEmitTraceResult mips32_emit_syscall(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_SYSCALL, TJ_READS_PC | TJ_TRANSFERS_CONTROL);
}

enum TJEmitTraceResult mips32_emit_teq(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_TEQ, TJ_READS_PC | TJ_CHECK_STOP);
}

/* Handles emitting the slow path for memory accessor calls which return with
 * an exception already created.
 *
 * The task of this slow path is to write back any registers that still
 * contain unwritten Nintendo 64 state before the exception handler starts,
 * then either jump to the exception handler or escape the JIT.
 */
enum TJEmitTraceResult mips32_exception_writeback(struct mips32_state* state, struct mips32_reg_cache* cache, void* usual_path, uint32_t userdata)
{
	mips32_free_all(state, cache);
	if (state->in_delay_slot) {
		/* Set skip_jump to 0 to indicate we're handling this exception. */
		mips32_sw_abs(state, 0, 9, &skip_jump);
	}
	mips32_i32(state, REG_PIC_CALL, (uintptr_t) mips32_indirect_jump_entry);
	mips32_lw_abs(state, REG_ARG1, 9, &TJ_PC.addr);
	mips32_jr(state, REG_PIC_CALL);
	mips32_borrow_delay(state);
	return TJ_SUCCESS;
}

/* Handles the rare case where a Coprocessor Unusable exception must be
 * created at the start of a trace.
 *
 * The task of this slow path is to create the exception. The Program Counter
 * can be assumed to be correct, and g_state.delay_slot can be assumed to be
 * false.
 */
enum TJEmitTraceResult mips32_raise_cop1_unusable(struct mips32_state* state, struct mips32_reg_cache* cache, void* usual_path, uint32_t userdata)
{
	uint8_t ncause = mips32_alloc_int_out_32(state, cache, &g_state.regs.cp0[CP0_CAUSE_REG]);

	mips32_i32(state, ncause, (UINT32_C(11) << 2) | UINT32_C(0x10000000));
	mips32_free_all(state, cache);
	mips32_pic_call(state, &exception_general);

	mips32_i32(state, REG_PIC_CALL, (uintptr_t) mips32_indirect_jump_entry);
	mips32_lw_abs(state, REG_ARG1, 9, &TJ_PC.addr);
	mips32_jr(state, REG_PIC_CALL);
	mips32_borrow_delay(state);
	return TJ_SUCCESS;
}
