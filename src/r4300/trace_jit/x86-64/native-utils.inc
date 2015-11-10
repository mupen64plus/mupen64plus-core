/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - native-utils.inc                                        *
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

/* The functions in this file emit variable-length code that takes advantage
 * of everything it can. For actual opcodes, see native-ops.inc. */

/* Determines whether it is possible to use a 32-bit relative offset to handle
 * the difference between the source and the target. More formally, the result
 * is true if the target and source are within +/- (2 GiB - 64 bytes) of each
 * other. */
static bool x86_64_can_use_si32(void* target, void* source)
{
	ptrdiff_t offset = (uint8_t*) target - (uint8_t*) source;
	return offset >= -INT32_C(0x7FFFFFC0) && offset <= INT32_C(0x7FFFFFC0);
}

/* reg = (uint64_t) addr;
 * This function optimises the memory reference on the assumption that it is
 * likely to be close to the code making the reference. */
static void x86_64_mov_r64_ia(struct x86_64_state* state, uint8_t reg, void* addr)
{
	/* Substitute other encodings as they are shorter:
	 * a) zero-extended 32-bit */
	if ((uintptr_t) addr == (uintptr_t) (uint32_t) (uintptr_t) addr) {
		x86_64_mov_r64_zi32(state, reg, (uint32_t) (uintptr_t) addr);
	}
	/* b) sign-extended 32-bit */
	else if ((intptr_t) addr == (intptr_t) (int32_t) (intptr_t) addr) {
		x86_64_mov_r64_si32(state, reg, (int32_t) (intptr_t) addr);
	}
	/* c) RIP-relative */
	else if (x86_64_can_use_si32(addr, state->code)) {
		x86_64_lea_r64_RIPsi32(state, reg, addr);
	}
	else {
		x86_64_mov_r64_i64(state, reg, (uintptr_t) addr);
	}
}

/* (*(function*) (addr)))(any parameters placed in registers already);
 * This function uses the temporary register only if the target function
 * cannot be called with a signed 32-bit offset. */
static void x86_64_call_ia(struct x86_64_state* state, void* addr, uint8_t temp_reg)
{
	if (x86_64_can_use_si32(addr, state->code)) {
		x86_64_call_si32(state, addr);
	} else {
		x86_64_mov_r64_ia(state, temp_reg, addr);
		x86_64_call_r(state, temp_reg);
	}
}

/* absolute goto target;
 * This function uses the temporary register only if the target address
 * cannot be jumped to with a signed 32-bit offset. */
static void x86_64_jmp_ia(struct x86_64_state* state, void* addr, uint8_t temp_reg)
{
	if (x86_64_can_use_si32(addr, state->code)) {
		x86_64_jmp_si32(state, addr);
	} else {
		x86_64_mov_r64_ia(state, temp_reg, addr);
		x86_64_jmp_r(state, temp_reg);
	}
}

/* reg_dst = *(uint64_t*) ((uint8_t*) reg_src + imm);
 * This function optimises away the displacement if it's 0. */
static void x86_64_mov_r64_m64rosi8(struct x86_64_state* state, uint8_t reg_dst, uint8_t reg_src, int8_t imm)
{
	if (imm == 0) {
		x86_64_mov_r64_m64r(state, reg_dst, reg_src);
	} else {
		x86_64_mov_r64_m64rsi8(state, reg_dst, reg_src, imm);
	}
}

/* *(uint64_t*) ((uint8_t*) reg_dst + imm) = reg_src;
 * This function optimises away the displacement if it's 0. */
static void x86_64_mov_m64rosi8_r64(struct x86_64_state* state, uint8_t reg_dst, int8_t imm, uint8_t reg_src)
{
	if (imm == 0) {
		x86_64_mov_m64r_r64(state, reg_dst, reg_src);
	} else {
		x86_64_mov_m64rsi8_r64(state, reg_dst, imm, reg_src);
	}
}
