/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - assemble.h                                              *
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

/* 8-bit registers. These represent the lowest bits of the 64-bit registers.
 * (AH, BH, CH and DH are not supported universally in 64-bit mode.) */
#define AL 0
#define BL 3
#define CL 1
#define DL 2
#define SIL 6
#define DIL 7
#define SPL 4
#define BPL 5
#define R8B 8
#define R9B 9
#define R10B 10
#define R11B 11
#define R12B 12
#define R13B 13
#define R14B 14
#define R15B 15

/* 16-bit registers. These represent the lowest bits of the 64-bit registers. */
#define AX 0
#define BX 3
#define CX 1
#define DX 2
#define SI 6
#define DI 7
#define SP 4
#define BP 5
#define R8W 8
#define R9W 9
#define R10W 10
#define R11W 11
#define R12W 12
#define R13W 13
#define R14W 14
#define R15W 15

/* 32-bit registers. These represent the lowest bits of the 64-bit registers. */
#define EAX 0
#define EBX 3
#define ECX 1
#define EDX 2
#define ESI 6
#define EDI 7
#define ESP 4
#define EBP 5
#define R8D 8
#define R9D 9
#define R10D 10
#define R11D 11
#define R12D 12
#define R13D 13
#define R14D 14
#define R15D 15

/* The 64-bit registers. */
#define RAX 0
#define RBX 3
#define RCX 1
#define RDX 2
#define RSI 6
#define RDI 7
#define RSP 4
#define RBP 5
#define R8 8
#define R9 9
#define R10 10
#define R11 11
#define R12 12
#define R13 13
#define R14 14
#define R15 15

#define REG_BIT3(reg) (((reg) >> 3) & 1)
#define REG_LOW3(reg) ((reg) & 7)

/* Registers used by the ABI. */
#if defined(_WIN64)
#  define REG_INT_ARG1 RCX
#  define REG_INT_ARG2 RDX
#  define REG_INT_ARG3 R8
#  define REG_INT_ARG4 R9
#else
/* XXX: Not sure if #else is necessarily the System V AMD64 ABI */
#  define REG_INT_ARG1 RDI
#  define REG_INT_ARG2 RSI
#  define REG_INT_ARG3 RDX
#  define REG_INT_ARG4 RCX
#endif

#define CHECK_REG(reg) assert((reg) < 16)

static void x86_64_byte(struct x86_64_state* state, uint8_t data)
{
	if (state->code && state->avail >= 1) {
		*(uint8_t*) state->code = data;
		state->avail -= 1;
		state->code = (uint8_t*) state->code + 1;
	}
	else state->code = NULL;
}

static void x86_64_hword(struct x86_64_state* state, uint16_t data)
{
	if (state->code && state->avail >= 2) {
		*(uint16_t*) state->code = data;
		state->avail -= 2;
		state->code = (uint16_t*) state->code + 1;
	}
	else state->code = NULL;
}

static void x86_64_dword(struct x86_64_state* state, uint32_t data)
{
	if (state->code && state->avail >= 4) {
		*(uint32_t*) state->code = data;
		state->avail -= 4;
		state->code = (uint32_t*) state->code + 1;
	}
	else state->code = NULL;
}

static void x86_64_qword(struct x86_64_state* state, uint64_t data)
{
	if (state->code && state->avail >= 8) {
		*(uint64_t*) state->code = data;
		state->avail -= 8;
		state->code = (uint64_t*) state->code + 1;
	}
	else state->code = NULL;
}

/* return from procedure
 * RET */
static void x86_64_ret(struct x86_64_state* state)
{
	x86_64_byte(state, 0xC3);
}

/* reg = (int64_t) ((int32_t) imm);
 * MOV <reg64>, <imm32> */
static void x86_64_mov_r64_si32(struct x86_64_state* state, uint8_t reg, int32_t imm)
{
	CHECK_REG(reg);
	/* writing to the upper 8 registers turns the 0x48 prefix into 0x49 */
	x86_64_byte(state, 0x48 | REG_BIT3(reg));
	x86_64_byte(state, 0xC7);
	x86_64_byte(state, 0xC0 | REG_LOW3(reg));
	x86_64_dword(state, (uint32_t) imm);
}

/* reg = (uint64_t) ((uint32_t) imm);
 * MOV <reg32>, <imm32> */
static void x86_64_mov_r64_zi32(struct x86_64_state* state, uint8_t reg, uint32_t imm)
{
	CHECK_REG(reg);
	/* writing to the upper 8 registers requires the 0x41 prefix */
	if (REG_BIT3(reg))
		x86_64_byte(state, 0x41);
	x86_64_byte(state, 0xB8 | REG_LOW3(reg));
	x86_64_dword(state, imm);
}

/* reg = imm;
 * MOVABS <reg64>, <imm64> */
static void x86_64_mov_r64_i64(struct x86_64_state* state, uint8_t reg, uint64_t imm)
{
	CHECK_REG(reg);
	/* writing to the upper 8 registers turns the 0x48 prefix into 0x49 */
	x86_64_byte(state, 0x48 | REG_BIT3(reg));
	x86_64_byte(state, 0xB8 | REG_LOW3(reg));
	x86_64_qword(state, imm);
}

/* (*(function*) (target)))(any parameters placed in registers already);
 * if target is within range of state->code (the instruction being written)
 * with a 32-bit offset
 * CALL <imm32> */
static void x86_64_call_si32(struct x86_64_state* state, void* target)
{
	/* the 32-bit offset is relative to the end of the instruction, so +5 */
	int32_t imm = (int32_t) ((uint8_t*) target - ((uint8_t*) state->code + 5));
	x86_64_byte(state, 0xE8);
	x86_64_dword(state, (uint32_t) imm);
}

/* (*(function*) (reg)))(any parameters placed in registers already);
 * CALL <reg64> */
static void x86_64_call_r(struct x86_64_state* state, uint8_t reg)
{
	CHECK_REG(reg);
	/* reading from the upper 8 registers requires the 0x41 prefix */
	if (REG_BIT3(reg))
		x86_64_byte(state, 0x41);
	x86_64_byte(state, 0xFF);
	x86_64_byte(state, 0xD0 | REG_LOW3(reg));
}

