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

#ifndef M64P_TRACE_JIT_MIPS32_NATIVE_REGCACHE_H
#define M64P_TRACE_JIT_MIPS32_NATIVE_REGCACHE_H

#include <stdbool.h>
#include <stdint.h>

/* Forward declaration of this type instead of circular include. */
struct mips32_state;

enum mips32_reg_int_type {
	/* Not mapped to any value. May still contain a constant; in that case,
	 * the constant is available to any instruction that wants it, but it's
	 * not linked to the current instruction. */
	MRIT_UNUSED,
	/* Mapped to a temporary value assigned to the current instruction. Not
	 * backed by any memory, so the 'source' member of the mips32_reg_int
	 * structure corresponding to it is undefined. */
	MRIT_TEMP,
	/* Mapped to the high 32 bits of a 64-bit memory value. The 'source'
	 * member of the mips32_reg_int structure corresponding to it points to
	 * the first byte of the 64-bit value. Another register has the type
	 * MRIT_MEM_LO32 and it has the other half of the value. */
	MRIT_MEM_HI32,
	/* Mapped to the low 32 bits of a 64-bit memory value. The 'source' member
	 * of the mips32_reg_int structure corresponding to it points to the
	 * first byte of the 64-bit value. Another register has the type
	 * MRIT_MEM_HI32 and it has the other half of the value. */
	MRIT_MEM_LO32,
	/* Mapped to the low 32 bits of a 64-bit memory value, with the additional
	 * stipulation that its high 32 bits are the sign-extension of these low
	 * 32 bits. The 'source' member of the mips32_reg_int structure
	 * corresponding to it points to the first byte of the 64-bit value. */
	MRIT_MEM_SE32,
	/* Mapped to the low 32 bits of a 64-bit memory value, with the additional
	 * stipulation that its high 32 bits are unset. The 'source' member of the
	 * mips32_reg_int structure corresponding to it points to the first byte
	 * of the 64-bit value. */
	MRIT_MEM_ZE32,
	/* Mapped to the low 32 bits of a 64-bit memory value, with the additional
	 * stipulation that its high 32 bits are set. The 'source' member of the
	 * mips32_reg_int structure corresponding to it points to the first byte
	 * of the 64-bit value. */
	MRIT_MEM_OE32,
	/* Mapped to a 32-bit memory value. The 'source' member of the
	 * mips32_reg_int structure corresponding to it points to the first byte
	 * of the 32-bit value. */
	MRIT_MEM_32,
};

struct mips32_reg_int {
	enum mips32_reg_int_type type;
	union {
		void* ptr;
		uintptr_t addr;
	} source;
	bool dirty;
	bool is_constant;
	union {
		void* ptr;
		uintptr_t addr;
		uint32_t value;
	} constant;
};

enum mips32_reg_float_type {
	/* Not mapped to any value. */
	MRFT_UNUSED,
	/* Mapped to a Nintendo 64 32-bit floating-point register. Its number is
	 * in the 'source.fpr_number' member of the corresponding
	 * mips32_reg_float structure. */
	MRFT_N64_FPR_32,
	/* Mapped to a Nintendo 64 64-bit floating-point register. Its number is
	 * in the 'source.fpr_number' member of the corresponding
	 * mips32_reg_float structure. */
	MRFT_N64_FPR_64,
};

struct mips32_reg_float {
	enum mips32_reg_float_type type;
	union {
		uint8_t fpr_number;
	} source;
	bool dirty;
};

struct mips32_reg_cache {
	struct mips32_reg_int reg_int[32];
	struct mips32_reg_float reg_float[32];
	/* Keeps the names of opcodes being compiled at opcode_level <= 2 for
	 * diagnostic purposes (for example, they are available to debuggers). */
	const char* op_names[2];
	/* true if the native HI register has the result of the latest 32-bit
	 * multiplication, division or MTHI done on the Nintendo 64; false if
	 * g_state.regs.hi has the latest result. */
	bool native_hi;
	/* true if the native LO register has the result of the latest 32-bit
	 * multiplication, division or MTLO done on the Nintendo 64; false if
	 * g_state.regs.lo has the latest result. */
	bool native_lo;
	/* Mask representing the integer registers allocated by (only) the current
	 * instruction. */
	uint32_t opcode_int;
	/* Mask representing the floating-point registers allocated by (only) the
	 * current instruction. */
	uint32_t opcode_float;
	/* Number of start_opcode calls that have not been paired with
	 * end_opcode calls. Nesting instructions is intended to compile a
	 * branch's delay slot while keeping the branch's registers allocated. */
	uint32_t opcode_level;
	/* Mask representing the integer registers allocated as of the current
	 * instruction. Does not include constants. Cached from reg_int[n].type !=
	 * MRIT_UNUSED. */
	uint32_t allocated_int;
	/* Mask representing the floating-point registers allocated as of the
	 * current instruction. Cached from reg_float[n].type != MRFT_UNUSED. */
	uint32_t allocated_float;
	/* Mask representing the integer registers that are dirty as of the
	 * current instruction. Cached from reg_int[n].dirty. */
	uint32_t dirty_int;
	/* Mask representing the floating-point registers that are dirty as of the
	 * current instruction. Cached from reg_float[n].dirty. */
	uint32_t dirty_float;
	/* Mask representing the integer registers containing constants as of the
	 * current instruction. Cached from reg_int[n].is_constant. */
	uint32_t constant_int;
};

