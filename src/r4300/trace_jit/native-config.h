/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - native-config.h                                         *
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

#ifndef M64P_TRACE_JIT_NATIVE_CONFIG_H
#define M64P_TRACE_JIT_NATIVE_CONFIG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__x86_64__) || defined(_M_X64)
#  include "x86-64/config.h"
#elif defined(__i386) || defined(_M_IX86)
#  include "x86/config.h"
#elif defined(__MIPSEL) || defined(__MIPSEB)
#  include "mips32/config.h"
#else
#  warning "No JIT compiler is available for this architecture. An interpreter fallback will be used."
#  include "dummy/config.h"
#endif

#if defined(ARCH_EMIT_TRACE)
#  if defined(ARCH_JIT_ENTRY)
#    define NOT_CODE ((void*) 0)
#    define FORMERLY_CODE ((void*) 1)
#  else
#    include "mips-jit.h"
#    define NOT_CODE &TJ_NOT_CODE
#    define FORMERLY_CODE &TJ_FORMERLY_CODE
#  endif
#else
#  define NOT_CODE NULL
#  define FORMERLY_CODE NULL
#endif

#if defined(ARCH_TRACE_JIT_INIT)
/*
 * Performs architecture-specific initialisation for the Trace JIT.
 *
 * If the architecture supports code emission, this will prepare a code cache
 * of an appropriate size by calling AllocExec then SetCodeCache, declared in
 * native-tracecache.h.
 *
 * This function may also, for example, check CPU features, fill tables, look
 * at the current game and set up speed hacks for it, etc.
 *
 * It is not appropriate to enter the JIT in this function.
 *
 * Returns:
 *   true if the initialisation succeeded; false if it failed.
 */
extern bool ARCH_TRACE_JIT_INIT(void);
#endif

#if defined(ARCH_TRACE_JIT_EXIT)
/*
 * Performs architecture-specific finalisation for the Trace JIT.
 *
 * Memory allocated by the initialisation function is freed here.
 */
extern void ARCH_TRACE_JIT_EXIT(void);
#endif