/* absolute goto target;
 * JMP <imm32> */
static void x86_64_jmp_si32(struct x86_64_state* state, void* target)
{
	/* the 32-bit offset is relative to the end of the instruction, so +5 */
	int32_t imm = (int32_t) ((uint8_t*) target - ((uint8_t*) state->code + 5));
	x86_64_byte(state, 0xE9);
	x86_64_dword(state, (uint32_t) imm);
}

/* absolute goto reg;
 * JMP <reg64> */
static void x86_64_jmp_r(struct x86_64_state* state, uint8_t reg)
{
	CHECK_REG(reg);
	/* reading from the upper 8 registers requires the 0x41 prefix */
	if (REG_BIT3(reg))
		x86_64_byte(state, 0x41);
	x86_64_byte(state, 0xFF);
	x86_64_byte(state, 0xE0 | REG_LOW3(reg));
}

/* Determines whether it is possible to jump to the given target from the
 * given source using a 32-bit relative offset. */
static bool x86_64_can_jcond_si32(void* target, void* source)
{
	/* the 32-bit offset is relative to the end of the instruction, so +6 */
	ptrdiff_t offset = (uint8_t*) target - ((uint8_t*) source + 6);
	return offset == (ptrdiff_t) (int32_t) offset;
}

/* if (last_compare was not equal) absolute goto target;
 * JNE <imm32> */
static void x86_64_jne_si32(struct x86_64_state* state, void* target)
{
	assert(x86_64_can_jcond_si32(target, state->code));
	/* the 32-bit offset is relative to the end of the instruction, so +6 */
	int32_t imm = (int32_t) ((uint8_t*) target - ((uint8_t*) state->code + 6));
	x86_64_byte(state, 0x0F);
	x86_64_byte(state, 0x85);
	x86_64_dword(state, (uint32_t) imm);
}

/* reg_dst = (uint64_t) (*(uint8_t*) reg_src)
 *         | (reg_dst & ~UINT64_C(0xFF));
 * MOV <reg8>, [<reg64>]
 * Preserves the upper 56 bits of the destination register, as it only affects
 * the lower 8 bits. */
static void x86_64_mov_r8_m8r(struct x86_64_state* state, uint8_t reg_dst, uint8_t reg_src)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	/* lower 8 registers for both = no prefix; otherwise... */
	if (REG_BIT3(reg_dst) || REG_BIT3(reg_src))
		/* dst upper register = 0x44; src = 0x41; both = 0x45 */
		x86_64_byte(state, 0x40 | REG_BIT3(reg_src) | (REG_BIT3(reg_dst) << 2));
	x86_64_byte(state, 0x8A);
	/* for some reason, RBP and R13 as source require bit 6 of this byte to be set */
	x86_64_byte(state, 0x00 | REG_LOW3(reg_src) | (REG_LOW3(reg_dst) << 3)
	                        | (REG_LOW3(reg_src) == 5 ? 0x40 : 0x00));
	/* and then RBP and R13 as source require an extra 0x00 byte */
	if (REG_LOW3(reg_src) == 5)
		x86_64_byte(state, 0x00);
	/* and then RSP and R12 as source require an extra 0x24 byte */
	else if (REG_LOW3(reg_src) == 4)
		x86_64_byte(state, 0x24);
}

/* reg_dst = (uint64_t) (*(uint16_t*) reg_src)
 *         | (reg_dst & ~UINT64_C(0xFFFF));
 * MOV <reg16>, [<reg64>]
 * Preserves the upper 48 bits of the destination register, as it only affects
 * the lower 16 bits. */
static void x86_64_mov_r16_m16r(struct x86_64_state* state, uint8_t reg_dst, uint8_t reg_src)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	/* size override prefix */
	x86_64_byte(state, 0x66);
	/* lower 8 registers for both = no prefix; otherwise... */
	if (REG_BIT3(reg_dst) || REG_BIT3(reg_src))
		/* dst upper register = 0x44; src = 0x41; both = 0x45 */
		x86_64_byte(state, 0x40 | REG_BIT3(reg_src) | (REG_BIT3(reg_dst) << 2));
	x86_64_byte(state, 0x8B);
	/* for some reason, RBP and R13 as source require bit 6 of this byte to be set */
	x86_64_byte(state, 0x00 | REG_LOW3(reg_src) | (REG_LOW3(reg_dst) << 3)
	                        | (REG_LOW3(reg_src) == 5 ? 0x40 : 0x00));
	/* and then RBP and R13 as source require an extra 0x00 byte */
	if (REG_LOW3(reg_src) == 5)
		x86_64_byte(state, 0x00);
	/* and then RSP and R12 as source require an extra 0x24 byte */
	else if (REG_LOW3(reg_src) == 4)
		x86_64_byte(state, 0x24);
}

/* reg_dst = (uint64_t) (*(uint32_t*) reg_src);
 * MOV <reg32>, [<reg64>] */
static void x86_64_mov_r32_m32r(struct x86_64_state* state, uint8_t reg_dst, uint8_t reg_src)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	/* lower 8 registers for both = no prefix; otherwise... */
	if (REG_BIT3(reg_dst) || REG_BIT3(reg_src))
		/* dst upper register = 0x44; src = 0x41; both = 0x45 */
		x86_64_byte(state, 0x40 | REG_BIT3(reg_src) | (REG_BIT3(reg_dst) << 2));
	x86_64_byte(state, 0x8B);
	/* for some reason, RBP and R13 as source require bit 6 of this byte to be set */
	x86_64_byte(state, 0x00 | REG_LOW3(reg_src) | (REG_LOW3(reg_dst) << 3)
	                        | (REG_LOW3(reg_src) == 5 ? 0x40 : 0x00));
	/* and then RBP and R13 as source require an extra 0x00 byte */
	if (REG_LOW3(reg_src) == 5)
		x86_64_byte(state, 0x00);
	/* and then RSP and R12 as source require an extra 0x24 byte */
	else if (REG_LOW3(reg_src) == 4)
		x86_64_byte(state, 0x24);
}

/* reg_dst = *(uint64_t*) reg_src;
 * MOV <reg64>, [<reg64>] */
