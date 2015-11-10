/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - MIPS (Nintendo 64) memory access instructions           *
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
#include <stdint.h>
#include <stdio.h>

#include "mips-memory.h"

#include "mips-alu.h"
#include "mips-emit.h"
#include "mips-except.h"
#include "native-endian.h"
#include "native-ops.h"
#include "native-utils.h"

#include "../mips-interp.h"
#include "../mips-parse.h"
#include "../native-tracecache.h"

#include "../../r4300.h"
#include "../../cached_interp.h" /* For invalid_code */
#include "../../../memory/memory.h"
#include "../../../main/main.h"

/* This is the type of a memory accessor function in memory/memory.h. */
typedef void (*MemFunc) (void);

/* - - - HELPER FUNCTIONS - - - */

static bool IsConstRDRAMAddress(const struct ConstAnalysis* CA, uint8_t reg, int16_t offset)
{
	if (IntIsMaskKnown(CA, reg, UINT64_C(0xDFFFFFFF))) {
		uint32_t addr = IntGetKnownBitsInMask(CA, reg, UINT64_C(0xDFFFFFFF)) + offset;
		return (addr & UINT32_C(0xDF800000)) == UINT32_C(0x80000000);
	} else {
		return false;
	}
}

static uint32_t GetConstRDRAMAddress(const struct ConstAnalysis* CA, uint8_t reg, int16_t offset)
{
	assert(IsConstRDRAMAddress(CA, reg, offset));
	return (IntGetKnownBitsInMask(CA, reg, UINT64_C(0xDFFFFFFF)) + offset) & UINT32_C(0x7FFFFF);
}

/* Handles emitting the slow path for memory reads which need to go to a C
 * accessor function.
 *
 * The task of this slow path is to:
 * - save allocated caller-saved registers (not necessarily to the N64 state)
 *   in preparation for the call to C;
 * - use the N64 address, in native register #userdata, to get the correct
 *   function and call it;
 * - reload caller-saved registers;
 * - load the value into the native register(s) assigned to the opcode's 'rt'
 *   field as expected by the usual path;
 * - return to the usual path.
 *
 * Because #userdata may refer to the 'rs' register, if no address offset was
 * used, it may not be rewritten. It may be used by the following instruction.
 */
static enum TJEmitTraceResult load_int_slow(struct mips32_state* state, struct mips32_reg_cache* cache, void* usual_path, uint32_t userdata, uint8_t bytes, bool sign_extend)
{
	uint8_t naddr = userdata,
	        naccessors = 2, nnext_pc = 3, npc_addr_hi = 4, nt1 = 5,
	        nt_hi = 0, nt_lo;
	uint8_t rt = RT_OF(state->ops[0]);
	MemFunc* accessors;

	assert(bytes == 1 || bytes == 2 || bytes == 4 || bytes == 8);

	/* We're about to call C. Save caller-saved registers to the stack. */
	mips32_save_to_stack(state, cache);

	/* Avoid overlap between the temporary registers and 'naddr'. */
	if (naccessors == naddr) {
		naccessors = 6;
	} else if (nnext_pc == naddr) {
		nnext_pc = 7;
	} else if (npc_addr_hi == naddr) {
		npc_addr_hi = 8;
	} else if (nt1 == naddr) {
		nt1 = 9;
	}

	switch (bytes) {
	case 1:  accessors = readmemb; break;
	case 2:  accessors = readmemh; break;
	case 4:  accessors = readmem;  break;
	case 8:  accessors = readmemd; break;
	default: accessors = NULL;     break;
	}

	uint16_t pc_addr_hi;
	int16_t pc_addr_lo;
	mips32_split_mem_ref(&TJ_PC.addr, &pc_addr_hi, &pc_addr_lo);

	mips32_i32(state, naccessors, (uintptr_t) accessors);
	mips32_i32(state, nnext_pc, state->pc + 4);
	mips32_lui(state, npc_addr_hi, pc_addr_hi);

	/* The memory accessor may raise an exception. Make sure TJ_PC.addr has
	 * the address of the instruction past this one. */
	mips32_sw(state, nnext_pc, pc_addr_lo, npc_addr_hi);
	/* Read into g_state.regs.gpr[rt]. */
	mips32_addiu(state, nt1, REG_STATE, STATE_OFFSET(regs.gpr[rt]));
	mips32_sw(state, nt1, STATE_OFFSET(read_dest), REG_STATE);
	/* Write the address to be loaded into g_state.access_addr. */
	mips32_sw(state, naddr, STATE_OFFSET(access_addr), REG_STATE);
	mips32_srl(state, nt1, naddr, 16); /* grab the high 16 bits of the address */
	mips32_sll(state, nt1, nt1, 2); /* function pointers are 4 bytes large */
	mips32_addu(state, nt1, naccessors, nt1); /* $8 = &accessors[addr >> 16] */
	mips32_lw(state, REG_PIC_CALL, 0, nt1);
	mips32_jalr(state, REG_PIC_CALL);
	mips32_nop(state);

	/* On return... */

	/* Restore caller-saved registers from the stack. */
	mips32_load_from_stack(state, cache);

	/* If access_addr has become 0, a TLB Refill exception was raised. */
	mips32_lw(state, REG_EMERGENCY, STATE_OFFSET(access_addr), REG_STATE);
	mips32_beq(state, REG_EMERGENCY, 0, +0);
	FAIL_AS(mips32_add_slow_path(state, cache, &mips32_exception_writeback, state->code,
		NULL /* no usual path */, 0 /* no userdata */));
	mips32_nop(state);

	/* Load the new value of Nintendo 64 register rt. The native register(s)
	 * for this were already allocated by the fast path. */
	if (bytes == 8) {
		nt_hi = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rt]);
	}
	nt_lo = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]);

	if (bytes == 8) {
		mips32_lw(state, nt_lo, STATE_OFFSET(regs.gpr[rt]) + LO32_64_BYTES, REG_STATE);
	}

	mips32_bgez(state, 0, +0);
	FAIL_AS(mips32_return_to_usual_path(state, cache, usual_path));
	/* 1 instruction of the load is done in the delay slot, except for LD. */
	if (bytes == 1 && sign_extend) {
		mips32_lb(state, nt_lo, STATE_OFFSET(regs.gpr[rt]) + LO8_64_BYTES, REG_STATE);
	} else if (bytes == 1 /* && !sign_extend */) {
		mips32_lbu(state, nt_lo, STATE_OFFSET(regs.gpr[rt]) + LO8_64_BYTES, REG_STATE);
	} else if (bytes == 2 && sign_extend) {
		mips32_lh(state, nt_lo, STATE_OFFSET(regs.gpr[rt]) + LO16_64_BYTES, REG_STATE);
	} else if (bytes == 2 /* && !sign_extend */) {
		mips32_lhu(state, nt_lo, STATE_OFFSET(regs.gpr[rt]) + LO16_64_BYTES, REG_STATE);
	} else if (bytes == 4) {
		mips32_lw(state, nt_lo, STATE_OFFSET(regs.gpr[rt]) + LO32_64_BYTES, REG_STATE);
	} else {
		mips32_lw(state, nt_hi, STATE_OFFSET(regs.gpr[rt]) + HI32_64_BYTES, REG_STATE);
	}

	return TJ_SUCCESS;
}

static enum TJEmitTraceResult lb_slow(struct mips32_state* state, struct mips32_reg_cache* cache, void* usual_path, uint32_t userdata)
{
	return load_int_slow(state, cache, usual_path, userdata, 1, true);
}

static enum TJEmitTraceResult lbu_slow(struct mips32_state* state, struct mips32_reg_cache* cache, void* usual_path, uint32_t userdata)
{
	return load_int_slow(state, cache, usual_path, userdata, 1, false);
}

static enum TJEmitTraceResult lh_slow(struct mips32_state* state, struct mips32_reg_cache* cache, void* usual_path, uint32_t userdata)
{
	return load_int_slow(state, cache, usual_path, userdata, 2, true);
}

static enum TJEmitTraceResult lhu_slow(struct mips32_state* state, struct mips32_reg_cache* cache, void* usual_path, uint32_t userdata)
{
	return load_int_slow(state, cache, usual_path, userdata, 2, false);
}

static enum TJEmitTraceResult lw_slow(struct mips32_state* state, struct mips32_reg_cache* cache, void* usual_path, uint32_t userdata)
{
	return load_int_slow(state, cache, usual_path, userdata, 4, true);
}

static enum TJEmitTraceResult lwu_slow(struct mips32_state* state, struct mips32_reg_cache* cache, void* usual_path, uint32_t userdata)
{
	return load_int_slow(state, cache, usual_path, userdata, 4, false);
}

static enum TJEmitTraceResult ld_slow(struct mips32_state* state, struct mips32_reg_cache* cache, void* usual_path, uint32_t userdata)
{
	return load_int_slow(state, cache, usual_path, userdata, 8, false);
}

/* Handles emitting the slow path for LWC1 instructions which need to go to a
 * C accessor function.
 *
 * The task of this slow path is to:
 * - save allocated caller-saved registers (not necessarily to the N64 state)
 *   in preparation for the call to C;
 * - use the N64 address, in native register #userdata, to get the correct
 *   function and call it;
 * - reload caller-saved registers;
 * - load the value into the native register assigned to the opcode's 'ft'
 *   field as expected by the usual path;
 * - return to the usual path.
 *
 * Because #userdata may refer to the 'rs' register, if no address offset was
 * used, it may not be rewritten. It may be used by the following instruction.
 */
static enum TJEmitTraceResult lwc1_slow(struct mips32_state* state, struct mips32_reg_cache* cache, void* usual_path, uint32_t userdata)
{
	uint8_t naddr = userdata,
	        naccessors = 2, nnext_pc = 3, npc_addr_hi = 4, nt1 = 5,
	        nft;
	uint8_t ft = FT_OF(state->ops[0]);

	/* We're about to call C. Save caller-saved registers to the stack. */
	mips32_save_to_stack(state, cache);

	/* Avoid overlap between the temporary registers and 'naddr'. */
	if (naccessors == naddr) {
		naccessors = 6;
	} else if (nnext_pc == naddr) {
		nnext_pc = 7;
	} else if (npc_addr_hi == naddr) {
		npc_addr_hi = 8;
	} else if (nt1 == naddr) {
		nt1 = 9;
	}

	uint16_t pc_addr_hi;
	int16_t pc_addr_lo;
	mips32_split_mem_ref(&TJ_PC.addr, &pc_addr_hi, &pc_addr_lo);
	mips32_i32(state, naccessors, (uintptr_t) readmem);
	mips32_i32(state, nnext_pc, state->pc + 4);
	mips32_lui(state, npc_addr_hi, pc_addr_hi);

