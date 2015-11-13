/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - MIPS (native) register assignment cache                 *
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

#if !defined(NDEBUG)
#  include <stdio.h>
#  define __STDC_FORMAT_MACROS
#  include <inttypes.h>
#endif

#include "native-endian.h"
#include "native-ops.h"
#include "native-regcache.h"
#include "native-utils.h"
#include "state.h"

#include "../mips-analysis.h"

#include "../../r4300.h"

uint32_t mips32_saved_int_mask = BIT(17) | BIT(18) | BIT(19) | BIT(20) | BIT(21) | BIT(22) | BIT(30);

uint32_t mips32_temp_int_mask = BIT(2) | BIT(3) | BIT(4) | BIT(5) | BIT(6) | BIT(7) | BIT(8) | BIT(9) | BIT(10) | BIT(11) | BIT(12) | BIT(13) | BIT(14) | BIT(15) | BIT(24) | BIT(25);

uint32_t mips32_saved_float_mask = BIT(20) | BIT(22) | BIT(24) | BIT(26) | BIT(28) | BIT(30);

uint32_t mips32_temp_float_mask = BIT(0) | BIT(2) | BIT(4) | BIT(6) | BIT(8) | BIT(10) | BIT(12) | BIT(14) | BIT(16) | BIT(18);

/* - - - HELPER FUNCTIONS - - - */

static void set_int_dirty(struct mips32_reg_cache* cache, uint8_t reg)
{
	cache->reg_int[reg].dirty = true;
	cache->dirty_int |= BIT(reg);
}

static void unset_int_dirty(struct mips32_reg_cache* cache, uint8_t reg)
{
	cache->reg_int[reg].dirty = false;
	cache->dirty_int &= ~BIT(reg);
}

static void set_int_dirty_as(struct mips32_reg_cache* cache, uint8_t reg, uint8_t other)
{
	if (cache->reg_int[other].dirty)
		set_int_dirty(cache, reg);
	else
		unset_int_dirty(cache, reg);
}

static void set_int_const(struct mips32_reg_cache* cache, uint8_t reg, uint32_t value)
{
	cache->reg_int[reg].is_constant = true;
	cache->reg_int[reg].constant.value = value;
	cache->constant_int |= BIT(reg);
}

static void unset_int_const(struct mips32_reg_cache* cache, uint8_t reg)
{
	cache->reg_int[reg].is_constant = false;
	cache->constant_int &= ~BIT(reg);
}

static void discard_int(struct mips32_reg_cache* cache, uint8_t reg)
{
	cache->reg_int[reg].type = MRIT_UNUSED;
	cache->opcode_int &= ~BIT(reg);
	cache->allocated_int &= ~BIT(reg);
	unset_int_dirty(cache, reg);
}

static void discard_int_const(struct mips32_reg_cache* cache, uint8_t reg)
{
	discard_int(cache, reg);
	unset_int_const(cache, reg);
}

static void refresh_int(struct mips32_reg_cache* cache, uint8_t reg)
{
	cache->opcode_int |= BIT(reg);
}

static void add_int_in(struct mips32_reg_cache* cache, uint8_t reg, enum mips32_reg_int_type type, void* memory)
{
	cache->reg_int[reg].type = type;
	cache->opcode_int |= BIT(reg);
	cache->allocated_int |= BIT(reg);
	cache->reg_int[reg].source.ptr = memory;
	unset_int_const(cache, reg);
}

static void add_int_out(struct mips32_reg_cache* cache, uint8_t reg, enum mips32_reg_int_type type, void* memory)
{
	cache->reg_int[reg].type = type;
	cache->opcode_int |= BIT(reg);
	cache->allocated_int |= BIT(reg);
	cache->reg_int[reg].source.ptr = memory;
	set_int_dirty(cache, reg);
	unset_int_const(cache, reg);
}

static void add_int_temp(struct mips32_reg_cache* cache, uint8_t reg)
{
	cache->reg_int[reg].type = MRIT_TEMP;
	cache->opcode_int |= BIT(reg);
	cache->allocated_int |= BIT(reg);
	set_int_dirty(cache, reg);
	unset_int_const(cache, reg);
}

static void add_int_const(struct mips32_reg_cache* cache, uint8_t reg, uint32_t value)
{
	cache->reg_int[reg].type = MRIT_TEMP;
	cache->opcode_int |= BIT(reg);
	cache->allocated_int |= BIT(reg);
	set_int_dirty(cache, reg);
	set_int_const(cache, reg, value);
}

static void set_float_dirty(struct mips32_reg_cache* cache, uint8_t reg)
{
	cache->reg_float[reg].dirty = true;
	cache->dirty_float |= BIT(reg);
}

static void unset_float_dirty(struct mips32_reg_cache* cache, uint8_t reg)
{
	cache->reg_float[reg].dirty = false;
	cache->dirty_float &= ~BIT(reg);
}

static void discard_float(struct mips32_reg_cache* cache, uint8_t reg)
{
	cache->reg_float[reg].type = MRIT_UNUSED;
	cache->opcode_float &= ~BIT(reg);
	cache->allocated_float &= ~BIT(reg);
	unset_float_dirty(cache, reg);
}

static void refresh_float(struct mips32_reg_cache* cache, uint8_t reg)
{
	cache->opcode_float |= BIT(reg);
}

static void add_float_in(struct mips32_reg_cache* cache, uint8_t reg, enum mips32_reg_int_type type, uint8_t fpr_number)
{
	cache->reg_float[reg].type = type;
	cache->opcode_float |= BIT(reg);
	cache->allocated_float |= BIT(reg);
	cache->reg_float[reg].source.fpr_number = fpr_number;
}

static void add_float_out(struct mips32_reg_cache* cache, uint8_t reg, enum mips32_reg_float_type type, uint8_t fpr_number)
{
	cache->reg_float[reg].type = type;
	cache->opcode_float |= BIT(reg);
	cache->allocated_float |= BIT(reg);
	cache->reg_float[reg].source.fpr_number = fpr_number;
	set_float_dirty(cache, reg);
}

static void* get_fpr_ptr_ptr(enum mips32_reg_float_type type, uint8_t fpr_number)
{
	switch (type) {
	case MRFT_N64_FPR_32:
		return &g_state.regs.cp1_s[fpr_number];
	case MRFT_N64_FPR_64:
		return &g_state.regs.cp1_d[fpr_number];
	default:
		return NULL;
	}
}

static uint8_t get_fpr_ptr_reg(struct mips32_reg_cache* cache, enum mips32_reg_float_type type, uint8_t fpr_number)
{
	int i;
	void* memory = get_fpr_ptr_ptr(type, fpr_number);
	uint32_t mask = cache->allocated_int;

	for (i = FindFirstSet(mask); i >= 0; mask &= ~BIT(i), i = FindFirstSet(mask)) {
		struct mips32_reg_int* entry = &cache->reg_int[i];
		if (entry->source.ptr == memory && entry->type == MRIT_MEM_32) {
			assert(!entry->dirty);
			return i;
		}
	}

	return 0;
}

#if !defined(NDEBUG)
/* Ensures that various cached bitmasks are still synchronised with the
 * current state of the register cache. */