static void x86_64_mov_r64_m64r(struct x86_64_state* state, uint8_t reg_dst, uint8_t reg_src)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	/* dst upper register = 0x4C; src = 0x49; both = 0x4D; none = 0x48 */
	x86_64_byte(state, 0x48 | REG_BIT3(reg_src) | (REG_BIT3(reg_dst) << 2));
	x86_64_byte(state, 0x8B);
	/* for some reason, RBP and R13 as source require bit 6 of this byte to be set */
	x86_64_byte(state, 0x00 | REG_LOW3(reg_src) | (REG_LOW3(reg_dst) << 3)
	                        | (REG_LOW3(reg_src) == 5 ? 0x40 : 0x00));
	/* and then RBP and R13 as source require an extra 0x00 byte */
	if (REG_LOW3(reg_src) == 5)
		x86_64_byte(state, 0x00);
	/* and then RSP and R12 as source require an extra 0x24 byte */
	else if (REG_LOW3(reg_src) == 4)
		x86_64_byte(state, 0x24);
}

/* reg_dst = *(uint64_t*) ((uint8_t*) reg_src + imm);
 * MOV <reg64>, [<reg64> + <imm8>] */
static void x86_64_mov_r64_m64rsi8(struct x86_64_state* state, uint8_t reg_dst, uint8_t reg_src, int8_t imm)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	/* dst upper register = 0x4C; src = 0x49; both = 0x4D; none = 0x48 */
	x86_64_byte(state, 0x48 | REG_BIT3(reg_src) | (REG_BIT3(reg_dst) << 2));
	x86_64_byte(state, 0x8B);
	x86_64_byte(state, 0x40 | REG_LOW3(reg_src) | (REG_LOW3(reg_dst) << 3));
	/* and then RSP and R12 as source require an extra 0x24 byte */
	if (REG_LOW3(reg_src) == 4)
		x86_64_byte(state, 0x24);
	x86_64_byte(state, (uint8_t) imm);
}

/* *(uint8_t*) reg_dst = (uint8_t) reg_src;
 * MOV [<reg64>], <reg8> */
static void x86_64_mov_m8r_r8(struct x86_64_state* state, uint8_t reg_dst, uint8_t reg_src)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	/* lower 8 registers for both = no prefix; otherwise... */
	if (REG_BIT3(reg_dst) || REG_BIT3(reg_src))
		/* dst upper register = 0x41; src = 0x44; both = 0x45 */
		x86_64_byte(state, 0x40 | REG_BIT3(reg_dst) | (REG_BIT3(reg_src) << 2));
	x86_64_byte(state, 0x88);
	/* for some reason, RBP and R13 as destination require bit 6 of this byte to be set */
	x86_64_byte(state, 0x00 | REG_LOW3(reg_dst) | (REG_LOW3(reg_src) << 3)
	                        | (REG_LOW3(reg_dst) == 5 ? 0x40 : 0x00));
	/* and then RBP and R13 as destination require an extra 0x00 byte */
	if (REG_LOW3(reg_dst) == 5)
		x86_64_byte(state, 0x00);
	/* and then RSP and R12 as destination require an extra 0x24 byte */
	else if (REG_LOW3(reg_dst) == 4)
		x86_64_byte(state, 0x24);
}

/* *(uint16_t*) reg_dst = (uint16_t) reg_src;
 * MOV [<reg64>], <reg16> */
static void x86_64_mov_m16r_r16(struct x86_64_state* state, uint8_t reg_dst, uint8_t reg_src)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	/* size override prefix */
	x86_64_byte(state, 0x66);
	/* lower 8 registers for both = no prefix; otherwise... */
	if (REG_BIT3(reg_dst) || REG_BIT3(reg_src))
		/* dst upper register = 0x41; src = 0x44; both = 0x45 */
		x86_64_byte(state, 0x40 | REG_BIT3(reg_dst) | (REG_BIT3(reg_src) << 2));
	x86_64_byte(state, 0x89);
	/* for some reason, RBP and R13 as destination require bit 6 of this byte to be set */
	x86_64_byte(state, 0x00 | REG_LOW3(reg_dst) | (REG_LOW3(reg_src) << 3)
	                        | (REG_LOW3(reg_dst) == 5 ? 0x40 : 0x00));
	/* and then RBP and R13 as destination require an extra 0x00 byte */
	if (REG_LOW3(reg_dst) == 5)
		x86_64_byte(state, 0x00);
	/* and then RSP and R12 as destination require an extra 0x24 byte */
	else if (REG_LOW3(reg_dst) == 4)
		x86_64_byte(state, 0x24);
}

/* *(uint32_t*) reg_dst = (uint32_t) reg_src;
 * MOV [<reg64>], <reg32> */
static void x86_64_mov_m32r_r32(struct x86_64_state* state, uint8_t reg_dst, uint8_t reg_src)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	/* lower 8 registers for both = no prefix; otherwise... */
	if (REG_BIT3(reg_dst) || REG_BIT3(reg_src))
		/* dst upper register = 0x41; src = 0x44; both = 0x45 */
		x86_64_byte(state, 0x40 | REG_BIT3(reg_dst) | (REG_BIT3(reg_src) << 2));
	x86_64_byte(state, 0x89);
	/* for some reason, RBP and R13 as destination require bit 6 of this byte to be set */
	x86_64_byte(state, 0x00 | REG_LOW3(reg_dst) | (REG_LOW3(reg_src) << 3)
	                        | (REG_LOW3(reg_dst) == 5 ? 0x40 : 0x00));
	/* and then RBP and R13 as destination require an extra 0x00 byte */
	if (REG_LOW3(reg_dst) == 5)
		x86_64_byte(state, 0x00);
	/* and then RSP and R12 as destination require an extra 0x24 byte */
	else if (REG_LOW3(reg_dst) == 4)
		x86_64_byte(state, 0x24);
}

/* *(uint64_t*) reg_dst = reg_src;
 * MOV [<reg64>], <reg64> */