	/* The memory accessor may raise an exception. Make sure TJ_PC.addr has
	 * the address of the instruction past this one. */
	mips32_sw(state, nnext_pc, pc_addr_lo, npc_addr_hi);
	/* Read into the temporary stack slot allocated by mips32_jit_entry.
	 * Memory accessors write 64 bits, and we don't want to load 64 bits,
	 * only the 32 bits of a float value. Store the address of the slot
	 * into read_dest. */
	mips32_addiu(state, nt1, REG_STACK, STACK_OFFSET_MEMORY_READ);
	mips32_sw(state, nt1, STATE_OFFSET(read_dest), REG_STATE);
	/* Write the address to be loaded into g_state.access_addr. */
	mips32_sw(state, naddr, STATE_OFFSET(access_addr), REG_STATE);
	mips32_srl(state, nt1, naddr, 16); /* grab the high 16 bits of the address */
	mips32_sll(state, nt1, nt1, 2); /* function pointers are 4 bytes large */
	mips32_addu(state, nt1, naccessors, nt1); /* $8 = &accessors[addr >> 16] */
	mips32_lw(state, REG_PIC_CALL, 0, nt1);
	mips32_jalr(state, REG_PIC_CALL);
	mips32_nop(state);

	/* On return... */

	/* Restore caller-saved registers from the stack. */
	mips32_load_from_stack(state, cache);

	/* If access_addr has become 0, a TLB Refill exception was raised. */
	mips32_lw(state, REG_EMERGENCY, STATE_OFFSET(access_addr), REG_STATE);
	mips32_beq(state, REG_EMERGENCY, 0, +0);
	FAIL_AS(mips32_add_slow_path(state, cache, &mips32_exception_writeback, state->code,
		NULL /* no usual path */, 0 /* no userdata */));
	mips32_nop(state);

	/* Load the new value of Nintendo 64 register ft. The native register
	 * for it was already allocated by the fast path. */
	nft = mips32_alloc_float_in_32(state, cache, ft);

	mips32_bgez(state, 0, +0);
	FAIL_AS(mips32_return_to_usual_path(state, cache, usual_path));
	mips32_lwc1(state, nft, STACK_OFFSET_MEMORY_READ, REG_STACK); /* Delay slot */

	return TJ_SUCCESS;
}

/* Handles emitting the slow path for LDC1 instructions which need to go to a
 * C accessor function.
 *
 * The task of this slow path is to:
 * - save allocated caller-saved registers (not necessarily to the N64 state)
 *   in preparation for the call to C;
 * - use the N64 address, in native register #userdata, to get the correct
 *   function and call it;
 * - reload caller-saved registers;
 * - load the value into the native register assigned to the opcode's 'ft'
 *   field as expected by the usual path;
 * - return to the usual path.
 *
 * Because #userdata may refer to the 'rs' register, if no address offset was
 * used, it may not be rewritten. It may be used by the following instruction.
 */
static enum TJEmitTraceResult ldc1_slow(struct mips32_state* state, struct mips32_reg_cache* cache, void* usual_path, uint32_t userdata)
{
	uint8_t naddr = userdata,
	        naccessors = 2, nnext_pc = 3, npc_addr_hi = 4, nt1 = 5,
	        nft;
	uint8_t ft = FT_OF(state->ops[0]);

	/* We're about to call C. Save caller-saved registers to the stack. */
	mips32_save_to_stack(state, cache);

	/* Avoid overlap between the temporary registers and 'naddr'. */
	if (naccessors == naddr) {
		naccessors = 6;
	} else if (nnext_pc == naddr) {
		nnext_pc = 7;
	} else if (npc_addr_hi == naddr) {
		npc_addr_hi = 8;
	} else if (nt1 == naddr) {
		nt1 = 9;
	}

	uint16_t pc_addr_hi;
	int16_t pc_addr_lo;
	mips32_split_mem_ref(&TJ_PC.addr, &pc_addr_hi, &pc_addr_lo);
	mips32_i32(state, naccessors, (uintptr_t) readmemd);
	mips32_i32(state, nnext_pc, state->pc + 4);
	mips32_lui(state, npc_addr_hi, pc_addr_hi);

	/* The memory accessor may raise an exception. Make sure TJ_PC.addr has
	 * the address of the instruction past this one. */
	mips32_sw(state, nnext_pc, pc_addr_lo, npc_addr_hi);
	/* Read into the temporary stack slot. */
	mips32_addiu(state, nt1, REG_STACK, STACK_OFFSET_MEMORY_READ);
	mips32_sw(state, nt1, STATE_OFFSET(read_dest), REG_STATE);
	/* Write the address to be loaded into g_state.access_addr. */
	mips32_sw(state, naddr, STATE_OFFSET(access_addr), REG_STATE);
	mips32_srl(state, nt1, naddr, 16); /* grab the high 16 bits of the address */
	mips32_sll(state, nt1, nt1, 2); /* function pointers are 4 bytes large */
	mips32_addu(state, nt1, naccessors, nt1); /* $8 = &accessors[addr >> 16] */
	mips32_lw(state, REG_PIC_CALL, 0, nt1);
	mips32_jalr(state, REG_PIC_CALL);
	mips32_nop(state);

	/* On return... */

	/* Restore caller-saved registers from the stack. */
	mips32_load_from_stack(state, cache);

	/* If access_addr has become 0, a TLB Refill exception was raised. */
	mips32_lw(state, REG_EMERGENCY, STATE_OFFSET(access_addr), REG_STATE);
	mips32_beq(state, REG_EMERGENCY, 0, +0);
	FAIL_AS(mips32_add_slow_path(state, cache, &mips32_exception_writeback, state->code,
		NULL /* no usual path */, 0 /* no userdata */));
	mips32_nop(state);

	/* Load the new value of Nintendo 64 register ft. The native register
	 * for it was already allocated by the fast path. */
	nft = mips32_alloc_float_in_64(state, cache, ft);

	mips32_bgez(state, 0, +0);
	FAIL_AS(mips32_return_to_usual_path(state, cache, usual_path));
	mips32_ldc1(state, nft, STACK_OFFSET_MEMORY_READ, REG_STATE); /* Delay slot */

	return TJ_SUCCESS;
}

/* Handles emitting the slow path for memory writes which need to go to a C
 * accessor function.
 *
 * The task of this slow path is to:
 * - write all registers back to the Nintendo 64 in case an interrupt is
 *   generated by a write to 0x8430_0010;
 * - set up the value to be written, from native registers assigned to the
 *   opcode's 'rt' (input value) field;
 * - use the N64 address, in native register #userdata, to get the correct
 *   function and call it;
 * - reload all registers;
 * - return to the usual path.
 *
 * Because #userdata may refer to the 'rs' register, if no address offset was
 * used, it may not be rewritten. It may be used by the following instruction.
 */
static enum TJEmitTraceResult write_int_slow(struct mips32_state* state, struct mips32_reg_cache* cache, void* usual_path, uint32_t userdata, uint8_t bytes)
{
	uint8_t naddr = userdata,
	        nt_hi = 0, nt_lo;
	uint8_t rt = RT_OF(state->ops[0]);
	MemFunc* accessors;
	struct mips32_reg_cache cache_copy;

	assert(bytes == 1 || bytes == 2 || bytes == 4 || bytes == 8);

	/* Load the value of Nintendo 64 register rt. The native register(s) for
	 * this were already allocated by the fast path. */
	if (bytes == 8) {
		nt_hi = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rt]);
	}
	nt_lo = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]);

	/* We're about to call something that may require all registers to be
	 * written to memory. Prepare to save absolutely everything. */
	mips32_copy_reg_cache(&cache_copy, cache);
	mips32_free_all_except_ints(state, cache, BIT(naddr) | BIT(nt_hi) | BIT(nt_lo));

	switch (bytes) {
	case 1:  accessors = writememb; break;
	case 2:  accessors = writememh; break;
	case 4:  accessors = writemem;  break;
	case 8:  accessors = writememd; break;
	default: accessors = NULL;      break;
	}

	uint16_t pc_addr_hi;
	int16_t pc_addr_lo;
	mips32_split_mem_ref(&TJ_PC.addr, &pc_addr_hi, &pc_addr_lo);

	/* Because few registers are live here, we may allocate registers in the
	 * 'cache' until 'cache_copy' is regenerated. */
	uint8_t naccessors = mips32_alloc_int_const(state, cache, (uintptr_t) accessors),
	        nnext_pc = mips32_alloc_int_const(state, cache, state->pc + 4),
	        npc_addr_hi = mips32_alloc_int_const(state, cache, (uint32_t) pc_addr_hi << 16),
	        nt1 = mips32_alloc_int_temp(state, cache);

	/* The memory accessor may raise an exception. Make sure TJ_PC.addr has
	 * the address of the instruction past this one. */
	mips32_sw(state, nnext_pc, pc_addr_lo, npc_addr_hi);
	/* Store what's in 'rt'... */
	switch (bytes) {
	case 1:
		mips32_sb(state, nt_lo, STATE_OFFSET(write.b), REG_STATE);
		break;
	case 2:
		mips32_sh(state, nt_lo, STATE_OFFSET(write.h), REG_STATE);
		break;
	case 4:
		mips32_sw(state, nt_lo, STATE_OFFSET(write.w), REG_STATE);
		break;
	case 8:
		mips32_sw(state, nt_lo, STATE_OFFSET(write.d) + LO32_64_BYTES, REG_STATE);
		mips32_sw(state, nt_hi, STATE_OFFSET(write.d) + HI32_64_BYTES, REG_STATE);
		break;
	}
	/* ... at this Nintendo 64 address. */
	mips32_sw(state, naddr, STATE_OFFSET(access_addr), REG_STATE);

	mips32_srl(state, nt1, naddr, 16); /* grab the high 16 bits of the address */
	mips32_sll(state, nt1, nt1, 2); /* function pointers are 4 bytes large */
	mips32_addu(state, nt1, naccessors, nt1); /* $8 = &accessors[addr >> 16] */
	mips32_free_all_except_ints(state, cache, BIT(nt1));
	mips32_lw(state, REG_PIC_CALL, 0, nt1);
	mips32_free_all(state, cache);
	mips32_jalr(state, REG_PIC_CALL);
	mips32_nop(state);

	/* On return... */

	/* Restore all registers from their respective sources. */
	mips32_regenerate_cache(state, &cache_copy);

	/* If the Program Counter is not where we expect, either a TLB Refill
	 * exception was raised, or an interrupt was triggered by a write to
	 * 0x8430_0010. */
	assert(REG_EMERGENCY != 2);
	mips32_sw(state, 2, STACK_OFFSET_TEMP_INT, REG_STACK);
	mips32_lw_abs(state, 2, 2, &TJ_PC.addr);
	mips32_i32(state, REG_EMERGENCY, state->pc + 4);
	mips32_bne(state, REG_EMERGENCY, 2, +0);
	/* In that case, the registers are already written to the Nintendo 64.
	 * We still go to mips32_exception_writeback, but we pass it our empty cache. */
	FAIL_AS(mips32_add_slow_path(state, cache, &mips32_exception_writeback, state->code,
		NULL /* no usual path */, 0 /* no userdata */));
	mips32_lw(state, 2, STACK_OFFSET_TEMP_INT, REG_STACK);

	mips32_bgez(state, 0, +0);
	FAIL_AS(mips32_return_to_usual_path(state, cache, usual_path));
	/* Load the latest value of g_state.access_addr for invalid_code_check, if
	 * applicable. (Delay slot.) */
	mips32_lw(state, naddr, STATE_OFFSET(access_addr), REG_STATE);

	return TJ_SUCCESS;
}

