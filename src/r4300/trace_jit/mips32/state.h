/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - MIPS (native) code emission state                       *
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

#ifndef M64P_TRACE_JIT_MIPS32_STATE_H
#define M64P_TRACE_JIT_MIPS32_STATE_H

#include <stddef.h>
#include <stdint.h>

#include "native-regcache.h"

#include "../mips-analysis.h"
#include "../mips-jit.h"

/* Gives the offset from g_state to the given structure member, which is baked
 * into native loads and stores. */
#define STATE_OFFSET(member) offsetof(struct r4300_state, member)

struct mips32_state; /* Forward declaration */

typedef enum TJEmitTraceResult (*mips32_slow_path_handler) (struct mips32_state* state, struct mips32_reg_cache* cache, void* usual_path, uint32_t userdata);

/* Whenever an opcode's usual path detects that a slow path must be taken, one
 * of these is created.
 *
 * Slow paths exist in the context of a specific opcode with registers already
 * assigned. If no exception is created in the slow path, it must jump back to
 * the usual path to continue with the next opcode, with all assigned register
 * values intact as expected by the next opcode.
 *
 * Some uses of slow paths would be:
 * - Implementing memory accesses which are rarely expected to reach outside
 *   RDRAM. The usual path would check if the access goes to RDRAM, then, if
 *   it doesn't, branch to the slow path, which must branch after the RDRAM-
 *   accessing case.
 * - Implementing blocks that save any Nintendo 64 registers still contained
 *   in some native registers after an exception has been created. Exception
 *   creation is expected to be rare, and writing back the values in callee-
 *   saved registers is not required in the usual path, which expects to use
 *   their values again soon.
 *
 * In the code that branches to a slow path, emit a 16-bit conditional branch,
 * with an unknown offset, then add a slow path that will be emitted later on.
 * The following things are necessary to add the slow path:
 * - The Program Counter (i.e. address of the first byte) of the opcode.
 * - The register assignments that are live at the branch to the slow path.
 * - The point in the usual path to which the slow path should jump back to,
 *   when it has done its work.
 * - The function that is tasked with emitting the slow path for the current
 *   opcode.
 */
struct mips32_slow_path {
	struct mips32_reg_cache* cache;
	mips32_slow_path_handler handler;

	/* Saved environment from mips32_state when the opcode's usual path was
	 * being emitted. */
	uint32_t pc;
	const uint32_t* ops;
	size_t op_count;
	bool in_delay_slot;

	void* source; /* Where the jump to the slow path is. */
	void* usual_path; /* Where the slow path must jump back at its end. */
	uint32_t userdata; /* Some data used by the slow path handler. */
};

struct mips32_state {
	const uint32_t* original_ops; /* MIPS III operations. Not advanced. */
	size_t original_op_count; /* Number of operations in the trace. */
	uint32_t original_pc; /* Program Counter at the start of emission. */
	void* original_code; /* Native code pointer at the start of emission. */

	const uint32_t* ops; /* MIPS III operations. Advanced after emission. */
	size_t op_count; /* Number of operations remaining. */
	uint32_t pc; /* Program Counter during emission. */
	uint32_t stored_pc; /* What value the global PC variable has. Advanced
	                       after emitting an interpreter call. */
	uint32_t last_count_update_pc;
	bool in_delay_slot;
	struct ConstAnalysis consts; /* Constants in MIPS III registers. */
	struct WidthAnalysis widths; /* Value widths in MIPS III registers. */
	bool rounding_set; /* Whether the rounding mode is synced to native. */

	void* code; /* Current native code pointer */
	size_t avail; /* Number of bytes available to emit code into */

	/* Pointer to the instruction after all setup code, e.g. Coprocessor 1
	 * usability check, rounding mode setting. Useful for loops (backwards
	 * jumps to the start of the current trace). */
	void* after_setup;

	size_t slow_path_count;
	struct mips32_slow_path* slow_paths;

	size_t except_reloc_count; /* These allow writing jumps towards the  */
	void** except_relocs; /*  exception handler and have them patched at the
	                      end of mips32_emit_trace */
	bool fallthrough; /* false if the trace's ending jump has been seen  */
	                  /*  and it jumps away unconditionally */
	bool ending_compiled; /* true if the trace's ending jump emitted native  */
	                      /*  code which handles its taken and untaken cases */
};

#endif /* !M64P_TRACE_JIT_MIPS32_STATE_H */