static void x86_64_mov_m64r_r64(struct x86_64_state* state, uint8_t reg_dst, uint8_t reg_src)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	/* src upper register = 0x4C; dst = 0x49; both = 0x4D; none = 0x48 */
	x86_64_byte(state, 0x48 | REG_BIT3(reg_dst) | (REG_BIT3(reg_src) << 2));
	x86_64_byte(state, 0x89);
	/* for some reason, RBP and R13 as destination require bit 6 of this byte to be set */
	x86_64_byte(state, 0x00 | REG_LOW3(reg_dst) | (REG_LOW3(reg_src) << 3)
	                        | (REG_LOW3(reg_dst) == 5 ? 0x40 : 0x00));
	/* and then RBP and R13 as destination require an extra 0x00 byte */
	if (REG_LOW3(reg_dst) == 5)
		x86_64_byte(state, 0x00);
	/* and then RSP and R12 as destination require an extra 0x24 byte */
	else if (REG_LOW3(reg_dst) == 4)
		x86_64_byte(state, 0x24);
}

/* *(uint64_t*) ((uint8_t*) reg_dst + imm) = reg_src;
 * MOV [<reg64> + <imm8>], <reg64> */
static void x86_64_mov_m64rsi8_r64(struct x86_64_state* state, uint8_t reg_dst, int8_t imm, uint8_t reg_src)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	/* src upper register = 0x4C; dst = 0x49; both = 0x4D; none = 0x48 */
	x86_64_byte(state, 0x48 | REG_BIT3(reg_dst) | (REG_BIT3(reg_src) << 2));
	x86_64_byte(state, 0x89);
	x86_64_byte(state, 0x40 | REG_LOW3(reg_dst) | (REG_LOW3(reg_src) << 3));
	/* and then RSP and R12 as destination require an extra 0x24 byte */
	if (REG_LOW3(reg_dst) == 4)
		x86_64_byte(state, 0x24);
	x86_64_byte(state, (uint8_t) imm);
}

/* reg_dst = (uint64_t) ((uint8_t) reg_src);
 * MOVZX <reg64>, <reg8> */
static void x86_64_movzx_r64_r8(struct x86_64_state* state, uint8_t reg_dst, uint8_t reg_src)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	/* src upper register = 0x49; dst = 0x4C; both = 0x4D; none = 0x48 */
	x86_64_byte(state, 0x48 | REG_BIT3(reg_src) | (REG_BIT3(reg_dst) << 2));
	x86_64_byte(state, 0x0F);
	x86_64_byte(state, 0xB6);
	x86_64_byte(state, 0xC0 | REG_LOW3(reg_src) | (REG_LOW3(reg_dst) << 3));
}

/* reg_dst = (int64_t) ((int8_t) reg_src);
 * MOVSX <reg64>, <reg8> */
static void x86_64_movsx_r64_r8(struct x86_64_state* state, uint8_t reg_dst, uint8_t reg_src)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	/* src upper register = 0x49; dst = 0x4C; both = 0x4D; none = 0x48 */
	x86_64_byte(state, 0x48 | REG_BIT3(reg_src) | (REG_BIT3(reg_dst) << 2));
	x86_64_byte(state, 0x0F);
	x86_64_byte(state, 0xBE);
	x86_64_byte(state, 0xC0 | REG_LOW3(reg_src) | (REG_LOW3(reg_dst) << 3));
}

/* reg_dst = (uint64_t) ((uint16_t) reg_src);
 * MOVZX <reg64>, <reg16> */
static void x86_64_movzx_r64_r16(struct x86_64_state* state, uint8_t reg_dst, uint8_t reg_src)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	/* src upper register = 0x49; dst = 0x4C; both = 0x4D; none = 0x48 */
	x86_64_byte(state, 0x48 | REG_BIT3(reg_src) | (REG_BIT3(reg_dst) << 2));
	x86_64_byte(state, 0x0F);
	x86_64_byte(state, 0xB7);
	x86_64_byte(state, 0xC0 | REG_LOW3(reg_src) | (REG_LOW3(reg_dst) << 3));
}

/* reg_dst = (int64_t) ((int16_t) reg_src);
 * MOVSX <reg64>, <reg16> */
static void x86_64_movsx_r64_r16(struct x86_64_state* state, uint8_t reg_dst, uint8_t reg_src)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	/* src upper register = 0x49; dst = 0x4C; both = 0x4D; none = 0x48 */
	x86_64_byte(state, 0x48 | REG_BIT3(reg_src) | (REG_BIT3(reg_dst) << 2));
	x86_64_byte(state, 0x0F);
	x86_64_byte(state, 0xBF);
	x86_64_byte(state, 0xC0 | REG_LOW3(reg_src) | (REG_LOW3(reg_dst) << 3));
}

/* reg_dst = (int64_t) ((int32_t) reg_src);
 * MOVSX <reg64>, <reg32> */
static void x86_64_movsx_r64_r32(struct x86_64_state* state, uint8_t reg_dst, uint8_t reg_src)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	/* src upper register = 0x49; dst = 0x4C; both = 0x4D; none = 0x48 */
	x86_64_byte(state, 0x48 | REG_BIT3(reg_src) | (REG_BIT3(reg_dst) << 2));
	x86_64_byte(state, 0x63);
	x86_64_byte(state, 0xC0 | REG_LOW3(reg_src) | (REG_LOW3(reg_dst) << 3));
}

/* reg = RIP + (int32_t) (target - RIP);
 * Uses RIP as of the end of the instruction.
 * LEA <reg64>, [RIP + <imm32>] */
static void x86_64_lea_r64_RIPsi32(struct x86_64_state* state, uint8_t reg, void* target)
{
	ptrdiff_t imm = (uint8_t*) target - ((uint8_t*) state->code + 7);
	CHECK_REG(reg);
	/* writing to the upper 8 registers turns the 0x48 prefix into 0x4C */
	x86_64_byte(state, 0x48 | (REG_BIT3(reg) << 2));
	x86_64_byte(state, 0x8D);
	x86_64_byte(state, 0x05 | (REG_LOW3(reg) << 3));
	x86_64_dword(state, (uint32_t) imm);
}

/* set_flags(*(int32_t*) reg, (int32_t) ((int8_t) imm));
 * CMP [<reg64>], <imm8> */