static enum TJEmitTraceResult sb_slow(struct mips32_state* state, struct mips32_reg_cache* cache, void* usual_path, uint32_t userdata)
{
	return write_int_slow(state, cache, usual_path, userdata, 1);
}

static enum TJEmitTraceResult sh_slow(struct mips32_state* state, struct mips32_reg_cache* cache, void* usual_path, uint32_t userdata)
{
	return write_int_slow(state, cache, usual_path, userdata, 2);
}

static enum TJEmitTraceResult sw_slow(struct mips32_state* state, struct mips32_reg_cache* cache, void* usual_path, uint32_t userdata)
{
	return write_int_slow(state, cache, usual_path, userdata, 4);
}

static enum TJEmitTraceResult sd_slow(struct mips32_state* state, struct mips32_reg_cache* cache, void* usual_path, uint32_t userdata)
{
	return write_int_slow(state, cache, usual_path, userdata, 8);
}

/* Handles emitting the slow path for SWC1 instructions which need to go to a
 * C accessor function.
 *
 * The task of this slow path is to:
 * - write all registers back to the Nintendo 64 in case an interrupt is
 *   generated by a write to 0x8430_0010;
 * - set up the value to be written, from the native register assigned to the
 *   opcode's 'ft' (input value) field;
 * - use the N64 address, in native register #userdata, to get the correct
 *   function and call it;
 * - reload all registers;
 * - return to the usual path.
 *
 * Because #userdata may refer to the 'rs' register, if no address offset was
 * used, it may not be rewritten. It may be used by the following instruction.
 */
static enum TJEmitTraceResult swc1_slow(struct mips32_state* state, struct mips32_reg_cache* cache, void* usual_path, uint32_t userdata)
{
	uint8_t naddr = userdata,
	        nft;
	uint8_t ft = FT_OF(state->ops[0]);
	struct mips32_reg_cache cache_copy;

	/* Load the value of Nintendo 64 register ft. The native register for
	 * this was already allocated by the fast path. */
	nft = mips32_alloc_float_in_32(state, cache, ft);

	/* We're about to call something that may require all registers to be
	 * written to memory. Prepare to save absolutely everything. */
	mips32_copy_reg_cache(&cache_copy, cache);
	mips32_free(state, cache,
		UINT32_C(0xFFFFFFFF) & ~BIT(naddr) /* integer registers to free */,
		true /* HI and LO */,
		UINT32_C(0xFFFFFFFF) & ~BIT(nft) /* floating-point registers */);

	uint16_t pc_addr_hi;
	int16_t pc_addr_lo;
	mips32_split_mem_ref(&TJ_PC.addr, &pc_addr_hi, &pc_addr_lo);

	/* Because few registers are live here, we may allocate registers in the
	 * 'cache' until 'cache_copy' is regenerated. */
	uint8_t naccessors = mips32_alloc_int_const(state, cache, (uintptr_t) writemem),
	        nnext_pc = mips32_alloc_int_const(state, cache, state->pc + 4),
	        npc_addr_hi = mips32_alloc_int_const(state, cache, (uint32_t) pc_addr_hi << 16),
	        nt1 = mips32_alloc_int_temp(state, cache);

	/* The memory accessor may raise an exception. Make sure TJ_PC.addr has
	 * the address of the instruction past this one. */
	mips32_sw(state, nnext_pc, pc_addr_lo, npc_addr_hi);
	/* Store what's in 'ft'... */
	mips32_swc1(state, nft, STATE_OFFSET(write.w), REG_STATE);
	/* ... at this Nintendo 64 address. */
	mips32_sw(state, naddr, STATE_OFFSET(access_addr), REG_STATE);

	mips32_srl(state, nt1, naddr, 16); /* grab the high 16 bits of the address */
	mips32_sll(state, nt1, nt1, 2); /* function pointers are 4 bytes large */
	mips32_addu(state, nt1, naccessors, nt1); /* $8 = &accessors[addr >> 16] */
	mips32_free_all_except_ints(state, cache, BIT(nt1));
	mips32_lw(state, REG_PIC_CALL, 0, nt1);
	mips32_free_all(state, cache);
	mips32_jalr(state, REG_PIC_CALL);
	mips32_nop(state);

	/* On return... */

	/* Restore all registers from their respective sources. */
	mips32_regenerate_cache(state, &cache_copy);

	/* If the Program Counter is not where we expect, either a TLB Refill
	 * exception was raised, or an interrupt was triggered by a write to
	 * 0x8430_0010. */
	/* TODO Because memory-mapped IO writes are unlikely through the FPU, make
	 * this a Trace JIT setting */
	assert(REG_EMERGENCY != 2);
	mips32_sw(state, 2, STACK_OFFSET_TEMP_INT, REG_STACK);
	mips32_lw_abs(state, 2, 2, &TJ_PC.addr);
	mips32_i32(state, REG_EMERGENCY, state->pc + 4);
	mips32_bne(state, REG_EMERGENCY, 2, +0);
	/* In that case, the registers are already written to the Nintendo 64.
	 * We still go to mips32_exception_writeback, but we pass it our empty cache. */
	FAIL_AS(mips32_add_slow_path(state, cache, &mips32_exception_writeback, state->code,
		NULL /* no usual path */, 0 /* no userdata */));
	mips32_lw(state, 2, STACK_OFFSET_TEMP_INT, REG_STACK);

	mips32_bgez(state, 0, +0);
	FAIL_AS(mips32_return_to_usual_path(state, cache, usual_path));
	/* Load the latest value of g_state.access_addr for invalid_code_check, if
	 * applicable. (Delay slot.) */
	mips32_lw(state, naddr, STATE_OFFSET(access_addr), REG_STATE);

	return TJ_SUCCESS;
}

/* Handles emitting the slow path for SDC1 instructions which need to go to a
 * C accessor function.
 *
 * The task of this slow path is to:
 * - write all registers back to the Nintendo 64 in case an interrupt is
 *   generated by a write to 0x8430_0010;
 * - set up the value to be written, from the native register assigned to the
 *   opcode's 'ft' (input value) field;
 * - use the N64 address, in native register #userdata, to get the correct
 *   function and call it;
 * - reload all registers;
 * - return to the usual path.
 *
 * Because #userdata may refer to the 'rs' register, if no address offset was
 * used, it may not be rewritten. It may be used by the following instruction.
 */
static enum TJEmitTraceResult sdc1_slow(struct mips32_state* state, struct mips32_reg_cache* cache, void* usual_path, uint32_t userdata)
{
	uint8_t naddr = userdata,
	        nft;
	uint8_t ft = FT_OF(state->ops[0]);
	struct mips32_reg_cache cache_copy;

	/* Load the value of Nintendo 64 register ft. The native register for
	 * this was already allocated by the fast path. */
	nft = mips32_alloc_float_in_64(state, cache, ft);

	/* We're about to call something that may require all registers to be
	 * written to memory. Prepare to save absolutely everything. */
	mips32_copy_reg_cache(&cache_copy, cache);
	mips32_free(state, cache,
		UINT32_C(0xFFFFFFFF) & ~BIT(naddr) /* integer registers to free */,
		true /* HI and LO */,
		UINT32_C(0xFFFFFFFF) & ~BIT(nft) /* floating-point registers */);

	uint16_t pc_addr_hi;
	int16_t pc_addr_lo;
	mips32_split_mem_ref(&TJ_PC.addr, &pc_addr_hi, &pc_addr_lo);

	/* Because few registers are live here, we may allocate registers in the
	 * 'cache' until 'cache_copy' is regenerated. */
	uint8_t naccessors = mips32_alloc_int_const(state, cache, (uintptr_t) writememd),
	        nnext_pc = mips32_alloc_int_const(state, cache, state->pc + 4),
	        npc_addr_hi = mips32_alloc_int_const(state, cache, (uint32_t) pc_addr_hi << 16),
	        nt1 = mips32_alloc_int_temp(state, cache);

	/* The memory accessor may raise an exception. Make sure TJ_PC.addr has
	 * the address of the instruction past this one. */
	mips32_sw(state, nnext_pc, pc_addr_lo, npc_addr_hi);
	/* Store what's in 'ft'... */
	mips32_sdc1(state, nft, STATE_OFFSET(write.d), REG_STATE);
	/* ... at this Nintendo 64 address. */
	mips32_sw(state, naddr, STATE_OFFSET(access_addr), REG_STATE);

	mips32_srl(state, nt1, naddr, 16); /* grab the high 16 bits of the address */
	mips32_sll(state, nt1, nt1, 2); /* function pointers are 4 bytes large */
	mips32_addu(state, nt1, naccessors, nt1); /* $8 = &accessors[addr >> 16] */
	mips32_free_all_except_ints(state, cache, BIT(nt1));
	mips32_lw(state, REG_PIC_CALL, 0, nt1);
	mips32_free_all(state, cache);
	mips32_jalr(state, REG_PIC_CALL);
	mips32_nop(state);

	/* On return... */

	/* Restore all registers from their respective sources. */
	mips32_regenerate_cache(state, &cache_copy);

	/* If the Program Counter is not where we expect, either a TLB Refill
	 * exception was raised, or an interrupt was triggered by a write to
	 * 0x8430_0010. */
	/* TODO Because memory-mapped IO writes are unlikely through the FPU, make
	 * this a Trace JIT setting */
	assert(REG_EMERGENCY != 2);
	mips32_sw(state, 2, STACK_OFFSET_TEMP_INT, REG_STACK);
	mips32_lw_abs(state, 2, 2, &TJ_PC.addr);
	mips32_i32(state, REG_EMERGENCY, state->pc + 4);
	mips32_bne(state, REG_EMERGENCY, 2, +0);
	/* In that case, the registers are already written to the Nintendo 64.
	 * We still go to mips32_exception_writeback, but we pass it our empty cache. */
	FAIL_AS(mips32_add_slow_path(state, cache, &mips32_exception_writeback, state->code,
		NULL /* no usual path */, 0 /* no userdata */));
	mips32_lw(state, 2, STACK_OFFSET_TEMP_INT, REG_STACK);

	mips32_bgez(state, 0, +0);
	FAIL_AS(mips32_return_to_usual_path(state, cache, usual_path));
	/* Load the latest value of g_state.access_addr for invalid_code_check, if
	 * applicable. (Delay slot.) */
	mips32_lw(state, naddr, STATE_OFFSET(access_addr), REG_STATE);

	return TJ_SUCCESS;
}

static enum TJEmitTraceResult emit_accessor_read_int(struct mips32_state* state, struct mips32_reg_cache* cache, MemFunc* accessors)
{
	uint8_t rs = RS_OF(state->ops[0]), rt = RT_OF(state->ops[0]);
	int16_t imm = IMM16S_OF(state->ops[0]);

	uint16_t pc_addr_hi;
	int16_t pc_addr_lo;
	mips32_split_mem_ref(&TJ_PC.addr, &pc_addr_hi, &pc_addr_lo);