static bool cache_in_sync(const struct mips32_reg_cache* cache)
{
	size_t i;
	bool result = true;
	uint32_t allocated_int = 0, dirty_int = 0, constant_int = 0,
	         allocated_float = 0, dirty_float = 0;

	for (i = 0; i < 32; i++) {
		if (cache->reg_int[i].type != MRIT_UNUSED)
			allocated_int |= BIT(i);
		if (cache->reg_int[i].dirty)
			dirty_int |= BIT(i);
		if (cache->reg_int[i].is_constant)
			constant_int |= BIT(i);
		if (cache->reg_float[i].type != MRFT_UNUSED)
			allocated_float |= BIT(i);
		if (cache->reg_float[i].dirty)
			dirty_float |= BIT(i);
	}

	if (cache->allocated_int != allocated_int) {
		fprintf(stderr, "allocated_int is not in sync: missing bits %08" PRIX32 ", extra bits %08" PRIX32 "\n", cache->allocated_int & ~allocated_int, allocated_int & ~cache->allocated_int);
		result = false;
	}

	if (cache->allocated_float != allocated_float) {
		fprintf(stderr, "allocated_float is not in sync: missing bits %08" PRIX32 ", extra bits %08" PRIX32 "\n", cache->allocated_float & ~allocated_float, allocated_float & ~cache->allocated_float);
		result = false;
	}

	if (cache->dirty_int != dirty_int) {
		fprintf(stderr, "dirty_int is not in sync: missing bits %08" PRIX32 ", extra bits %08" PRIX32 "\n", cache->dirty_int & ~dirty_int, dirty_int & ~cache->dirty_int);
		result = false;
	}

	if (cache->dirty_float != dirty_float) {
		fprintf(stderr, "dirty_float is not in sync: missing bits %08" PRIX32 ", extra bits %08" PRIX32 "\n", cache->dirty_float & ~dirty_float, dirty_float & ~cache->dirty_float);
		result = false;
	}

	if (cache->constant_int != constant_int) {
		fprintf(stderr, "constant_int is not in sync: missing bits %08" PRIX32 ", extra bits %08" PRIX32 "\n", cache->constant_int & ~constant_int, constant_int & ~cache->constant_int);
		result = false;
	}

	if (!(cache->constant_int & BIT(0))) {
		fprintf(stderr, "constant_int incorrect: $0's value not marked as constant\n");
		result = false;
	}

	if (!cache->reg_int[0].is_constant || cache->reg_int[0].constant.value != 0) {
		fprintf(stderr, "reg_int[0] incorrect: not marked as constant, or constant value not 0\n");
		result = false;
	}

	return result;
}
#endif

/* Creates a memory reference from a register whose value is currently known
 * to be near the given memory. If no register contains a nearby address, the
 * emergency register is used.
 *
 * In:
 *   state: Code emission state. Used if no register contains a constant near
 *     'memory', in order to use the emergency register.
 *   cache: The current register cache.
 *   memory: The memory address to create a reference to.
 * Out:
 *   reg: Updated to contain the base register for the memory reference.
 *   off: Updated to contain the offset from the base register for the memory
 *     reference.
 */
static void get_int_mem_ref(struct mips32_state* state, struct mips32_reg_cache* cache, void* memory, uint8_t* reg, int16_t* off)
{
	int i;
	ptrdiff_t distance;
	uint32_t mask;

	/* References are likely to be from the pointer to g_state. Try that
	 * first. */
	distance = (uintptr_t) memory - (uintptr_t) &g_state;
	if (distance == (ptrdiff_t) (int16_t) distance) {
		*reg = REG_STATE;
		*off = (int16_t) distance;
		return;
	}

	mask = cache->constant_int;
	for (i = FindFirstSet(mask); i >= 0; mask &= ~BIT(i), i = FindFirstSet(mask)) {
		uintptr_t constant = cache->reg_int[i].constant.addr;
		distance = (uintptr_t) memory - constant;
		if (distance == (ptrdiff_t) (int16_t) distance) {
			*reg = i;
			*off = (int16_t) distance;
			return;
		}
	}

	/* Nothing contains the right constant. Use the emergency register to make
	 * the reference. */
	mips32_lui(state, REG_EMERGENCY, (uintptr_t) memory >> 16);
	*reg = REG_EMERGENCY;
	*off = (uintptr_t) memory & 0xFFFF;
}

static void write_self_int(struct mips32_state* state, struct mips32_reg_cache* cache, uint8_t reg)
{
	struct mips32_reg_int* entry = &cache->reg_int[reg];
	uint8_t mem_reg;
	int16_t mem_off;

	assert(cache_in_sync(cache));

	switch (entry->type) {
	case MRIT_MEM_HI32:
		get_int_mem_ref(state, cache, HI32_64(entry->source.ptr), &mem_reg, &mem_off);
		mips32_sw(state, reg, mem_off, mem_reg);
		break;
	case MRIT_MEM_LO32:
	case MRIT_MEM_SE32:
	case MRIT_MEM_ZE32:
	case MRIT_MEM_OE32:
		get_int_mem_ref(state, cache, LO32_64(entry->source.ptr), &mem_reg, &mem_off);
		mips32_sw(state, reg, mem_off, mem_reg);
		break;
	case MRIT_MEM_32:
		get_int_mem_ref(state, cache, entry->source.ptr, &mem_reg, &mem_off);
		mips32_sw(state, reg, mem_off, mem_reg);
		break;
	default:
		break;
	}
}

static void extend_int_into(struct mips32_state* state, struct mips32_reg_cache* cache, uint8_t reg, uint8_t target_reg)
{
	struct mips32_reg_int* entry = &cache->reg_int[reg];

	assert(cache_in_sync(cache));

	switch (entry->type) {
	case MRIT_MEM_SE32:
		mips32_sra(state, target_reg, reg, 31);
		break;
	case MRIT_MEM_ZE32:
		mips32_or(state, target_reg, 0, 0);
		break;
	case MRIT_MEM_OE32:
		mips32_nor(state, target_reg, 0, 0);
		break;
	default:
		break;
	}
}

static void extend_int_into_self(struct mips32_state* state, struct mips32_reg_cache* cache, uint8_t reg)
{
	struct mips32_reg_int* entry = &cache->reg_int[reg];

	assert(cache_in_sync(cache));

	switch (entry->type) {
	case MRIT_MEM_SE32:
		mips32_sra(state, reg, reg, 31);
		break;
	case MRIT_MEM_OE32:
	{
		int i;
		uint8_t reg_all1 = 0;
		uint32_t mask = cache->constant_int;

		for (i = FindFirstSet(mask); i >= 0; mask &= ~BIT(i), i = FindFirstSet(mask)) {
			if (cache->reg_int[i].constant.value == UINT32_C(0xFFFFFFFF)) {
				reg_all1 = i;
				break;
			}
		}

		if (reg_all1 == 0) {
			mips32_nor(state, reg, 0, 0);
			set_int_const(cache, reg, UINT32_C(0xFFFFFFFF));
		}
		break;
	}
	default:
		break;
	}
}

static void write_extension_int(struct mips32_state* state, struct mips32_reg_cache* cache, uint8_t reg)
{
	struct mips32_reg_int* entry = &cache->reg_int[reg];
	uint8_t mem_reg;
	int16_t mem_off;

	assert(cache_in_sync(cache));

	switch (entry->type) {
	case MRIT_MEM_SE32:
		get_int_mem_ref(state, cache, HI32_64(entry->source.ptr), &mem_reg, &mem_off);
		mips32_sw(state, reg, mem_off, mem_reg);
		break;
	case MRIT_MEM_ZE32:
		get_int_mem_ref(state, cache, HI32_64(entry->source.ptr), &mem_reg, &mem_off);
		mips32_sw(state, 0, mem_off, mem_reg);
		break;
	case MRIT_MEM_OE32:
	{
		int i;
		uint8_t reg_all1 = 0;
		uint32_t mask = cache->constant_int;

		for (i = FindFirstSet(mask); i >= 0; mask &= ~BIT(i), i = FindFirstSet(mask)) {
			if (cache->reg_int[i].constant.value == UINT32_C(0xFFFFFFFF)) {
				reg_all1 = i;
				break;
			}
		}

		assert(reg_all1 != 0);

		get_int_mem_ref(state, cache, HI32_64(entry->source.ptr), &mem_reg, &mem_off);
		mips32_sw(state, reg_all1, mem_off, mem_reg);
		break;
	}
	default:
		break;
	}
}