static void x86_64_cmp_m32r_si8(struct x86_64_state* state, uint8_t reg, int8_t imm)
{
	CHECK_REG(reg);
	/* reading from the upper 8 registers requires the 0x41 prefix */
	if (REG_BIT3(reg))
		x86_64_byte(state, 0x41);
	x86_64_byte(state, 0x83);
	/* for some reason, RBP and R13 as source require bit 6 of this byte to be set */
	x86_64_byte(state, 0x38 | REG_LOW3(reg) | (REG_LOW3(reg) == 5 ? 0x40 : 0x00));
	/* and then RBP and R13 as destination require an extra 0x00 byte */
	if (REG_LOW3(reg) == 5)
		x86_64_byte(state, 0x00);
	/* and then RSP and R12 as destination require an extra 0x24 byte */
	else if (REG_LOW3(reg) == 4)
		x86_64_byte(state, 0x24);
	x86_64_byte(state, (uint8_t) imm);
}

/* set_flags(*(int32_t*) reg, imm);
 * CMP [<reg64>], <imm32> */
static void x86_64_cmp_m32r_i32(struct x86_64_state* state, uint8_t reg, int32_t imm)
{
	CHECK_REG(reg);
	/* reading from the upper 8 registers requires the 0x41 prefix */
	if (REG_BIT3(reg))
		x86_64_byte(state, 0x41);
	x86_64_byte(state, 0x81);
	/* for some reason, RBP and R13 as source require bit 6 of this byte to be set */
	x86_64_byte(state, 0x38 | REG_LOW3(reg) | (REG_LOW3(reg) == 5 ? 0x40 : 0x00));
	/* and then RBP and R13 as destination require an extra 0x00 byte */
	if (REG_LOW3(reg) == 5)
		x86_64_byte(state, 0x00);
	/* and then RSP and R12 as destination require an extra 0x24 byte */
	else if (REG_LOW3(reg) == 4)
		x86_64_byte(state, 0x24);
	x86_64_dword(state, (uint32_t) imm);
}

/* reg = reg + (int64_t) (int8_t) imm;
 * ADD <reg64>, <imm8> */
static void x86_64_add_r64_si8(struct x86_64_state* state, uint8_t reg, int8_t imm)
{
	CHECK_REG(reg);
	/* writing to the upper 8 registers turns the 0x48 prefix into 0x49 */
	x86_64_byte(state, 0x48 | REG_BIT3(reg));
	x86_64_byte(state, 0x83);
	x86_64_byte(state, 0xC0 | REG_LOW3(reg));
	x86_64_byte(state, (uint8_t) imm);
}

/* RAX = RAX + (int64_t) (int32_t) imm;
 * ADD RAX, <imm32> */
static void x86_64_add_RAX_si32(struct x86_64_state* state, int32_t imm)
{
	x86_64_byte(state, 0x48);
	x86_64_byte(state, 0x05);
	x86_64_dword(state, (uint32_t) imm);
}

/* reg = reg + (int64_t) (int32_t) imm;
 * ADD <reg64>, <imm32> */
static void x86_64_add_r64_si32(struct x86_64_state* state, uint8_t reg, int32_t imm)
{
	/* Substitute other encodings as they are shorter: ADD to RAX */
	if (reg == RAX) {
		x86_64_add_RAX_si32(state, imm);
	} else {
		CHECK_REG(reg);
		/* writing to the upper 8 registers turns the 0x48 prefix into 0x49 */
		x86_64_byte(state, 0x48 | REG_BIT3(reg));
		x86_64_byte(state, 0x81);
		x86_64_byte(state, 0xC0 | REG_LOW3(reg));
		x86_64_dword(state, (uint32_t) imm);
	}
}

/* reg_dst = reg_dst + reg_src;
 * ADD <reg64>, <reg64> */
static void x86_64_add_r64_r64(struct x86_64_state* state, uint8_t reg_dst, uint8_t reg_src)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	/* src upper register = 0x4C; dst = 0x49; both = 0x4D; none = 0x48 */
	x86_64_byte(state, 0x48 | REG_BIT3(reg_dst) | (REG_BIT3(reg_src) << 2));
	x86_64_byte(state, 0x01);
	x86_64_byte(state, 0xC0 | REG_LOW3(reg_dst) | (REG_LOW3(reg_src) << 3));
}

/* reg = reg - (int64_t) (int8_t) imm;
 * SUB <reg64>, <imm8> */
static void x86_64_sub_r64_si8(struct x86_64_state* state, uint8_t reg, int8_t imm)
{
	CHECK_REG(reg);
	/* writing to the upper 8 registers turns the 0x48 prefix into 0x49 */
	x86_64_byte(state, 0x48 | REG_BIT3(reg));
	x86_64_byte(state, 0x83);
	x86_64_byte(state, 0xE8 | REG_LOW3(reg));
	x86_64_byte(state, (uint8_t) imm);
}

/* RAX = RAX - (int64_t) (int32_t) imm;
 * SUB RAX, <imm32> */
static void x86_64_sub_RAX_si32(struct x86_64_state* state, int32_t imm)
{
	x86_64_byte(state, 0x48);
	x86_64_byte(state, 0x2D);
	x86_64_dword(state, (uint32_t) imm);
}

/* reg = reg - (int64_t) (int32_t) imm;
 * SUB <reg64>, <imm32> */
static void x86_64_sub_r64_si32(struct x86_64_state* state, uint8_t reg, int32_t imm)
{
	/* Substitute other encodings as they are shorter: SUB to RAX */
	if (reg == RAX) {
		x86_64_sub_RAX_si32(state, imm);
	} else {
		CHECK_REG(reg);
		/* writing to the upper 8 registers turns the 0x48 prefix into 0x49 */
		x86_64_byte(state, 0x48 | REG_BIT3(reg));
		x86_64_byte(state, 0x81);
		x86_64_byte(state, 0xE8 | REG_LOW3(reg));
		x86_64_dword(state, (uint32_t) imm);
	}
}

/* reg_dst = reg_dst - reg_src;
 * SUB <reg64>, <reg64> */
