/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - MIPS (native) multi-instruction emission & utilities    *
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

#ifndef M64P_TRACE_JIT_MIPS32_NATIVE_UTILS_H
#define M64P_TRACE_JIT_MIPS32_NATIVE_UTILS_H

#include <stddef.h>
#include <stdint.h>

#include "state.h"

#if defined(__MIPSEL) || defined(__MIPSEB)
#  include <sys/sysmips.h>
   /* If not compiling for MIPS32R2, SYNCI is not available. The cacheflush
    * function is required. */
#  if !defined(_MIPS_ARCH_MIPS32R2)
#    include <asm/cachectl.h>
#  endif
#else
   /* If not compiling for MIPS (such as compiling this file on foreign
    * architectures to check for it building properly), these header files will
    * not be available. Dummy out the functions and their parameters. */
#  define MIPS_FIXADE 0
static void sysmips(int cmd, int arg1, int arg2, int arg3)
{
}

#  define BCACHE 0
static void cacheflush(void* addr, int length, int cache)
{
}
#endif /* !defined(__MIPSEL) && !defined(__MIPSEB) */

/* Native-side register assignments. */

/* Number of a register that is not allocated by the JIT but usable if there
 * is no register containing a constant required to write back a register to
 * memory. */
#define REG_EMERGENCY 1

/* Range of registers that may be used to save the values of Nintendo 64
 * registers if one is compared by a branch but written by its delay slot.
 * These are necessarily callee-saved. The rationale is that, if the delay
 * slot is implemented by emitting code that uses the register cache, it will
 * honor those allocations, but if it's implemented by emitting an interpreter
 * call, it will save those values across the call, only by virtue of them
 * being in callee-saved registers. */
#define REG_BRANCH_SAVE_START 18
#define REG_BRANCH_SAVE_END   19

/* Number of a register that contains &g_state. */
#define REG_STATE 16

/* Number of a register that contains the address of the JIT escape code. */
#define REG_ESCAPE 23

/* Number of the register that is used to make calls in position-independent
 * code. All calls emitted by the JIT must be made through this unless the
 * target subroutine accepts JAL or J. */
#define REG_PIC_CALL 25

#define REG_STACK 29

#define REG_RETVAL 2

#define REG_ARG1 4
#define REG_ARG2 5
#define REG_ARG3 6
#define REG_ARG4 7

/* Offset on the stack of a 64-bit-wide temporary area for memory accessors
 * to store the result into. */
#define STACK_OFFSET_MEMORY_READ 112

/* Offset on the stack of the integer caller-saved register area. */
#define STACK_OFFSET_TEMP_INT 120

/* Offsets on the stack of the HI and LO save area. */
#define STACK_OFFSET_HI 188
#define STACK_OFFSET_LO 192

/* Offset on the stack of the floating-point caller-saved register area. */
#define STACK_OFFSET_TEMP_FLOAT 200

extern void mips32_flush_cache(uint8_t* code, size_t length);

/* reg = imm;
 * LUI reg, %hi(imm); ORI reg, reg, %lo(imm)
 * LUI reg, %hi(imm)                          : (imm & 0xFFFF) == 0
 * ORI reg, 0, %lo(imm)                       : (imm & 0xFFFF0000) == 0
 * OR reg, $0, $0                             : imm == 0
 */
extern void mips32_i32(struct mips32_state* state, uint8_t reg, uint32_t imm);

/* Emits code to make a PIC call (through the correct register). */
extern void mips32_pic_call(struct mips32_state* state, void* target);

/* reg_dst = reg_src;
 * OR reg_dst, reg_src, $0
 * Suppressed if reg_dst == reg_src
 */
extern void mips32_move(struct mips32_state* state, uint8_t reg_dst, uint8_t reg_src);

/* reg_dst = *(int8_t*) ((uintptr_t) addr);
 * LUI reg_via, %hi(addr); LB reg_dst, %lo(addr)(reg_via)
 */