/* Gets a (native) register containing the address of the given (Nintendo 64)
 * floating-point register of the given type. If no register contains its
 * address, the emergency register is used.
 *
 * In:
 *   state: Code emission state. Used if no register contains the address of
 *     Nintendo 64 floating-point register #fpr_number, in order to use the
 *     emergency register.
 *   cache: The current register cache.
 *   type: The type of Nintendo 64 floating-point register to get the address
 *     of.
 *   fpr_number: The Nintendo 64 floating-point register number.
 * Returns:
 *   A register that contains the FPR's address.
 */
static uint8_t get_float_mem_ref(struct mips32_state* state, struct mips32_reg_cache* cache, enum mips32_reg_float_type type, uint8_t fpr_number)
{
	uint8_t mem_reg = get_fpr_ptr_reg(cache, type, fpr_number);

	if (mem_reg == 0) {
		/* Nothing contains the right address. Use the emergency register to
		 * make the reference. */
		mem_reg = REG_EMERGENCY;
		mips32_lw(state, mem_reg, (uint8_t*) get_fpr_ptr_ptr(type, fpr_number) - (uint8_t*) &g_state, REG_STATE);
	}
	return mem_reg;
}

static void write_float(struct mips32_state* state, struct mips32_reg_cache* cache, uint8_t reg)
{
	struct mips32_reg_float* entry = &cache->reg_float[reg];
	uint8_t mem_reg = get_float_mem_ref(state, cache, entry->type, entry->source.fpr_number);

	assert(cache_in_sync(cache));

	switch (entry->type) {
	case MRFT_N64_FPR_32:
		mips32_swc1(state, reg, 0, mem_reg);
		break;
	case MRFT_N64_FPR_64:
		mips32_sdc1(state, reg, 0, mem_reg);
		break;
	default:
		break;
	}
}

/* Gets a (native) register containing the address of the given (Nintendo 64)
 * floating-point register of the given type. If no register contains its
 * address, one is allocated.
 *
 * In:
 *   state: Code emission state. Used if no register contains the address of
 *     Nintendo 64 floating-point register #fpr_number, in order to load it.
 *   cache: The current register cache.
 *   type: The type of Nintendo 64 floating-point register to get the address
 *     of.
 *   fpr_number: The Nintendo 64 floating-point register number.
 * Returns:
 *   A register that contains the FPR's address.
 */
static uint8_t alloc_float_mem_ref(struct mips32_state* state, struct mips32_reg_cache* cache, enum mips32_reg_float_type type, uint8_t fpr_number)
{
	return mips32_alloc_int_in_32(state, cache, get_fpr_ptr_ptr(type, fpr_number));
}

static uint8_t choose_int(struct mips32_state* state, struct mips32_reg_cache* cache, bool saved_first)
{
	uint32_t mask;
	uint8_t chosen_reg;
	mask = (saved_first ? mips32_saved_int_mask : mips32_temp_int_mask) & ~cache->allocated_int;
	if (mask != 0)
		return FindFirstSet(mask);
	mask = (saved_first ? mips32_temp_int_mask : mips32_saved_int_mask) & ~cache->allocated_int;
	if (mask != 0)
		return FindFirstSet(mask);
	/* Here, we have register pressure. TODO Implement a better algorithm */
	mask = (mips32_saved_int_mask | mips32_temp_int_mask) & ~cache->opcode_int;
	assert(mask != 0); /* If this fails, a single instruction uses all regs */
	chosen_reg = FindFirstSet(mask);
	mips32_free_specific_int(state, cache, chosen_reg);
	return chosen_reg;
}

static uint8_t choose_float(struct mips32_state* state, struct mips32_reg_cache* cache, bool saved_first)
{
	uint32_t mask;
	uint8_t chosen_reg;
	mask = (saved_first ? mips32_saved_float_mask : mips32_temp_float_mask) & ~cache->allocated_float;
	if (mask != 0)
		return FindFirstSet(mask);
	mask = (saved_first ? mips32_temp_float_mask : mips32_saved_float_mask) & ~cache->allocated_float;
	if (mask != 0)
		return FindFirstSet(mask);
	/* Here, we have register pressure. TODO Implement a better algorithm */
	mask = (mips32_saved_float_mask | mips32_temp_float_mask) & ~cache->opcode_float;
	assert(mask != 0); /* If this fails, a single instruction uses all regs */
	chosen_reg = FindFirstSet(mask);
	mips32_free_specific_float(state, cache, chosen_reg);
	return chosen_reg;
}

/* - - - PUBLIC FUNCTIONS - - - */

void mips32_init_reg_cache(struct mips32_reg_cache* cache)
{
	size_t i;
	for (i = 0; i < 32; i++) {
		cache->reg_int[i].type = MRIT_UNUSED;
		cache->reg_int[i].dirty = cache->reg_int[i].is_constant = false;
	}
	cache->reg_int[0].is_constant = true;
	cache->reg_int[0].constant.value = 0;
	cache->opcode_int = cache->allocated_int = cache->dirty_int = 0;
	cache->constant_int = BIT(0);
	for (i = 0; i < 32; i++) {
		cache->reg_float[i].type = MRFT_UNUSED;
		cache->reg_float[i].dirty = false;
	}
	cache->opcode_float = cache->allocated_float = cache->dirty_float = 0;
	cache->opcode_level = 0;
	cache->native_hi = cache->native_lo = false;
}

void mips32_copy_reg_cache(struct mips32_reg_cache* dst, const struct mips32_reg_cache* src)
{
	size_t i;

	for (i = 0; i < 32; i++) {
		dst->reg_int[i].type = src->reg_int[i].type;
		if (src->reg_int[i].type != MRIT_UNUSED) {
			dst->reg_int[i].source = src->reg_int[i].source;
		}
		dst->reg_int[i].dirty = src->reg_int[i].dirty;
		dst->reg_int[i].is_constant = src->reg_int[i].is_constant;
		if (src->reg_int[i].is_constant) {
			dst->reg_int[i].constant = src->reg_int[i].constant;
		}

		dst->reg_float[i].type = src->reg_float[i].type;
		if (src->reg_float[i].type != MRFT_UNUSED) {
			dst->reg_float[i].source = src->reg_float[i].source;
		}
		dst->reg_float[i].dirty = src->reg_float[i].dirty;
	}

	for (i = 0; i < sizeof(src->op_names) / sizeof(src->op_names[0]); i++) {
		dst->op_names[i] = src->op_names[i];
	}
	dst->native_hi = src->native_hi;
	dst->native_lo = src->native_lo;
	dst->opcode_int = src->opcode_int;
	dst->opcode_float = src->opcode_float;
	dst->opcode_level = src->opcode_level;
	dst->allocated_int = src->allocated_int;
	dst->allocated_float = src->allocated_float;
	dst->dirty_int = src->dirty_int;
	dst->dirty_float = src->dirty_float;
	dst->constant_int = src->constant_int;
}

uint8_t mips32_alloc_int_temp(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	uint8_t reg;

	assert(cache_in_sync(cache));
	assert(cache->opcode_int != (mips32_saved_int_mask | mips32_temp_int_mask));

	reg = choose_int(state, cache, false);
	add_int_temp(cache, reg);
	return reg;
}

void mips32_alloc_specific_int_temp(struct mips32_state* state, struct mips32_reg_cache* cache, uint8_t reg)
{
	assert(cache_in_sync(cache));

	mips32_free_specific_int(state, cache, reg);
	add_int_temp(cache, reg);
}