	mips32_alloc_specific_int_temp(state, cache, 8);
	uint8_t ns = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
	        naddr,
	        naccessors = mips32_alloc_int_const(state, cache, (uintptr_t) accessors),
	        nnext_pc = mips32_alloc_int_const(state, cache, state->pc + 4),
	        npc_addr_hi = mips32_alloc_int_const(state, cache, (uint32_t) pc_addr_hi << 16);

	/* The memory accessor may raise an exception. Make sure TJ_PC.addr has
	 * the address of the instruction past this one. */
	mips32_sw(state, nnext_pc, pc_addr_lo, npc_addr_hi);
	state->stored_pc = state->pc + 4;
	/* Read into g_state.regs.gpr[rt]. */
	mips32_addiu(state, 8, REG_STATE, STATE_OFFSET(regs.gpr[rt]));
	mips32_sw(state, 8, STATE_OFFSET(read_dest), REG_STATE);
	if (imm != 0) {
		naddr = mips32_alloc_int_temp(state, cache);
		mips32_addiu(state, naddr, ns, imm); /* naddr = ns + imm, don't modify ns */
	} else {
		naddr = ns;
	}
	/* Write the address to be loaded into g_state.access_addr. */
	mips32_sw(state, naddr, STATE_OFFSET(access_addr), REG_STATE);
	mips32_srl(state, 8, naddr, 16); /* grab the high 16 bits of the address */
	mips32_sll(state, 8, 8, 2); /* function pointers are 4 bytes large */
	mips32_addu(state, 8, naccessors, 8); /* $8 = &accessors[addr >> 16] */

	/* Only free caller-saved registers, except $8; let the exception slow
	 * path write callee-saved registers. */
	mips32_free(state, cache, mips32_temp_int_mask & ~BIT(8), true, mips32_temp_float_mask);
	mips32_lw(state, REG_PIC_CALL, 0, 8);
	mips32_jalr(state, REG_PIC_CALL);
	mips32_nop(state);

	/* On return... */
	mips32_alloc_specific_int_temp(state, cache, 8);
	/* If access_addr has become 0, a TLB Refill exception was raised. */
	mips32_lw(state, 8, STATE_OFFSET(access_addr), REG_STATE);
	mips32_beq(state, 8, 0, +0);
	FAIL_AS(mips32_add_slow_path(state, cache, &mips32_exception_writeback, state->code,
		NULL /* no usual path */, 0 /* no userdata */));
	mips32_nop(state);

	/* Nintendo 64 register rt's value is no longer in a native register.
	 * This forces later code to reload it from g_state. */
	mips32_discard_int(cache, &g_state.regs.gpr[rt]);
	return TJ_SUCCESS;
}

/* Emits code to write a value to Nintendo 64 memory and load the new value
 * of g_state.access_addr, as written by the accessor, into a register.
 *
 * 'n64addr_reg' is updated with the number of that register, for
 * invalid_code_check to use.
 */
static enum TJEmitTraceResult emit_accessor_write_int(struct mips32_state* state, struct mips32_reg_cache* cache, MemFunc* accessors, uint8_t* n64addr_reg)
{
	uint8_t rs = RS_OF(state->ops[0]);
	int16_t imm = IMM16S_OF(state->ops[0]);

	uint16_t pc_addr_hi;
	int16_t pc_addr_lo;
	mips32_split_mem_ref(&TJ_PC.addr, &pc_addr_hi, &pc_addr_lo);

	mips32_alloc_specific_int_temp(state, cache, 8);
	uint8_t ns = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
	        naddr,
	        naccessors = mips32_alloc_int_const(state, cache, (uintptr_t) accessors),
	        nnext_pc = mips32_alloc_int_const(state, cache, state->pc + 4),
	        npc_addr_hi = mips32_alloc_int_const(state, cache, (uint32_t) pc_addr_hi << 16);

	/* The memory accessor may raise an exception. Make sure TJ_PC.addr has
	 * the address of the instruction past this one. */
	mips32_sw(state, nnext_pc, pc_addr_lo, npc_addr_hi);
	state->stored_pc = state->pc + 4;
	if (imm != 0) {
		naddr = mips32_alloc_int_temp(state, cache);
		mips32_addiu(state, naddr, ns, imm); /* naddr = ns + imm, don't modify ns */
	} else {
		naddr = ns;
	}
	mips32_sw(state, naddr, STATE_OFFSET(access_addr), REG_STATE);
	mips32_srl(state, 8, naddr, 16); /* grab the high 16 bits of the address */
	mips32_sll(state, 8, 8, 2); /* function pointers are 4 bytes large */
	mips32_addu(state, 8, naccessors, 8); /* $8 = &accessors[addr >> 16] */

	/* If an interrupt is raised after a write to 0x8430_0010, Mupen64plus may
	 * need the values of all registers in order to create a saved state.
	 * Write everything. */
	mips32_free_all_except_ints(state, cache, BIT(8));
	mips32_lw(state, REG_PIC_CALL, 0, 8);
	mips32_jalr(state, REG_PIC_CALL);
	mips32_nop(state);

	/* On return... */
	mips32_alloc_specific_int_temp(state, cache, 8);
	/* If the Program Counter is not where we expect, either a TLB Refill
	 * exception was raised, or an interrupt was triggered by a write to
	 * 0x8430_0010. */
	nnext_pc = mips32_alloc_int_const(state, cache, state->pc + 4);
	npc_addr_hi = mips32_alloc_int_const(state, cache, (uint32_t) pc_addr_hi << 16);
	*n64addr_reg = mips32_alloc_int_temp(state, cache);
	mips32_lw(state, 8, pc_addr_lo, npc_addr_hi);
	mips32_bne(state, 8, nnext_pc, +0);
	FAIL_AS(mips32_add_except_reloc(state));
	mips32_lw(state, *n64addr_reg, STATE_OFFSET(access_addr), REG_STATE);
	return TJ_SUCCESS;
}

static enum TJEmitTraceResult invalid_code_check(struct mips32_state* state, struct mips32_reg_cache* cache, uint8_t n64addr_reg)
{
	uint8_t ntraces = mips32_alloc_int_const(state, cache, (uintptr_t) Traces),
	        ninvalid_code = mips32_alloc_int_const(state, cache, (uintptr_t) invalid_code),
	        n1 = mips32_alloc_int_const(state, cache, 1),
	        nnot_code = mips32_alloc_int_const(state, cache, (uintptr_t) NOT_CODE),
	        nt1 = mips32_alloc_int_temp(state, cache),
	        nt2 = mips32_alloc_int_temp(state, cache);

	mips32_srl(state, nt1, n64addr_reg, N64_PAGE_SHIFT); /* nt1 = addr >> pageshift; */
	mips32_sll(state, nt1, nt1, 2); /* nt1 = (addr >> pageshift) * sizeof(void*) bytes; */
	mips32_addu(state, nt1, ntraces, nt1); /* nt1 = &Traces[...] */
	/* Dereference the pointer to the page of trace pointers. */
	mips32_lw(state, nt1, 0, nt1); /* nt1 = Traces[...] */
	/* If it's NULL, none of the instructions of the page are recompiled. */
	mips32_beq(state, nt1, 0, +7); /* to End */
	/* If there is a page, grab the
	 *   n64addr & (N64_PAGE_SIZE - 1) / N64_INSN_SIZE 'th
	 * trace in it. Traces are pointers, and on MIPS32 those are 4 bytes, the
	 * same size as an N64 instruction. Normally we'd need to divide by 4 and
	 * multiply by 4, but here we can just remove the low 2 bits of
	 * N64_PAGE_SIZE - 1 in the mask of this ANDI. (Delay slot.) */
	mips32_andi(state, nt2, n64addr_reg, (N64_PAGE_SIZE - 1) & ~3);
	mips32_addu(state, nt1, nt1, nt2); /* nt1 = &Traces[...][addr & ...]; */
	mips32_lw(state, nt1, 0, nt1); /* nt1 = Traces[...][addr & ...]; */

	/* If it's NOT_CODE, this specific instruction is not recompiled. */
	mips32_beq(state, nt1, nnot_code, +3); /* to End */
	/* Here, we must invalidate the page.
	 * (The first instruction is inconsequential. BEQ's delay slot.) */
	mips32_srl(state, nt1, n64addr_reg, 12);
	mips32_addu(state, nt1, ninvalid_code, nt1);
	mips32_sb(state, n1, 0, nt1);

	/* Here, no data previously seen as code was modified. */
	/* End: */
	return TJ_SUCCESS;
}

static enum TJEmitTraceResult invalid_code_check_const(struct mips32_state* state, struct mips32_reg_cache* cache, uint32_t n64addr)
{
	uint16_t trace_page_hi, invalid_byte_hi;
	int16_t trace_page_lo, invalid_byte_lo;
	mips32_split_mem_ref(&Traces[n64addr >> N64_PAGE_SHIFT], &trace_page_hi, &trace_page_lo);
	mips32_split_mem_ref(&invalid_code[n64addr >> 12], &invalid_byte_hi, &invalid_byte_lo);
	uint8_t ntrace_page_hi = mips32_alloc_int_const(state, cache, (uint32_t) trace_page_hi << 16),
	        ninvalid_byte_hi = mips32_alloc_int_const(state, cache, (uint32_t) invalid_byte_hi << 16),
	        nnot_code = mips32_alloc_int_const(state, cache, (uintptr_t) NOT_CODE),
	        nt1 = mips32_alloc_int_temp(state, cache);

	/* Dereference the pointer to the page of trace pointers. */
	mips32_lw(state, nt1, trace_page_lo, ntrace_page_hi);
	/* If it's NULL, none of the instructions of the page are recompiled. */
	mips32_beq(state, nt1, 0, +5); /* to End */
	mips32_nop(state);

	/* If there is a page, grab the
	 *   n64addr & (N64_PAGE_SIZE - 1) / N64_INSN_SIZE 'th
	 * trace in it. */
	mips32_lw(state, nt1, (n64addr & (N64_PAGE_SIZE - 1) / N64_INSN_SIZE) * sizeof(TraceEntry), nt1);
	/* If it's NOT_CODE, this specific instruction is not recompiled. */
	mips32_beq(state, nt1, nnot_code, +2); /* to End */
	mips32_ori(state, nt1, 0, 1); /* Delay slot: nt1 = 1, for the store */

	/* Here, we must invalidate the page. */
	mips32_sb(state, nt1, invalid_byte_lo, ninvalid_byte_hi);

	/* Here, no data previously seen as code was modified. */
	/* End: */
	return TJ_SUCCESS;
}

static enum TJEmitTraceResult emit_memory_read_int(struct mips32_state* state, struct mips32_reg_cache* cache, uint8_t bytes, bool sign_extend)
{
	uint8_t rs = RS_OF(state->ops[0]), rt = RT_OF(state->ops[0]);
	int16_t imm = IMM16S_OF(state->ops[0]);

	assert(bytes == 1 || bytes == 2 || bytes == 4 || bytes == 8);