/* Mask representing the callee-saved integer registers allocated to the
 * JIT. Bit #n is set if register $n is callee-saved and allocated.
 * 0 is the least-significant bit. */
extern uint32_t mips32_saved_int_mask;

/* Mask representing the caller-saved integer registers allocated to the
 * JIT. Bit #n is set if register $n is caller-saved and allocated.
 * 0 is the least-significant bit. */
extern uint32_t mips32_temp_int_mask;

/* Mask representing the callee-saved floating-point registers allocated to
 * the JIT. Bit #n is set if register $n is callee-saved and allocated.
 * 0 is the least-significant bit. */
extern uint32_t mips32_saved_float_mask;

/* Mask representing the caller-saved floating-point registers allocated to
 * the JIT. Bit #n is set if register $n is caller-saved and allocated.
 * 0 is the least-significant bit. */
extern uint32_t mips32_temp_float_mask;

extern void mips32_init_reg_cache(struct mips32_reg_cache* cache);

extern void mips32_copy_reg_cache(struct mips32_reg_cache* dst, const struct mips32_reg_cache* src);

/* Allocates a register to contain a temporary value for the duration of the
 * current instruction.
 *
 * The chosen register is marked as dirty. No particular value is loaded into
 * it.
 */
extern uint8_t mips32_alloc_int_temp(struct mips32_state* state, struct mips32_reg_cache* cache);

/* Allocates the given (native) register to contain a temporary value for the
 * duration of the current instruction.
 *
 * The register is marked as dirty.
 */
extern void mips32_alloc_specific_int_temp(struct mips32_state* state, struct mips32_reg_cache* cache, uint8_t reg);

/* Allocates a register to contain the upper 32 bits of the given 64-bit
 * value.
 *
 * The chosen register is not marked as dirty. If an allocation of type SE32,
 * ZE32 or OE32 exists for the same memory value, the extension of the lower
 * 32 bits is loaded into the chosen register, and the allocation for the
 * lower 32 bits is converted to a LO32 allocation. Otherwise, the upper 32
 * bits of the memory value are loaded into the chosen register.
 *
 * Because of this replacement behavior, if both the upper and the lower 32
 * bits are needed, the upper 32 bits should be allocated with this function
 * BEFORE the lower 32 bits with mips32_alloc_int_in_lo32.
 */
extern uint8_t mips32_alloc_int_in_hi32(struct mips32_state* state, struct mips32_reg_cache* cache, void* memory);

/* Allocates a register to contain the lower 32 bits of the given 64-bit
 * value.
 *
 * The chosen register is not marked as dirty. If an allocation of type SE32,
 * ZE32 or OE32 exists for the same memory value, it is left alone until the
 * next call to mips32_alloc_int_in_hi32. Otherwise, the lower 32 bits
 * of the memory value are loaded into the chosen register.
 *
 * Because of this replacement behavior, if both the upper and the lower 32
 * bits are needed, the upper 32 bits should be allocated with
 * mips32_alloc_int_in_hi32 BEFORE the lower 32 bits with this function.
 */
extern uint8_t mips32_alloc_int_in_lo32(struct mips32_state* state, struct mips32_reg_cache* cache, void* memory);

/* Allocates a register to contain the given 32-bit value.
 *
 * The chosen register is not marked as dirty. If an allocation exists and is
 * of a type suggesting a 64-bit value, the behavior is undefined. Otherwise,
 * the memory value is loaded entirely into the chosen register.
 */
extern uint8_t mips32_alloc_int_in_32(struct mips32_state* state, struct mips32_reg_cache* cache, void* memory);

/* Allocates a register that will contain the upper 32 bits of the given
 * 64-bit value.
 *
 * The chosen register is marked as dirty. If an allocation of type SE32, ZE32
 * or OE32 exists for the same memory value, it is converted to an allocation
 * of type LO32. No particular value is loaded into the chosen register.
 *
 * The chosen register may also be used as input to the current instruction.
 * If the output register allocated by this function is written, then one of
 * the registers allocated to an input register is read, it may therefore have
 * the value written to the output register. Because of this, the output
 * register should only be written ONCE.
 */