uint8_t mips32_alloc_int_in_hi32(struct mips32_state* state, struct mips32_reg_cache* cache, void* memory)
{
	int i;
	uint8_t reg = 0, lo_reg = 0, mem_reg;
	int16_t mem_off;
	enum mips32_reg_int_type lo_type = MRIT_UNUSED;
	uint32_t mask = cache->allocated_int;

	if (memory == &g_state.regs.gpr[0])
		return 0; /* No bookkeeping needed for this register */

	assert(cache_in_sync(cache));
	assert(cache->opcode_int != (mips32_saved_int_mask | mips32_temp_int_mask));

	for (i = FindFirstSet(mask); i >= 0; mask &= ~BIT(i), i = FindFirstSet(mask)) {
		struct mips32_reg_int* entry = &cache->reg_int[i];
		if (entry->source.ptr == memory) {
			switch (entry->type) {
			case MRIT_MEM_LO32:
			case MRIT_MEM_SE32:
			case MRIT_MEM_ZE32:
			case MRIT_MEM_OE32:
				lo_reg = i;
				lo_type = entry->type;
				break;
			case MRIT_MEM_HI32:
				reg = i;
				break;
			default:
				break;
			}
		}
	}

	if (reg != 0) {
		/* Existing HI32 allocations require the lower 32 bits to be in a
		 * non-extended allocation, if there is even one. */
		assert(lo_type != MRIT_MEM_SE32 && lo_type != MRIT_MEM_ZE32 && lo_type != MRIT_MEM_OE32);
		refresh_int(cache, reg);
		return reg;
	}

	reg = choose_int(state, cache, true);
	add_int_in(cache, reg, MRIT_MEM_HI32, memory);
	if (lo_reg != 0
	 && (lo_type == MRIT_MEM_SE32 || lo_type == MRIT_MEM_ZE32 || lo_type == MRIT_MEM_OE32)) {
		extend_int_into(state, cache, lo_reg, reg);
		cache->reg_int[lo_reg].type = MRIT_MEM_LO32;
		set_int_dirty_as(cache, reg, lo_reg);
	} else {
		get_int_mem_ref(state, cache, HI32_64(memory), &mem_reg, &mem_off);
		mips32_lw(state, reg, mem_off, mem_reg);
	}
	return reg;
}

uint8_t mips32_alloc_int_in_lo32(struct mips32_state* state, struct mips32_reg_cache* cache, void* memory)
{
	int i;
	uint8_t reg = 0, mem_reg;
	int16_t mem_off;
	uint32_t mask = cache->allocated_int;

	if (memory == &g_state.regs.gpr[0])
		return 0; /* No bookkeeping needed for this register */

	assert(cache_in_sync(cache));
	assert(cache->opcode_int != (mips32_saved_int_mask | mips32_temp_int_mask));

	for (i = FindFirstSet(mask); i >= 0; mask &= ~BIT(i), i = FindFirstSet(mask)) {
		struct mips32_reg_int* entry = &cache->reg_int[i];
		if (entry->source.ptr == memory) {
			switch (entry->type) {
			case MRIT_MEM_LO32:
			case MRIT_MEM_SE32:
			case MRIT_MEM_ZE32:
			case MRIT_MEM_OE32:
				reg = i;
				break;
			default:
				break;
			}
		}
	}

	if (reg != 0) {
		refresh_int(cache, reg);
		return reg;
	}

	reg = choose_int(state, cache, true);
	add_int_in(cache, reg, MRIT_MEM_LO32, memory);
	get_int_mem_ref(state, cache, LO32_64(memory), &mem_reg, &mem_off);
	mips32_lw(state, reg, mem_off, mem_reg);
	return reg;
}

uint8_t mips32_alloc_int_in_32(struct mips32_state* state, struct mips32_reg_cache* cache, void* memory)
{
	int i;
	uint8_t reg = 0, mem_reg;
	int16_t mem_off;
	uint32_t mask = cache->allocated_int;

	assert(cache_in_sync(cache));
	assert(cache->opcode_int != (mips32_saved_int_mask | mips32_temp_int_mask));

	for (i = FindFirstSet(mask); i >= 0; mask &= ~BIT(i), i = FindFirstSet(mask)) {
		struct mips32_reg_int* entry = &cache->reg_int[i];
		if (entry->source.ptr == memory && entry->type == MRIT_MEM_32) {
			reg = i;
			break;
		}
	}

	if (reg != 0) {
		refresh_int(cache, reg);
		return reg;
	}

	reg = choose_int(state, cache, true);
	add_int_in(cache, reg, MRIT_MEM_32, memory);
	get_int_mem_ref(state, cache, memory, &mem_reg, &mem_off);
	mips32_lw(state, reg, mem_off, mem_reg);
	return reg;
}

uint8_t mips32_alloc_int_out_hi32(struct mips32_state* state, struct mips32_reg_cache* cache, void* memory)
{
	int i;
	uint8_t reg = 0;
	uint32_t mask = cache->allocated_int;

	assert(cache_in_sync(cache));
	assert(memory != &g_state.regs.gpr[0]);
	assert(cache->opcode_int != (mips32_saved_int_mask | mips32_temp_int_mask));

	for (i = FindFirstSet(mask); i >= 0; mask &= ~BIT(i), i = FindFirstSet(mask)) {
		struct mips32_reg_int* entry = &cache->reg_int[i];
		if (entry->source.ptr == memory) {
			switch (entry->type) {
			case MRIT_MEM_SE32:
			case MRIT_MEM_ZE32:
			case MRIT_MEM_OE32:
				entry->type = MRIT_MEM_LO32;
				break;
			case MRIT_MEM_HI32:
				reg = i;
				break;
			default:
				break;
			}
		}
	}

	if (reg == 0) {
		reg = choose_int(state, cache, true);
	}
	add_int_out(cache, reg, MRIT_MEM_HI32, memory);
	return reg;
}

uint8_t mips32_alloc_int_out_lo32(struct mips32_state* state, struct mips32_reg_cache* cache, void* memory)
{
	int i;
	uint8_t reg = 0;
	uint32_t mask = cache->allocated_int;

	assert(cache_in_sync(cache));
	assert(memory != &g_state.regs.gpr[0]);
	assert(cache->opcode_int != (mips32_saved_int_mask | mips32_temp_int_mask));

	for (i = FindFirstSet(mask); i >= 0; mask &= ~BIT(i), i = FindFirstSet(mask)) {
		struct mips32_reg_int* entry = &cache->reg_int[i];
		if (entry->source.ptr == memory) {
			switch (entry->type) {
			case MRIT_MEM_LO32:
			case MRIT_MEM_SE32:
			case MRIT_MEM_ZE32:
			case MRIT_MEM_OE32:
				reg = i;
				break;
			default:
				break;
			}
		}
	}

	if (reg == 0) {
		reg = choose_int(state, cache, true);
	}
	add_int_out(cache, reg, MRIT_MEM_LO32, memory);
	return reg;
}

void mips32_set_int_ex32(struct mips32_reg_cache* cache, void* memory, enum mips32_reg_int_type type)
{
	int i;
	uint8_t reg = 0;
	uint32_t mask = cache->allocated_int;

	assert(cache_in_sync(cache));

	for (i = FindFirstSet(mask); i >= 0; mask &= ~BIT(i), i = FindFirstSet(mask)) {
		struct mips32_reg_int* entry = &cache->reg_int[i];
		if (entry->source.ptr == memory) {
			switch (entry->type) {
			case MRIT_MEM_HI32: /* Clear any allocation of type HI32. */
				discard_int(cache, i);
				/* Consider only registers allocated by this opcode from here
				 * on, just looking for the LO32. */
				mask &= cache->opcode_int;
				break;
			case MRIT_MEM_LO32:
			case MRIT_MEM_SE32:
			case MRIT_MEM_ZE32:
			case MRIT_MEM_OE32:
				reg = i;
				break;
			default:
				break;
			}
		}
	}

	/* There must be a LO32 register allocated in the current opcode, and it
	 * must be an output register. */
	assert(reg != 0);
	assert(cache->opcode_int & BIT(reg));
	assert(cache->reg_int[reg].dirty);

	cache->reg_int[reg].type = type;
}

