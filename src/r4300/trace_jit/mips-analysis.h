/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - mips-analysis.h                                         *
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

#ifndef M64P_TRACE_JIT_MIPS_ANALYSIS_H
#define M64P_TRACE_JIT_MIPS_ANALYSIS_H

#include <stdbool.h>
#include <stdint.h>

#define BIT(n) (UINT32_C(1) << (n))

struct ConstAnalysis {
	/* In each of these elements, representing the MIPS integer registers,
	 * a bit is set if the corresponding bit in IntBits is meaningful. */
	uint64_t IntMask[32];
	/* In each of these elements, representing the MIPS integer registers,
	 * a meaningful bit is set or unset according to its known value. */
	uint64_t IntBits[32];
};

struct WidthAnalysis {
	/* Each of these elements, representing the MIPS integer registers,
	 * contains that register's meaningful value width. Any bit above the
	 * highest meaningful bit is merely a copy of that one.
	 * (64 means every bit is meaningful. 0 is not acceptable.) */
	int8_t IntWidth[32];
	int8_t HIWidth;
	int8_t LOWidth;
};

enum RoundingModeSource {
	/* An operation does not use any FPU rounding mode. */
	RMS_NONE,
	/* An operation uses the rounding mode set on the Nintendo 64. */
	RMS_N64,
	/* An operation uses its own rounding mode. */
	RMS_OP
};

/* Returns the index of the lowest bit set in 'mask', among 0..31, or -1 if no
 * bit is set in 'mask'. Useful to iterate over register masks returned by
 * IntGetReads and IntGetWrites. */
extern int FindFirstSet(uint32_t mask);

/*
 * Initialises a constant analysis structure before the first instruction of a
 * trace, where only the value of $0 is known.
 */
extern void InitConstAnalysis(struct ConstAnalysis* CA);

/*
 * Updates a constant analysis structure.
 *
 * In:
 *   op: The opcode to update constants with.
 * In/out:
 *   CA: A constant analysis structure. Initially holding constants created
 *     by opcodes preceding 'op' in the trace, if any, it is updated with
 *     constants that exist after 'op' is done executing. Whether a register
 *     is newly considered constant or not depends on 'op' and the constancy
 *     of its operands.
 * Returns:
 *   'op', possibly simplified (e.g. to a move or NOP) if the known values of
 *   registers allow for it.
 */
extern uint32_t UpdateConstAnalysis(struct ConstAnalysis* CA, uint32_t op);

/*
 * Determines whether the given MIPS integer register has a constant value
 * before an operation would use it.
 */
extern bool IntIsKnown(const struct ConstAnalysis* CA, uint8_t Reg);

/*
 * Determines whether at least the bits given in the Mask are known among the
 * given MIPS integer register before an operation would use it.
 */
extern bool IntIsMaskKnown(const struct ConstAnalysis* CA, uint8_t Reg, uint64_t Mask);

/*
 * Gets the mask describing the bits whose value is known in the given MIPS
 * integer register.
 */
extern uint64_t IntGetKnownMask(const struct ConstAnalysis* CA, uint8_t Reg);

/*
 * Gets the intersection of the given mask and the mask describing the bits
 * whose value is known in the given MIPS integer register.
 */
extern uint64_t IntGetKnownMaskInMask(const struct ConstAnalysis* CA, uint8_t Reg, uint64_t Mask);

/*
 * Gets the value of the known bits in the given MIPS integer register. Known
 * bits are set or unset; unknown bits are unset.
 */
extern uint64_t IntGetKnownBits(const struct ConstAnalysis* CA, uint8_t Reg);

/*
 * Gets the value of the known bits in the given MIPS integer register, as
 * long as they are also in the given mask. Known bits are set or unset;
 * unknown or excluded bits are unset.
 */
extern uint64_t IntGetKnownBitsInMask(const struct ConstAnalysis* CA, uint8_t Reg, uint64_t Mask);

/*
 * Initialises a width analysis structure before the first instruction of a
 * trace, where only the width of $0 is known.
 */
extern void InitWidthAnalysis(struct WidthAnalysis* WA);

/*
 * Updates a width analysis structure.
 *
 * In:
 *   op: The opcode to update widths with.
 * In/out:
 *   CA: A width analysis structure. Initially holding the widths of values
 *     created by opcodes preceding 'op' in the trace, if any, it is updated
 *     with the widths of values that exist after 'op' is done executing.
 *     Whether a register's value is newly considered to be 32 bits wide or
 *     64 bits wide depends on 'op' and the widths of its operands.
 * Returns:
 *   'op', possibly simplified (e.g. to a 32-bit operation) if the known
 *   widths of registers allow for it.
 */
extern uint32_t UpdateWidthAnalysis(struct WidthAnalysis* WA, uint32_t op);

/*
 * Gets the number of meaningful bits in the given MIPS integer register,
 * above which any bits are just copies of the highest meaningful bit.
 */
extern int8_t IntGetWidth(const struct WidthAnalysis* WA, uint8_t Reg);

/*
 * Gets the number of meaningful bits in the MIPS HI register, above which
 * any bits are just copies of the highest meaningful bit.
 */
extern int8_t HIGetWidth(const struct WidthAnalysis* WA);

/*
 * Gets the number of meaningful bits in the MIPS HI register, above which
 * any bits are just copies of the highest meaningful bit.
 */
extern int8_t LOGetWidth(const struct WidthAnalysis* WA);

/*
 * Returns a bitfield representing the MIPS integer registers read by the
 * given opcode. Bit #n being set indicates that the opcode reads MIPS integer
 * register #n.
 */
extern uint32_t IntGetReads(uint32_t op);

/*
 * Returns a bitfield representing the MIPS integer registers written by the
 * given opcode. Bit #n being set indicates that the opcode writes to MIPS
 * integer register #n.
 */
extern uint32_t IntGetWrites(uint32_t op);

/*
 * Determines whether the given instruction is able to raise the TLB Refill
 * exception in general.
 */
extern bool CanRaiseTLBRefill(uint32_t op);

/*
 * Determines whether the given instruction is able to raise the Coprocessor
 * Unusable exception in general.
 */
extern bool CanRaiseCop1Unusable(uint32_t op);

/*
 * Determines whether the given instruction uses a FPU rounding mode, and if
 * so, whether that is the Nintendo 64 rounding mode or an operation-specific
 * rounding mode.
 */
extern enum RoundingModeSource UsesRoundingMode(uint32_t op);

#endif /* !M64P_TRACE_JIT_MIPS_ANALYSIS_H */