extern uint8_t mips32_alloc_int_out_hi32(struct mips32_state* state, struct mips32_reg_cache* cache, void* memory);

/* Allocates a register that will contain the lower 32 bits of the given
 * 64-bit value.
 *
 * The chosen register is marked as dirty. If an allocation of type SE32, ZE32
 * or OE32 exists for the same memory value, it is converted to an allocation
 * of type LO32. No particular value is loaded into the chosen register.
 *
 * The chosen register may also be used as input to the current instruction.
 * If the output register allocated by this function is written, then one of
 * the registers allocated to an input register is read, it may therefore have
 * the value written to the output register. Because of this, the output
 * register should only be written ONCE.
 */
extern uint8_t mips32_alloc_int_out_lo32(struct mips32_state* state, struct mips32_reg_cache* cache, void* memory);

/* Declares that the upper 32 bits of the given 64-bit memory value are no
 * longer needed in a register and can be regenerated from the extension (ex)
 * of the lower 32 bits.
 *
 * The given memory value must have had its lower 32 bits allocated for output
 * (with mips32_alloc_int_out_lo32) during the current instruction. That
 * register has its type set to the 'type' given as a parameter, and any
 * register containing the upper 32 bits is deallocated.
 */
extern void mips32_set_int_ex32(struct mips32_reg_cache* cache, void* memory, enum mips32_reg_int_type type);

/* Forwards to mips32_set_int_ex32 with 'type' set to MRIT_MEM_SE32 (sign). */
extern void mips32_set_int_se32(struct mips32_reg_cache* cache, void* memory);

/* Forwards to mips32_set_int_ex32 with 'type' set to MRIT_MEM_ZE32 (zero). */
extern void mips32_set_int_ze32(struct mips32_reg_cache* cache, void* memory);

/* Forwards to mips32_set_int_ex32 with 'type' set to MRIT_MEM_OE32 (ones). */
extern void mips32_set_int_oe32(struct mips32_reg_cache* cache, void* memory);

/* Allocates a register that will contain the given 32-bit value.
 *
 * The chosen register is not marked as dirty. If an allocation exists and is
 * of a type suggesting a 64-bit value, the behavior is undefined.
 * No particular value is loaded into the chosen register.
 *
 * The chosen register may also be used as input to the current instruction.
 * If the output register allocated by this function is written, then one of
 * the registers allocated to an input register is read, it may therefore have
 * the value written to the output register. Because of this, the output
 * register should only be written ONCE.
 */
extern uint8_t mips32_alloc_int_out_32(struct mips32_state* state, struct mips32_reg_cache* cache, void* memory);

/* Allocates a register that contains a constant.
 *
 * The chosen register is not marked as dirty. */
extern uint8_t mips32_alloc_int_const(struct mips32_state* state, struct mips32_reg_cache* cache, uint32_t constant);

/* Declares that native integer registers no longer contain the most recent
 * value at the given memory (32-bit or 64-bit).
 *
 * The value in the native registers is not written back.
 */
extern void mips32_discard_int(struct mips32_reg_cache* cache, void* memory);

/* Marks the native HI register as containing the most recent (sign-extended)
 * 32-bit result. */
extern void mips32_set_hi_native(struct mips32_state* state, struct mips32_reg_cache* cache);

/* Marks the Nintendo 64 HI register as containing the most recent result. */
extern void mips32_unset_hi_native(struct mips32_state* state, struct mips32_reg_cache* cache);

/* Marks the native LO register as containing the most recent (sign-extended)
 * 32-bit result. */
extern void mips32_set_lo_native(struct mips32_state* state, struct mips32_reg_cache* cache);

/* Marks the Nintendo 64 LO register as containing the most recent result. */
extern void mips32_unset_lo_native(struct mips32_state* state, struct mips32_reg_cache* cache);

/* Allocates a register to contain the given Nintendo 64 32-bit floating-point
 * register.
 *
 * The address of the FPR's actual storage must also be gotten in an integer
 * register. This counts against the active instruction.
 *
 * The chosen register is not marked as dirty. The floating-point value is
 * loaded into the chosen register.
 */
extern uint8_t mips32_alloc_float_in_32(struct mips32_state* state, struct mips32_reg_cache* cache, uint8_t fpr_number);

/* Allocates a register to contain the given Nintendo 64 64-bit floating-point
 * register.
 *
 * The address of the FPR's actual storage must also be gotten in an integer
 * register. This counts against the active instruction.
 *
 * The chosen register is not marked as dirty. The floating-point value is
 * loaded into the chosen register.
 */
extern uint8_t mips32_alloc_float_in_64(struct mips32_state* state, struct mips32_reg_cache* cache, uint8_t fpr_number);