void mips32_set_int_se32(struct mips32_reg_cache* cache, void* memory)
{
	mips32_set_int_ex32(cache, memory, MRIT_MEM_SE32);
}

void mips32_set_int_ze32(struct mips32_reg_cache* cache, void* memory)
{
	mips32_set_int_ex32(cache, memory, MRIT_MEM_ZE32);
}

void mips32_set_int_oe32(struct mips32_reg_cache* cache, void* memory)
{
	mips32_set_int_ex32(cache, memory, MRIT_MEM_OE32);
}

uint8_t mips32_alloc_int_out_32(struct mips32_state* state, struct mips32_reg_cache* cache, void* memory)
{
	int i;
	uint8_t reg = 0;
	uint32_t mask = cache->allocated_int;

	assert(cache_in_sync(cache));
	assert(cache->opcode_int != (mips32_saved_int_mask | mips32_temp_int_mask));

	for (i = FindFirstSet(mask); i >= 0; mask &= ~BIT(i), i = FindFirstSet(mask)) {
		struct mips32_reg_int* entry = &cache->reg_int[i];
		if (entry->source.ptr == memory && entry->type == MRIT_MEM_32) {
			reg = i;
			break;
		}
	}

	if (reg == 0) {
		reg = choose_int(state, cache, true);
	}
	add_int_out(cache, reg, MRIT_MEM_32, memory);
	return reg;
}

uint8_t mips32_alloc_int_const(struct mips32_state* state, struct mips32_reg_cache* cache, uint32_t constant)
{
	int i;
	int reg = -1;
	uint32_t mask = cache->constant_int;

	assert(cache_in_sync(cache));
	assert(cache->opcode_int != (mips32_saved_int_mask | mips32_temp_int_mask));

	for (i = FindFirstSet(mask); i >= 0; mask &= ~BIT(i), i = FindFirstSet(mask)) {
		struct mips32_reg_int* entry = &cache->reg_int[i];
		if (entry->constant.value == constant) {
			reg = i;
			break;
		}
	}

	if (reg == -1) {
		bool near_reg_found = false;
		reg = choose_int(state, cache, false);
		/* Try to find a register containing a value near the one sought.
		 * Since we're using it only to improve the instruction count for
		 * this allocation, don't count it against the opcode. */
		mask = cache->constant_int;
		for (i = FindFirstSet(mask); i >= 0; mask &= ~BIT(i), i = FindFirstSet(mask)) {
			struct mips32_reg_int* entry = &cache->reg_int[i];
			int32_t distance = constant - entry->constant.value;
			if (distance == (int32_t) (int16_t) distance) {
				near_reg_found = true;
				mips32_addiu(state, reg, i, distance);
				break;
			} else if ((constant >> 16) == (entry->constant.value >> 16)
			        && (entry->constant.value & 0xFFFF) == 0) {
				near_reg_found = true;
				mips32_ori(state, reg, i, constant & 0xFFFF);
				break;
			}
		}
		if (!near_reg_found) {
			mips32_i32(state, reg, constant);
		}
	}
	add_int_const(cache, reg, constant);
	return reg;
}

void mips32_discard_int(struct mips32_reg_cache* cache, void* memory)
{
	int i;
	uint32_t mask = cache->allocated_int;

	assert(cache_in_sync(cache));

	for (i = FindFirstSet(mask); i >= 0; mask &= ~BIT(i), i = FindFirstSet(mask)) {
		struct mips32_reg_int* entry = &cache->reg_int[i];
		if (entry->type != MRIT_UNUSED && entry->type != MRIT_TEMP
		 && entry->source.ptr == memory) {
			discard_int_const(cache, i);
		}
	}
}

uint8_t mips32_alloc_float_in_32(struct mips32_state* state, struct mips32_reg_cache* cache, uint8_t fpr_number)
{
	int i;
	int reg = -1;
	uint8_t mem_reg;
	uint32_t mask = cache->allocated_float;

	assert(cache_in_sync(cache));
	assert(cache->opcode_float != (mips32_saved_float_mask | mips32_temp_float_mask));

	for (i = FindFirstSet(mask); i >= 0; mask &= ~BIT(i), i = FindFirstSet(mask)) {
		struct mips32_reg_float* entry = &cache->reg_float[i];
		if ((entry->source.fpr_number & ~1) == (fpr_number & ~1)
		 && entry->type == MRFT_N64_FPR_64) {
			/* If a value known to be 'double' is being used by the N64 as
			 * 'float', we need to store the 'double' value first.
			 * On MIPS I, this is a "register pair split".
			 * On MIPS III, this is reading the low part of the 'double'
			 * value. */
			mips32_free_specific_float(state, cache, i);
		} else if (entry->source.fpr_number == fpr_number && entry->type == MRFT_N64_FPR_32) {
			reg = i;
		}
	}

	if (reg != -1) {
		refresh_float(cache, reg);
		return reg;
	}

	reg = choose_float(state, cache, true);
	add_float_in(cache, reg, MRFT_N64_FPR_32, fpr_number);
	mem_reg = alloc_float_mem_ref(state, cache, MRFT_N64_FPR_32, fpr_number);
	mips32_lwc1(state, reg, 0, mem_reg);
	return reg;
}

uint8_t mips32_alloc_float_in_64(struct mips32_state* state, struct mips32_reg_cache* cache, uint8_t fpr_number)
{
	int i;
	int reg = -1;
	uint8_t mem_reg;
	uint32_t mask = cache->allocated_float;

	assert(cache_in_sync(cache));
	assert(cache->opcode_float != (mips32_saved_float_mask | mips32_temp_float_mask));

	for (i = FindFirstSet(mask); i >= 0; mask &= ~BIT(i), i = FindFirstSet(mask)) {
		struct mips32_reg_float* entry = &cache->reg_float[i];
		if ((entry->source.fpr_number & ~1) == (fpr_number & ~1)
		 && entry->type == MRFT_N64_FPR_32) {
			/* If a value known to be 'float' is being used by the N64 as
			 * 'double', we need to store the 'float' value first.
			 * On MIPS I, this is a "register pair merge".
			 * On MIPS III, this is reading the allocated low part (the
			 * 'float' value) as well as the unallocated high part. */
			mips32_free_specific_float(state, cache, i);
		} else if (entry->source.fpr_number == fpr_number && entry->type == MRFT_N64_FPR_64) {
			reg = i;
		}
	}

	if (reg != -1) {
		refresh_float(cache, reg);
		return reg;
	}

	reg = choose_float(state, cache, true);
	add_float_in(cache, reg, MRFT_N64_FPR_64, fpr_number);
	mem_reg = alloc_float_mem_ref(state, cache, MRFT_N64_FPR_64, fpr_number);
	mips32_ldc1(state, reg, 0, mem_reg);
	return reg;
}

uint8_t mips32_alloc_float_out_32(struct mips32_state* state, struct mips32_reg_cache* cache, uint8_t fpr_number)
{
	int i;
	int reg = -1;
	uint32_t mask = cache->allocated_float;

	assert(cache_in_sync(cache));
	assert(cache->opcode_float != (mips32_saved_float_mask | mips32_temp_float_mask));

	for (i = FindFirstSet(mask); i >= 0; mask &= ~BIT(i), i = FindFirstSet(mask)) {
		struct mips32_reg_float* entry = &cache->reg_float[i];
		/* If a value known to be 'double' will be written by the N64 as
		 * 'float'... the second part of the former 'double' value becomes
		 * UNPREDICTABLE.
		 * On MIPS I, this is a "register pair split", and the value of the
		 * counterpart register becomes UNPREDICTABLE, and is unallocated
		 * without writeback.
		 * On MIPS III, this is writing the low part of the register. Its
		 * high part becomes UNPREDICTABLE, so the register is reallocated
		 * as a 'float' value without writeback of the old value.
		 * Using an unknown layout, the target FPR's value is discarded,
		 * and the counterpart register (odd for even and even for odd) is
		 * written back. */
		if (entry->source.fpr_number == fpr_number && entry->type == MRFT_N64_FPR_64) {
			discard_float(cache, i);
		} else if (entry->source.fpr_number == (fpr_number ^ 1) && entry->type == MRFT_N64_FPR_64) {
			mips32_free_specific_float(state, cache, i);
		} else if (entry->source.fpr_number == fpr_number && entry->type == MRFT_N64_FPR_32) {
			reg = i;
		}
	}

	if (reg == -1) {
		reg = choose_float(state, cache, true);
	}
	add_float_out(cache, reg, MRFT_N64_FPR_32, fpr_number);
	return reg;
}

