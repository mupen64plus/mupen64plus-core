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

#include <assert.h>

#include "native-endian.h"
#include "native-ops.h"
#include "native-utils.h"

void mips32_flush_cache(uint8_t* code, size_t length)
{
#ifdef _MIPS_ARCH_MIPS32R2
	void* cur = code;
	void* end = (unsigned char*) code + length;
	unsigned int cont;
	int step_bytes, step_mask;
	__asm__ __volatile__ (
		".set noreorder                          \n"
		/* Read SYNCI_Step and return if using SYNCI is not needed. */
		"  rdhwr %[step_bytes], $1               \n"
		"  beq   %[step_bytes], $0, 5f           \n"
		"  nop                                   \n"
		"  addiu $sp, $sp, -4                    \n"
		"  sw    $ra, 0($sp)                     \n"
		/* Call 1: as a procedure, so jr.hb $ra can clear instruction
		 * hazards and come back here. */
		"  bal   1f                              \n"
		"  addiu %[step_mask], %[step_bytes], -1 \n" /* (delay slot) */
		/* Once 2: returns, substitute the real return address. */
		"  lw    $ra, 0($sp)                     \n"
		"  b     5f                              \n"
		"  addiu $sp, $sp, 4                     \n" /* (delay slot) */
		"1:                                      \n"
		"  beq   %[len], $0, 4f                  \n"
		/* Round down the start to the cache line containing it. */
		"  nor   %[step_mask], %[step_mask], $0  \n"
		"  and   %[cur], %[cur], %[step_mask]    \n"
		/* Make the new code visible. */
		"2:                                      \n"
		".set mips32r2                           \n"
		"  synci 0(%[cur])                       \n"
		".set mips0                              \n"
		"  addu  %[cur], %[cur], %[step_bytes]   \n"
		"  sltu  %[cont], %[cur], %[end]         \n"
		"  bne   %[cont], $0, 2b                 \n"
		"  nop                                   \n"
		/* Clear memory hazards on multiprocessor systems. */
		"  sync                                  \n"
		"3:                                      \n"
		/* Instruction hazard barrier. */
		".set mips32r2                           \n"
		"  jr.hb $ra                             \n"
		".set mips0                              \n"
		"  nop                                   \n"
		"4:                                      \n"
		/* Normal return. */
		"  jr    $ra                             \n"
		"  nop                                   \n"
		"5:                                      \n"
		".set reorder                            \n"
		: /* output */  [step_bytes] "=r" (step_bytes), [step_mask] "=r" (step_mask), [cur] "+r" (cur), [end] "+r" (end), [cont] "=r" (cont)
		: /* input */   [start] "r" (code), [len] "r" (length)
		: /* clobber */ "memory"
	);
#else
	cacheflush(code, length, BCACHE);
#endif
}

void mips32_i32(struct mips32_state* state, uint8_t reg, uint32_t imm)
{
	uint16_t hi = (uint16_t) (imm >> 16), lo = (uint16_t) imm;
	if (hi != 0) {
		mips32_lui(state, reg, hi);
		if (lo != 0) {
			mips32_ori(state, reg, reg, lo);
		}
	} else if (lo != 0) {
		mips32_ori(state, reg, 0, lo);
	} else {
		mips32_or(state, reg, 0, 0);
	}
}

void mips32_pic_call(struct mips32_state* state, void* target)
{
	mips32_i32(state, REG_PIC_CALL, (uintptr_t) target);

	if (!state->code)
		return;

	/* If JAL can be used to jump to the target, make sure that the call can
	 * start out as:
	 *   LUI REG_PIC_CALL, %hi(somewhere)
	 *  [ORI REG_PIC_CALL, REG_PIC_CALL, %lo(somewhere)]
	 *   JAL somewhere                     ; at state->code
	 * and end up as either:
	 *   JAL somewhere                     ; at state->code - 1 instruction
	 *   LUI REG_PIC_CALL, %hi(somewhere)
	 * or:
	 *   LUI REG_PIC_CALL, %hi(somewhere)
	 *   JAL somewhere                     ; at state->code - 1 instruction
	 *   ORI REG_PIC_CALL, REG_PIC_CALL, %lo(somewhere)
	 */
	if (mips32_can_jump26((uint32_t*) state->code - 1, target)
	 && mips32_can_jump26((uint32_t*) state->code, target)) {
		mips32_jal(state, target);
		mips32_borrow_delay(state);
	} else {
		mips32_jalr(state, REG_PIC_CALL);
		mips32_nop(state);
	}
}

void mips32_move(struct mips32_state* state, uint8_t reg_dst, uint8_t reg_src)
{
	if (reg_dst != reg_src) {
		mips32_or(state, reg_dst, reg_src, 0);
	}
}