#if defined(ARCH_EMIT_TRACE)
/*
 * Asks the architecture emitter to emit code for a trace of MIPS III
 * instructions. The trace ends at a branch, or at the end of a page.
 *
 * In:
 *   pc: The N64 address of the first instruction in *ops.
 *   ops: The simplified opcodes to emit native code for.
 *   op_count: The number of opcodes to emit native code for.
 *   avail: The number of contiguous bytes available in the code cache to hold
 *     the native code to be generated.
 * In/Out:
 *   code_ptr: A pointer to the native code pointer, to be used and updated by
 *     this function. On entry to this function, the inner pointer will point
 *     to the first byte that can be used for the native code. This function
 *     will update the inner pointer so that it points to the last byte of
 *     native code that was emitted (if the return value is TJ_SUCCESS).
 * Returns:
 *   TJ_SUCCESS: if native code was emitted successfully, and *code_ptr refers
 *     to the first byte that can be used for further native code.
 *   TJ_MEMORY_ERROR: if 'avail' is not enough bytes to emit native code for
 *     the requested trace.
 *   TJ_FAILURE: if the architecture emitter cannot emit a trace for some
 *     reason, such as an unimplemented opcode, or an internal error.
 * Generated code assumptions:
 *
 * CALLING CONVENTION
 *
 * If a JIT entry function is defined, it acts as the callee, so it preserves
 * callee-saved registers to the stack, may load some registers with pointers
 * to often-used data, and then jumps to emitted code. Emitted code acts like
 * a continuation to a huge function that starts with the JIT entry function,
 * jumps an arbitrary number of times, and ends with the JIT escape function.
 * Therefore, it need not save callee-saved registers again.
 *
 * If no JIT entry function is defined, the emitted code is a function, which
 * must obey the native calling convention for functions that return void and
 * take no parameters. If the function uses any callee-saved registers, those
 * registers must be saved in the prologue and restored in the epilogue.
 *
 * In either case, the emitted code may call C functions. If it does, it must
 * consider all caller-saved registers to be overwritten by the C function on
 * return, but the callee-saved registers will have their previous values.
 *
 * NINTENDO 64 STATE
 *
 * Before calling a C function, all of the Nintendo 64 state it reads must be
 * written to memory, and all of the Nintendo 64 state it writes must be read
 * again when it returns. For example, with interpreter functions:
 *
 * - Integer opcodes must have the latest value of every register they read;
 * - Memory access opcodes must have the latest value of every register they
 *   read, as well as the latest Program Counter in TJ_PC.addr if they might
 *   raise exceptions;
 * - Coprocessor 1 opcodes must have the latest value of the Status register
 *   in g_state.regs.cp0[CP0_REG_STATUS], as well as the latest value of all
 *   floating-point registers, and the Program Counter in TJ_PC.addr if they
 *   might raise exceptions.
 *
 * gen_interupt must have the latest value of everything, because it may need
 * to create a saved state at the user's request.
 *
 * When returning to C from a function of emitted code (see above for more on
 * what a 'function' is), all of the Nintendo 64 state must have been written
 * to memory.
 *
 * TRACE ENDING (MIPS III)
 *
 * If the trace ends with a branch having a delay slot, and the opcode for it
 * is not in the trace, then the branch must be interpreted. The rationale is
 * that, if a branch is missing its delay slot:
 *
 * - The delay slot may be on a different page which often gets invalidated,
 *   or is in virtual memory, so the JIT may have decided that a barrier was
 *   required after the branch.
 * - JITing the delay slot by executing its trace function would execute all
 *   of the opcodes in the trace, instead of only one, which is not allowed.
 *   Creating a separate trace just for the delay slot would be a waste, and
 *   executing that separate trace would introduce a dependency across pages
 *   which would need to be tracked.
 *
 * TRACE ENDING (NATIVE) - STAND-ALONE FUNCTIONS
 *
 * If the trace does not end with a branch or jump at all, or if it ends with
 * a branch that is not taken, the emitted code must call tj_jump_to with the
 * address of the very next instruction as an argument, then return.
 *
 * TRACE ENDING (NATIVE) - WITH JIT ENTRY FUNCTION
 *
 * Emitted code may jump directly to traces in the same 4 KiB page as itself.
 * Jumps to code that has not yet been emitted may be implemented as calls to
 * a special "linker" function that will rewrite the call with a direct jump.
 *
 * Jumps to code in other 4 KiB pages must be done via tj_jump_to.
 */
extern enum TJEmitTraceResult ARCH_EMIT_TRACE(void** code_ptr, size_t avail, uint32_t pc, const uint32_t* ops, uint32_t op_count);
#endif

#if defined(ARCH_JIT_ENTRY)
/*
 * Initialises the architecture's JIT code context and executes the passed-in
 * trace.
 *
 * Because the trace may jump to another trace, and that trace may also issue
 * a jump of its own, and so on, control returns to the caller only when code
 * jumps to, or calls, the architecture-specific JIT escape function.
 *
 * In order to be somewhat responsive to asynchronous stops (such as when the
 * user presses Ctrl+C or closes the emulator window, depending on the front-
 * end, and the 'stop' variable gets the value 1), emitted code should escape
 * the JIT at interrupts.
 *
 * For more details on the behavior of this function as it applies to a given
 * architecture, including the calling convention of emitted code, the method
 * to use to escape the JIT and so on, see its definition of ARCH_JIT_ENTRY.
 */
#  if defined(ARCH_JIT_ENTRY_IS_GENERATED)
extern void (*ARCH_JIT_ENTRY) (void* first_trace); /* A function pointer */
#  else
extern void ARCH_JIT_ENTRY(void* first_trace); /* A global function */
#  endif
#endif

#endif /* !M64P_TRACE_JIT_NATIVE_CONFIG_H */