uint8_t mips32_alloc_float_out_64(struct mips32_state* state, struct mips32_reg_cache* cache, uint8_t fpr_number)
{
	int i;
	int reg = -1;
	uint32_t mask = cache->allocated_float;

	assert(cache_in_sync(cache));
	assert(cache->opcode_float != (mips32_saved_float_mask | mips32_temp_float_mask));

	for (i = FindFirstSet(mask); i >= 0; mask &= ~BIT(i), i = FindFirstSet(mask)) {
		struct mips32_reg_float* entry = &cache->reg_float[i];
		/* If a value known to be 'float' will be written by the N64 as
		 * 'double'... the former 'float' value does not need to be written.
		 * On MIPS I, this is a "register pair merge", and the value of the
		 * counterpart register does not need to be written back either.
		 * On MIPS III, this is discarding the 'float' value to write a new
		 * 'double' value.
		 * Using an unknown layout, the target FPR's value is discarded,
		 * and the counterpart register (odd for even and even for odd) is
		 * written back. */
		if (entry->source.fpr_number == fpr_number && entry->type == MRFT_N64_FPR_32) {
			discard_float(cache, i);
		} else if (entry->source.fpr_number == (fpr_number ^ 1) && entry->type == MRFT_N64_FPR_32) {
			mips32_free_specific_float(state, cache, i);
		} else if (entry->source.fpr_number == fpr_number && entry->type == MRFT_N64_FPR_64) {
			reg = i;
		}
	}

	if (reg == -1) {
		reg = choose_float(state, cache, true);
	}
	add_float_out(cache, reg, MRFT_N64_FPR_64, fpr_number);
	return reg;
}

void mips32_discard_float(struct mips32_reg_cache* cache, uint8_t fpr_number)
{
	int i;
	uint32_t mask = cache->allocated_float;

	assert(cache_in_sync(cache));

	for (i = FindFirstSet(mask); i >= 0; mask &= ~BIT(i), i = FindFirstSet(mask)) {
		struct mips32_reg_float* entry = &cache->reg_float[i];
		if (entry->type != MRFT_UNUSED && entry->source.fpr_number == fpr_number) {
			discard_float(cache, i);
		}
	}
}

void mips32_set_hi_native(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	int i;
	uint32_t mask = cache->allocated_int;

	for (i = FindFirstSet(mask); i >= 0; mask &= ~BIT(i), i = FindFirstSet(mask)) {
		struct mips32_reg_int* entry = &cache->reg_int[i];
		if (entry->source.ptr == &g_state.regs.hi
		 && (entry->type == MRIT_MEM_HI32 || entry->type == MRIT_MEM_LO32)) {
			discard_int_const(cache, i);
		}
	}

	cache->native_hi = true;
}

void mips32_unset_hi_native(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	cache->native_hi = false;
}

void mips32_set_lo_native(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	int i;
	uint32_t mask = cache->allocated_int;

	for (i = FindFirstSet(mask); i >= 0; mask &= ~BIT(i), i = FindFirstSet(mask)) {
		struct mips32_reg_int* entry = &cache->reg_int[i];
		if (entry->source.ptr == &g_state.regs.lo
		 && (entry->type == MRIT_MEM_HI32 || entry->type == MRIT_MEM_LO32)) {
			discard_int_const(cache, i);
		}
	}

	cache->native_lo = true;
}

void mips32_unset_lo_native(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	cache->native_lo = false;
}

void mips32_save_to_stack(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	int i, hi = -1, lo = -1;
	size_t stack_offset = STACK_OFFSET_TEMP_INT;
	uint32_t mask = cache->allocated_int & ~cache->constant_int & mips32_temp_int_mask;

	for (i = FindFirstSet(mask); i >= 0; mask &= ~BIT(i), i = FindFirstSet(mask)) {
		mips32_sw(state, i, stack_offset, REG_STACK);
		stack_offset += 4;
	}

	mask = mips32_temp_int_mask;
	if (cache->native_lo) {
		lo = FindFirstSet(mask);
		mask &= ~BIT(lo);
		mips32_mflo(state, lo);
	}
	if (cache->native_hi) {
		hi = FindFirstSet(mask);
		mask &= ~BIT(hi);
		mips32_mfhi(state, hi);
	}
	if (cache->native_lo) {
		mips32_sw(state, lo, stack_offset, REG_STACK);
		stack_offset += 4;
	}
	if (cache->native_hi) {
		mips32_sw(state, hi, stack_offset, REG_STACK);
		stack_offset += 4;
	}

	/* If this fails, integers overlap floats on the stack */
	assert(stack_offset <= STACK_OFFSET_TEMP_FLOAT);

	stack_offset = STACK_OFFSET_TEMP_FLOAT;
	mask = cache->allocated_float & mips32_temp_float_mask;
	for (i = FindFirstSet(mask); i >= 0; mask &= ~BIT(i), i = FindFirstSet(mask)) {
		struct mips32_reg_float* entry = &cache->reg_float[i];
		switch (entry->type) {
		case MRFT_N64_FPR_32:
			mips32_swc1(state, i, stack_offset, REG_STACK);
			/* Keep the stack aligned by 8 for the next SDC1 */
			stack_offset += 8;
			break;
		case MRFT_N64_FPR_64:
			mips32_sdc1(state, i, stack_offset, REG_STACK);
			stack_offset += 8;
			break;
		default:
			break;
		}
	}
}

void mips32_load_from_stack(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	int i, hi = -1, lo = -1;
	size_t stack_offset = STACK_OFFSET_TEMP_INT;
	uint32_t mask = cache->allocated_int & ~cache->constant_int & mips32_temp_int_mask;

	for (i = FindFirstSet(mask); i >= 0; mask &= ~BIT(i), i = FindFirstSet(mask)) {
		mips32_lw(state, i, stack_offset, REG_STACK);
		stack_offset += 4;
	}

	mask = cache->constant_int & ~BIT(0) & mips32_temp_int_mask;
	for (i = FindFirstSet(mask); i >= 0; mask &= ~BIT(i), i = FindFirstSet(mask)) {
		mips32_i32(state, i, cache->reg_int[i].constant.value);
	}

	mask = mips32_temp_int_mask;
	if (cache->native_lo) {
		lo = FindFirstSet(mask);
		mask &= ~BIT(lo);
		mips32_lw(state, lo, stack_offset, REG_STACK);
		stack_offset += 4;
	}
	if (cache->native_hi) {
		hi = FindFirstSet(mask);
		mask &= ~BIT(hi);
		mips32_lw(state, hi, stack_offset, REG_STACK);
		stack_offset += 4;
	}
	if (cache->native_lo) mips32_mtlo(state, lo);
	if (cache->native_hi) mips32_mthi(state, hi);

	/* If this fails, integers overlap floats on the stack */
	assert(stack_offset <= STACK_OFFSET_TEMP_FLOAT);

	stack_offset = STACK_OFFSET_TEMP_FLOAT;
	mask = cache->allocated_float & mips32_temp_float_mask;
	for (i = FindFirstSet(mask); i >= 0; mask &= ~BIT(i), i = FindFirstSet(mask)) {
		struct mips32_reg_float* entry = &cache->reg_float[i];
		switch (entry->type) {
		case MRFT_N64_FPR_32:
			mips32_lwc1(state, i, stack_offset, REG_STACK);
			/* Keep the stack aligned by 8 for the next SDC1 */
			stack_offset += 8;
			break;
		case MRFT_N64_FPR_64:
			mips32_ldc1(state, i, stack_offset, REG_STACK);
			stack_offset += 8;
			break;
		default:
			break;
		}
	}
}