void mips32_lb_abs(struct mips32_state* state, uint8_t reg_dst, uint8_t reg_via, void* addr)
{
	uint16_t addr_hi;
	int16_t addr_lo;
	mips32_split_mem_ref(addr, &addr_hi, &addr_lo);
	mips32_lui(state, reg_via, addr_hi);
	mips32_lb(state, reg_dst, addr_lo, reg_via);
}

void mips32_lbu_abs(struct mips32_state* state, uint8_t reg_dst, uint8_t reg_via, void* addr)
{
	uint16_t addr_hi;
	int16_t addr_lo;
	mips32_split_mem_ref(addr, &addr_hi, &addr_lo);
	mips32_lui(state, reg_via, addr_hi);
	mips32_lbu(state, reg_dst, addr_lo, reg_via);
}

void mips32_lh_abs(struct mips32_state* state, uint8_t reg_dst, uint8_t reg_via, void* addr)
{
	uint16_t addr_hi;
	int16_t addr_lo;
	mips32_split_mem_ref(addr, &addr_hi, &addr_lo);
	mips32_lui(state, reg_via, addr_hi);
	mips32_lh(state, reg_dst, addr_lo, reg_via);
}

void mips32_lhu_abs(struct mips32_state* state, uint8_t reg_dst, uint8_t reg_via, void* addr)
{
	uint16_t addr_hi;
	int16_t addr_lo;
	mips32_split_mem_ref(addr, &addr_hi, &addr_lo);
	mips32_lui(state, reg_via, addr_hi);
	mips32_lhu(state, reg_dst, addr_lo, reg_via);
}

void mips32_lw_abs(struct mips32_state* state, uint8_t reg_dst, uint8_t reg_via, void* addr)
{
	uint16_t addr_hi;
	int16_t addr_lo;
	mips32_split_mem_ref(addr, &addr_hi, &addr_lo);
	mips32_lui(state, reg_via, addr_hi);
	mips32_lw(state, reg_dst, addr_lo, reg_via);
}

void mips32_sb_abs(struct mips32_state* state, uint8_t reg_src, uint8_t reg_via, void* addr)
{
	uint16_t addr_hi;
	int16_t addr_lo;
	mips32_split_mem_ref(addr, &addr_hi, &addr_lo);
	mips32_lui(state, reg_via, addr_hi);
	mips32_sb(state, reg_src, addr_lo, reg_via);
}

void mips32_sh_abs(struct mips32_state* state, uint8_t reg_src, uint8_t reg_via, void* addr)
{
	uint16_t addr_hi;
	int16_t addr_lo;
	mips32_split_mem_ref(addr, &addr_hi, &addr_lo);
	mips32_lui(state, reg_via, addr_hi);
	mips32_sh(state, reg_src, addr_lo, reg_via);
}

void mips32_sw_abs(struct mips32_state* state, uint8_t reg_src, uint8_t reg_via, void* addr)
{
	uint16_t addr_hi;
	int16_t addr_lo;
	mips32_split_mem_ref(addr, &addr_hi, &addr_lo);
	mips32_lui(state, reg_via, addr_hi);
	mips32_sw(state, reg_src, addr_lo, reg_via);
}

void mips32_split_mem_ref(void* addr, uint16_t* addr_hi, int16_t* addr_lo)
{
	uintptr_t addr_n = (uintptr_t) addr;
	*addr_hi = (addr_n >> 16) + ((addr_n >> 15) & 1);
	*addr_lo = (int16_t) addr_n;
}

bool mips32_can_jump16(void* source, void* target)
{
	ptrdiff_t distance = (uint32_t*) target - ((uint32_t*) source + 1);
	return distance == (ptrdiff_t) (int16_t) distance;
}

int16_t mips32_get_jump16(void* source, void* target)
{
	return (int16_t) ((uint32_t*) target - ((uint32_t*) source + 1));
}

bool mips32_can_jump26(void* source, void* target)
{
	return ((uintptr_t) (source + 4) & ~(uintptr_t) 0xFFFFFFF)
	    == ((uintptr_t) target & ~(uintptr_t) 0xFFFFFFF);
}

void mips32_borrow_delay(struct mips32_state* state)
{
	if (state->code != NULL) {
		uint32_t* code = (uint32_t*) state->code;
		uint32_t insn = *(code - 2);
		*(code - 2) = *(code - 1);
		*(code - 1) = insn;
	}
}

void mips32_rewrite_delay(struct mips32_state* state)
{
	if (state->code != NULL) {
		uint32_t* code = (uint32_t*) state->code;
		assert(*(code - 1) == 0);
		state->code = code - 1;
	}
}

void* mips32_anticipate_label(const struct mips32_state* state)
{
	return state->code;
}

void mips32_realize_label(const struct mips32_state* state, void* source)
{
	if (source != NULL && state->code != NULL) {
		assert(mips32_can_jump16((uint32_t*) source - 1, state->code));
		*LO16_32((uint32_t*) source - 1) = mips32_get_jump16((uint32_t*) source - 1, state->code);
	}
}