extern void mips32_lb_abs(struct mips32_state* state, uint8_t reg_dst, uint8_t reg_via, void* addr);

/* reg_dst = *(uint8_t*) ((uintptr_t) addr);
 * LUI reg_via, %hi(addr); LBU reg_dst, %lo(addr)(reg_via)
 */
extern void mips32_lbu_abs(struct mips32_state* state, uint8_t reg_dst, uint8_t reg_via, void* addr);

/* reg_dst = *(int16_t*) ((uintptr_t) addr);
 * LUI reg_via, %hi(addr); LH reg_dst, %lo(addr)(reg_via)
 */
extern void mips32_lh_abs(struct mips32_state* state, uint8_t reg_dst, uint8_t reg_via, void* addr);

/* reg_dst = *(uint16_t*) ((uintptr_t) addr);
 * LUI reg_via, %hi(addr); LHU reg_dst, %lo(addr)(reg_via)
 */
extern void mips32_lhu_abs(struct mips32_state* state, uint8_t reg_dst, uint8_t reg_via, void* addr);

/* reg_dst = *(int32_t*) ((uintptr_t) addr);
 * LUI reg_via, %hi(addr); LW reg_dst, %lo(addr)(reg_via)
 */
extern void mips32_lw_abs(struct mips32_state* state, uint8_t reg_dst, uint8_t reg_via, void* addr);

/* *(int8_t*) ((uintptr_t) addr) = reg_src;
 * LUI reg_via, %hi(addr); SB reg_dst, %lo(addr)(reg_via)
 */
extern void mips32_sb_abs(struct mips32_state* state, uint8_t reg_src, uint8_t reg_via, void* addr);

/* *(int16_t*) ((uintptr_t) addr) = reg_src;
 * LUI reg_via, %hi(addr); SH reg_dst, %lo(addr)(reg_via)
 */
extern void mips32_sh_abs(struct mips32_state* state, uint8_t reg_src, uint8_t reg_via, void* addr);

/* *(int32_t*) ((uintptr_t) addr) = reg_src;
 * LUI reg_via, %hi(addr); SW reg_dst, %lo(addr)(reg_via)
 */
extern void mips32_sw_abs(struct mips32_state* state, uint8_t reg_src, uint8_t reg_via, void* addr);

/* Splits the address of the given pointer into its constituents, usable as
 * LUI + offset in memory access opcodes. Correctly handles the case where bit
 * #15 is set and the access would require using a negative offset. */
extern void mips32_split_mem_ref(void* addr, uint16_t* addr_hi, int16_t* addr_lo);

/* Determines whether the instruction at 'source' can jump to the instruction
 * at 'target' with a 16-bit jump offset (+/- 128 KiB). */
extern bool mips32_can_jump16(void* source, void* target);

/* Gets the 16-bit jump offset required to jump from 'source' to 'target'. */
extern int16_t mips32_get_jump16(void* source, void* target);

/* Determines whether the instruction at 'source' can jump to the instruction
 * at 'target' with a 26-bit jump offset (in the same 256 MiB segment). */
extern bool mips32_can_jump26(void* source, void* target);

/* Called after emitting a native non-jump instruction then a native jump
 * instruction, this function swaps the two so that the jump appears first,
 * its delay slot filled with the non-jump instruction. */
extern void mips32_borrow_delay(struct mips32_state* state);

/* Called after emitting a NOP in a branch delay slot, if subsequent code can
 * write a better instruction into it. */
extern void mips32_rewrite_delay(struct mips32_state* state);

/* Called after emitting a branch to the future, skipping an unknown number of
 * instructions. Returns enough information for mips32_realize_label to link
 * the preceding opcode with its branch target. */
extern void* mips32_anticipate_label(const struct mips32_state* state);

/* Called after emitting a variable-length block of instructions that is to be
 * skipped by a branch emitted in the past. */
extern void mips32_realize_label(const struct mips32_state* state, void* source);

#endif /* !M64P_TRACE_JIT_MIPS32_NATIVE_UTILS_H */