	if (fast_memory && IsConstRDRAMAddress(&state->consts, rs, imm)) {
		uint32_t n64addr = GetConstRDRAMAddress(&state->consts, rs, imm);

		if (bytes == 8) {
			uint16_t addr_loword_hi, addr_hiword_hi;
			int16_t addr_loword_lo, addr_hiword_lo;

			mips32_split_mem_ref((uint8_t*) g_rdram + n64addr + 4,
				&addr_loword_hi, &addr_loword_lo);
			mips32_split_mem_ref((uint8_t*) g_rdram + n64addr,
				&addr_hiword_hi, &addr_hiword_lo);
			uint8_t naddr_loword_hi = mips32_alloc_int_const(state, cache, (uint32_t) addr_loword_hi << 16),
			        naddr_hiword_hi = mips32_alloc_int_const(state, cache, (uint32_t) addr_hiword_hi << 16),
			        nt_hi = mips32_alloc_int_out_hi32(state, cache, &g_state.regs.gpr[rt]),
			        nt_lo = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rt]);
			mips32_lw(state, nt_lo, addr_loword_lo, naddr_loword_hi);
			mips32_lw(state, nt_hi, addr_hiword_lo, naddr_hiword_hi);
		} else {
			uint16_t addr_hi;
			int16_t addr_lo;
			unsigned int endian_adj;

			switch (bytes) {
			case 1:  endian_adj = S8;  break;
			case 2:  endian_adj = S16; break;
			default: endian_adj = 0;   break;
			}

			mips32_split_mem_ref((uint8_t*) g_rdram + (n64addr ^ endian_adj), &addr_hi, &addr_lo);
			uint8_t naddr_hi = mips32_alloc_int_const(state, cache, (uint32_t) addr_hi << 16),
			        nt = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rt]);

			if (bytes == 1 && sign_extend) {
				mips32_lb(state, nt, addr_lo, naddr_hi);
			} else if (bytes == 1 /* && !sign_extend */) {
				mips32_lbu(state, nt, addr_lo, naddr_hi);
			} else if (bytes == 2 && sign_extend) {
				mips32_lh(state, nt, addr_lo, naddr_hi);
			} else if (bytes == 2 /* && !sign_extend */) {
				mips32_lhu(state, nt, addr_lo, naddr_hi);
			} else if (bytes == 4) {
				mips32_lw(state, nt, addr_lo, naddr_hi);
			}

			mips32_set_int_ex32(cache, &g_state.regs.gpr[rt],
				sign_extend ? MRIT_MEM_SE32 : MRIT_MEM_ZE32);
		} /* if (bytes == 8) */
	} /* fast_memory && constant RDRAM address */
	else if (fast_memory) {
		unsigned int endian_adj;
		mips32_slow_path_handler handler;
		uint8_t ns = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
		        naddr,
		        n0xdf80 = mips32_alloc_int_const(state, cache, UINT32_C(0xDF800000)),
		        n0x8000 = mips32_alloc_int_const(state, cache, UINT32_C(0x80000000)),
		        nrdram_addr = mips32_alloc_int_const(state, cache, (uintptr_t) &g_rdram),
		        nt1 = mips32_alloc_int_temp(state, cache),
		        nt_hi = 0,
		        nt_lo;

		switch (bytes) {
		case 1:  endian_adj = S8;  break;
		case 2:  endian_adj = S16; break;
		default: endian_adj = 0;   break;
		}

		if (bytes == 8) {
			nt_hi = mips32_alloc_int_out_hi32(state, cache, &g_state.regs.gpr[rt]);
		}
		nt_lo = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rt]);

		if (imm != 0) {
			naddr = mips32_alloc_int_temp(state, cache);
			mips32_addiu(state, naddr, ns, imm);
		} else {
			naddr = ns;
		}

		/* Now that we have the Nintendo 64 address of the load, we must check
		 * whether it's in RDRAM. RDRAM is at 0x8000_0000..0x807F_FFFF or
		 * 0xA000_0000..0xA07F_FFFF. This can be chcked, at runtime, using the
		 * expression [(addr & 0xDF80_0000) == 0x8000_0000]. */
		mips32_and(state, nt1, naddr, n0xdf80);
		mips32_bne(state, nt1, n0x8000, +0); /* to Slow Path */
		/* If the access is not in RDRAM, this will go to a slow path which
		 * will return after the RDRAM load. */
		void* slow_path_source = state->code;
		/* Now, as we expect accesses to RDRAM to be much more common, we
		 * prepare for one right away. (BNE's delay slot.) */
		if (endian_adj != 0) {
			mips32_xori(state, nt1, naddr, endian_adj);
			mips32_sll(state, nt1, nt1, 9);
		} else {
			mips32_sll(state, nt1, naddr, 9);
		}
		mips32_srl(state, nt1, nt1, 9); /* nt1 = naddr & 0x7FFFFF; */
		mips32_addu(state, nt1, nrdram_addr, nt1); /* nt1 = g_rdram + ... bytes; */

		if (bytes == 1 && sign_extend) {
			handler = &lb_slow;
			mips32_lb(state, nt_lo, 0, nt1);
			mips32_set_int_se32(cache, &g_state.regs.gpr[rt]);
		} else if (bytes == 1 /* && !sign_extend */) {
			handler = &lbu_slow;
			mips32_lbu(state, nt_lo, 0, nt1);
			mips32_set_int_ze32(cache, &g_state.regs.gpr[rt]);
		} else if (bytes == 2 && sign_extend) {
			handler = &lh_slow;
			mips32_lh(state, nt_lo, 0, nt1);
			mips32_set_int_se32(cache, &g_state.regs.gpr[rt]);
		} else if (bytes == 2 /* && !sign_extend */) {
			handler = &lhu_slow;
			mips32_lhu(state, nt_lo, 0, nt1);
			mips32_set_int_ze32(cache, &g_state.regs.gpr[rt]);
		} else if (bytes == 4 && sign_extend) {
			handler = &lw_slow;
			mips32_lw(state, nt_lo, 0, nt1);
			mips32_set_int_se32(cache, &g_state.regs.gpr[rt]);
		} else if (bytes == 4 /* && !sign_extend */) {
			handler = &lwu_slow;
			mips32_lw(state, nt_lo, 0, nt1);
			mips32_set_int_ze32(cache, &g_state.regs.gpr[rt]);
		} else /* if (bytes == 8) */ {
			handler = &ld_slow;
			mips32_lw(state, nt_lo, 4, nt1);
			mips32_lw(state, nt_hi, 0, nt1);
		}
		FAIL_AS(mips32_add_slow_path(state, cache, handler, slow_path_source,
			state->code /* return to usual path */,
			naddr /* userdata: tell it which reg contains the N64 address */));
	} /* fast_memory && !constant RDRAM address */
	else {
		MemFunc* accessors;

		switch (bytes) {
		case 1:  accessors = readmemb; break;
		case 2:  accessors = readmemh; break;
		case 4:  accessors = readmem;  break;
		case 8:  accessors = readmemd; break;
		default: accessors = NULL;     break;
		}

		FAIL_AS(emit_accessor_read_int(state, cache, accessors));

		/* Mandatory reloads and fixups for sign-extensions */
		if (sign_extend) {
			uint8_t nt = mips32_alloc_int_out_lo32(state, cache, &g_state.regs.gpr[rt]);
			
			if (bytes == 1) {
				mips32_lb(state, nt, STATE_OFFSET(regs.gpr[rt]) + LO8_64_BYTES, REG_STATE);
			} else if (bytes == 2) {
				mips32_lh(state, nt, STATE_OFFSET(regs.gpr[rt]) + LO16_64_BYTES, REG_STATE);
			} else if (bytes == 4) {
				mips32_lw(state, nt, STATE_OFFSET(regs.gpr[rt]) + LO32_64_BYTES, REG_STATE);
			}

			mips32_set_int_se32(cache, &g_state.regs.gpr[rt]);
		}
	} /* !fast_memory */
	return TJ_SUCCESS;
}

static enum TJEmitTraceResult emit_memory_write_int(struct mips32_state* state, struct mips32_reg_cache* cache, uint8_t bytes)
{
	uint8_t rs = RS_OF(state->ops[0]), rt = RT_OF(state->ops[0]);
	int16_t imm = IMM16S_OF(state->ops[0]);
	bool check_invalid_code = false;

	assert(bytes == 1 || bytes == 2 || bytes == 4 || bytes == 8);

	if (bytes == 1) {
		check_invalid_code = TraceJITSettings.SBCanModifyCode;
	} else if (bytes == 2) {
		check_invalid_code = TraceJITSettings.SHCanModifyCode;
	} else if (bytes == 4) {
		check_invalid_code = true;
	} else if (bytes == 8) {
		check_invalid_code = TraceJITSettings.SDCanModifyCode;
	}