static void x86_64_sub_r64_r64(struct x86_64_state* state, uint8_t reg_dst, uint8_t reg_src)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	/* src upper register = 0x4C; dst = 0x49; both = 0x4D; none = 0x48 */
	x86_64_byte(state, 0x48 | REG_BIT3(reg_dst) | (REG_BIT3(reg_src) << 2));
	x86_64_byte(state, 0x29);
	x86_64_byte(state, 0xC0 | REG_LOW3(reg_dst) | (REG_LOW3(reg_src) << 3));
}

/* reg = -reg;
 * NEG <reg64> */
static void x86_64_neg_r64(struct x86_64_state* state, uint8_t reg)
{
	CHECK_REG(reg);
	/* using the upper 8 registers turns the 0x48 prefix into 0x49 */
	x86_64_byte(state, 0x48 | REG_BIT3(reg));
	x86_64_byte(state, 0xF7);
	x86_64_byte(state, 0xD8 | REG_LOW3(reg));
}

/* reg_dst = reg_dst & reg_src;
 * AND <reg64>, <reg64> */
static void x86_64_and_r64_r64(struct x86_64_state* state, uint8_t reg_dst, uint8_t reg_src)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	/* src upper register = 0x4C; dst = 0x49; both = 0x4D; none = 0x48 */
	x86_64_byte(state, 0x48 | REG_BIT3(reg_dst) | (REG_BIT3(reg_src) << 2));
	x86_64_byte(state, 0x21);
	x86_64_byte(state, 0xC0 | REG_LOW3(reg_dst) | (REG_LOW3(reg_src) << 3));
}

/* RAX = RAX & (int64_t) ((int32_t) imm);
 * AND RAX, <imm32> */
static void x86_64_and_RAX_si32(struct x86_64_state* state, int32_t imm)
{
	x86_64_byte(state, 0x48);
	x86_64_byte(state, 0x25);
	x86_64_dword(state, (uint32_t) imm);
}

/* reg = reg & (int64_t) ((int32_t) imm);
 * AND <reg64>, <imm32> */
static void x86_64_and_r64_si32(struct x86_64_state* state, uint8_t reg, int32_t imm)
{
	/* Substitute other encodings as they are shorter: AND with RAX */
	if (reg == RAX) {
		x86_64_and_RAX_si32(state, imm);
	} else {
		CHECK_REG(reg);
		/* using the upper 8 registers turns the 0x48 prefix into 0x49 */
		x86_64_byte(state, 0x48 | REG_BIT3(reg));
		x86_64_byte(state, 0x81);
		x86_64_byte(state, 0xE0 | REG_LOW3(reg));
		x86_64_dword(state, (uint32_t) imm);
	}
}

/* reg = reg & (int64_t) ((int8_t) imm);
 * AND <reg64>, <imm8> */
static void x86_64_and_r64_si8(struct x86_64_state* state, uint8_t reg, int8_t imm)
{
	CHECK_REG(reg);
	/* using the upper 8 registers turns the 0x48 prefix into 0x49 */
	x86_64_byte(state, 0x48 | REG_BIT3(reg));
	x86_64_byte(state, 0x83);
	x86_64_byte(state, 0xE0 | REG_LOW3(reg));
	x86_64_byte(state, (uint8_t) imm);
}

/* reg_dst = reg_dst | reg_src;
 * OR <reg64>, <reg64> */
static void x86_64_or_r64_r64(struct x86_64_state* state, uint8_t reg_dst, uint8_t reg_src)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	/* src upper register = 0x4C; dst = 0x49; both = 0x4D; none = 0x48 */
	x86_64_byte(state, 0x48 | REG_BIT3(reg_dst) | (REG_BIT3(reg_src) << 2));
	x86_64_byte(state, 0x09);
	x86_64_byte(state, 0xC0 | REG_LOW3(reg_dst) | (REG_LOW3(reg_src) << 3));
}

/* RAX = RAX | (int64_t) ((int32_t) imm);
 * OR RAX, <imm32> */
static void x86_64_or_RAX_si32(struct x86_64_state* state, int32_t imm)
{
	x86_64_byte(state, 0x48);
	x86_64_byte(state, 0x0D);
	x86_64_dword(state, (uint32_t) imm);
}

/* reg = reg | (int64_t) ((int32_t) imm);
 * OR <reg64>, <imm32> */
static void x86_64_or_r64_si32(struct x86_64_state* state, uint8_t reg, int32_t imm)
{
	/* Substitute other encodings as they are shorter: OR with RAX */
	if (reg == RAX) {
		x86_64_or_RAX_si32(state, imm);
	} else {
		CHECK_REG(reg);
		/* using the upper 8 registers turns the 0x48 prefix into 0x49 */
		x86_64_byte(state, 0x48 | REG_BIT3(reg));
		x86_64_byte(state, 0x81);
		x86_64_byte(state, 0xC8 | REG_LOW3(reg));
		x86_64_dword(state, (uint32_t) imm);
	}
}

/* reg = reg | (int64_t) ((int8_t) imm);
 * OR <reg64>, <imm8> */
static void x86_64_or_r64_si8(struct x86_64_state* state, uint8_t reg, int8_t imm)
{
	CHECK_REG(reg);
	/* using the upper 8 registers turns the 0x48 prefix into 0x49 */
	x86_64_byte(state, 0x48 | REG_BIT3(reg));
	x86_64_byte(state, 0x83);
	x86_64_byte(state, 0xC8 | REG_LOW3(reg));
	x86_64_byte(state, (uint8_t) imm);
}

/* reg_dst = reg_dst ^ reg_src;
 * XOR <reg64>, <reg64> */
static void x86_64_xor_r64_r64(struct x86_64_state* state, uint8_t reg_dst, uint8_t reg_src)
{
	CHECK_REG(reg_dst);
	CHECK_REG(reg_src);
	/* src upper register = 0x4C; dst = 0x49; both = 0x4D; none = 0x48 */
	x86_64_byte(state, 0x48 | REG_BIT3(reg_dst) | (REG_BIT3(reg_src) << 2));
	x86_64_byte(state, 0x31);
	x86_64_byte(state, 0xC0 | REG_LOW3(reg_dst) | (REG_LOW3(reg_src) << 3));
}

/* RAX = RAX ^ (int64_t) ((int32_t) imm);
 * XOR RAX, <imm32> */