/* Allocates a register that will contain the value of given Nintendo 64
 * 32-bit floating-point register.
 *
 * The address of the FPR's actual storage must also be gotten in an integer
 * register. This counts against the active instruction.
 *
 * The chosen register is marked as dirty. No particular value is loaded into
 * into the chosen register.
 */
extern uint8_t mips32_alloc_float_out_32(struct mips32_state* state, struct mips32_reg_cache* cache, uint8_t fpr_number);

/* Allocates a register that will contain the value of given Nintendo 64
 * 64-bit floating-point register.
 *
 * The address of the FPR's actual storage must also be gotten in an integer
 * register. This counts against the active instruction.
 *
 * The chosen register is marked as dirty. No particular value is loaded into
 * into the chosen register.
 */
extern uint8_t mips32_alloc_float_out_64(struct mips32_state* state, struct mips32_reg_cache* cache, uint8_t fpr_number);

/* Declares that native floating-point registers no longer contain the most
 * recent value of the given Nintendo 64 floating-point register.
 *
 * The value in the native registers is not written back.
 */
extern void mips32_discard_float(struct mips32_reg_cache* cache, uint8_t fpr_number);

/* Writes back, to the stack, the values of caller-saved registers. Does not
 * save registers containing constants to the stack.
 */
extern void mips32_save_to_stack(struct mips32_state* state, struct mips32_reg_cache* cache);

/* Reloads, from the stack, the values of caller-saved registers. Regenerates
 * registers containing constants.
 */
extern void mips32_load_from_stack(struct mips32_state* state, struct mips32_reg_cache* cache);

/* Reloads all native registers from their sources stated in the given
 * register cache after they have all been lost.
 *
 * This is only useful if a function calls mips32_free_all because it needs
 * the various values in the native registers to be written back to the
 * Nintendo 64, but it keeps another copy of the cache and needs the values
 * again as it jumps back to code which expects the values to be present in
 * the native registers.
 */
extern void mips32_regenerate_cache(struct mips32_state* state, struct mips32_reg_cache* cache);

/* Writes back the specified registers, then frees them.
 *
 * Overwrites integer registers having allocations of type SE32, ZE32 and OE32
 * with their own extension; doesn't use the stack to preserve values, etc.
 *
 * int_mask refers to the integer registers that must be freed.
 * mdu is true if the multiply/divide unit (MDU)'s registers, HI and LO, must
 * be freed.
 * fpr_mask refers to the floating-point registers that must be freed.
 */
void mips32_free(struct mips32_state* state, struct mips32_reg_cache* cache, uint32_t int_mask, bool mdu, uint32_t fpr_mask);

/* Writes back all registers, then frees them.
 *
 * Forwards to mips32_free with all bits set.
 */
extern void mips32_free_all(struct mips32_state* state, struct mips32_reg_cache* cache);

/* Writes back all registers except the integer registers referred to by the
 * given 'except_mask', then frees them.
 *
 * Forwards to mips32_free with all bits set except for integer registers in
 * except_mask.
 */
extern void mips32_free_all_except_ints(struct mips32_state* state, struct mips32_reg_cache* cache, uint32_t except_mask);

/* Writes back all caller-saved registers, then frees them, before calling C.
 *
 * Forwards to mips32_free with the appropriate bits set.
 */
extern void mips32_free_caller_saved(struct mips32_state* state, struct mips32_reg_cache* cache);

/* Writes back the given register, then frees it. */
extern void mips32_free_specific_int(struct mips32_state* state, struct mips32_reg_cache* cache, uint8_t reg);

/* Writes back the given register, then frees it. */
extern void mips32_free_specific_float(struct mips32_state* state, struct mips32_reg_cache* cache, uint8_t reg);

/* Marks the start of an instruction, increasing the instruction nesting level
 * by 1.
 *
 * Until the next time the instruction nesting level becomes 0:
 * - allocations of distinct values will not use another register already
 *   used for another purpose in the same instruction;
 * - new allocations are counted against the instruction for accounting
 *   purposes (a single instruction cannot request all allocatable registers
 *   then request one more).
 */
extern void mips32_start_opcode(const struct mips32_state* state, struct mips32_reg_cache* cache, const char* op_name);

/* Marks the end of an instruction, decreasing the instruction nesting level
 * by 1.
 *
 * If the instruction nesting level has reached 0:
 * - all allocations of type TEMP become unused, but their constants remain;
 * - new allocations are not counted against the same instruction for
 *   accounting purposes (a single instruction cannot request all allocatable
 *   registers then request one more). */
extern void mips32_end_opcode(const struct mips32_state* state, struct mips32_reg_cache* cache);

#endif /* !M64P_TRACE_JIT_MIPS32_NATIVE_REGCACHE_H */
