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

#ifndef M64P_TRACE_JIT_MIPS32_MIPS_EMIT_H
#define M64P_TRACE_JIT_MIPS32_MIPS_EMIT_H

#include <stddef.h>
#include <stdint.h>

#include "../mips-jit.h"
#include "native-regcache.h"
#include "state.h"

#define TJ_MAY_RAISE_TLB_REFILL    0x01
#define TJ_MAY_RAISE_INTERRUPT     0x02
#define TJ_CHECK_STOP              0x04
#define TJ_TRANSFERS_CONTROL       0x08
#define TJ_HAS_DELAY_SLOT          0x10
#define TJ_READS_PC                0x20

#define FAIL_AS(expr) \
	do { \
		enum TJEmitTraceResult result = (expr); \
		if (result != TJ_SUCCESS) \
			return result; \
	} while (0)

extern enum TJEmitTraceResult mips32_add_except_reloc(struct mips32_state* state);

extern enum TJEmitTraceResult mips32_next_opcode(struct mips32_state* state);

extern enum TJEmitTraceResult mips32_emit_interpret(struct mips32_state* state, struct mips32_reg_cache* cache, void (*func) (uint32_t), int flags);

extern enum TJEmitTraceResult mips32_emit_opcode(struct mips32_state* state, struct mips32_reg_cache* cache);

/* Adds a slow path that will be the target of a conditional jump in the
 * previous emitted instruction.
 *
 * See the documentation of 'struct mips32_slow_path' in state.h for more
 * information. */
extern enum TJEmitTraceResult mips32_add_slow_path(struct mips32_state* state, const struct mips32_reg_cache* cache, mips32_slow_path_handler handler, void* source, void* usual_path, uint32_t userdata);

extern enum TJEmitTraceResult mips32_return_to_usual_path(struct mips32_state* state, const struct mips32_reg_cache* cache, void* usual_path);

/* Rewrites a call to itself into a direct jump when appropriate, then returns
 * to it.
 *
 * A call to this function is emitted into native code when the target address
 * of a jump or branch on the Nintendo 64 is fully known but no native code is
 * compiled for it yet. This is intended to solve an issue whereby two traces,
 * needing to be emitted into one code cache, would be interleaved if the jump
 * emitter was allowed to call a recursive instance of the recompiler. It also
 * prevents stack space exhaustion.
 *
 * In:
 *   pc: The value of the Nintendo 64 Program Counter when the jump occurs. It
 *     is essentially the address of the jump instruction, not its delay slot.
 *   target: The Nintendo 64 address that the caller wants to jump to.
 *   start: The native address of the first instruction that sets up a call to
 *     this function, including loading its arguments.
 *   length: The number of bytes at and after 'start' that set up the call.
 *
 * Input assertions:
 * - 'target' is in the same 4 KiB page as 'pc' to prevent a dependency across
 *   pages.
 * - 'length' bytes are enough to write a direct jump to the target trace. Due
 *   to the number of arguments that must be loaded, that's almost a given.
 *
 * Output assertions:
 * - The trace at Nintendo 64 address 'target' is compiled.
 * - One of the following types of jumps is at 'start':
 *   a) BGEZ/NOP;
 *   b) J/NOP;
 *   c) LUI ORI JR/NOP.
 * - Further code until 'end' has been padded with NOP instructions.
 * - The instruction cache is synchronised or invalidated properly.
 *
 * Entry mechanism:
 * - JALR $25 (PIC), JAL or J are acceptable.
 * - Arguments are passed in via $4..$7 as usual.
 *
 * Return mechanism:
 *   For efficiency, this function returns by jumping directly to the compiled
 *   trace. The next time 'start' is encountered, it will also jump there.
 *
 *   Jumping directly to the new trace instead of jumping to the newly-written
 *   jump also avoids a situation where the trace that called this function is
 *   the oldest one in the cache and it gets overwritten by the new trace. The
 *   code at the former return address would be part of the new trace.
 */
extern void (*mips32_linker_entry) (uint32_t pc, uint32_t target, void* start, size_t length);

/* Does the heavy lifting for mips32_linker_entry above.
 *
 * In:
 *   Arguments forwarded from mips32_linker_entry.
 *
 * Returns:
 *   The address of the next trace, or NULL (after displaying an error message
 *   to the console) if the emitter failed.
 */
extern void* mips32_dynamic_linker(uint32_t pc, uint32_t target, void* start, size_t length);

/* Jumps to a piece of code emitted for N64 instructions starting at the given
 * address.
 *
 * It is possible that the code existed before but became invalid after stores
 * to the same page as the target invalidated the code, and that the code will
 * be revalidated before this function returns.
 *
 * It is also possible that the code didn't exist and will be recompiled after
 * the call to this function.
 *
 * In:
 *   target: The Nintendo 64 address that the caller wants to jump to.
 *
 * Output assertions:
 * - The trace at Nintendo 64 address 'target' is compiled.
 *
 * Entry mechanism:
 * - JALR $25 (PIC), JAL or J are acceptable.
 * - The argument is passed in via $4 as usual.
 *
 * Return mechanism:
 *   For efficiency, this function returns by jumping directly to the compiled
 *   trace.
 *
 *   Jumping directly to the new trace also avoids a situation where the trace
 *   that called this function is the oldest one in the cache and it becomes a
 *   new trace written for the jump target, whose code had been overwritten by
 *   the game.
 */
extern void (*mips32_indirect_jump_entry) (uint32_t target);

#endif /* !M64P_TRACE_JIT_MIPS32_MIPS_EMIT_H */