static void x86_64_xor_RAX_si32(struct x86_64_state* state, int32_t imm)
{
	x86_64_byte(state, 0x48);
	x86_64_byte(state, 0x35);
	x86_64_dword(state, (uint32_t) imm);
}

/* reg = reg ^ (int64_t) ((int32_t) imm);
 * XOR <reg64>, <imm32> */
static void x86_64_xor_r64_si32(struct x86_64_state* state, uint8_t reg, int32_t imm)
{
	/* Substitute other encodings as they are shorter: XOR with RAX */
	if (reg == RAX) {
		x86_64_xor_RAX_si32(state, imm);
	} else {
		CHECK_REG(reg);
		/* using the upper 8 registers turns the 0x48 prefix into 0x49 */
		x86_64_byte(state, 0x48 | REG_BIT3(reg));
		x86_64_byte(state, 0x81);
		x86_64_byte(state, 0xF0 | REG_LOW3(reg));
		x86_64_dword(state, (uint32_t) imm);
	}
}

/* reg = reg ^ (int64_t) ((int8_t) imm);
 * XOR <reg64>, <imm8> */
static void x86_64_xor_r64_si8(struct x86_64_state* state, uint8_t reg, int8_t imm)
{
	CHECK_REG(reg);
	/* using the upper 8 registers turns the 0x48 prefix into 0x49 */
	x86_64_byte(state, 0x48 | REG_BIT3(reg));
	x86_64_byte(state, 0x83);
	x86_64_byte(state, 0xF0 | REG_LOW3(reg));
	x86_64_byte(state, (uint8_t) imm);
}

/* reg = ~reg;
 * NOT <reg64> */
static void x86_64_not_r64(struct x86_64_state* state, uint8_t reg)
{
	CHECK_REG(reg);
	/* using the upper 8 registers turns the 0x48 prefix into 0x49 */
	x86_64_byte(state, 0x48 | REG_BIT3(reg));
	x86_64_byte(state, 0xF7);
	x86_64_byte(state, 0xD0 | REG_LOW3(reg));
}

/* reg = reg << 1;
 * SHL <reg64>, 1 */
static void x86_64_shl_r64_1(struct x86_64_state* state, uint8_t reg)
{
	CHECK_REG(reg);
	/* using the upper 8 registers turns the 0x48 prefix into 0x49 */
	x86_64_byte(state, 0x48 | REG_BIT3(reg));
	x86_64_byte(state, 0xD1);
	x86_64_byte(state, 0xE0 | REG_LOW3(reg));
}

/* reg = reg << amt;
 * SHL <reg64>, <imm6> */
static void x86_64_shl_r64_i6(struct x86_64_state* state, uint8_t reg, uint8_t amt)
{
	/* Substitute other encodings as they are shorter: SHL by 1 */
	if (amt == 1)
		x86_64_shl_r64_1(state, reg);
	else {
		CHECK_REG(reg);
		assert(amt != 0 && amt < 64);
		/* using the upper 8 registers turns the 0x48 prefix into 0x49 */
		x86_64_byte(state, 0x48 | REG_BIT3(reg));
		x86_64_byte(state, 0xC1);
		x86_64_byte(state, 0xE0 | REG_LOW3(reg));
		x86_64_byte(state, amt);
	}
}

/* reg = reg << CL;
 * SHL <reg64>, CL */
static void x86_64_shl_r64_CL(struct x86_64_state* state, uint8_t reg)
{
	CHECK_REG(reg);
	/* using the upper 8 registers turns the 0x48 prefix into 0x49 */
	x86_64_byte(state, 0x48 | REG_BIT3(reg));
	x86_64_byte(state, 0xD3);
	x86_64_byte(state, 0xE0 | REG_LOW3(reg));
}

/* reg = reg << 1;
 * SHL <reg32>, 1 */
static void x86_64_shl_r32_1(struct x86_64_state* state, uint8_t reg)
{
	CHECK_REG(reg);
	/* using the upper 8 registers requires the 0x41 prefix */
	if (REG_BIT3(reg))
		x86_64_byte(state, 0x41);
	x86_64_byte(state, 0xD1);
	x86_64_byte(state, 0xE0 | REG_LOW3(reg));
}

/* reg = reg << amt;
 * SHL <reg32>, <imm5> */
static void x86_64_shl_r32_i5(struct x86_64_state* state, uint8_t reg, uint8_t amt)
{
	/* Substitute other encodings as they are shorter: SHL by 1 */
	if (amt == 1)
		x86_64_shl_r32_1(state, reg);
	else {
		CHECK_REG(reg);
		assert(amt != 0 && amt < 32);
		/* using the upper 8 registers requires the 0x41 prefix */
		if (REG_BIT3(reg))
			x86_64_byte(state, 0x41);
		x86_64_byte(state, 0xC1);
		x86_64_byte(state, 0xE0 | REG_LOW3(reg));
		x86_64_byte(state, amt);
	}
}

/* reg = reg << CL;
 * SHL <reg32>, CL */
static void x86_64_shl_r32_CL(struct x86_64_state* state, uint8_t reg)
{
	CHECK_REG(reg);
	/* using the upper 8 registers requires the 0x41 prefix */
	if (REG_BIT3(reg))
		x86_64_byte(state, 0x41);
	x86_64_byte(state, 0xD3);
	x86_64_byte(state, 0xE0 | REG_LOW3(reg));
}

/* reg = (uint64_t) reg >> 1;
 * SHR <reg64>, 1 */
static void x86_64_shr_r64_1(struct x86_64_state* state, uint8_t reg)
{
	CHECK_REG(reg);
	/* using the upper 8 registers turns the 0x48 prefix into 0x49 */
	x86_64_byte(state, 0x48 | REG_BIT3(reg));
	x86_64_byte(state, 0xD1);
	x86_64_byte(state, 0xE8 | REG_LOW3(reg));
}

/* reg = (uint64_t) reg >> amt;
 * SHR <reg64>, <imm6> */