void mips32_regenerate_cache(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	int i, hi = -1, lo = -1;
	uint32_t regs;

	assert(cache_in_sync(cache));

	/* 1. Regenerate HI and LO if the cache says the native HI and LO have the
	 * most recent version. */
	regs = mips32_temp_int_mask;

	if (cache->native_lo) {
		lo = FindFirstSet(regs);
		regs &= ~BIT(lo);
		mips32_lw(state, lo, STATE_OFFSET(regs.lo) + LO32_64_BYTES, REG_STATE);
	}
	if (cache->native_hi) {
		hi = FindFirstSet(regs);
		regs &= ~BIT(hi);
		mips32_lw(state, hi, STATE_OFFSET(regs.hi) + LO32_64_BYTES, REG_STATE);
	}

	if (cache->native_lo) mips32_mtlo(state, lo);
	if (cache->native_hi) mips32_mthi(state, hi);

	/* 2. Regenerate integer constants, because those may be useful to reload
	 * some data loaded from memory at addresses near the constant. */
	regs = cache->constant_int & ~BIT(0);
	for (i = FindFirstSet(regs); i >= 0; regs &= ~BIT(i), i = FindFirstSet(regs)) {
		mips32_i32(state, i, cache->reg_int[i].constant.value);
	}

	/* 3. Regenerate integer memory data. This may use the constants. */
	regs = cache->allocated_int & ~cache->constant_int;
	for (i = FindFirstSet(regs); i >= 0; regs &= ~BIT(i), i = FindFirstSet(regs)) {
		struct mips32_reg_int* entry = &cache->reg_int[i];
		uint8_t mem_reg = 0;
		int16_t mem_off = 0;
		switch (entry->type) {
		case MRIT_MEM_LO32:
		case MRIT_MEM_SE32:
		case MRIT_MEM_ZE32:
		case MRIT_MEM_OE32:
			get_int_mem_ref(state, cache, LO32_64(entry->source.ptr), &mem_reg, &mem_off);
			mips32_lw(state, i, mem_off, mem_reg);
			break;
		case MRIT_MEM_HI32:
			get_int_mem_ref(state, cache, HI32_64(entry->source.ptr), &mem_reg, &mem_off);
			mips32_lw(state, i, mem_off, mem_reg);
			break;
		case MRIT_MEM_32:
			get_int_mem_ref(state, cache, entry->source.ptr, &mem_reg, &mem_off);
			mips32_lw(state, i, mem_off, mem_reg);
			break;
		default:
			break;
		}
	}

	/* 4. Regenerate floating-point memory data. This may use the Nintendo 64
	 * floating-point register pointers. */
	regs = cache->allocated_float;
	for (i = FindFirstSet(regs); i >= 0; regs &= ~BIT(i), i = FindFirstSet(regs)) {
		struct mips32_reg_float* entry = &cache->reg_float[i];
		uint8_t mem_reg = get_float_mem_ref(state, cache, entry->type, entry->source.fpr_number);
		switch (entry->type) {
		case MRFT_N64_FPR_32:
			mips32_lwc1(state, i, 0, mem_reg);
			break;
		case MRFT_N64_FPR_64:
			mips32_ldc1(state, i, 0, mem_reg);
			break;
		default:
			break;
		}
	}
}