	if (fast_memory && IsConstRDRAMAddress(&state->consts, rs, imm)) {
		uint32_t n64addr = GetConstRDRAMAddress(&state->consts, rs, imm);

		if (bytes == 8) {
			uint16_t addr_loword_hi, addr_hiword_hi;
			int16_t addr_loword_lo, addr_hiword_lo;

			mips32_split_mem_ref((uint8_t*) g_rdram + n64addr + 4,
				&addr_loword_hi, &addr_loword_lo);
			mips32_split_mem_ref((uint8_t*) g_rdram + n64addr,
				&addr_hiword_hi, &addr_hiword_lo);
			uint8_t naddr_loword_hi = mips32_alloc_int_const(state, cache, (uint32_t) addr_loword_hi << 16),
			        naddr_hiword_hi = mips32_alloc_int_const(state, cache, (uint32_t) addr_hiword_hi << 16),
			        nt_hi = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rt]),
			        nt_lo = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]);
			mips32_sw(state, nt_lo, addr_loword_lo, naddr_loword_hi);
			mips32_sw(state, nt_hi, addr_hiword_lo, naddr_hiword_hi);
		} else {
			uint16_t addr_hi;
			int16_t addr_lo;
			unsigned int endian_adj;

			switch (bytes) {
			case 1:  endian_adj = S8;  break;
			case 2:  endian_adj = S16; break;
			default: endian_adj = 0;   break;
			}

			mips32_split_mem_ref((uint8_t*) g_rdram + (n64addr ^ endian_adj), &addr_hi, &addr_lo);
			uint8_t naddr_hi = mips32_alloc_int_const(state, cache, (uint32_t) addr_hi << 16),
			        nt = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]);

			if (bytes == 1) {
				mips32_sb(state, nt, addr_lo, naddr_hi);
			} else if (bytes == 2) {
				mips32_sh(state, nt, addr_lo, naddr_hi);
			} else if (bytes == 4) {
				mips32_sw(state, nt, addr_lo, naddr_hi);
			}
		} /* if (bytes == 8) */

		if (check_invalid_code) {
			invalid_code_check_const(state, cache, UINT32_C(0x80000000) | n64addr);
		}
	} /* fast_memory && constant RDRAM address */
	else if (fast_memory) {
		unsigned int endian_adj;
		mips32_slow_path_handler handler;
		uint8_t ns = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
		        naddr = mips32_alloc_int_temp(state, cache),
		        n0xdf80 = mips32_alloc_int_const(state, cache, UINT32_C(0xDF800000)),
		        n0x8000 = mips32_alloc_int_const(state, cache, UINT32_C(0x80000000)),
		        nrdram_addr = mips32_alloc_int_const(state, cache, (uintptr_t) &g_rdram),
		        nt1 = mips32_alloc_int_temp(state, cache),
		        nt_hi = 0,
		        nt_lo;

		switch (bytes) {
		case 1:  endian_adj = S8;  break;
		case 2:  endian_adj = S16; break;
		default: endian_adj = 0;   break;
		}

		if (bytes == 8) {
			nt_hi = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rt]);
		}
		nt_lo = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]);

		if (imm != 0) {
			mips32_addiu(state, naddr, ns, imm);
		} else {
			/* This is a separate register to avoid 'rs' being modified, while
			 * allowing the new value of g_state.access_addr to be loaded into
			 * it by the slow path of stores before the code rewrite check. */
			mips32_or(state, naddr, ns, 0);
		}

		/* Now that we have the Nintendo 64 address of the store, we must
		 * check whether it's in RDRAM. RDRAM is at 0x8000_0000..0x807F_FFFF
		 * of 0xA000_0000..0xA07F_FFFF. This can be chcked, at runtime, using
		 * the expression [(addr & 0xDF80_0000) == 0x8000_0000]. */
		mips32_and(state, nt1, naddr, n0xdf80);
		mips32_bne(state, nt1, n0x8000, +0); /* to Slow Path */
		/* If the access is not in RDRAM, this will go to a slow path which
		 * will return after the RDRAM store. */
		void* slow_path_source = state->code;
		/* Now, as we expect accesses to RDRAM to be much more common, we
		 * prepare for one right away. (BNE's delay slot.) */
		if (endian_adj != 0) {
			mips32_xori(state, nt1, naddr, endian_adj);
			mips32_sll(state, nt1, nt1, 9);
		} else {
			mips32_sll(state, nt1, naddr, 9);
		}
		mips32_srl(state, nt1, nt1, 9); /* nt1 = naddr & 0x7FFFFF; */
		mips32_addu(state, nt1, nrdram_addr, nt1); /* nt1 = g_rdram + ... bytes; */

		if (bytes == 1) {
			handler = &sb_slow;
			mips32_sb(state, nt_lo, 0, nt1);
		} else if (bytes == 2) {
			handler = &sh_slow;
			mips32_sh(state, nt_lo, 0, nt1);
		} else if (bytes == 4) {
			handler = &sw_slow;
			mips32_sw(state, nt_lo, 0, nt1);
		} else /* if (bytes == 8) */ {
			handler = &sd_slow;
			mips32_sw(state, nt_lo, 4, nt1);
			mips32_sw(state, nt_hi, 0, nt1);
		}
		FAIL_AS(mips32_add_slow_path(state, cache, handler, slow_path_source,
			state->code /* return to usual path */,
			naddr /* userdata: tell it which reg contains the N64 address */));

		if (check_invalid_code) {
			FAIL_AS(invalid_code_check(state, cache, naddr));
		}
	} /* fast_memory && !constant RDRAM address */
	else {
		MemFunc* accessors;
		uint8_t n64addr_reg;
		check_invalid_code = check_invalid_code /* can SB, SH, SD modify code? */ &&
			(rs != 29 || TraceJITSettings.StackCanModifyCode);

		switch (bytes) {
			case 1:
			{
				uint8_t nt = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]);
				mips32_sb(state, nt, STATE_OFFSET(write.b), REG_STATE);
				accessors = writememb;
				break;
			}
			case 2:
			{
				uint8_t nt = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]);
				mips32_sh(state, nt, STATE_OFFSET(write.h), REG_STATE);
				accessors = writememh;
				break;
			}
			case 4:
			{
				uint8_t nt = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]);
				mips32_sw(state, nt, STATE_OFFSET(write.w), REG_STATE);
				accessors = writemem;
				break;
			}
			case 8:
			{
				uint8_t nt_hi = mips32_alloc_int_in_hi32(state, cache, &g_state.regs.gpr[rt]),
				        nt_lo = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rt]);
				mips32_sw(state, nt_lo, STATE_OFFSET(write.d) + LO32_64_BYTES, REG_STATE);
				mips32_sw(state, nt_hi, STATE_OFFSET(write.d) + HI32_64_BYTES, REG_STATE);
				accessors = writememd;
				break;
			}
		}

		FAIL_AS(emit_accessor_write_int(state, cache, accessors, &n64addr_reg));

		if (check_invalid_code) {
			FAIL_AS(invalid_code_check(state, cache, n64addr_reg));
		}
	} /* !fast_memory */
	return TJ_SUCCESS;
}

/* - - - PUBLIC FUNCTIONS - - - */

enum TJEmitTraceResult mips32_emit_ldl(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_LDL, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
}

enum TJEmitTraceResult mips32_emit_ldr(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_LDR, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
}