static void x86_64_shr_r64_i6(struct x86_64_state* state, uint8_t reg, uint8_t amt)
{
	/* Substitute other encodings as they are shorter: SHR by 1 */
	if (amt == 1)
		x86_64_shr_r64_1(state, reg);
	else {
		CHECK_REG(reg);
		assert(amt != 0 && amt < 64);
		/* using the upper 8 registers turns the 0x48 prefix into 0x49 */
		x86_64_byte(state, 0x48 | REG_BIT3(reg));
		x86_64_byte(state, 0xC1);
		x86_64_byte(state, 0xE8 | REG_LOW3(reg));
		x86_64_byte(state, amt);
	}
}

/* reg = (uint64_t) reg >> CL;
 * SHR <reg64>, CL */
static void x86_64_shr_r64_CL(struct x86_64_state* state, uint8_t reg)
{
	CHECK_REG(reg);
	/* using the upper 8 registers turns the 0x48 prefix into 0x49 */
	x86_64_byte(state, 0x48 | REG_BIT3(reg));
	x86_64_byte(state, 0xD3);
	x86_64_byte(state, 0xE8 | REG_LOW3(reg));
}

/* reg = (uint32_t) reg >> 1;
 * SHR <reg32>, 1 */
static void x86_64_shr_r32_1(struct x86_64_state* state, uint8_t reg)
{
	CHECK_REG(reg);
	/* using the upper 8 registers requires the 0x41 prefix */
	if (REG_BIT3(reg))
		x86_64_byte(state, 0x41);
	x86_64_byte(state, 0xD1);
	x86_64_byte(state, 0xE8 | REG_LOW3(reg));
}

/* reg = (uint32_t) reg >> amt;
 * SHR <reg32>, <imm5> */
static void x86_64_shr_r32_i5(struct x86_64_state* state, uint8_t reg, uint8_t amt)
{
	/* Substitute other encodings as they are shorter: SHR by 1 */
	if (amt == 1)
		x86_64_shr_r32_1(state, reg);
	else {
		CHECK_REG(reg);
		assert(amt != 0 && amt < 32);
		/* using the upper 8 registers requires the 0x41 prefix */
		if (REG_BIT3(reg))
			x86_64_byte(state, 0x41);
		x86_64_byte(state, 0xC1);
		x86_64_byte(state, 0xE8 | REG_LOW3(reg));
		x86_64_byte(state, amt);
	}
}

/* reg = (uint32_t) reg >> CL;
 * SHR <reg32>, CL */
static void x86_64_shr_r32_CL(struct x86_64_state* state, uint8_t reg)
{
	CHECK_REG(reg);
	/* using the upper 8 registers requires the 0x41 prefix */
	if (REG_BIT3(reg))
		x86_64_byte(state, 0x41);
	x86_64_byte(state, 0xD3);
	x86_64_byte(state, 0xE8 | REG_LOW3(reg));
}

/* reg = (int64_t) reg >> 1;
 * SAR <reg64>, 1 */
static void x86_64_sar_r64_1(struct x86_64_state* state, uint8_t reg)
{
	CHECK_REG(reg);
	/* using the upper 8 registers turns the 0x48 prefix into 0x49 */
	x86_64_byte(state, 0x48 | REG_BIT3(reg));
	x86_64_byte(state, 0xD1);
	x86_64_byte(state, 0xF8 | REG_LOW3(reg));
}

/* reg = (int64_t) reg >> amt;
 * SAR <reg64>, <imm6> */
static void x86_64_sar_r64_i6(struct x86_64_state* state, uint8_t reg, uint8_t amt)
{
	/* Substitute other encodings as they are shorter: SAR by 1 */
	if (amt == 1)
		x86_64_sar_r64_1(state, reg);
	else {
		CHECK_REG(reg);
		assert(amt != 0 && amt < 64);
		/* using the upper 8 registers turns the 0x48 prefix into 0x49 */
		x86_64_byte(state, 0x48 | REG_BIT3(reg));
		x86_64_byte(state, 0xC1);
		x86_64_byte(state, 0xF8 | REG_LOW3(reg));
		x86_64_byte(state, amt);
	}
}

/* reg = (int64_t) reg >> CL;
 * SAR <reg64>, CL */
static void x86_64_sar_r64_CL(struct x86_64_state* state, uint8_t reg)
{
	CHECK_REG(reg);
	/* using the upper 8 registers turns the 0x48 prefix into 0x49 */
	x86_64_byte(state, 0x48 | REG_BIT3(reg));
	x86_64_byte(state, 0xD3);
	x86_64_byte(state, 0xF8 | REG_LOW3(reg));
}

/* reg = (int32_t) reg >> 1;
 * SAR <reg32>, 1 */
static void x86_64_sar_r32_1(struct x86_64_state* state, uint8_t reg)
{
	CHECK_REG(reg);
	/* using the upper 8 registers requires the 0x41 prefix */
	if (REG_BIT3(reg))
		x86_64_byte(state, 0x41);
	x86_64_byte(state, 0xD1);
	x86_64_byte(state, 0xF8 | REG_LOW3(reg));
}

/* reg = (int32_t) reg >> amt;
 * SAR <reg32>, <imm5> */
static void x86_64_sar_r32_i5(struct x86_64_state* state, uint8_t reg, uint8_t amt)
{
	/* Substitute other encodings as they are shorter: SAR by 1 */
	if (amt == 1)
		x86_64_sar_r32_1(state, reg);
	else {
		CHECK_REG(reg);
		assert(amt != 0 && amt < 32);
		/* using the upper 8 registers requires the 0x41 prefix */
		if (REG_BIT3(reg))
			x86_64_byte(state, 0x41);
		x86_64_byte(state, 0xC1);
		x86_64_byte(state, 0xF8 | REG_LOW3(reg));
		x86_64_byte(state, amt);
	}
}

/* reg = (int32_t) reg >> CL;
 * SAR <reg32>, CL */
static void x86_64_sar_r32_CL(struct x86_64_state* state, uint8_t reg)
{
	CHECK_REG(reg);
	/* using the upper 8 registers requires the 0x41 prefix */
	if (REG_BIT3(reg))
		x86_64_byte(state, 0x41);
	x86_64_byte(state, 0xD3);
	x86_64_byte(state, 0xF8 | REG_LOW3(reg));
}