void mips32_free(struct mips32_state* state, struct mips32_reg_cache* cache, uint32_t int_mask, bool mdu, uint32_t fpr_mask)
{
	int i;
	uint32_t regs, fpr_regs;

	assert(cache_in_sync(cache));

	int_mask &= mips32_temp_int_mask | mips32_saved_int_mask;
	fpr_mask &= mips32_temp_float_mask | mips32_saved_float_mask;

	/* 1. Discard all non-dirty registers except those that contain constants,
	 * useful for writing back integer values, and addresses of Nintendo 64
	 * floating-point registers. Also discard MRIT_TEMP registers that contain
	 * no constants. (Speeds up later code.) */
	regs = cache->allocated_int & ~cache->dirty_int & ~cache->constant_int & int_mask;
	for (i = FindFirstSet(regs); i >= 0; regs &= ~BIT(i), i = FindFirstSet(regs)) {
		struct mips32_reg_int* entry = &cache->reg_int[i];
		if (entry->type == MRIT_MEM_32
		 && (((uint8_t*) entry->source.ptr >= (uint8_t*) &g_state.regs.cp1_s[0]
		   && (uint8_t*) entry->source.ptr <= (uint8_t*) &g_state.regs.cp1_s[31])
		  || ((uint8_t*) entry->source.ptr >= (uint8_t*) &g_state.regs.cp1_d[0]
		   && (uint8_t*) entry->source.ptr <= (uint8_t*) &g_state.regs.cp1_d[31]))) {
			/* Do nothing */
		} else {
			discard_int(cache, i);
		}
	}

	regs = cache->allocated_float & ~cache->dirty_float & fpr_mask;
	for (i = FindFirstSet(regs); i >= 0; regs &= ~BIT(i), i = FindFirstSet(regs)) {
		discard_float(cache, i);
	}

	/* 2. Write out everything that doesn't need to be extended, then compute
	 * extensions, then write those.
	 * Doing this all in batches is better than interleaving extensions and
	 * stores on superscalar processors. It may also offer a benefit on some
	 * CPUs' pipelines given the lack of immediate data dependencies. */
	regs = cache->dirty_int & int_mask;
	for (i = FindFirstSet(regs); i >= 0; regs &= ~BIT(i), i = FindFirstSet(regs)) {
		struct mips32_reg_int* entry = &cache->reg_int[i];
		write_self_int(state, cache, i);
		switch (entry->type) {
		case MRIT_MEM_HI32:
		case MRIT_MEM_LO32:
		case MRIT_MEM_32:
		case MRIT_TEMP:
			discard_int(cache, i);
			break;
		default:
			break;
		}
	}

	regs = cache->dirty_int & int_mask;
	for (i = FindFirstSet(regs); i >= 0; regs &= ~BIT(i), i = FindFirstSet(regs)) {
		extend_int_into_self(state, cache, i);
	}

	regs = cache->dirty_int & int_mask;
	for (i = FindFirstSet(regs); i >= 0; regs &= ~BIT(i), i = FindFirstSet(regs)) {
		struct mips32_reg_int* entry = &cache->reg_int[i];
		write_extension_int(state, cache, i);
		switch (entry->type) {
		case MRIT_MEM_SE32:
		case MRIT_MEM_ZE32:
		case MRIT_MEM_OE32:
			discard_int(cache, i);
			break;
		default:
			break;
		}
	}

	assert((cache->dirty_int & int_mask) == 0);

	/* 3. Discard constants, which are no longer needed. */
	regs = cache->constant_int & int_mask;
	for (i = FindFirstSet(regs); i >= 0; regs &= ~BIT(i), i = FindFirstSet(regs)) {
		discard_int_const(cache, i);
	}

	assert((cache->constant_int & int_mask) == 0);

	cache->opcode_int &= ~int_mask;

	/* 4. Write floating-point values. */

	/* First write those for which we have FPR pointers loaded. */
	regs = cache->dirty_float & fpr_mask;
	for (i = FindFirstSet(regs); i >= 0; regs &= ~BIT(i), i = FindFirstSet(regs)) {
		struct mips32_reg_float* entry = &cache->reg_float[i];
		uint8_t mem_reg = get_fpr_ptr_reg(cache, entry->type, entry->source.fpr_number);
		if (mem_reg != 0) {
			write_float(state, cache, i);
			discard_float(cache, i);
		}
	}

	/* Then get rid of FPR address pointers that have been left in registers.
	 * They must be pointers to non-dirty floating-point values by now. */
	regs = cache->allocated_int & int_mask;
	for (i = FindFirstSet(regs); i >= 0; regs &= ~BIT(i), i = FindFirstSet(regs)) {
		discard_int(cache, i);
	}

	assert((cache->allocated_int & int_mask) == 0);

	/* Finally, load as many integer registers as possible with addresses, and
	 * write the remaining floating-point registers.
	 * Interleaved in the same manner as the integer register extensions. */
	fpr_regs = cache->dirty_float & fpr_mask;
	while (fpr_regs != 0) {
		regs = int_mask;
		if (regs != 0) {
			while (fpr_regs != 0 && regs != 0) {
				int reg = FindFirstSet(fpr_regs);
				uint8_t mem_reg = FindFirstSet(regs);
				struct mips32_reg_float* entry = &cache->reg_float[reg];
				void* fpr_ptr_ptr = get_fpr_ptr_ptr(entry->type, entry->source.fpr_number);

				add_int_in(cache, mem_reg, MRIT_MEM_32, fpr_ptr_ptr);
				mips32_lw(state, mem_reg, (uint8_t*) fpr_ptr_ptr - (uint8_t*) &g_state, REG_STATE);

				fpr_regs &= ~BIT(reg);
				regs &= ~BIT(mem_reg);
			}

			regs = cache->dirty_float & fpr_mask & ~fpr_regs;
			for (i = FindFirstSet(regs); i >= 0; regs &= ~BIT(i), i = FindFirstSet(regs)) {
				struct mips32_reg_float* entry = &cache->reg_float[i];
				uint8_t fpr_ptr_reg = get_fpr_ptr_reg(cache, entry->type, entry->source.fpr_number);
				write_float(state, cache, i);
				discard_float(cache, i);

				assert(fpr_ptr_reg != 0);
				discard_int(cache, fpr_ptr_reg);
			}
		} else {
			/* No integer registers are being freed. Use the regular function
			 * to free floating-point registers. */
			int reg = FindFirstSet(fpr_regs);
			mips32_free_specific_float(state, cache, reg);
			fpr_regs &= ~BIT(reg);
		}
	}

	assert((cache->dirty_float & fpr_mask) == 0);
	assert((cache->allocated_float & fpr_mask) == 0);

	cache->opcode_float &= ~fpr_mask;

	/* 6. Write LO/HI. This is as far as possible to the MULT[U]/DIV[U] that
	 * generated the result in order to give the native MDU some time.
	 * Interleaved in the same manner as the integer register extensions. */
	if (mdu) {
		int hi = -1, lo = -1;

		regs = mips32_temp_int_mask & int_mask;
		if (regs == 0 || (regs & ~BIT(FindFirstSet(regs))) == 0) {
			/* Fewer than 2 integer registers are being freed. Use the regular
			 * way to free HI and LO. */
			if (cache->native_lo) {
				mips32_mflo(state, REG_EMERGENCY);
				mips32_sw(state, REG_EMERGENCY, STATE_OFFSET(regs.lo) + LO32_64_BYTES, REG_STATE);
				mips32_sra(state, REG_EMERGENCY, REG_EMERGENCY, 31);
				mips32_sw(state, REG_EMERGENCY, STATE_OFFSET(regs.lo) + HI32_64_BYTES, REG_STATE);
			}
			if (cache->native_hi) {
				mips32_mfhi(state, REG_EMERGENCY);
				mips32_sw(state, REG_EMERGENCY, STATE_OFFSET(regs.hi) + LO32_64_BYTES, REG_STATE);
				mips32_sra(state, REG_EMERGENCY, REG_EMERGENCY, 31);
				mips32_sw(state, REG_EMERGENCY, STATE_OFFSET(regs.hi) + HI32_64_BYTES, REG_STATE);
			}
		} else {
			if (cache->native_lo) {
				lo = FindFirstSet(regs);
				regs &= ~BIT(lo);
				mips32_mflo(state, lo);
			}
			if (cache->native_hi) {
				hi = FindFirstSet(regs);
				regs &= ~BIT(hi);
				mips32_mfhi(state, hi);
			}

			if (cache->native_lo) mips32_sw(state, lo, STATE_OFFSET(regs.lo) + LO32_64_BYTES, REG_STATE);
			if (cache->native_hi) mips32_sw(state, hi, STATE_OFFSET(regs.hi) + LO32_64_BYTES, REG_STATE);

			if (cache->native_lo) mips32_sra(state, lo, lo, 31);
			if (cache->native_hi) mips32_sra(state, hi, hi, 31);

			if (cache->native_lo) mips32_sw(state, lo, STATE_OFFSET(regs.lo) + HI32_64_BYTES, REG_STATE);
			if (cache->native_hi) mips32_sw(state, hi, STATE_OFFSET(regs.hi) + HI32_64_BYTES, REG_STATE);
		}

		cache->native_hi = cache->native_lo = false;
	}
}

void mips32_free_all(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	mips32_free(state, cache, ~(uint32_t) 0, true, ~(uint32_t) 0);
}

void mips32_free_all_except_ints(struct mips32_state* state, struct mips32_reg_cache* cache, uint32_t except_mask)
{
	mips32_free(state, cache, ~except_mask, true, ~(uint32_t) 0);
}

void mips32_free_caller_saved(struct mips32_state* state, struct mips32_reg_cache* cache)
{
	mips32_free(state, cache, mips32_temp_int_mask, true, mips32_temp_float_mask);
}

void mips32_free_specific_int(struct mips32_state* state, struct mips32_reg_cache* cache, uint8_t reg)
{
	if (cache->reg_int[reg].dirty) {
		write_self_int(state, cache, reg);
		extend_int_into_self(state, cache, reg);
		write_extension_int(state, cache, reg);
	}

	discard_int_const(cache, reg);
}

void mips32_free_specific_float(struct mips32_state* state, struct mips32_reg_cache* cache, uint8_t reg)
{
	if (cache->reg_float[reg].dirty) {
		write_float(state, cache, reg);
	}

	discard_float(cache, reg);
}

void mips32_start_opcode(const struct mips32_state* state, struct mips32_reg_cache* cache, const char* op_name)
{
	if (cache->opcode_level < 2) {
		cache->op_names[cache->opcode_level] = op_name;
	}
	cache->opcode_level++;

	if (cache->opcode_level >= 2)
		DebugMessage(M64MSG_VERBOSE, "Compiling %s/%s at native %" PRIXPTR, cache->op_names[0], cache->op_names[1], state->code);
	else
		DebugMessage(M64MSG_VERBOSE, "Compiling %s at native %" PRIXPTR, cache->op_names[0], state->code);
}

void mips32_end_opcode(const struct mips32_state* state, struct mips32_reg_cache* cache)
{
	assert(cache->opcode_level != 0);

	cache->opcode_level--;
	if (cache->opcode_level < 2) {
		cache->op_names[cache->opcode_level] = NULL;
	}
	if (cache->opcode_level == 0) {
		int i;
		uint32_t mask = cache->opcode_int;
		for (i = FindFirstSet(mask); i >= 0; mask &= ~BIT(i), i = FindFirstSet(mask)) {
			if (cache->reg_int[i].type == MRIT_TEMP) {
				discard_int(cache, i);
			}
		}
		cache->opcode_int = cache->opcode_float = 0;
	}
}
