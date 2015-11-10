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

#ifndef M64P_TRACE_JIT_MIPS32_MIPS_COMMON_H
#define M64P_TRACE_JIT_MIPS32_MIPS_COMMON_H

#include "state.h"

/* Emits, or does not emit, code to ensure that the native rounding mode
 * matches the one on the emulated Nintendo 64. Uses and maintains
 * state->rounding_set to know whether to emit code. */
extern void mips32_ensure_rounding(struct mips32_state* state, struct mips32_reg_cache* cache);

/* Emits code to update the Count register using the number of instructions
 * emitted so far in the trace. */
extern void mips32_add_to_count(struct mips32_state* state, struct mips32_reg_cache* cache);

/* Emits code to perform a Nintendo 64 jump to a constant address.
 *
 * The jump may be emitted in one of three ways:
 * a) Directly to the target, if it has already been recompiled and is in the
 *    same 4 KiB page as the jump instruction at 'source'.
 * b) As a call to mips32_linker_entry (mips-emit.h), if the target has not
 *    been recompiled but is in the same 4 KiB page as the jump instruction
 *    at 'source'.
 * c) As a call to mips32_indirect_jump_entry (mips-emit.h) otherwise.
 *
 * In:
 *   state: Information concerning the current trace and its emission.
 *   source: The Nintendo 64 address at which the jump instruction is found.
 *   target: The Nintendo 64 address to which the jump instruction is going.
 */
extern void mips32_jump(struct mips32_state* state, uint32_t source, uint32_t target);

/* Emits code to perform an interrupt-checking jump. Calls mips32_jump to
 * emit the jump. */
extern void mips32_check_interrupt_and_jump(struct mips32_state* state, struct mips32_reg_cache* cache, uint32_t source, uint32_t target);

/* Emits code to perform an interrupt-checking jump to the Nintendo 64 address
 * contained in native register 'ns'. */
extern void mips32_check_interrupt_and_jump_indirect(struct mips32_state* state, struct mips32_reg_cache* cache, uint8_t ns);

#endif /* !M64P_TRACE_JIT_MIPS32_MIPS_COMMON_H */