enum TJEmitTraceResult mips32_emit_lb(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretLB) {
		return mips32_emit_interpret(state, cache, &TJ_LB, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
	} else {
		mips32_start_opcode(state, cache, "LB");
		FAIL_AS(emit_memory_read_int(state, cache, 1, true));
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_lh(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretLH) {
		return mips32_emit_interpret(state, cache, &TJ_LH, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
	} else {
		mips32_start_opcode(state, cache, "LH");
		FAIL_AS(emit_memory_read_int(state, cache, 2, true));
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_lwl(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_LWL, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
}

enum TJEmitTraceResult mips32_emit_lw(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretLW) {
		return mips32_emit_interpret(state, cache, &TJ_LW, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
	} else {
		mips32_start_opcode(state, cache, "LW");
		FAIL_AS(emit_memory_read_int(state, cache, 4, true));
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_lbu(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretLBU) {
		return mips32_emit_interpret(state, cache, &TJ_LBU, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
	} else {
		mips32_start_opcode(state, cache, "LBU");
		FAIL_AS(emit_memory_read_int(state, cache, 1, false));
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_lhu(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretLHU) {
		return mips32_emit_interpret(state, cache, &TJ_LHU, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
	} else {
		mips32_start_opcode(state, cache, "LHU");
		FAIL_AS(emit_memory_read_int(state, cache, 2, false));
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_lwr(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_LWR, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
}

enum TJEmitTraceResult mips32_emit_lwu(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretLWU) {
		return mips32_emit_interpret(state, cache, &TJ_LWU, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
	} else {
		mips32_start_opcode(state, cache, "LWU");
		FAIL_AS(emit_memory_read_int(state, cache, 4, false));
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_sb(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretSB) {
		return mips32_emit_interpret(state, cache, &TJ_SB, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
	} else {
		mips32_start_opcode(state, cache, "SB");
		FAIL_AS(emit_memory_write_int(state, cache, 1));
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_sh(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretSH) {
		return mips32_emit_interpret(state, cache, &TJ_SH, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
	} else {
		mips32_start_opcode(state, cache, "SH");
		FAIL_AS(emit_memory_write_int(state, cache, 2));
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_swl(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_SWL, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
}

enum TJEmitTraceResult mips32_emit_sw(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretSW) {
		return mips32_emit_interpret(state, cache, &TJ_SW, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
	} else {
		mips32_start_opcode(state, cache, "SW");
		FAIL_AS(emit_memory_write_int(state, cache, 4));
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_sdl(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_SDL, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
}

enum TJEmitTraceResult mips32_emit_sdr(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_SDR, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
}

enum TJEmitTraceResult mips32_emit_swr(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_SWR, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
}

enum TJEmitTraceResult mips32_emit_ll(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_LL, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
}

enum TJEmitTraceResult mips32_emit_lwc1(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretLWC1) {
		return mips32_emit_interpret(state, cache, &TJ_LWC1, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
	} else {
		uint8_t rs = RS_OF(state->ops[0]), ft = FT_OF(state->ops[0]);
		int16_t imm = IMM16S_OF(state->ops[0]);

		mips32_start_opcode(state, cache, "LWC1");
		if (fast_memory && IsConstRDRAMAddress(&state->consts, rs, imm)) {
			uint32_t n64addr = GetConstRDRAMAddress(&state->consts, rs, imm);
			uint16_t addr_hi;
			int16_t addr_lo;

			mips32_split_mem_ref((uint8_t*) g_rdram + n64addr, &addr_hi, &addr_lo);
			uint8_t naddr_hi = mips32_alloc_int_const(state, cache, (uint32_t) addr_hi << 16),
			        nft = mips32_alloc_float_out_32(state, cache, ft);
			mips32_lwc1(state, nft, addr_lo, naddr_hi);
		} /* fast_memory && constant RDRAM address */
		else if (fast_memory) {
			uint8_t ns = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
			        naddr,
			        n0xdf80 = mips32_alloc_int_const(state, cache, UINT32_C(0xDF800000)),
			        n0x8000 = mips32_alloc_int_const(state, cache, UINT32_C(0x80000000)),
			        nrdram_addr = mips32_alloc_int_const(state, cache, (uintptr_t) &g_rdram),
			        nt1 = mips32_alloc_int_temp(state, cache),
			        nft = mips32_alloc_float_out_32(state, cache, ft);

			if (imm != 0) {
				naddr = mips32_alloc_int_temp(state, cache);
				mips32_addiu(state, naddr, ns, imm);
			} else {
				naddr = ns;
			}

			/* Now that we have the Nintendo 64 address of the load, we must check
			 * whether it's in RDRAM. RDRAM is at 0x8000_0000..0x807F_FFFF or
			 * 0xA000_0000..0xA07F_FFFF. This can be chcked, at runtime, using the
			 * expression [(addr & 0xDF80_0000) == 0x8000_0000]. */
			mips32_and(state, nt1, naddr, n0xdf80);
			mips32_bne(state, nt1, n0x8000, +0); /* to Slow Path */
			/* If the access is not in RDRAM, this will go to a slow path which
			 * will return after the RDRAM load. */
			void* slow_path_source = state->code;
			/* Now, as we expect accesses to RDRAM to be much more common, we
			 * prepare for one right away. (BNE's delay slot.) */
			mips32_sll(state, nt1, naddr, 9);
			mips32_srl(state, nt1, nt1, 9); /* nt1 = naddr & 0x7FFFFF; */
			mips32_addu(state, nt1, nrdram_addr, nt1); /* nt1 = g_rdram + ... bytes; */
			mips32_lwc1(state, nft, 0, nt1);

			FAIL_AS(mips32_add_slow_path(state, cache, &lwc1_slow, slow_path_source,
				state->code /* return to usual path */,
				naddr /* userdata: tell it which reg contains the N64 address */));
		} /* fast_memory && !constant RDRAM address */
		else {
			uint16_t pc_addr_hi;
			int16_t pc_addr_lo;
			mips32_split_mem_ref(&TJ_PC.addr, &pc_addr_hi, &pc_addr_lo);

			mips32_alloc_specific_int_temp(state, cache, 8);
			uint8_t ns = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
			        naddr,
			        naccessors = mips32_alloc_int_const(state, cache, (uintptr_t) readmem),
			        nnext_pc = mips32_alloc_int_const(state, cache, state->pc + 4),
			        npc_addr_hi = mips32_alloc_int_const(state, cache, (uint32_t) pc_addr_hi << 16);

			/* The memory accessor may raise an exception. Make sure TJ_PC.addr has
			 * the address of the instruction past this one. */
			mips32_sw(state, nnext_pc, pc_addr_lo, npc_addr_hi);
			state->stored_pc = state->pc + 4;
			/* Read into the temporary stack slot allocated by mips32_jit_entry.
			 * Memory accessors write 64 bits, and we don't want to load 64 bits,
			 * only the 32 bits of a float value. Store the address of the slot
			 * into read_dest. */
			mips32_addiu(state, 8, REG_STACK, STACK_OFFSET_MEMORY_READ);
			mips32_sw(state, 8, STATE_OFFSET(read_dest), REG_STATE);
			if (imm != 0) {
				naddr = mips32_alloc_int_temp(state, cache);
				mips32_addiu(state, naddr, ns, imm); /* naddr = ns + imm, don't modify ns */
			} else {
				naddr = ns;
			}
			/* Write the address to be loaded into g_state.access_addr. */
			mips32_sw(state, naddr, STATE_OFFSET(access_addr), REG_STATE);
			mips32_srl(state, 8, naddr, 16); /* grab the high 16 bits of the address */
			mips32_sll(state, 8, 8, 2); /* function pointers are 4 bytes large */
			mips32_addu(state, 8, naccessors, 8); /* $8 = &accessors[addr >> 16] */

			/* Only free caller-saved registers, except $8; let the exception slow
			 * path write callee-saved registers.
			 * Registers assigned to hold ft will no longer have the most recent
			 * value, either. */
			mips32_free(state, cache, mips32_temp_int_mask & ~BIT(8), true, mips32_temp_float_mask);
			mips32_lw(state, REG_PIC_CALL, 0, 8);
			mips32_jalr(state, REG_PIC_CALL);
			mips32_nop(state);

			/* On return... */
			mips32_alloc_specific_int_temp(state, cache, 8);
			/* If access_addr has become 0, a TLB Refill exception was raised. */
			mips32_lw(state, 8, STATE_OFFSET(access_addr), REG_STATE);
			mips32_beq(state, 8, 0, +0);
			FAIL_AS(mips32_add_slow_path(state, cache, &mips32_exception_writeback, state->code,
				NULL /* no usual path */, 0 /* no userdata */));
			mips32_nop(state);

			mips32_discard_float(cache, ft);
			/* Post-processing: Copy the result to the FPR. */
			uint8_t nft = mips32_alloc_float_out_32(state, cache, ft);
			mips32_lwc1(state, nft, STACK_OFFSET_MEMORY_READ, REG_STACK);
		} /* !fast_memory */
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_ldc1(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretLDC1) {
		return mips32_emit_interpret(state, cache, &TJ_LDC1, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
	} else {
		uint8_t rs = RS_OF(state->ops[0]), ft = FT_OF(state->ops[0]);
		int16_t imm = IMM16S_OF(state->ops[0]);

		mips32_start_opcode(state, cache, "LDC1");
		if (fast_memory && IsConstRDRAMAddress(&state->consts, rs, imm)) {
			uint32_t n64addr = GetConstRDRAMAddress(&state->consts, rs, imm);
			uint16_t addr_loword_hi, addr_hiword_hi;
			int16_t addr_loword_lo, addr_hiword_lo;

			mips32_split_mem_ref((uint8_t*) g_rdram + n64addr + 4,
				&addr_loword_hi, &addr_loword_lo);
			mips32_split_mem_ref((uint8_t*) g_rdram + n64addr,
				&addr_hiword_hi, &addr_hiword_lo);
			uint8_t naddr_loword_hi = mips32_alloc_int_const(state, cache, (uint32_t) addr_loword_hi << 16),
			        naddr_hiword_hi = mips32_alloc_int_const(state, cache, (uint32_t) addr_hiword_hi << 16),
			        nft = mips32_alloc_float_out_64(state, cache, ft),
			        nt1 = mips32_alloc_int_temp(state, cache),
			        nt2 = mips32_alloc_int_temp(state, cache);
			/* LDC1 is recompiled like this because the native-side MIPS FPU
			 * is essentially incompatible with itself.
			 *
			 * In Mupen64plus, RDRAM is swapped to the native endian. However,
			 * this swap only goes up to 32-bit quantities.
			 *
			 * Thus, the memory bytes required to satisfy LDC1 are as follows:
			 * - Little-endian: [4 5 6 7 0 1 2 3]
			 * - Big-endian:    [0 1 2 3 4 5 6 7]
			 *
			 * On little-endian systems, LDC1 must be satisfied by two loads,
			 * which exposes the same problem as DMTC1. See the comment for
			 * DMTC1 in mips-cp1.c.
			*/
			mips32_lw(state, nt1, addr_loword_lo, naddr_loword_hi);
			mips32_lw(state, nt2, addr_hiword_lo, naddr_hiword_hi);
			mips32_sw(state, nt1, STACK_OFFSET_MEMORY_READ + LO32_64_BYTES, REG_STACK);
			mips32_sw(state, nt2, STACK_OFFSET_MEMORY_READ + HI32_64_BYTES, REG_STACK);
			mips32_ldc1(state, nft, STACK_OFFSET_MEMORY_READ, REG_STACK);
		} /* fast_memory && constant RDRAM address */
		else if (fast_memory) {
			uint8_t ns = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
			        naddr,
			        n0xdf80 = mips32_alloc_int_const(state, cache, UINT32_C(0xDF800000)),
			        n0x8000 = mips32_alloc_int_const(state, cache, UINT32_C(0x80000000)),
			        nrdram_addr = mips32_alloc_int_const(state, cache, (uintptr_t) &g_rdram),
			        nt1 = mips32_alloc_int_temp(state, cache),
			        nt2 = mips32_alloc_int_temp(state, cache),
			        nft = mips32_alloc_float_out_64(state, cache, ft);

			if (imm != 0) {
				naddr = mips32_alloc_int_temp(state, cache);
				mips32_addiu(state, naddr, ns, imm);
			} else {
				naddr = ns;
			}

			/* Now that we have the Nintendo 64 address of the load, we must check
			 * whether it's in RDRAM. RDRAM is at 0x8000_0000..0x807F_FFFF or
			 * 0xA000_0000..0xA07F_FFFF. This can be chcked, at runtime, using the
			 * expression [(addr & 0xDF80_0000) == 0x8000_0000]. */
			mips32_and(state, nt1, naddr, n0xdf80);
			mips32_bne(state, nt1, n0x8000, +0); /* to Slow Path */
			/* If the access is not in RDRAM, this will go to a slow path which
			 * will return after the RDRAM load. */
			void* slow_path_source = state->code;
			/* Now, as we expect accesses to RDRAM to be much more common, we
			 * prepare for one right away. (BNE's delay slot.) */
			mips32_sll(state, nt1, naddr, 9);
			mips32_srl(state, nt1, nt1, 9); /* nt1 = naddr & 0x7FFFFF; */
			mips32_addu(state, nt1, nrdram_addr, nt1); /* nt1 = g_rdram + ... bytes; */
			mips32_lw(state, nt2, 0, nt1); /* high part */
			mips32_lw(state, nt1, 4, nt1); /* low part */
			mips32_sw(state, nt1, STACK_OFFSET_MEMORY_READ + LO32_64_BYTES, REG_STACK);
			mips32_sw(state, nt2, STACK_OFFSET_MEMORY_READ + HI32_64_BYTES, REG_STACK);
			mips32_ldc1(state, nft, STACK_OFFSET_MEMORY_READ, REG_STACK);

			FAIL_AS(mips32_add_slow_path(state, cache, &ldc1_slow, slow_path_source,
				state->code /* return to usual path */,
				naddr /* userdata: tell it which reg contains the N64 address */));
		} /* fast_memory && !constant RDRAM address */
		else {
			uint16_t pc_addr_hi;
			int16_t pc_addr_lo;
			mips32_split_mem_ref(&TJ_PC.addr, &pc_addr_hi, &pc_addr_lo);

			mips32_alloc_specific_int_temp(state, cache, 8);
			uint8_t ns = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
			        naddr,
			        naccessors = mips32_alloc_int_const(state, cache, (uintptr_t) readmemd),
			        nnext_pc = mips32_alloc_int_const(state, cache, state->pc + 4),
			        npc_addr_hi = mips32_alloc_int_const(state, cache, (uint32_t) pc_addr_hi << 16),
			        nft_addr = mips32_alloc_int_in_32(state, cache, &g_state.regs.cp1_d[ft]);

			/* The memory accessor may raise an exception. Make sure TJ_PC.addr has
			 * the address of the instruction past this one. */
			mips32_sw(state, nnext_pc, pc_addr_lo, npc_addr_hi);
			state->stored_pc = state->pc + 4;
			/* Read into *g_state.regs.cp1_d[ft]. */
			mips32_sw(state, nft_addr, STATE_OFFSET(read_dest), REG_STATE);
			if (imm != 0) {
				naddr = mips32_alloc_int_temp(state, cache);
				mips32_addiu(state, naddr, ns, imm); /* naddr = ns + imm, don't modify ns */
			} else {
				naddr = ns;
			}
			/* Write the address to be loaded into g_state.access_addr. */
			mips32_sw(state, naddr, STATE_OFFSET(access_addr), REG_STATE);
			mips32_srl(state, 8, naddr, 16); /* grab the high 16 bits of the address */
			mips32_sll(state, 8, 8, 2); /* function pointers are 4 bytes large */
			mips32_addu(state, 8, naccessors, 8); /* $8 = &accessors[addr >> 16] */

			/* Only free caller-saved registers, except $8; let the exception slow
			 * path write callee-saved registers.
			 * Registers assigned to hold ft will no longer have the most recent
			 * value, either. */
			mips32_free(state, cache, mips32_temp_int_mask & ~BIT(8), true, mips32_temp_float_mask);
			mips32_lw(state, REG_PIC_CALL, 0, 8);
			mips32_jalr(state, REG_PIC_CALL);
			mips32_nop(state);

			/* On return... */
			mips32_alloc_specific_int_temp(state, cache, 8);
			/* If access_addr has become 0, a TLB Refill exception was raised. */
			mips32_lw(state, 8, STATE_OFFSET(access_addr), REG_STATE);
			mips32_beq(state, 8, 0, +0);
			FAIL_AS(mips32_add_slow_path(state, cache, &mips32_exception_writeback, state->code,
				NULL /* no usual path */, 0 /* no userdata */));
			mips32_nop(state);

			/* Native registers no longer contain the latest version of ft. */
			mips32_discard_float(cache, ft);
		} /* !fast_memory */
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_ld(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretLD) {
		return mips32_emit_interpret(state, cache, &TJ_LD, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
	} else {
		mips32_start_opcode(state, cache, "LD");
		FAIL_AS(emit_memory_read_int(state, cache, 8, false));
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_sc(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	return mips32_emit_interpret(state, cache, &TJ_SC, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
}

enum TJEmitTraceResult mips32_emit_swc1(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretSWC1) {
		return mips32_emit_interpret(state, cache, &TJ_SWC1, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
	} else {
		uint8_t rs = RS_OF(state->ops[0]), ft = FT_OF(state->ops[0]);
		int16_t imm = IMM16S_OF(state->ops[0]);

		mips32_start_opcode(state, cache, "SWC1");
		if (fast_memory && IsConstRDRAMAddress(&state->consts, rs, imm)) {
			uint32_t n64addr = GetConstRDRAMAddress(&state->consts, rs, imm);
			uint16_t addr_hi;
			int16_t addr_lo;

			mips32_split_mem_ref((uint8_t*) g_rdram + n64addr, &addr_hi, &addr_lo);
			uint8_t naddr_hi = mips32_alloc_int_const(state, cache, (uint32_t) addr_hi << 16),
			        nft = mips32_alloc_float_in_32(state, cache, ft);
			mips32_swc1(state, nft, addr_lo, naddr_hi);
			if (TraceJITSettings.Cop1CanModifyCode) {
				invalid_code_check_const(state, cache, UINT32_C(0x80000000) | n64addr);
			}
		} /* fast_memory && constant RDRAM address */
		else if (fast_memory) {
			uint8_t ns = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
			        naddr = mips32_alloc_int_temp(state, cache),
			        n0xdf80 = mips32_alloc_int_const(state, cache, UINT32_C(0xDF800000)),
			        n0x8000 = mips32_alloc_int_const(state, cache, UINT32_C(0x80000000)),
			        nrdram_addr = mips32_alloc_int_const(state, cache, (uintptr_t) &g_rdram),
			        nt1 = mips32_alloc_int_temp(state, cache),
			        nft = mips32_alloc_float_in_32(state, cache, ft);

			if (imm != 0) {
				mips32_addiu(state, naddr, ns, imm);
			} else {
				/* This is a separate register to avoid 'rs' being modified, while
				 * allowing the new value of g_state.access_addr to be loaded into
				 * it by the slow path before the code rewrite check. */
				mips32_or(state, naddr, ns, 0);
			}

			/* Now that we have the Nintendo 64 address of the store, we must
			 * check whether it's in RDRAM. RDRAM is at 0x8000_0000..0x807F_FFFF
			 * of 0xA000_0000..0xA07F_FFFF. This can be chcked, at runtime, using
			 * the expression [(addr & 0xDF80_0000) == 0x8000_0000]. */
			mips32_and(state, nt1, naddr, n0xdf80);
			mips32_bne(state, nt1, n0x8000, +0); /* to Slow Path */
			/* If the access is not in RDRAM, this will go to a slow path which
			 * will return after the RDRAM store. */
			void* slow_path_source = state->code;
			/* Now, as we expect accesses to RDRAM to be much more common, we
			 * prepare for one right away. (BNE's delay slot.) */
			mips32_sll(state, nt1, naddr, 9);
			mips32_srl(state, nt1, nt1, 9); /* nt1 = naddr & 0x7FFFFF; */
			mips32_addu(state, nt1, nrdram_addr, nt1); /* nt1 = g_rdram + ... bytes; */

			mips32_swc1(state, nft, 0, nt1);
			FAIL_AS(mips32_add_slow_path(state, cache, &swc1_slow, slow_path_source,
				state->code /* return to usual path */,
				naddr /* userdata: tell it which reg contains the N64 address */));

			if (TraceJITSettings.Cop1CanModifyCode) {
				FAIL_AS(invalid_code_check(state, cache, naddr));
			}
		} /* fast_memory && !constant RDRAM address */
		else {
			uint8_t nft = mips32_alloc_float_in_32(state, cache, ft),
			        n64addr_reg;
			mips32_swc1(state, nft, STATE_OFFSET(write.w), REG_STATE);
			FAIL_AS(emit_accessor_write_int(state, cache, writemem, &n64addr_reg));
			if (TraceJITSettings.Cop1CanModifyCode) {
				FAIL_AS(invalid_code_check(state, cache, n64addr_reg));
			}
		} /* !fast_memory */
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_sdc1(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretSDC1) {
		return mips32_emit_interpret(state, cache, &TJ_SDC1, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
	} else {
		uint8_t rs = RS_OF(state->ops[0]), ft = FT_OF(state->ops[0]);
		int16_t imm = IMM16S_OF(state->ops[0]);

		mips32_start_opcode(state, cache, "SDC1");
		if (fast_memory && IsConstRDRAMAddress(&state->consts, rs, imm)) {
			uint32_t n64addr = GetConstRDRAMAddress(&state->consts, rs, imm);
			uint16_t addr_loword_hi, addr_hiword_hi;
			int16_t addr_loword_lo, addr_hiword_lo;

			mips32_split_mem_ref((uint8_t*) g_rdram + n64addr + 4,
				&addr_loword_hi, &addr_loword_lo);
			mips32_split_mem_ref((uint8_t*) g_rdram + n64addr,
				&addr_hiword_hi, &addr_hiword_lo);
			uint8_t naddr_loword_hi = mips32_alloc_int_const(state, cache, (uint32_t) addr_loword_hi << 16),
			        naddr_hiword_hi = mips32_alloc_int_const(state, cache, (uint32_t) addr_hiword_hi << 16),
			        nft = mips32_alloc_float_in_64(state, cache, ft),
			        nt1 = mips32_alloc_int_temp(state, cache),
			        nt2 = mips32_alloc_int_temp(state, cache);
			/* SDC1 is recompiled like this because the native-side MIPS FPU
			 * is essentially incompatible with itself.
			 *
			 * In Mupen64plus, RDRAM is swapped to the native endian. However,
			 * this swap only goes up to 32-bit quantities.
			 *
			 * Thus, the memory bytes required to satisfy LDC1 are as follows:
			 * - Little-endian: [4 5 6 7 0 1 2 3]
			 * - Big-endian:    [0 1 2 3 4 5 6 7]
			 *
			 * On little-endian systems, LDC1 must be satisfied by two loads,
			 * which exposes the same problem as DMFC1. See the comment for
			 * DMFC1 in mips-cp1.c.
			*/
			mips32_sdc1(state, nft, STACK_OFFSET_MEMORY_READ, REG_STACK);
			mips32_lw(state, nt1, STACK_OFFSET_MEMORY_READ + LO32_64_BYTES, REG_STACK);
			mips32_lw(state, nt2, STACK_OFFSET_MEMORY_READ + HI32_64_BYTES, REG_STACK);
			mips32_sw(state, nt1, addr_loword_lo, naddr_loword_hi);
			mips32_sw(state, nt2, addr_hiword_lo, naddr_hiword_hi);
			if (TraceJITSettings.Cop1CanModifyCode) {
				invalid_code_check_const(state, cache, UINT32_C(0x80000000) | n64addr);
			}
		} /* fast_memory && constant RDRAM address */
		else if (fast_memory) {
			uint8_t ns = mips32_alloc_int_in_lo32(state, cache, &g_state.regs.gpr[rs]),
			        naddr = mips32_alloc_int_temp(state, cache),
			        n0xdf80 = mips32_alloc_int_const(state, cache, UINT32_C(0xDF800000)),
			        n0x8000 = mips32_alloc_int_const(state, cache, UINT32_C(0x80000000)),
			        nrdram_addr = mips32_alloc_int_const(state, cache, (uintptr_t) &g_rdram),
			        nt1 = mips32_alloc_int_temp(state, cache),
			        nt2 = mips32_alloc_int_temp(state, cache),
			        nft = mips32_alloc_float_in_64(state, cache, ft);

			if (imm != 0) {
				mips32_addiu(state, naddr, ns, imm);
			} else {
				/* This is a separate register to avoid 'rs' being modified, while
				 * allowing the new value of g_state.access_addr to be loaded into
				 * it by the slow path of stores before the code rewrite check. */
				mips32_or(state, naddr, ns, 0);
			}

			/* Now that we have the Nintendo 64 address of the store, we must
			 * check whether it's in RDRAM. RDRAM is at 0x8000_0000..0x807F_FFFF
			 * of 0xA000_0000..0xA07F_FFFF. This can be chcked, at runtime, using
			 * the expression [(addr & 0xDF80_0000) == 0x8000_0000]. */
			mips32_and(state, nt1, naddr, n0xdf80);
			mips32_bne(state, nt1, n0x8000, +0); /* to Slow Path */
			/* If the access is not in RDRAM, this will go to a slow path which
			 * will return after the RDRAM store. */
			void* slow_path_source = state->code;
			/* Now, as we expect accesses to RDRAM to be much more common, we
			 * prepare for one right away. (BNE's delay slot.) */
			mips32_sll(state, nt1, naddr, 9);
			mips32_srl(state, nt1, nt1, 9); /* nt1 = naddr & 0x7FFFFF; */
			mips32_addu(state, nt1, nrdram_addr, nt1); /* nt1 = g_rdram + ... bytes; */

			mips32_sdc1(state, nft, STACK_OFFSET_MEMORY_READ, REG_STACK);
			mips32_lw(state, nt2, STACK_OFFSET_MEMORY_READ + LO32_64_BYTES, REG_STACK);
			mips32_sw(state, nt2, 4, nt1);
			mips32_lw(state, nt2, STACK_OFFSET_MEMORY_READ + HI32_64_BYTES, REG_STACK);
			mips32_sw(state, nt2, 0, nt1);
			FAIL_AS(mips32_add_slow_path(state, cache, &swc1_slow, slow_path_source,
				state->code /* return to usual path */,
				naddr /* userdata: tell it which reg contains the N64 address */));

			if (TraceJITSettings.Cop1CanModifyCode) {
				FAIL_AS(invalid_code_check(state, cache, naddr));
			}
		} /* fast_memory && !constant RDRAM address */
		else {
			uint8_t nft = mips32_alloc_float_in_64(state, cache, ft),
			        n64addr_reg;
			mips32_sdc1(state, nft, STATE_OFFSET(write.d), REG_STATE);
			FAIL_AS(emit_accessor_write_int(state, cache, writememd, &n64addr_reg));
			if (TraceJITSettings.Cop1CanModifyCode) {
				FAIL_AS(invalid_code_check(state, cache, n64addr_reg));
			}
		} /* !fast_memory */
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_sd(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretSD) {
		return mips32_emit_interpret(state, cache, &TJ_SD, TJ_READS_PC | TJ_MAY_RAISE_TLB_REFILL);
	} else {
		mips32_start_opcode(state, cache, "SD");
		FAIL_AS(emit_memory_write_int(state, cache, 8));
		mips32_end_opcode(state, cache);
		return mips32_next_opcode(state);
	}
}

enum TJEmitTraceResult mips32_emit_sync(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretSYNC || TraceJITSettings.InterpretNOP) {
		return mips32_emit_interpret(state, cache, &TJ_SYNC, 0);
	} else {
		return mips32_emit_nop(state, cache);
	}
}

enum TJEmitTraceResult mips32_emit_cache(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	if (TraceJITSettings.InterpretCACHE || TraceJITSettings.InterpretNOP) {
		return mips32_emit_interpret(state, cache, &TJ_CACHE, 0);
	} else {
		return mips32_emit_nop(state, cache);
	}
}
