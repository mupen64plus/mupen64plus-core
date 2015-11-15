/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - mips-analysis.c                                         *
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
#include <stdint.h>
#include <stdlib.h>

#include "mips-analysis.h"
#include "mips-make.h"
#include "mips-parse.h"
#include "mips-simplify.h"

static int8_t Min(int8_t A, int8_t B)
{
	return (A < B) ? A : B;
}

static int8_t Max(int8_t A, int8_t B)
{
	return (A > B) ? A : B;
}

static int8_t Clamp(int8_t Value, int8_t Lo, int8_t Hi)
{
	return (Value < Lo) ? Lo :
	       (Value > Hi) ? Hi :
	       Value;
}

static uint64_t OnesBelow(uint8_t Bits)
{
	assert(Bits <= 64);
	if (Bits == 64)
		return ~(uint64_t) 0;
	else
		return (UINT64_C(1) << Bits) - 1;
}

static uint64_t OnesAbove(uint8_t Bits)
{
	assert(Bits <= 64);
	if (Bits == 64)
		return 0;
	else
		return ~((UINT64_C(1) << Bits) - 1);
}

static uint64_t SE64(uint64_t Value, uint8_t Bits)
{
	if (Bits == 64)
		return Value;
	else
		return (Value & OnesBelow(Bits))
		     | ((Value & (UINT64_C(1) << (Bits - 1))) ? OnesAbove(Bits) : 0);
}

int FindFirstSet(uint32_t mask)
{
	if (mask == 0) {
		return -1;
	} else {
		int result = 0, guess = 16;
		/* Extract the lowest set bit at the position it occupies. */
		mask ^= mask & (mask - 1);
		/* Figure out the position of the bit using binary search. */
		while (guess != 0) {
			if ((mask >> (result + guess)) != 0) {
				result += guess;
			}
			guess /= 2;
		}
		return result;
	}
}

void InitConstAnalysis(struct ConstAnalysis* CA)
{
	uint8_t i;
	CA->IntMask[0] = ~(uint64_t) 0;
	CA->IntBits[0] = 0;
	for (i = 1; i < 32; i++) {
		CA->IntMask[i] = 0;
	}
}

static void IntSetKnownBits(struct ConstAnalysis* CA, uint8_t Reg, uint64_t Value)
{
	assert(Reg != 0 && Reg < 32);
	CA->IntBits[Reg] = Value;
}

static void IntSetKnownMask(struct ConstAnalysis* CA, uint8_t Reg, uint64_t Value)
{
	assert(Reg != 0 && Reg < 32);
	CA->IntMask[Reg] = Value;
}

static void IntSetKnown(struct ConstAnalysis* CA, uint8_t Reg, uint64_t Value)
{
	assert(Reg != 0 && Reg < 32);
	CA->IntBits[Reg] = Value;
	CA->IntMask[Reg] = ~(uint64_t) 0;
}

static void IntSetUnknown(struct ConstAnalysis* CA, uint8_t Reg)
{
	assert(Reg != 0 && Reg < 32);
	CA->IntMask[Reg] = 0;
}

/*
 * If the given register contains a simple constant, replaces the operation
 * that yielded it with a new operation that loads that constant.
 *
 * In:
 *   CA: Constant analysis structure, updated to reflect the operation.
 *   op: The original operation.
 *   Reg: The register into which the operation yielded its result.
 * Result:
 *   One of the following, if the operation yielded a constant result they
 *   can represent:
 *   - OR Reg, $0, $0 (loading 0);
 *   - NOR Reg, $0, $0 (loading -1);
 *   - ORI Reg, $0, Imm16 (loading 1..65535);
 *   - ADDIU Reg, $0, Imm16 (loading -32768..-2).
 *   Otherwise, 'op'.
 */
static uint32_t SimplifyResult(struct ConstAnalysis* CA, uint32_t op, uint8_t Reg)
{
	uint64_t Bits = IntGetKnownBits(CA, Reg);
	uint64_t Mask = IntGetKnownMask(CA, Reg);
	if (Mask == ~(uint64_t) 0) {
		if (Bits == 0) return MAKE_OR(Reg, 0, 0);
		else if (Bits == ~(uint64_t) 0) return MAKE_NOR(Reg, 0, 0);
		else if (Bits <= UINT64_C(0xFFFF)) return MAKE_ORI(Reg, 0, (uint16_t) Bits);
		else if (Bits >= UINT64_C(0xFFFFFFFFFFFF8000)) return MAKE_ADDIU(Reg, 0, (uint16_t) Bits);
		else return SimplifyOpcode(op);
	}
	else return SimplifyOpcode(op);
}

/*
 * If the given operation's 'Rs' register is known to contain 0, replaces the
 * 'Rs' register with $0.
 */
static uint32_t ReplaceZeroRs(struct ConstAnalysis* CA, uint32_t op)
{
	if (RS_OF(op) != 0) {
		uint64_t Bits = IntGetKnownBits(CA, RS_OF(op));
		uint64_t Mask = IntGetKnownMask(CA, RS_OF(op));
		if (Mask == ~(uint64_t) 0 && Bits == 0)
			return op & ~(UINT32_C(0x1F) << 21);
	}
	return op;
}

/*
 * If the given operation's 'Rt' register is known to contain 0, replaces the
 * 'Rt' register with $0.
 */
static uint32_t ReplaceZeroRt(struct ConstAnalysis* CA, uint32_t op)
{
	if (RT_OF(op) != 0) {
		uint64_t Bits = IntGetKnownBits(CA, RT_OF(op));
		uint64_t Mask = IntGetKnownMask(CA, RT_OF(op));
		if (Mask == ~(uint64_t) 0 && Bits == 0)
			return op & ~(UINT32_C(0x1F) << 16);
	}
	return op;
}

/*
 * Determines whether the (fully-known) value that is about to be written to a
 * register is already in that register.
 *
 * In:
 *   CA: Constant analysis structure, not yet updated to reflect the operation.
 *   Reg: The register into which the operation will yield its result.
 *   Bits: The bits that are about to be written to register 'Reg'.
 *   Mask: A mask of the known bits in the bits that are about to be written.
 */
static bool AlreadyContains(struct ConstAnalysis* CA, uint8_t Reg, uint64_t Bits, uint64_t Mask)
{
	return Mask == ~(uint64_t) 0 && IntIsKnown(CA, Reg)
	    && IntGetKnownBits(CA, Reg) == Bits;
}

uint32_t UpdateConstAnalysis(struct ConstAnalysis* CA, uint32_t op)
{
	uint64_t RsBits, RsMask, RtBits, RtMask, RdBits, RdMask;
	switch ((op >> 26) & 0x3F) {
	case 0: /* SPECIAL prefix */
		switch (op & 0x3F) {
		case 0: /* SPECIAL opcode 0: SLL */
			/* SLL is a special case: it must check whether it's writing to
			 * $0, because the encoding for NOP is an SLL. */
			if (RD_OF(op) != 0) {
				op = ReplaceZeroRt(CA, op);
				RtBits = IntGetKnownBitsInMask(CA, RT_OF(op), UINT64_C(0xFFFFFFFF));
				RtMask = IntGetKnownMaskInMask(CA, RT_OF(op), UINT64_C(0xFFFFFFFF));
				RdBits = SE64(RtBits << SA_OF(op), 32);
				/* The lower 'sa' bits are also known to be 0 now. */
				RdMask = SE64((RtMask << SA_OF(op)) | OnesBelow(SA_OF(op)), 32);
				if (AlreadyContains(CA, RD_OF(op), RdBits, RdMask)) return MAKE_NOP();
				IntSetKnownBits(CA, RD_OF(op), RdBits);
				IntSetKnownMask(CA, RD_OF(op), RdMask);
				return SimplifyResult(CA, op, RD_OF(op));
			}
			break;
		case 2: /* SPECIAL opcode 2: SRL */
			op = ReplaceZeroRt(CA, op);
			RtBits = IntGetKnownBitsInMask(CA, RT_OF(op), UINT64_C(0xFFFFFFFF));
			RtMask = IntGetKnownMaskInMask(CA, RT_OF(op), UINT64_C(0xFFFFFFFF));
			RdBits = RtBits >> SA_OF(op);
			/* The upper 'sa' bits (of the lower 32) are also known to be 0
			 * now. */
			RdMask = (RtMask >> SA_OF(op))
			       | ((uint32_t) OnesAbove(32 - SA_OF(op)));
			RdMask = SE64(RdMask, 32);
			if (AlreadyContains(CA, RD_OF(op), RdBits, RdMask)) return MAKE_NOP();
			IntSetKnownBits(CA, RD_OF(op), RdBits);
			IntSetKnownMask(CA, RD_OF(op), RdMask);
			return SimplifyResult(CA, op, RD_OF(op));
		case 3: /* SPECIAL opcode 3: SRA */
			op = ReplaceZeroRt(CA, op);
			RtBits = IntGetKnownBitsInMask(CA, RT_OF(op), UINT64_C(0xFFFFFFFF));
			RtMask = IntGetKnownMaskInMask(CA, RT_OF(op), UINT64_C(0xFFFFFFFF));
			/* If bit 31 (which is now at bit 31 - sa) was known, then all
			 * copies of it are known, all the way to bit 63. Otherwise, we
			 * don't know those bits anymore. */
			RdBits = SE64(RtBits >> SA_OF(op), 32 - SA_OF(op));
			RdMask = SE64(RtMask >> SA_OF(op), 32 - SA_OF(op));
			if (AlreadyContains(CA, RD_OF(op), RdBits, RdMask)) return MAKE_NOP();
			IntSetKnownBits(CA, RD_OF(op), RdBits);
			IntSetKnownMask(CA, RD_OF(op), RdMask);
			return SimplifyResult(CA, op, RD_OF(op));
		case 4: /* SPECIAL opcode 4: SLLV */
			op = ReplaceZeroRs(CA, op);
			op = ReplaceZeroRt(CA, op);
			if (IntIsMaskKnown(CA, RS_OF(op), UINT64_C(0x1F))) {
				uint8_t Sa = IntGetKnownBitsInMask(CA, RS_OF(op), UINT64_C(0x1F));
				return UpdateConstAnalysis(CA, SimplifyOpcode(MAKE_SLL(RD_OF(op), RT_OF(op), Sa)));
			}
			else IntSetUnknown(CA, RD_OF(op));
			return SimplifyOpcode(op);
		case 6: /* SPECIAL opcode 6: SRLV */
			op = ReplaceZeroRs(CA, op);
			op = ReplaceZeroRt(CA, op);
			if (IntIsMaskKnown(CA, RS_OF(op), UINT64_C(0x1F))) {
				uint8_t Sa = IntGetKnownBitsInMask(CA, RS_OF(op), UINT64_C(0x1F));
				return UpdateConstAnalysis(CA, SimplifyOpcode(MAKE_SRL(RD_OF(op), RT_OF(op), Sa)));
			}
			else IntSetUnknown(CA, RD_OF(op));
			return SimplifyOpcode(op);
		case 7: /* SPECIAL opcode 7: SRAV */
			op = ReplaceZeroRs(CA, op);
			op = ReplaceZeroRt(CA, op);
			if (IntIsMaskKnown(CA, RS_OF(op), UINT64_C(0x1F))) {
				uint8_t Sa = IntGetKnownBitsInMask(CA, RS_OF(op), UINT64_C(0x1F));
				return UpdateConstAnalysis(CA, SimplifyOpcode(MAKE_SRA(RD_OF(op), RT_OF(op), Sa)));
			}
			else IntSetUnknown(CA, RD_OF(op));
			return SimplifyOpcode(op);
		case 8: /* SPECIAL opcode 8: JR */
			break;
		case 9: /* SPECIAL opcode 8: JALR */
			IntSetUnknown(CA, RD_OF(op));
			break;
		case 12: /* SPECIAL opcode 12: SYSCALL */
		case 15: /* SPECIAL opcode 15: SYNC */
			break;
		case 16: /* SPECIAL opcode 16: MFHI */
			IntSetUnknown(CA, RD_OF(op));
			break;
		case 17: /* SPECIAL opcode 17: MTHI */
			/* Does not affect the status of integer registers, only of HI */
			break;
		case 18: /* SPECIAL opcode 18: MFLO */
			IntSetUnknown(CA, RD_OF(op));
			break;
		case 19: /* SPECIAL opcode 19: MTLO */
			/* Does not affect the status of integer registers, only of LO */
			break;
		case 20: /* SPECIAL opcode 20: DSLLV */
			op = ReplaceZeroRs(CA, op);
			op = ReplaceZeroRt(CA, op);
			if (IntIsMaskKnown(CA, RS_OF(op), UINT64_C(0x3F))) {
				uint8_t Sa = IntGetKnownBitsInMask(CA, RS_OF(op), UINT64_C(0x3F));
				if (Sa & 0x20) {
					return UpdateConstAnalysis(CA, SimplifyOpcode(MAKE_DSLL32(RD_OF(op), RT_OF(op), Sa & 0x1F)));
				} else {
					return UpdateConstAnalysis(CA, SimplifyOpcode(MAKE_DSLL(RD_OF(op), RT_OF(op), Sa & 0x1F)));
				}
			}
			else IntSetUnknown(CA, RD_OF(op));
			return SimplifyOpcode(op);
		case 22: /* SPECIAL opcode 22: DSRLV */
			op = ReplaceZeroRs(CA, op);
			op = ReplaceZeroRt(CA, op);
			if (IntIsMaskKnown(CA, RS_OF(op), UINT64_C(0x3F))) {
				uint8_t Sa = IntGetKnownBitsInMask(CA, RS_OF(op), UINT64_C(0x3F));
				if (Sa & 0x20) {
					return UpdateConstAnalysis(CA, SimplifyOpcode(MAKE_DSRL32(RD_OF(op), RT_OF(op), Sa & 0x1F)));
				} else {
					return UpdateConstAnalysis(CA, SimplifyOpcode(MAKE_DSRL(RD_OF(op), RT_OF(op), Sa & 0x1F)));
				}
			}
			else IntSetUnknown(CA, RD_OF(op));
			return SimplifyOpcode(op);
		case 23: /* SPECIAL opcode 23: DSRAV */
			op = ReplaceZeroRs(CA, op);
			op = ReplaceZeroRt(CA, op);
			if (IntIsMaskKnown(CA, RS_OF(op), UINT64_C(0x3F))) {
				uint8_t Sa = IntGetKnownBitsInMask(CA, RS_OF(op), UINT64_C(0x3F));
				if (Sa & 0x20) {
					return UpdateConstAnalysis(CA, SimplifyOpcode(MAKE_DSRA32(RD_OF(op), RT_OF(op), Sa & 0x1F)));
				} else {
					return UpdateConstAnalysis(CA, SimplifyOpcode(MAKE_DSRA(RD_OF(op), RT_OF(op), Sa & 0x1F)));
				}
			}
			else IntSetUnknown(CA, RD_OF(op));
			return SimplifyOpcode(op);
		case 24: /* SPECIAL opcode 24: MULT */
		case 25: /* SPECIAL opcode 25: MULTU */
		case 26: /* SPECIAL opcode 26: DIV */
		case 27: /* SPECIAL opcode 27: DIVU */
		case 28: /* SPECIAL opcode 28: DMULT */
		case 29: /* SPECIAL opcode 29: DMULTU */
		case 30: /* SPECIAL opcode 30: DDIV */
		case 31: /* SPECIAL opcode 31: DDIVU */
			/* Do not affect the status of integer registers, only of HI and LO */
			break;
		case 32: /* SPECIAL opcode 32: ADD (Integer Overflow ignored) */
		case 33: /* SPECIAL opcode 33: ADDU */
			op = ReplaceZeroRs(CA, op);
			op = ReplaceZeroRt(CA, op);
			if (IntIsMaskKnown(CA, RS_OF(op), UINT64_C(0xFFFFFFFF))
			 && IntIsMaskKnown(CA, RT_OF(op), UINT64_C(0xFFFFFFFF))) {
				RsBits = IntGetKnownBitsInMask(CA, RS_OF(op), UINT64_C(0xFFFFFFFF));
				RtBits = IntGetKnownBitsInMask(CA, RT_OF(op), UINT64_C(0xFFFFFFFF));
				RdBits = SE64(RsBits + RtBits, 32);
				if (AlreadyContains(CA, RD_OF(op), RdBits, ~(uint64_t) 0)) return MAKE_NOP();
				IntSetKnown(CA, RD_OF(op), RdBits);
			}
			else IntSetUnknown(CA, RD_OF(op));
			return SimplifyOpcode(op);
		case 34: /* SPECIAL opcode 34: SUB (Integer Overflow ignored) */
		case 35: /* SPECIAL opcode 35: SUBU */
			op = ReplaceZeroRs(CA, op);
			op = ReplaceZeroRt(CA, op);
			if (IntIsMaskKnown(CA, RS_OF(op), UINT64_C(0xFFFFFFFF))
			 && IntIsMaskKnown(CA, RT_OF(op), UINT64_C(0xFFFFFFFF))) {
				RsBits = IntGetKnownBitsInMask(CA, RS_OF(op), UINT64_C(0xFFFFFFFF));
				RtBits = IntGetKnownBitsInMask(CA, RT_OF(op), UINT64_C(0xFFFFFFFF));
				RdBits = SE64(RsBits - RtBits, 32);
				if (AlreadyContains(CA, RD_OF(op), RdBits, ~(uint64_t) 0)) return MAKE_NOP();
				IntSetKnown(CA, RD_OF(op), RdBits);
			}
			else IntSetUnknown(CA, RD_OF(op));
			return SimplifyOpcode(op);
		case 36: /* SPECIAL opcode 36: AND */
			op = ReplaceZeroRs(CA, op);
			op = ReplaceZeroRt(CA, op);
			RsBits = IntGetKnownBits(CA, RS_OF(op));
			RsMask = IntGetKnownMask(CA, RS_OF(op));
			RtBits = IntGetKnownBits(CA, RT_OF(op));
			RtMask = IntGetKnownMask(CA, RT_OF(op));
			/* If all the bits of one operand are known, try to figure out if the
			 * masking operation is superfluous. It is superfluous if an operand
			 * that is fully known has 0s in at least the bits that are already
			 * known to contain 0 in the other operand. The operation can then be
			 * converted into a move from the other operand. */
			if (RsMask == ~(uint64_t) 0
			 && (~RsBits & RtMask) == ~RsBits /* all 0s in Rs known (1) in Rt */
			 && (~RsBits & RtBits) == 0 /* all 0s in Rs unset (0) in Rt */) {
				return UpdateConstAnalysis(CA, SimplifyOpcode(MAKE_OR(RD_OF(op), RT_OF(op), 0)));
			} else if (RtMask == ~(uint64_t) 0
			        && (~RtBits & RsMask) == ~RtBits /* all 0s in Rt known (1) in Rs */
			        && (~RtBits & RsBits) == 0 /* all 0s in Rt unset (0) in Rs */) {
				return UpdateConstAnalysis(CA, SimplifyOpcode(MAKE_OR(RD_OF(op), RS_OF(op), 0)));
			}
			/* For each bit position N, for inputs S and T and output D:
			 * - if S[N] and T[N] are known to be 1, D[N] is known to be 1;
			 * - if S[N] or T[N] is known to be 0, D[N] is known to be 0;
			 * - otherwise, D[N] is unknown. */
			/* a) All bits known to be 1 in S and T are known to be 1 in D. */
			RdBits = RdMask = RsBits & RsMask & RtBits & RtMask;
			/* b) All bits known to be 0 in S or T are known to be 0 in D.
			 * XOR is used to implement this check across all 64 bits:
			 * mask |  bit | result | notes
			 * -----+------+--------+---------------------------------------------
			 *    0 |    0 |      0 | the bit is unknown, so it stays unknown
			 *    0 |    1 |      1 | this cannot happen: unknowns are 0 in 'bits'
			 *    1 |    0 |      1 | this bit is known to be 0; so is the result
			 *    1 |    1 |      0 | this bit is known to be 1; already decided */
			RdMask |= (RsBits ^ RsMask) | (RtBits ^ RtMask);
			if (AlreadyContains(CA, RD_OF(op), RdBits, RdMask)) return MAKE_NOP();
			IntSetKnownBits(CA, RD_OF(op), RdBits);
			IntSetKnownMask(CA, RD_OF(op), RdMask);
			return SimplifyResult(CA, op, RD_OF(op));
		case 37: /* SPECIAL opcode 37: OR */
			op = ReplaceZeroRs(CA, op);
			op = ReplaceZeroRt(CA, op);
			RsBits = IntGetKnownBits(CA, RS_OF(op));
			RsMask = IntGetKnownMask(CA, RS_OF(op));
			RtBits = IntGetKnownBits(CA, RT_OF(op));
			RtMask = IntGetKnownMask(CA, RT_OF(op));
			/* If all the bits of one operand are known, try to figure out if the
			 * bit-set operation is superfluous. It is superfluous if an operand
			 * that is fully known has 1s in at least the bits that are already
			 * known to contain 1 in the other operand. The operation can then be
			 * converted into a move from the other operand. */
			if (RS_OF(op) != 0 && RsMask == ~(uint64_t) 0
			 && (RsBits & RtMask & RtBits) == RsBits) {
				return UpdateConstAnalysis(CA, SimplifyOpcode(MAKE_OR(RD_OF(op), RT_OF(op), 0)));
			} else if (RT_OF(op) != 0 && RtMask == ~(uint64_t) 0
			        && (RtBits & RsMask & RsBits) == RtBits) {
				return UpdateConstAnalysis(CA, SimplifyOpcode(MAKE_OR(RD_OF(op), RS_OF(op), 0)));
			}
			/* For each bit position N, for inputs S and T and output D:
			 * - if S[N] or T[N] is known to be 1, D[N] is known to be 1;
			 * - if S[N] and T[N] are known to be 0, D[N] is known to be 0;
			 * - otherwise, D[N] is unknown. */
			/* a) All bits known to be 1 in S or T are known to be 1 in D. */
			RdBits = RdMask = (RsBits & RsMask) | (RtBits & RtMask);
			/* b) All bits known to be 0 in S and T are known to be 0 in D.
			 * mask |  bit | result | notes
			 * -----+------+--------+---------------------------------------------
			 *    0 |    0 |      0 | the bit is unknown, so it stays unknown
			 *    0 |    1 |      1 | this cannot happen: unknowns are 0 in 'bits'
			 *    1 |    0 |      1 | bits are known to be 0; so is the result
			 *    1 |    1 |      0 | this bit is known to be 1; already decided */
			RdMask |= (RsBits ^ RsMask) & (RtBits ^ RtMask);
			if (AlreadyContains(CA, RD_OF(op), RdBits, RdMask)) return MAKE_NOP();
			IntSetKnownBits(CA, RD_OF(op), RdBits);
			IntSetKnownMask(CA, RD_OF(op), RdMask);
			return SimplifyResult(CA, op, RD_OF(op));
		case 38: /* SPECIAL opcode 38: XOR */
			op = ReplaceZeroRs(CA, op);
			op = ReplaceZeroRt(CA, op);
			RsBits = IntGetKnownBits(CA, RS_OF(op));
			RsMask = IntGetKnownMask(CA, RS_OF(op));
			RtBits = IntGetKnownBits(CA, RT_OF(op));
			RtMask = IntGetKnownMask(CA, RT_OF(op));
			/* XOR only bits known in both operands, because any unknown bit
			 * in one operand could either leave the known bit alone or flip
			 * it. */
			RdBits = RsBits ^ RtBits;
			RdMask = RsMask & RtMask;
			if (AlreadyContains(CA, RD_OF(op), RdBits, RdMask)) return MAKE_NOP();
			IntSetKnownBits(CA, RD_OF(op), RdBits);
			IntSetKnownMask(CA, RD_OF(op), RdMask);
			return SimplifyResult(CA, op, RD_OF(op));
		case 39: /* SPECIAL opcode 39: NOR */
			op = ReplaceZeroRs(CA, op);
			op = ReplaceZeroRt(CA, op);
			RsBits = IntGetKnownBits(CA, RS_OF(op));
			RsMask = IntGetKnownMask(CA, RS_OF(op));
			RtBits = IntGetKnownBits(CA, RT_OF(op));
			RtMask = IntGetKnownMask(CA, RT_OF(op));
			/* For each bit position N, for inputs S and T and output D:
			 * - if S[N] or T[N] is known to be 1, D[N] is known to be 0;
			 * - if S[N] and T[N] are known to be 0, D[N] is known to be 1;
			 * - otherwise, D[N] is unknown. */
			/* a) All bits known to be 0 in S and T are known to be 1 in D.
			 * mask |  bit | result | notes
			 * -----+------+--------+---------------------------------------------
			 *    0 |    0 |      0 | the bit is unknown, so it stays unknown
			 *    0 |    1 |      1 | this cannot happen: unknowns are 0 in 'bits'
			 *    1 |    0 |      1 | bits are known to be 0; so is the result
			 *    1 |    1 |      0 | this bit is known to be 1; already decided */
			RdBits = RdMask = (RsBits ^ RsMask) & (RtBits ^ RtMask);
			/* b) All bits known to be 1 in S or T are known to be 0 in D. */
			RdMask |= (RsBits & RsMask) | (RtBits & RtMask);
			if (AlreadyContains(CA, RD_OF(op), RdBits, RdMask)) return MAKE_NOP();
			IntSetKnownBits(CA, RD_OF(op), RdBits);
			IntSetKnownMask(CA, RD_OF(op), RdMask);
			return SimplifyResult(CA, op, RD_OF(op));
		case 42: /* SPECIAL opcode 42: SLT */
		{
			uint64_t CommonMask;
			op = ReplaceZeroRs(CA, op);
			op = ReplaceZeroRt(CA, op);
			RsBits = IntGetKnownBits(CA, RS_OF(op));
			RsMask = IntGetKnownMask(CA, RS_OF(op));
			RtBits = IntGetKnownBits(CA, RT_OF(op));
			RtMask = IntGetKnownMask(CA, RT_OF(op));
			/* Figure out how many bits are known at the top of both operands. */
			CommonMask = RsMask & RtMask;
			if (CommonMask != ~(uint64_t) 0) {
				uint8_t CommonBits = 0, Guess = 32;
				while (Guess != 0) {
					uint64_t GuessMask = OnesAbove(64 - (CommonBits + Guess));
					if ((CommonMask & GuessMask) == GuessMask) {
						CommonBits += Guess;
					}
					Guess >>= 1;
				}
				CommonMask = OnesAbove(64 - CommonBits);
			}
			/* If the top known bits of each register are unequal, we can
			 * figure out the result of SLT (signed). */
			if ((RsBits & CommonMask) != (RtBits & CommonMask)) {
				RdBits = (int64_t) (RsBits & CommonMask) < (int64_t) (RtBits & CommonMask);
				RdMask = ~(uint64_t) 0;
			} else {
				RdBits = 0;
				RdMask = UINT64_C(0xFFFFFFFFFFFFFFFE);
			}
			if (AlreadyContains(CA, RD_OF(op), RdBits, RdMask)) return MAKE_NOP();
			IntSetKnownBits(CA, RD_OF(op), RdBits);
			IntSetKnownMask(CA, RD_OF(op), RdMask);
			return SimplifyResult(CA, op, RD_OF(op));
		}
		case 43: /* SPECIAL opcode 43: SLTU */
		{
			uint64_t CommonMask;
			op = ReplaceZeroRs(CA, op);
			op = ReplaceZeroRt(CA, op);
			RsBits = IntGetKnownBits(CA, RS_OF(op));
			RsMask = IntGetKnownMask(CA, RS_OF(op));
			RtBits = IntGetKnownBits(CA, RT_OF(op));
			RtMask = IntGetKnownMask(CA, RT_OF(op));
			/* Figure out how many bits are known at the top of both operands. */
			CommonMask = RsMask & RtMask;
			if (CommonMask != ~(uint64_t) 0) {
				uint8_t CommonBits = 0, Guess = 32;
				while (Guess != 0) {
					uint64_t GuessMask = OnesAbove(64 - (CommonBits + Guess));
					if ((CommonMask & GuessMask) == GuessMask) {
						CommonBits += Guess;
					}
					Guess >>= 1;
				}
				CommonMask = OnesAbove(64 - CommonBits);
			}
			/* If the top known bits of each register are unequal, we can
			 * figure out the result of SLTU (unsigned). */
			if ((RsBits & CommonMask) != (RtBits & CommonMask)) {
				RdBits = (RsBits & CommonMask) < (RtBits & CommonMask);
				RdMask = ~(uint64_t) 0;
			} else {
				RdBits = 0;
				RdMask = UINT64_C(0xFFFFFFFFFFFFFFFE);
			}
			if (AlreadyContains(CA, RD_OF(op), RdBits, RdMask)) return MAKE_NOP();
			IntSetKnownBits(CA, RD_OF(op), RdBits);
			IntSetKnownMask(CA, RD_OF(op), RdMask);
			return SimplifyResult(CA, op, RD_OF(op));
		}
		case 44: /* SPECIAL opcode 44: DADD (Integer Overflow ignored) */
		case 45: /* SPECIAL opcode 45: DADDU */
			op = ReplaceZeroRs(CA, op);
			op = ReplaceZeroRt(CA, op);
			if (IntIsKnown(CA, RS_OF(op)) && IntIsKnown(CA, RT_OF(op))) {
				RsBits = IntGetKnownBits(CA, RS_OF(op));
				RtBits = IntGetKnownBits(CA, RT_OF(op));
				RdBits = RsBits + RtBits;
				if (AlreadyContains(CA, RD_OF(op), RdBits, ~(uint64_t) 0)) return MAKE_NOP();
				IntSetKnown(CA, RD_OF(op), RdBits);
				return SimplifyResult(CA, op, RD_OF(op));
			}
			else IntSetUnknown(CA, RD_OF(op));
			break;
		case 46: /* SPECIAL opcode 46: DSUB (Integer Overflow ignored) */
		case 47: /* SPECIAL opcode 47: DSUBU */
			op = ReplaceZeroRs(CA, op);
			op = ReplaceZeroRt(CA, op);
			if (IntIsKnown(CA, RS_OF(op)) && IntIsKnown(CA, RT_OF(op))) {
				RsBits = IntGetKnownBits(CA, RS_OF(op));
				RtBits = IntGetKnownBits(CA, RT_OF(op));
				RdBits = RsBits - RtBits;
				if (AlreadyContains(CA, RD_OF(op), RdBits, ~(uint64_t) 0)) return MAKE_NOP();
				IntSetKnown(CA, RD_OF(op), RdBits);
				return SimplifyResult(CA, op, RD_OF(op));
			}
			else IntSetUnknown(CA, RD_OF(op));
			break;
		case 52: /* SPECIAL opcode 52: TEQ */
			break;
		case 56: /* SPECIAL opcode 56: DSLL */
			op = ReplaceZeroRt(CA, op);
			RtBits = IntGetKnownBits(CA, RT_OF(op));
			RtMask = IntGetKnownMask(CA, RT_OF(op));
			RdBits = RtBits << SA_OF(op);
			/* The lower 'sa' bits are also known to be 0 now. */
			RdMask = (RtMask << SA_OF(op)) | OnesBelow(SA_OF(op));
			if (AlreadyContains(CA, RD_OF(op), RdBits, RdMask)) return MAKE_NOP();
			IntSetKnownBits(CA, RD_OF(op), RdBits);
			IntSetKnownMask(CA, RD_OF(op), RdMask);
			return SimplifyResult(CA, op, RD_OF(op));
		case 58: /* SPECIAL opcode 58: DSRL */
			op = ReplaceZeroRt(CA, op);
			RtBits = IntGetKnownBits(CA, RT_OF(op));
			RtMask = IntGetKnownMask(CA, RT_OF(op));
			RdBits = RtBits >> SA_OF(op);
			/* The upper 'sa' bits are also known to be 0 now. */
			RdMask = (RtMask >> SA_OF(op)) | OnesAbove(64 - SA_OF(op));
			if (AlreadyContains(CA, RD_OF(op), RdBits, RdMask)) return MAKE_NOP();
			IntSetKnownBits(CA, RD_OF(op), RdBits);
			IntSetKnownMask(CA, RD_OF(op), RdMask);
			return SimplifyResult(CA, op, RD_OF(op));
		case 59: /* SPECIAL opcode 59: DSRA */
			op = ReplaceZeroRt(CA, op);
			RtBits = IntGetKnownBits(CA, RT_OF(op));
			RtMask = IntGetKnownMask(CA, RT_OF(op));
			/* If bit 63 (which is now at bit 63 - sa) was known, then all
			 * copies of it are known, all the way to bit 63. Otherwise, we
			 * don't know those bits anymore. */
			RdBits = SE64(RtBits >> SA_OF(op), 64 - SA_OF(op));
			RdMask = SE64(RtMask >> SA_OF(op), 64 - SA_OF(op));
			if (AlreadyContains(CA, RD_OF(op), RdBits, RdMask)) return MAKE_NOP();
			IntSetKnownBits(CA, RD_OF(op), RdBits);
			IntSetKnownMask(CA, RD_OF(op), RdMask);
			return SimplifyResult(CA, op, RD_OF(op));
		case 60: /* SPECIAL opcode 60: DSLL32 */
			op = ReplaceZeroRt(CA, op);
			RtBits = IntGetKnownBits(CA, RT_OF(op));
			RtMask = IntGetKnownMask(CA, RT_OF(op));
			RdBits = RtBits << (32 + SA_OF(op));
			/* The lower '32 + sa' bits are also known to be 0 now. */
			RdMask = (RtMask << (32 + SA_OF(op))) | OnesBelow(32 + SA_OF(op));
			if (AlreadyContains(CA, RD_OF(op), RdBits, RdMask)) return MAKE_NOP();
			IntSetKnownBits(CA, RD_OF(op), RdBits);
			IntSetKnownMask(CA, RD_OF(op), RdMask);
			return SimplifyResult(CA, op, RD_OF(op));
		case 62: /* SPECIAL opcode 62: DSRL32 */
			op = ReplaceZeroRt(CA, op);
			RtBits = IntGetKnownBits(CA, RT_OF(op));
			RtMask = IntGetKnownMask(CA, RT_OF(op));
			RdBits = RtBits >> (32 + SA_OF(op));
			/* The upper '32 + sa' bits are also known to be 0 now. */
			RdMask = (RtMask >> (32 + SA_OF(op))) | OnesAbove(32 - SA_OF(op));
			if (AlreadyContains(CA, RD_OF(op), RdBits, RdMask)) return MAKE_NOP();
			IntSetKnownBits(CA, RD_OF(op), RdBits);
			IntSetKnownMask(CA, RD_OF(op), RdMask);
			return SimplifyResult(CA, op, RD_OF(op));
		case 63: /* SPECIAL opcode 63: DSRA32 */
			op = ReplaceZeroRt(CA, op);
			RtBits = IntGetKnownBits(CA, RT_OF(op));
			RtMask = IntGetKnownMask(CA, RT_OF(op));
			/* If bit 63 (which is now at bit 63 - sa) was known, then all
			 * copies of it are known, all the way to bit 63. Otherwise, we
			 * don't know those bits anymore. */
			RdBits = SE64(RtBits >> (32 + SA_OF(op)), 32 - SA_OF(op));
			RdMask = SE64(RtMask >> (32 + SA_OF(op)), 32 - SA_OF(op));
			if (AlreadyContains(CA, RD_OF(op), RdBits, RdMask)) return MAKE_NOP();
			IntSetKnownBits(CA, RD_OF(op), RdBits);
			IntSetKnownMask(CA, RD_OF(op), RdMask);
			return SimplifyResult(CA, op, RD_OF(op));
		default:
			/* We don't know what the opcode did. Assume the value of EVERY
			 * register became unknown. */
			InitConstAnalysis(CA);
			break;
		} /* switch ((op >> 16) & 0x1F) for the SPECIAL prefix */
		break;
	case 1: /* REGIMM prefix */
		switch ((op >> 16) & 0x1F) {
		case 0: /* REGIMM opcode 0: BLTZ */
			op = ReplaceZeroRs(CA, op);
			RsBits = IntGetKnownBits(CA, RS_OF(op));
			RsMask = IntGetKnownMask(CA, RS_OF(op));
			/* If bit 63 is known... */
			if (RsMask & UINT64_C(0x8000000000000000)) {
				if (RsBits & UINT64_C(0x8000000000000000)) {
					/* ... to be set, then the register's value is known to be
					 * negative. */
					return MAKE_BGEZ(0, IMM16S_OF(op));
				} else {
					/* ... to be unset, then the register's value is known to
					 * be positive. */
					return MAKE_NOP();
				}
			}
			return op;
		case 1: /* REGIMM opcode 1: BGEZ */
			op = ReplaceZeroRs(CA, op);
			RsBits = IntGetKnownBits(CA, RS_OF(op));
			RsMask = IntGetKnownMask(CA, RS_OF(op));
			/* If bit 63 is known... */
			if (RsMask & UINT64_C(0x8000000000000000)) {
				if (RsBits & UINT64_C(0x8000000000000000)) {
					/* ... to be set, then the register's value is known to be
					 * negative. */
					return MAKE_NOP();
				} else {
					/* ... to be unset, then the register's value is known to
					 * be positive. */
					return MAKE_BGEZ(0, IMM16S_OF(op));
				}
			}
			return op;
		case 2: /* REGIMM opcode 2: BLTZL */
		case 3: /* REGIMM opcode 3: BGEZL */
			break;
		case 16: /* REGIMM opcode 16: BLTZAL */
		case 17: /* REGIMM opcode 17: BGEZAL */
		case 18: /* REGIMM opcode 18: BLTZALL */
		case 19: /* REGIMM opcode 19: BGEZALL */
			IntSetUnknown(CA, 31);
			break;
		default:
			/* We don't know what the opcode did. Assume the value of EVERY
			 * register became unknown. */
			InitConstAnalysis(CA);
			break;
		} /* switch ((op >> 16) & 0x1F) for the REGIMM prefix */
		break;
	case 2: /* Major opcode 2: J */
		break;
	case 3: /* Major opcode 3: JAL */
		IntSetUnknown(CA, 31);
		break;
	case 4: /* Major opcode 4: BEQ */
		op = ReplaceZeroRs(CA, op);
		op = ReplaceZeroRt(CA, op);
		RsBits = IntGetKnownBits(CA, RS_OF(op));
		RsMask = IntGetKnownMask(CA, RS_OF(op));
		RtBits = IntGetKnownBits(CA, RT_OF(op));
		RtMask = IntGetKnownMask(CA, RT_OF(op));
		if (RsMask == ~(uint64_t) 0 && RtMask == ~(uint64_t) 0 && RsBits == RtBits) {
			return MAKE_BGEZ(0, IMM16S_OF(op));
		} else if ((RsBits & RsMask & RtMask) != (RtBits & RsMask & RtMask)) {
			return MAKE_NOP();
		}
		return op;
	case 5: /* Major opcode 5: BNE */
		op = ReplaceZeroRs(CA, op);
		op = ReplaceZeroRt(CA, op);
		RsBits = IntGetKnownBits(CA, RS_OF(op));
		RsMask = IntGetKnownMask(CA, RS_OF(op));
		RtBits = IntGetKnownBits(CA, RT_OF(op));
		RtMask = IntGetKnownMask(CA, RT_OF(op));
		if (RsMask == ~(uint64_t) 0 && RtMask == ~(uint64_t) 0 && RsBits == RtBits) {
			return MAKE_NOP();
		} else if ((RsBits & RsMask & RtMask) != (RtBits & RsMask & RtMask)) {
			return MAKE_BGEZ(0, IMM16S_OF(op));
		}
		return op;
	case 6: /* Major opcode 6: BLEZ */
	case 7: /* Major opcode 7: BGTZ */
		break;
	case 8: /* Major opcode 8: ADDI (Integer Overflow ignored) */
	case 9: /* Major opcode 9: ADDIU */
		op = ReplaceZeroRs(CA, op);
		if (IntIsMaskKnown(CA, RS_OF(op), UINT64_C(0xFFFFFFFF))) {
			RsBits = IntGetKnownBitsInMask(CA, RS_OF(op), UINT64_C(0xFFFFFFFF));
			RtBits = SE64(RsBits + SE64(IMM16S_OF(op), 16), 32);
			if (AlreadyContains(CA, RT_OF(op), RtBits, ~(uint64_t) 0)) return MAKE_NOP();
			IntSetKnown(CA, RT_OF(op), RtBits);
		}
		else IntSetUnknown(CA, RT_OF(op));
		break;
	case 10: /* Major opcode 10: SLTI */
	{
		uint64_t HighMask;
		op = ReplaceZeroRs(CA, op);
		RsBits = IntGetKnownBits(CA, RS_OF(op));
		RsMask = IntGetKnownMask(CA, RS_OF(op));
		/* Figure out how many bits are known at the top of the register
		 * operand. */
		HighMask = RsMask;
		if (HighMask != ~(uint64_t) 0) {
			uint8_t HighBits = 0, Guess = 32;
			while (Guess != 0) {
				uint64_t GuessMask = OnesAbove(64 - (HighBits + Guess));
				if ((HighMask & GuessMask) == GuessMask) {
					HighBits += Guess;
				}
				Guess >>= 1;
			}
			HighMask = OnesAbove(64 - HighBits);
		}
		/* If the top known bits of the register and immediate are unequal,
		 * we can figure out the result of SLTI (signed). */
		if ((RsBits & HighMask) != (SE64(IMM16S_OF(op), 16) & HighMask)) {
			RtBits = (int64_t) (RsBits & HighMask) < (int64_t) (SE64(IMM16S_OF(op), 16) & HighMask);
			RtMask = ~(uint64_t) 0;
		} else {
			RtBits = 0;
			RtMask = UINT64_C(0xFFFFFFFFFFFFFFFE);
		}
		if (AlreadyContains(CA, RT_OF(op), RtBits, RtMask)) return MAKE_NOP();
		IntSetKnownBits(CA, RT_OF(op), RtBits);
		IntSetKnownMask(CA, RT_OF(op), RtMask);
		return SimplifyResult(CA, op, RT_OF(op));
	}
	case 11: /* Major opcode 11: SLTIU */
	{
		uint64_t HighMask;
		op = ReplaceZeroRs(CA, op);
		RsBits = IntGetKnownBits(CA, RS_OF(op));
		RsMask = IntGetKnownMask(CA, RS_OF(op));
		/* Figure out how many bits are known at the top of the register
		 * operand. */
		HighMask = RsMask;
		if (HighMask != ~(uint64_t) 0) {
			uint8_t HighBits = 0, Guess = 32;
			while (Guess != 0) {
				uint64_t GuessMask = OnesAbove(64 - (HighBits + Guess));
				if ((HighMask & GuessMask) == GuessMask) {
					HighBits += Guess;
				}
				Guess >>= 1;
			}
			HighMask = OnesAbove(64 - HighBits);
		}
		/* If the top known bits of the register and immediate are unequal,
		 * we can figure out the result of SLTIU (unsigned). */
		if ((RsBits & HighMask) != (SE64(IMM16S_OF(op), 16) & HighMask)) {
			RtBits = (RsBits & HighMask) < (SE64(IMM16S_OF(op), 16) & HighMask);
			RtMask = ~(uint64_t) 0;
		} else {
			RtBits = 0;
			RtMask = UINT64_C(0xFFFFFFFFFFFFFFFE);
		}
		if (AlreadyContains(CA, RT_OF(op), RtBits, RtMask)) return MAKE_NOP();
		IntSetKnownBits(CA, RT_OF(op), RtBits);
		IntSetKnownMask(CA, RT_OF(op), RtMask);
		return SimplifyResult(CA, op, RT_OF(op));
	}
	case 12: /* Major opcode 12: ANDI */
		op = ReplaceZeroRs(CA, op);
		RsBits = IntGetKnownBits(CA, RS_OF(op));
		RsMask = IntGetKnownMask(CA, RS_OF(op));
		/* Try to figure out if the masking operation is superfluous. It is
		 * superfluous if the immediate (zero-extended to 64 bits) has 0s in
		 * at least the bits that are already known to contain 0 in the
		 * register operand. The operation can then be converted into a move
		 * from the register operand. */
		if ((~IMM16U_OF(op) & RsMask) == ~IMM16U_OF(op)
		 && (~IMM16U_OF(op) & RsBits) == 0) {
			return UpdateConstAnalysis(CA, SimplifyOpcode(MAKE_OR(RT_OF(op), RS_OF(op), 0)));
		}
		/* For each bit position N, for inputs S and T and output D:
		 * - if S[N] and T[N] are known to be 1, D[N] is known to be 1;
		 * - if S[N] or T[N] is known to be 0, D[N] is known to be 0;
		 * - otherwise, D[N] is unknown. */
		/* a) All bits known to be 1 in S and T are known to be 1 in D. */
		RtBits = RtMask = RsBits & RsMask & IMM16U_OF(op);
		/* b) All bits known to be 0 in S or T are known to be 0 in D.
		 * XOR is used to implement this check across all 64 bits:
		 * mask |  bit | result | notes
		 * -----+------+--------+---------------------------------------------
		 *    0 |    0 |      0 | the bit is unknown, so it stays unknown
		 *    0 |    1 |      1 | this cannot happen: unknowns are 0 in 'bits'
		 *    1 |    0 |      1 | this bit is known to be 0; so is the result
		 *    1 |    1 |      0 | this bit is known to be 1; already decided */
		RtMask |= (RsBits ^ RsMask) | ~(uint64_t) IMM16U_OF(op);
		if (AlreadyContains(CA, RT_OF(op), RtBits, RtMask)) return MAKE_NOP();
		IntSetKnownBits(CA, RT_OF(op), RtBits);
		IntSetKnownMask(CA, RT_OF(op), RtMask);
		return SimplifyResult(CA, op, RT_OF(op));
	case 13: /* Major opcode 13: ORI */
		op = ReplaceZeroRs(CA, op);
		RsBits = IntGetKnownBits(CA, RS_OF(op));
		RsMask = IntGetKnownMask(CA, RS_OF(op));
		/* Try to figure out if the bit-set operation is superfluous. It is
		 * superfluous if the immediate has 1s in at least the bits that are
		 * already known to contain 1 in the register operand. The operation
		 * can then be converted into a move from the register operand. */
		if ((IMM16U_OF(op) & RsMask & RsBits) == IMM16U_OF(op)) {
			return UpdateConstAnalysis(CA, SimplifyOpcode(MAKE_OR(RT_OF(op), RS_OF(op), 0)));
		}
		/* Every set bit in the mask is now known to be 1 in the target
		 * register. */
		RtBits = RsBits | IMM16U_OF(op);
		RtMask = RsMask | IMM16U_OF(op);
		if (AlreadyContains(CA, RT_OF(op), RtBits, RtMask)) return MAKE_NOP();
		IntSetKnownBits(CA, RT_OF(op), RtBits);
		IntSetKnownMask(CA, RT_OF(op), RtMask);
		return SimplifyResult(CA, op, RT_OF(op));
	case 14: /* Major opcode 14: XORI */
		op = ReplaceZeroRs(CA, op);
		RsBits = IntGetKnownBits(CA, RS_OF(op));
		RsMask = IntGetKnownMask(CA, RS_OF(op));
		RtMask = RsMask;
		RtBits = RsBits ^ IMM16U_OF(op);
		if (AlreadyContains(CA, RT_OF(op), RtBits, RtMask)) return MAKE_NOP();
		IntSetKnownBits(CA, RT_OF(op), RtBits);
		IntSetKnownMask(CA, RT_OF(op), RtMask);
		return SimplifyResult(CA, op, RT_OF(op));
	case 15: /* Major opcode 15: LUI */
		if (AlreadyContains(CA, RT_OF(op), SE64(IMM16S_OF(op) << 16, 32), ~(uint64_t) 0))
			return MAKE_NOP();
		IntSetKnown(CA, RT_OF(op), SE64(IMM16S_OF(op) << 16, 32));
		break;
	case 16: /* Coprocessor 0 prefix */
		switch ((op >> 21) & 0x1F) {
		case 0: /* Coprocessor 0 opcode 0: MFC0 */
			IntSetUnknown(CA, RT_OF(op));
			break;
		case 4: /* Coprocessor 0 opcode 4: MTC0 */
			break;
		case 16: /* Coprocessor 0 opcode 16: TLB */
			switch (op & 0x3F) {
			case 1: /* TLB opcode 1: TLBR */
			case 2: /* TLB opcode 2: TLBWI */
			case 6: /* TLB opcode 6: TLBWR */
			case 8: /* TLB opcode 8: TLBP */
			case 24: /* TLB opcode 24: ERET */
				break;
			default:
				/* We don't know what the opcode did. Assume the value of EVERY
				 * register became unknown. */
				InitConstAnalysis(CA);
				break;
			} /* switch (op & 0x3F) for Coprocessor 0 TLB opcodes */
			break;
		default:
			/* We don't know what the opcode did. Assume the value of EVERY
			 * register became unknown. */
			InitConstAnalysis(CA);
			break;
		} /* switch ((op >> 21) & 0x1F) for the Coprocessor 0 prefix */
		break;
	case 17: /* Coprocessor 1 prefix */
		switch ((op >> 21) & 0x1F) {
		case 0: /* Coprocessor 1 opcode 0: MFC1 */
		case 1: /* Coprocessor 1 opcode 1: DMFC1 */
		case 2: /* Coprocessor 1 opcode 2: CFC1 */
			IntSetUnknown(CA, RT_OF(op));
			break;
		case 4: /* Coprocessor 1 opcode 4: MTC1 */
		case 5: /* Coprocessor 1 opcode 5: DMTC1 */
		case 6: /* Coprocessor 1 opcode 6: CTC1 */
		case 8: /* Coprocessor 1 opcode 8: Branch on C1 condition... */
		case 16: /* Coprocessor 1 S-format opcodes */
		case 17: /* Coprocessor 1 D-format opcodes */
		case 20: /* Coprocessor 1 W-format opcodes */
		case 21: /* Coprocessor 1 L-format opcodes */
			/* Do not affect the status of integer registers, only floating-point ones */
			break;
		default:
			/* We don't know what the opcode did. Assume the value of EVERY
			 * register became unknown. */
			InitConstAnalysis(CA);
			break;
		} /* switch ((op >> 21) & 0x1F) for the Coprocessor 1 prefix */
		break;
	case 20: /* Major opcode 20: BEQL */
	case 21: /* Major opcode 21: BNEL */
	case 22: /* Major opcode 22: BLEZL */
	case 23: /* Major opcode 23: BGTZL */
		break;
	case 24: /* Major opcode 24: DADDI (Integer Overflow ignored) */
	case 25: /* Major opcode 25: DADDIU */
		op = ReplaceZeroRs(CA, op);
		if (IntIsKnown(CA, RS_OF(op))) {
			RsBits = IntGetKnownBits(CA, RS_OF(op));
			RtBits = RsBits + SE64(IMM16S_OF(op), 16);
			if (AlreadyContains(CA, RT_OF(op), RtBits, ~(uint64_t) 0)) return MAKE_NOP();
			IntSetKnown(CA, RT_OF(op), RtBits);
		}
		else IntSetUnknown(CA, RT_OF(op));
		break;
	case 26: /* Major opcode 26: LDL */
	case 27: /* Major opcode 27: LDR */
	case 32: /* Major opcode 32: LB */
	case 33: /* Major opcode 33: LH */
	case 34: /* Major opcode 34: LWL */
	case 35: /* Major opcode 35: LW */
		IntSetUnknown(CA, RT_OF(op));
		break;
	case 36: /* Major opcode 36: LBU */
		IntSetKnownBits(CA, RT_OF(op), 0);
		IntSetKnownMask(CA, RT_OF(op), UINT64_C(0xFFFFFFFFFFFFFF00));
		break;
	case 37: /* Major opcode 37: LHU */
		IntSetKnownBits(CA, RT_OF(op), 0);
		IntSetKnownMask(CA, RT_OF(op), UINT64_C(0xFFFFFFFFFFFF0000));
		break;
	case 38: /* Major opcode 38: LWR */
		IntSetUnknown(CA, RT_OF(op));
		break;
	case 39: /* Major opcode 39: LWU */
		IntSetKnownBits(CA, RT_OF(op), 0);
		IntSetKnownMask(CA, RT_OF(op), UINT64_C(0xFFFFFFFF00000000));
		break;
	case 40: /* Major opcode 40: SB */
	case 41: /* Major opcode 41: SH */
	case 42: /* Major opcode 42: SWL */
	case 43: /* Major opcode 43: SW */
	case 44: /* Major opcode 44: SDL */
	case 45: /* Major opcode 45: SDR */
	case 46: /* Major opcode 46: SWR */
	case 47: /* Major opcode 47: CACHE */
		break;
	case 48: /* Major opcode 48: LL */
		IntSetUnknown(CA, RT_OF(op));
		break;
	case 49: /* Major opcode 49: LWC1 */
	case 53: /* Major opcode 53: LDC1 */
		/* Do not affect the status of integer registers, only of floating-point registers */
		break;
	case 55: /* Major opcode 55: LD */
		IntSetUnknown(CA, RT_OF(op));
		break;
	case 56: /* Major opcode 56: SC */
		/* SC sets the source data register to 0 or 1 according to whether it
		 * successfully linked with the previous LL. */
		IntSetKnownBits(CA, RT_OF(op), 0);
		IntSetKnownMask(CA, RT_OF(op), UINT64_C(0xFFFFFFFFFFFFFFFE));
		break;
	case 57: /* Major opcode 57: SWC1 */
	case 61: /* Major opcode 61: SDC1 */
		break;
	case 63: /* Major opcode 63: SD */
		/* Do not affect the status of integer registers, only of memory */
		break;
	default:
		/* We don't know what the opcode did. Assume the value of EVERY
		 * register became unknown. */
		InitConstAnalysis(CA);
		break;
	}

	return op;
}

bool IntIsKnown(const struct ConstAnalysis* CA, uint8_t Reg)
{
	assert(Reg < 32);
	return CA->IntMask[Reg] == ~(uint64_t) 0;
}

bool IntIsMaskKnown(const struct ConstAnalysis* CA, uint8_t Reg, uint64_t Mask)
{
	assert(Reg < 32);
	return (CA->IntMask[Reg] & Mask) == Mask;
}

uint64_t IntGetKnownMask(const struct ConstAnalysis* CA, uint8_t Reg)
{
	assert(Reg < 32);
	return CA->IntMask[Reg];
}

uint64_t IntGetKnownMaskInMask(const struct ConstAnalysis* CA, uint8_t Reg, uint64_t Mask)
{
	assert(Reg < 32);
	return CA->IntMask[Reg] & Mask;
}

uint64_t IntGetKnownBits(const struct ConstAnalysis* CA, uint8_t Reg)
{
	assert(Reg < 32);
	return CA->IntBits[Reg] & CA->IntMask[Reg];
}

uint64_t IntGetKnownBitsInMask(const struct ConstAnalysis* CA, uint8_t Reg, uint64_t Mask)
{
	assert(Reg < 32);
	return CA->IntBits[Reg] & CA->IntMask[Reg] & Mask;
}

void InitWidthAnalysis(struct WidthAnalysis* WA)
{
	size_t i;
	WA->IntWidth[0] = 1;
	for (i = 1; i < 32; i++) {
		WA->IntWidth[i] = 64;
	}
	WA->HIWidth = WA->LOWidth = 64;
}

/*
 * Determines the meaningful width of the given constant.
 *
 * In:
 *   Constant: The constant to analyse.
 * Returns:
 *   The number of meaningful bits of the 'Constant', above which every bit is
 *   merely a copy of the highest meaningful bit.
 */
static int8_t GetConstantWidth(uint64_t Constant)
{
	int8_t HighBits = 0, Guess = 32;
	while (Guess != 0) {
		if (SE64(Constant, 64 - (HighBits + Guess)) == Constant) {
			HighBits += Guess;
		}
		Guess >>= 1;
	}
	if (HighBits <= 1)
		return 64;
	else
		return 64 - HighBits;
}

static void IntSetWidth(struct WidthAnalysis* WA, uint8_t Reg, int8_t Value)
{
	assert(Reg != 0 && Reg < 32);
	assert(Value > 0 && Value <= 64);
	WA->IntWidth[Reg] = Value;
}

static void HISetWidth(struct WidthAnalysis* WA, int8_t Value)
{
	assert(Value > 0 && Value <= 64);
	WA->HIWidth = Value;
}

static void LOSetWidth(struct WidthAnalysis* WA, int8_t Value)
{
	assert(Value > 0 && Value <= 64);
	WA->LOWidth = Value;
}

uint32_t UpdateWidthAnalysis(struct WidthAnalysis* WA, uint32_t op)
{
	int8_t RsWidth, RtWidth, ImmWidth, RdWidth;
	switch ((op >> 26) & 0x3F) {
	case 0: /* SPECIAL prefix */
		switch (op & 0x3F) {
		case 0: /* SPECIAL opcode 0: SLL */
			/* SLL is a special case: it must check whether it's writing to
			 * $0, because the encoding for NOP is an SLL. */
			if (RD_OF(op) != 0) {
				RtWidth = IntGetWidth(WA, RT_OF(op));
				RdWidth = Min(RtWidth + SA_OF(op), 32);
				IntSetWidth(WA, RD_OF(op), RdWidth);
			}
			break;
		case 2: /* SPECIAL opcode 2: SRL */
			RtWidth = IntGetWidth(WA, RT_OF(op));
			RdWidth = Max(Min(RtWidth, 32) - (SA_OF(op) - 1), 1);
			IntSetWidth(WA, RD_OF(op), RdWidth);
			break;
		case 3: /* SPECIAL opcode 3: SRA */
			RtWidth = IntGetWidth(WA, RT_OF(op));
			if (RtWidth == 1) {
				return UpdateWidthAnalysis(WA, SimplifyOpcode(MAKE_OR(RD_OF(op), RT_OF(op), 0)));
			}
			RdWidth = Max(Min(RtWidth, 32) - SA_OF(op), 1);
			IntSetWidth(WA, RD_OF(op), RdWidth);
			break;
		case 4: /* SPECIAL opcode 4: SLLV */
			/* We don't know how how many bits this shift would make the value
			 * grow by. Shifting by 0 wouldn't affect the width; shifting by
			 * 31 would make the width 32. Take the highest possible width. */
			IntSetWidth(WA, RD_OF(op), 32);
			break;
		case 6: /* SPECIAL opcode 6: SRLV */
			/* We don't know how how many bits this shift would make the value
			 * grow by. Shifting by 0 wouldn't affect the width; shifting by
			 * 31 would make the width 1. Take the highest possible width. */
			RtWidth = IntGetWidth(WA, RT_OF(op));
			RdWidth = Max(RtWidth, 32);
			IntSetWidth(WA, RD_OF(op), RdWidth);
			break;
		case 7: /* SPECIAL opcode 7: SRAV */
			/* We don't know how how many bits this shift would make the value
			 * grow by. Shifting by 0 wouldn't affect the width; shifting by
			 * 31 would make the width 1. Take the highest possible width. */
			RtWidth = IntGetWidth(WA, RT_OF(op));
			if (RtWidth == 1) {
				return UpdateWidthAnalysis(WA, SimplifyOpcode(MAKE_OR(RD_OF(op), RT_OF(op), 0)));
			}
			RdWidth = Max(RtWidth, 32);
			IntSetWidth(WA, RD_OF(op), RdWidth);
			break;
		case 8: /* SPECIAL opcode 8: JR */
			break;
		case 9: /* SPECIAL opcode 8: JALR */
			IntSetWidth(WA, RD_OF(op), 32);
			break;
		case 12: /* SPECIAL opcode 12: SYSCALL */
		case 15: /* SPECIAL opcode 15: SYNC */
			break;
		case 16: /* SPECIAL opcode 16: MFHI */
			IntSetWidth(WA, RD_OF(op), HIGetWidth(WA));
			break;
		case 17: /* SPECIAL opcode 17: MTHI */
			HISetWidth(WA, IntGetWidth(WA, RS_OF(op)));
			break;
		case 18: /* SPECIAL opcode 18: MFLO */
			IntSetWidth(WA, RD_OF(op), LOGetWidth(WA));
			break;
		case 19: /* SPECIAL opcode 19: MTLO */
			LOSetWidth(WA, IntGetWidth(WA, RS_OF(op)));
			break;
		case 20: /* SPECIAL opcode 20: DSLLV */
			/* We don't know how how many bits this shift would make the value
			 * grow by. Shifting by 0 wouldn't affect the width; shifting by
			 * 63 would make the width 64. Take the highest possible width. */
			IntSetWidth(WA, RD_OF(op), 64);
			break;
		case 22: /* SPECIAL opcode 22: DSRLV */
			/* We don't know how how many bits this shift would make the value
			 * grow by. Shifting by 0 wouldn't affect the width; shifting by
			 * 63 would make the width 1. Take the highest possible width. */
			RtWidth = IntGetWidth(WA, RT_OF(op));
			IntSetWidth(WA, RD_OF(op), 64);
			break;
		case 23: /* SPECIAL opcode 23: DSRAV */
			/* We don't know how how many bits this shift would make the value
			 * grow by. Shifting by 0 wouldn't affect the width; shifting by
			 * 63 would make the width 1. Take the highest possible width. */
			RtWidth = IntGetWidth(WA, RT_OF(op));
			if (RtWidth == 1) {
				return UpdateWidthAnalysis(WA, SimplifyOpcode(MAKE_OR(RD_OF(op), RT_OF(op), 0)));
			}
			IntSetWidth(WA, RD_OF(op), 64);
			break;
		case 24: /* SPECIAL opcode 24: MULT */
		case 25: /* SPECIAL opcode 25: MULTU */
		case 26: /* SPECIAL opcode 26: DIV */
		case 27: /* SPECIAL opcode 27: DIVU */
			HISetWidth(WA, 32);
			LOSetWidth(WA, 32);
			break;
		case 28: /* SPECIAL opcode 28: DMULT */
		case 29: /* SPECIAL opcode 29: DMULTU */
		case 30: /* SPECIAL opcode 30: DDIV */
		case 31: /* SPECIAL opcode 31: DDIVU */
			HISetWidth(WA, 64);
			LOSetWidth(WA, 64);
			break;
		case 32: /* SPECIAL opcode 32: ADD */
		case 33: /* SPECIAL opcode 33: ADDU */
			RsWidth = IntGetWidth(WA, RS_OF(op));
			RtWidth = IntGetWidth(WA, RT_OF(op));
			RdWidth = Min(Max(RsWidth, RtWidth) + 1, 32);
			IntSetWidth(WA, RD_OF(op), RdWidth);
			if (RT_OF(op) == 0 && RsWidth <= 32) {
				return SimplifyOpcode(MAKE_OR(RD_OF(op), RS_OF(op), 0));
			}
			break;
		case 34: /* SPECIAL opcode 34: SUB */
		case 35: /* SPECIAL opcode 35: SUBU */
			RsWidth = IntGetWidth(WA, RS_OF(op));
			RtWidth = IntGetWidth(WA, RT_OF(op));
			RdWidth = Min(Max(RsWidth, RtWidth) + 1, 32);
			IntSetWidth(WA, RD_OF(op), RdWidth);
			break;
		case 36: /* SPECIAL opcode 36: AND */
		case 37: /* SPECIAL opcode 37: OR */
		case 38: /* SPECIAL opcode 38: XOR */
		case 39: /* SPECIAL opcode 39: NOR */
			RsWidth = IntGetWidth(WA, RS_OF(op));
			RtWidth = IntGetWidth(WA, RT_OF(op));
			RdWidth = Max(RsWidth, RtWidth);
			IntSetWidth(WA, RD_OF(op), RdWidth);
			break;
		case 42: /* SPECIAL opcode 42: SLT */
		case 43: /* SPECIAL opcode 43: SLTU */
			IntSetWidth(WA, RD_OF(op), 2);
			break;
		case 44: /* SPECIAL opcode 44: DADD */
		case 45: /* SPECIAL opcode 45: DADDU */
			RsWidth = IntGetWidth(WA, RS_OF(op));
			RtWidth = IntGetWidth(WA, RT_OF(op));
			RdWidth = Min(Max(RsWidth, RtWidth) + 1, 64);
			IntSetWidth(WA, RD_OF(op), RdWidth);
			/* If both operands are 31-bit or under, the result must be
			 * within +/- 0x7FFF_FFFF (64-bit). The 64-bit result will end up
			 * having the upper 32 bits sign-extended. */
			if (RdWidth <= 32) {
				return MAKE_ADDU(RD_OF(op), RS_OF(op), RT_OF(op));
			}
			break;
		case 46: /* SPECIAL opcode 46: DSUB */
		case 47: /* SPECIAL opcode 47: DSUBU */
			RsWidth = IntGetWidth(WA, RS_OF(op));
			RtWidth = IntGetWidth(WA, RT_OF(op));
			RdWidth = Min(Max(RsWidth, RtWidth) + 1, 64);
			IntSetWidth(WA, RD_OF(op), RdWidth);
			/* If both operands are 31-bit or under, the result must be
			 * within +/- 0x7FFF_FFFF (64-bit). The 64-bit result will end up
			 * having the upper 32 bits sign-extended. */
			if (RdWidth <= 32) {
				return MAKE_SUBU(RD_OF(op), RS_OF(op), RT_OF(op));
			}
			break;
		case 52: /* SPECIAL opcode 52: TEQ */
			break;
		case 56: /* SPECIAL opcode 56: DSLL */
			RtWidth = IntGetWidth(WA, RT_OF(op));
			RdWidth = Min(RtWidth + SA_OF(op), 64);
			IntSetWidth(WA, RD_OF(op), RdWidth);
			/* If the width of the result is 31 or less, the 32nd bit will
			 * still be copied 32 times. In that case, make it an SLL. */
			if (RdWidth <= 31) {
				return SimplifyOpcode(MAKE_SLL(RD_OF(op), RT_OF(op), SA_OF(op)));
			}
			break;
		case 58: /* SPECIAL opcode 58: DSRL */
			RtWidth = IntGetWidth(WA, RT_OF(op));
			RdWidth = Max(RtWidth - (SA_OF(op) - 1), 1);
			/* Can't simplify this DSRL to SRL because a 32-bit source could
			 * have bits 32..63 set, not just unset. */
			IntSetWidth(WA, RD_OF(op), RdWidth);
			break;
		case 59: /* SPECIAL opcode 59: DSRA */
			RtWidth = IntGetWidth(WA, RT_OF(op));
			/* If the width of the source is 32 or less, the 32nd bit (or even
			 * a lower bit) will be copied below, and bits 32..63 will not
			 * change. In that case, make it an SRA. */
			if (RtWidth <= 32) {
				return UpdateWidthAnalysis(WA, MAKE_SRA(RD_OF(op), RT_OF(op), SA_OF(op)));
			}
			RdWidth = Max(RtWidth - SA_OF(op), 1);
			IntSetWidth(WA, RD_OF(op), RdWidth);
			break;
		case 60: /* SPECIAL opcode 60: DSLL32 */
			RtWidth = IntGetWidth(WA, RT_OF(op));
			RdWidth = Min(RtWidth + 32 + SA_OF(op), 64);
			IntSetWidth(WA, RD_OF(op), RdWidth);
			break;
		case 62: /* SPECIAL opcode 62: DSRL32 */
			RtWidth = IntGetWidth(WA, RT_OF(op));
			RdWidth = Max(RtWidth - (32 + SA_OF(op) - 1), 1);
			IntSetWidth(WA, RD_OF(op), RdWidth);
			break;
		case 63: /* SPECIAL opcode 63: DSRA32 */
			RtWidth = IntGetWidth(WA, RT_OF(op));
			if (RtWidth == 1) {
				return UpdateWidthAnalysis(WA, SimplifyOpcode(MAKE_OR(RD_OF(op), RT_OF(op), 0)));
			} else if (RtWidth <= 32) {
				/* The source is already 32-bit or less. Shifting it to the
				 * right by over 32 bits is unneeded. */
				return UpdateWidthAnalysis(WA, MAKE_SRA(RD_OF(op), RT_OF(op), 31));
			}
			RdWidth = Max(RtWidth - (32 + SA_OF(op)), 1);
			IntSetWidth(WA, RD_OF(op), RdWidth);
			break;
		default:
			/* We don't know what the opcode did. Assume the width of EVERY
			 * register became 64. */
			InitWidthAnalysis(WA);
			break;
		} /* switch (op & 0x3F) for the SPECIAL prefix */
		break;
	case 1: /* REGIMM prefix */
		switch ((op >> 16) & 0x1F) {
		case 0: /* REGIMM opcode 0: BLTZ */
		case 1: /* REGIMM opcode 1: BGEZ */
		case 2: /* REGIMM opcode 2: BLTZL */
		case 3: /* REGIMM opcode 3: BGEZL */
			break;
		case 16: /* REGIMM opcode 16: BLTZAL */
		case 17: /* REGIMM opcode 17: BGEZAL */
		case 18: /* REGIMM opcode 18: BLTZALL */
		case 19: /* REGIMM opcode 19: BGEZALL */
			IntSetWidth(WA, 31, 32);
			break;
		default:
			/* We don't know what the opcode did. Assume the width of EVERY
			 * register became 64. */
			InitWidthAnalysis(WA);
			break;
		} /* switch ((op >> 16) & 0x1F) for the REGIMM prefix */
		break;
	case 2: /* Major opcode 2: J */
		break;
	case 3: /* Major opcode 3: JAL */
		IntSetWidth(WA, 31, 32);
		break;
	case 4: /* Major opcode 4: BEQ */
	case 5: /* Major opcode 5: BNE */
	case 6: /* Major opcode 6: BLEZ */
	case 7: /* Major opcode 7: BGTZ */
		break;
	case 8: /* Major opcode 8: ADDI */
	case 9: /* Major opcode 9: ADDIU */
		RsWidth = IntGetWidth(WA, RS_OF(op));
		ImmWidth = GetConstantWidth(SE64(IMM16S_OF(op), 16));
		RtWidth = Min(Max(RsWidth, ImmWidth) + 1, 32);
		IntSetWidth(WA, RT_OF(op), RtWidth);
		break;
	case 10: /* Major opcode 10: SLTI */
	case 11: /* Major opcode 11: SLTIU */
		IntSetWidth(WA, RT_OF(op), 2);
	case 12: /* Major opcode 12: ANDI */
		ImmWidth = GetConstantWidth(IMM16U_OF(op));
		IntSetWidth(WA, RT_OF(op), ImmWidth);
		break;
	case 13: /* Major opcode 13: ORI */
	case 14: /* Major opcode 14: XORI */
		RsWidth = IntGetWidth(WA, RS_OF(op));
		ImmWidth = GetConstantWidth(IMM16U_OF(op));
		RtWidth = Max(RsWidth, ImmWidth);
		IntSetWidth(WA, RT_OF(op), RtWidth);
		break;
	case 15: /* Major opcode 15: LUI */
		ImmWidth = GetConstantWidth(IMM16S_OF(op));
		RtWidth = ImmWidth + 16;
		IntSetWidth(WA, RT_OF(op), RtWidth);
		break;
	case 16: /* Coprocessor 0 prefix */
		switch ((op >> 21) & 0x1F) {
		case 0: /* Coprocessor 0 opcode 0: MFC0 */
			IntSetWidth(WA, RT_OF(op), 32);
			break;
		case 4: /* Coprocessor 0 opcode 4: MTC0 */
			break;
		case 16: /* Coprocessor 0 opcode 16: TLB */
			switch (op & 0x3F) {
			case 1: /* TLB opcode 1: TLBR */
			case 2: /* TLB opcode 2: TLBWI */
			case 6: /* TLB opcode 6: TLBWR */
			case 8: /* TLB opcode 8: TLBP */
			case 24: /* TLB opcode 24: ERET */
				break;
			default:
				/* We don't know what the opcode did. Assume the width of EVERY
				 * register became 64. */
				InitWidthAnalysis(WA);
				break;
			} /* switch (op & 0x3F) for Coprocessor 0 TLB opcodes */
			break;
		default:
			/* We don't know what the opcode did. Assume the width of EVERY
			 * register became 64. */
			InitWidthAnalysis(WA);
			break;
		} /* switch ((op >> 21) & 0x1F) for the Coprocessor 0 prefix */
		break;
	case 17: /* Coprocessor 1 prefix */
		switch ((op >> 21) & 0x1F) {
		case 0: /* Coprocessor 1 opcode 0: MFC1 */
			IntSetWidth(WA, RT_OF(op), 32);
			break;
		case 1: /* Coprocessor 1 opcode 1: DMFC1 */
			IntSetWidth(WA, RT_OF(op), 64);
			break;
		case 2: /* Coprocessor 1 opcode 2: CFC1 */
			IntSetWidth(WA, RT_OF(op), 32);
			break;
		case 4: /* Coprocessor 1 opcode 4: MTC1 */
		case 5: /* Coprocessor 1 opcode 5: DMTC1 */
		case 6: /* Coprocessor 1 opcode 6: CTC1 */
			break;
		case 8: /* Coprocessor 1 opcode 8: Branch on C1 condition... */
		case 16: /* Coprocessor 1 S-format opcodes */
		case 17: /* Coprocessor 1 D-format opcodes */
		case 20: /* Coprocessor 1 W-format opcodes */
		case 21: /* Coprocessor 1 L-format opcodes */
			break;
		default:
			/* We don't know what the opcode did. Assume the width of EVERY
			 * register became 64. */
			InitWidthAnalysis(WA);
			break;
		} /* switch ((op >> 21) & 0x1F) for the Coprocessor 1 prefix */
		break;
	case 20: /* Major opcode 20: BEQL */
	case 21: /* Major opcode 21: BNEL */
	case 22: /* Major opcode 22: BLEZL */
	case 23: /* Major opcode 23: BGTZL */
		break;
	case 24: /* Major opcode 24: DADDI */
	case 25: /* Major opcode 25: DADDIU */
		RsWidth = IntGetWidth(WA, RS_OF(op));
		ImmWidth = GetConstantWidth(SE64(IMM16S_OF(op), 16));
		RtWidth = Min(Max(RsWidth, ImmWidth) + 1, 64);
		IntSetWidth(WA, RT_OF(op), RtWidth);
		/* If the register operand is 31-bit or under, the result must be
		 * within +/- 0x7FFF_FFFF (64-bit). The 64-bit result will end up
		 * having the upper 32 bits sign-extended. */
		if (RtWidth <= 32) {
			return MAKE_ADDIU(RT_OF(op), RS_OF(op), IMM16S_OF(op));
		}
		break;
	case 26: /* Major opcode 26: LDL */
	case 27: /* Major opcode 27: LDR */
		IntSetWidth(WA, RT_OF(op), 64);
		break;
	case 32: /* Major opcode 32: LB */
		IntSetWidth(WA, RT_OF(op), 8);
		break;
	case 33: /* Major opcode 33: LH */
		IntSetWidth(WA, RT_OF(op), 16);
		break;
	case 34: /* Major opcode 34: LWL */
	case 35: /* Major opcode 35: LW */
		IntSetWidth(WA, RT_OF(op), 32);
		break;
	case 36: /* Major opcode 36: LBU */
		IntSetWidth(WA, RT_OF(op), 9);
		break;
	case 37: /* Major opcode 37: LHU */
		IntSetWidth(WA, RT_OF(op), 17);
		break;
	case 38: /* Major opcode 38: LWR */
		IntSetWidth(WA, RT_OF(op), 32);
		break;
	case 39: /* Major opcode 39: LWU */
		IntSetWidth(WA, RT_OF(op), 33);
		break;
	case 40: /* Major opcode 40: SB */
	case 41: /* Major opcode 41: SH */
	case 42: /* Major opcode 42: SWL */
	case 43: /* Major opcode 43: SW */
	case 44: /* Major opcode 44: SDL */
	case 45: /* Major opcode 45: SDR */
	case 46: /* Major opcode 46: SWR */
	case 47: /* Major opcode 47: CACHE */
		break;
	case 48: /* Major opcode 48: LL */
		IntSetWidth(WA, RT_OF(op), 32);
		break;
	case 49: /* Major opcode 49: LWC1 */
	case 53: /* Major opcode 53: LDC1 */
		break;
	case 55: /* Major opcode 55: LD */
		IntSetWidth(WA, RT_OF(op), 64);
		break;
	case 56: /* Major opcode 56: SC */
		IntSetWidth(WA, RT_OF(op), 2);
		break;
	case 57: /* Major opcode 57: SWC1 */
	case 61: /* Major opcode 61: SDC1 */
	case 63: /* Major opcode 63: SD */
		break;
	default:
		/* We don't know what the opcode did. Assume the width of EVERY
		 * register became 64. */
		InitWidthAnalysis(WA);
		break;
	}

	return op;
}

int8_t IntGetWidth(const struct WidthAnalysis* WA, uint8_t Reg)
{
	assert(Reg < 32);
	return WA->IntWidth[Reg];
}

int8_t HIGetWidth(const struct WidthAnalysis* WA)
{
	return WA->HIWidth;
}

int8_t LOGetWidth(const struct WidthAnalysis* WA)
{
	return WA->LOWidth;
}

uint32_t IntGetReads(uint32_t op)
{
	switch ((op >> 26) & 0x3F) {
	case 0: /* SPECIAL prefix */
		switch (op & 0x3F) {
		case 0: /* SPECIAL opcode 0: SLL */
		case 2: /* SPECIAL opcode 2: SRL */
		case 3: /* SPECIAL opcode 3: SRA */
			return BIT(RT_OF(op));
		case 4: /* SPECIAL opcode 4: SLLV */
		case 6: /* SPECIAL opcode 6: SRLV */
		case 7: /* SPECIAL opcode 7: SRAV */
			return BIT(RT_OF(op)) | BIT(RS_OF(op));
		case 8: /* SPECIAL opcode 8: JR */
		case 9: /* SPECIAL opcode 8: JALR */
			return BIT(RS_OF(op));
		case 12: /* SPECIAL opcode 12: SYSCALL */
		case 15: /* SPECIAL opcode 15: SYNC */
			return 0;
		case 16: /* SPECIAL opcode 16: MFHI */
			/* Does not read integer registers, only HI */
			return 0;
		case 17: /* SPECIAL opcode 17: MTHI */
			return BIT(RS_OF(op));
		case 18: /* SPECIAL opcode 18: MFLO */
			/* Does not read integer registers, only LO */
			return 0;
		case 19: /* SPECIAL opcode 19: MTLO */
			return BIT(RS_OF(op));
		case 20: /* SPECIAL opcode 20: DSLLV */
		case 22: /* SPECIAL opcode 22: DSRLV */
		case 23: /* SPECIAL opcode 23: DSRAV */
			return BIT(RT_OF(op)) | BIT(RS_OF(op));
		case 24: /* SPECIAL opcode 24: MULT */
		case 25: /* SPECIAL opcode 25: MULTU */
		case 26: /* SPECIAL opcode 26: DIV */
		case 27: /* SPECIAL opcode 27: DIVU */
		case 28: /* SPECIAL opcode 28: DMULT */
		case 29: /* SPECIAL opcode 29: DMULTU */
		case 30: /* SPECIAL opcode 30: DDIV */
		case 31: /* SPECIAL opcode 31: DDIVU */
		case 32: /* SPECIAL opcode 32: ADD */
		case 33: /* SPECIAL opcode 33: ADDU */
		case 34: /* SPECIAL opcode 34: SUB */
		case 35: /* SPECIAL opcode 35: SUBU */
		case 36: /* SPECIAL opcode 36: AND */
		case 37: /* SPECIAL opcode 37: OR */
		case 38: /* SPECIAL opcode 38: XOR */
		case 39: /* SPECIAL opcode 39: NOR */
		case 42: /* SPECIAL opcode 42: SLT */
		case 43: /* SPECIAL opcode 43: SLTU */
		case 44: /* SPECIAL opcode 44: DADD */
		case 45: /* SPECIAL opcode 45: DADDU */
		case 46: /* SPECIAL opcode 46: DSUB */
		case 47: /* SPECIAL opcode 47: DSUBU */
		case 52: /* SPECIAL opcode 52: TEQ */
			return BIT(RS_OF(op)) | BIT(RT_OF(op));
		case 56: /* SPECIAL opcode 56: DSLL */
		case 58: /* SPECIAL opcode 58: DSRL */
		case 59: /* SPECIAL opcode 59: DSRA */
		case 60: /* SPECIAL opcode 60: DSLL32 */
		case 62: /* SPECIAL opcode 62: DSRL32 */
		case 63: /* SPECIAL opcode 63: DSRA32 */
			return BIT(RT_OF(op));
		} /* switch (op & 0x3F) for the SPECIAL prefix */
		break;
	case 1: /* REGIMM prefix */
		switch ((op >> 16) & 0x1F) {
		case 0: /* REGIMM opcode 0: BLTZ */
		case 1: /* REGIMM opcode 1: BGEZ */
		case 2: /* REGIMM opcode 2: BLTZL */
		case 3: /* REGIMM opcode 3: BGEZL */
		case 16: /* REGIMM opcode 16: BLTZAL */
		case 17: /* REGIMM opcode 17: BGEZAL */
		case 18: /* REGIMM opcode 18: BLTZALL */
		case 19: /* REGIMM opcode 19: BGEZALL */
			return BIT(RS_OF(op));
		} /* switch ((op >> 16) & 0x1F) for the REGIMM prefix */
		break;
	case 2: /* Major opcode 2: J */
	case 3: /* Major opcode 3: JAL */
		return 0;
	case 4: /* Major opcode 4: BEQ */
	case 5: /* Major opcode 5: BNE */
		return BIT(RS_OF(op)) | BIT(RT_OF(op));
	case 6: /* Major opcode 6: BLEZ */
	case 7: /* Major opcode 7: BGTZ */
		return BIT(RS_OF(op));
	case 8: /* Major opcode 8: ADDI */
	case 9: /* Major opcode 9: ADDIU */
	case 10: /* Major opcode 10: SLTI */
	case 11: /* Major opcode 11: SLTIU */
	case 12: /* Major opcode 12: ANDI */
	case 13: /* Major opcode 13: ORI */
	case 14: /* Major opcode 14: XORI */
		return BIT(RS_OF(op));
	case 15: /* Major opcode 15: LUI */
		return 0;
	case 16: /* Coprocessor 0 prefix */
		switch ((op >> 21) & 0x1F) {
		case 0: /* Coprocessor 0 opcode 0: MFC0 */
			return 0;
		case 4: /* Coprocessor 0 opcode 4: MTC0 */
			return BIT(RT_OF(op));
		case 16: /* Coprocessor 0 opcode 16: TLB */
			switch (op & 0x3F) {
			case 1: /* TLB opcode 1: TLBR */
			case 2: /* TLB opcode 2: TLBWI */
			case 6: /* TLB opcode 6: TLBWR */
			case 8: /* TLB opcode 8: TLBP */
			case 24: /* TLB opcode 24: ERET */
				return 0;
			} /* switch (op & 0x3F) for Coprocessor 0 TLB opcodes */
			break;
		} /* switch ((op >> 21) & 0x1F) for the Coprocessor 0 prefix */
		break;
	case 17: /* Coprocessor 1 prefix */
		switch ((op >> 21) & 0x1F) {
		case 0: /* Coprocessor 1 opcode 0: MFC1 */
		case 1: /* Coprocessor 1 opcode 1: DMFC1 */
		case 2: /* Coprocessor 1 opcode 2: CFC1 */
			return 0;
		case 4: /* Coprocessor 1 opcode 4: MTC1 */
		case 5: /* Coprocessor 1 opcode 5: DMTC1 */
		case 6: /* Coprocessor 1 opcode 6: CTC1 */
			return BIT(RT_OF(op));
		case 8: /* Coprocessor 1 opcode 8: Branch on C1 condition... */
		case 16: /* Coprocessor 1 S-format opcodes */
		case 17: /* Coprocessor 1 D-format opcodes */
		case 20: /* Coprocessor 1 W-format opcodes */
		case 21: /* Coprocessor 1 L-format opcodes */
			return 0;
		} /* switch ((op >> 21) & 0x1F) for the Coprocessor 1 prefix */
		break;
	case 20: /* Major opcode 20: BEQL */
	case 21: /* Major opcode 21: BNEL */
		return BIT(RS_OF(op)) | BIT(RT_OF(op));
	case 22: /* Major opcode 22: BLEZL */
	case 23: /* Major opcode 23: BGTZL */
	case 24: /* Major opcode 24: DADDI */
	case 25: /* Major opcode 25: DADDIU */
		return BIT(RS_OF(op));
	case 26: /* Major opcode 26: LDL */
	case 27: /* Major opcode 27: LDR */
	case 32: /* Major opcode 32: LB */
	case 33: /* Major opcode 33: LH */
	case 34: /* Major opcode 34: LWL */
	case 35: /* Major opcode 35: LW */
	case 36: /* Major opcode 36: LBU */
	case 37: /* Major opcode 37: LHU */
	case 38: /* Major opcode 38: LWR */
	case 39: /* Major opcode 39: LWU */
		return BIT(RS_OF(op));
	case 40: /* Major opcode 40: SB */
	case 41: /* Major opcode 41: SH */
	case 42: /* Major opcode 42: SWL */
	case 43: /* Major opcode 43: SW */
	case 44: /* Major opcode 44: SDL */
	case 45: /* Major opcode 45: SDR */
	case 46: /* Major opcode 46: SWR */
		return BIT(RS_OF(op)) | BIT(RT_OF(op));
	case 47: /* Major opcode 47: CACHE */
		/* Technically CACHE reads the Rs register, but mupen64plus does not
		 * implement CACHE. */
		return BIT(RS_OF(op));
	case 48: /* Major opcode 48: LL */
	case 49: /* Major opcode 49: LWC1 */
	case 53: /* Major opcode 53: LDC1 */
	case 55: /* Major opcode 55: LD */
		return BIT(RS_OF(op));
	case 56: /* Major opcode 56: SC */
		return BIT(RS_OF(op)) | BIT(RT_OF(op));
	case 57: /* Major opcode 57: SWC1 */
	case 61: /* Major opcode 61: SDC1 */
		return BIT(RS_OF(op));
	case 63: /* Major opcode 63: SD */
		return BIT(RS_OF(op)) | BIT(RT_OF(op));
	}

	/* We don't know what the opcode did. Assume EVERY register was read.
	 * This is a safe default for optimisation purposes, as this opcode will
	 * then act as a barrier. */
	return ~(uint32_t) 0;
}

uint32_t IntGetWrites(uint32_t op)
{
	switch ((op >> 26) & 0x3F) {
	case 0: /* SPECIAL prefix */
		switch (op & 0x3F) {
		case 0: /* SPECIAL opcode 0: SLL */
		case 2: /* SPECIAL opcode 2: SRL */
		case 3: /* SPECIAL opcode 3: SRA */
		case 4: /* SPECIAL opcode 4: SLLV */
		case 6: /* SPECIAL opcode 6: SRLV */
		case 7: /* SPECIAL opcode 7: SRAV */
			return BIT(RD_OF(op)) & ~BIT(0);
		case 8: /* SPECIAL opcode 8: JR */
			return 0;
		case 9: /* SPECIAL opcode 8: JALR */
			return BIT(RD_OF(op)) & ~BIT(0);
		case 12: /* SPECIAL opcode 12: SYSCALL */
		case 15: /* SPECIAL opcode 15: SYNC */
			return 0;
		case 16: /* SPECIAL opcode 16: MFHI */
			return BIT(RD_OF(op)) & ~BIT(0);
		case 17: /* SPECIAL opcode 17: MTHI */
			/* Does not write to integer registers, only to HI */
			return 0;
		case 18: /* SPECIAL opcode 18: MFLO */
			return BIT(RD_OF(op)) & ~BIT(0);
		case 19: /* SPECIAL opcode 19: MTLO */
			/* Does not write to integer registers, only to LO */
			return 0;
		case 20: /* SPECIAL opcode 20: DSLLV */
		case 22: /* SPECIAL opcode 22: DSRLV */
		case 23: /* SPECIAL opcode 23: DSRAV */
			return BIT(RD_OF(op)) & ~BIT(0);
		case 24: /* SPECIAL opcode 24: MULT */
		case 25: /* SPECIAL opcode 25: MULTU */
		case 26: /* SPECIAL opcode 26: DIV */
		case 27: /* SPECIAL opcode 27: DIVU */
		case 28: /* SPECIAL opcode 28: DMULT */
		case 29: /* SPECIAL opcode 29: DMULTU */
		case 30: /* SPECIAL opcode 30: DDIV */
		case 31: /* SPECIAL opcode 31: DDIVU */
			/* Does not write to integer registers, only to HI and LO */
			return 0;
		case 32: /* SPECIAL opcode 32: ADD */
		case 33: /* SPECIAL opcode 33: ADDU */
		case 34: /* SPECIAL opcode 34: SUB */
		case 35: /* SPECIAL opcode 35: SUBU */
		case 36: /* SPECIAL opcode 36: AND */
		case 37: /* SPECIAL opcode 37: OR */
		case 38: /* SPECIAL opcode 38: XOR */
		case 39: /* SPECIAL opcode 39: NOR */
		case 42: /* SPECIAL opcode 42: SLT */
		case 43: /* SPECIAL opcode 43: SLTU */
		case 44: /* SPECIAL opcode 44: DADD */
		case 45: /* SPECIAL opcode 45: DADDU */
		case 46: /* SPECIAL opcode 46: DSUB */
		case 47: /* SPECIAL opcode 47: DSUBU */
			return BIT(RD_OF(op)) & ~BIT(0);
		case 52: /* SPECIAL opcode 52: TEQ */
			return 0;
		case 56: /* SPECIAL opcode 56: DSLL */
		case 58: /* SPECIAL opcode 58: DSRL */
		case 59: /* SPECIAL opcode 59: DSRA */
		case 60: /* SPECIAL opcode 60: DSLL32 */
		case 62: /* SPECIAL opcode 62: DSRL32 */
		case 63: /* SPECIAL opcode 63: DSRA32 */
			return BIT(RD_OF(op)) & ~BIT(0);
		} /* switch (op & 0x3F) for the SPECIAL prefix */
		break;
	case 1: /* REGIMM prefix */
		switch ((op >> 16) & 0x1F) {
		case 0: /* REGIMM opcode 0: BLTZ */
		case 1: /* REGIMM opcode 1: BGEZ */
		case 2: /* REGIMM opcode 2: BLTZL */
		case 3: /* REGIMM opcode 3: BGEZL */
			return 0;
		case 16: /* REGIMM opcode 16: BLTZAL */
		case 17: /* REGIMM opcode 17: BGEZAL */
		case 18: /* REGIMM opcode 18: BLTZALL */
		case 19: /* REGIMM opcode 19: BGEZALL */
			return BIT(31);
		} /* switch ((op >> 16) & 0x1F) for the REGIMM prefix */
		break;
	case 2: /* Major opcode 2: J */
		return 0;
	case 3: /* Major opcode 3: JAL */
		return BIT(31);
	case 4: /* Major opcode 4: BEQ */
	case 5: /* Major opcode 5: BNE */
	case 6: /* Major opcode 6: BLEZ */
	case 7: /* Major opcode 7: BGTZ */
		return 0;
	case 8: /* Major opcode 8: ADDI */
	case 9: /* Major opcode 9: ADDIU */
	case 10: /* Major opcode 10: SLTI */
	case 11: /* Major opcode 11: SLTIU */
	case 12: /* Major opcode 12: ANDI */
	case 13: /* Major opcode 13: ORI */
	case 14: /* Major opcode 14: XORI */
	case 15: /* Major opcode 15: LUI */
		return BIT(RT_OF(op)) & ~BIT(0);
	case 16: /* Coprocessor 0 prefix */
		switch ((op >> 21) & 0x1F) {
		case 0: /* Coprocessor 0 opcode 0: MFC0 */
			return BIT(RT_OF(op)) & ~BIT(0);
		case 4: /* Coprocessor 0 opcode 4: MTC0 */
			return 0;
		case 16: /* Coprocessor 0 opcode 16: TLB */
			switch (op & 0x3F) {
			case 1: /* TLB opcode 1: TLBR */
			case 2: /* TLB opcode 2: TLBWI */
			case 6: /* TLB opcode 6: TLBWR */
			case 8: /* TLB opcode 8: TLBP */
			case 24: /* TLB opcode 24: ERET */
				return 0;
			} /* switch (op & 0x3F) for Coprocessor 0 TLB opcodes */
			break;
		} /* switch ((op >> 21) & 0x1F) for the Coprocessor 0 prefix */
		break;
	case 17: /* Coprocessor 1 prefix */
		switch ((op >> 21) & 0x1F) {
		case 0: /* Coprocessor 1 opcode 0: MFC1 */
		case 1: /* Coprocessor 1 opcode 1: DMFC1 */
		case 2: /* Coprocessor 1 opcode 2: CFC1 */
			return BIT(RT_OF(op)) & ~BIT(0);
		case 4: /* Coprocessor 1 opcode 4: MTC1 */
		case 5: /* Coprocessor 1 opcode 5: DMTC1 */
		case 6: /* Coprocessor 1 opcode 6: CTC1 */
		case 8: /* Coprocessor 1 opcode 8: Branch on C1 condition... */
		case 16: /* Coprocessor 1 S-format opcodes */
		case 17: /* Coprocessor 1 D-format opcodes */
		case 20: /* Coprocessor 1 W-format opcodes */
		case 21: /* Coprocessor 1 L-format opcodes */
			return 0;
		} /* switch ((op >> 21) & 0x1F) for the Coprocessor 1 prefix */
		break;
	case 20: /* Major opcode 20: BEQL */
	case 21: /* Major opcode 21: BNEL */
	case 22: /* Major opcode 22: BLEZL */
	case 23: /* Major opcode 23: BGTZL */
		return 0;
	case 24: /* Major opcode 24: DADDI */
	case 25: /* Major opcode 25: DADDIU */
		return BIT(RT_OF(op)) & ~BIT(0);
	case 26: /* Major opcode 26: LDL */
	case 27: /* Major opcode 27: LDR */
	case 32: /* Major opcode 32: LB */
	case 33: /* Major opcode 33: LH */
	case 34: /* Major opcode 34: LWL */
	case 35: /* Major opcode 35: LW */
	case 36: /* Major opcode 36: LBU */
	case 37: /* Major opcode 37: LHU */
	case 38: /* Major opcode 38: LWR */
	case 39: /* Major opcode 39: LWU */
		return BIT(RT_OF(op)) & ~BIT(0);
	case 40: /* Major opcode 40: SB */
	case 41: /* Major opcode 41: SH */
	case 42: /* Major opcode 42: SWL */
	case 43: /* Major opcode 43: SW */
	case 44: /* Major opcode 44: SDL */
	case 45: /* Major opcode 45: SDR */
	case 46: /* Major opcode 46: SWR */
	case 47: /* Major opcode 47: CACHE */
		return 0;
	case 48: /* Major opcode 48: LL */
		return BIT(RT_OF(op)) & ~BIT(0);
	case 49: /* Major opcode 49: LWC1 */
	case 53: /* Major opcode 53: LDC1 */
		return 0;
	case 55: /* Major opcode 55: LD */
	case 56: /* Major opcode 56: SC */
		return BIT(RT_OF(op)) & ~BIT(0);
	case 57: /* Major opcode 57: SWC1 */
	case 61: /* Major opcode 61: SDC1 */
	case 63: /* Major opcode 63: SD */
		return 0;
	}

	/* We don't know what the opcode did. Assume EVERY register was written.
	 * This is a safe default for optimisation purposes, as this opcode will
	 * then act as a barrier. */
	return ~(uint32_t) 0;
}

bool CanRaiseTLBRefill(uint32_t op)
{
	switch ((op >> 26) & 0x3F) {
	case 26: /* Major opcode 26: LDL */
	case 27: /* Major opcode 27: LDR */
	case 32: /* Major opcode 32: LB */
	case 33: /* Major opcode 33: LH */
	case 34: /* Major opcode 34: LWL */
	case 35: /* Major opcode 35: LW */
	case 36: /* Major opcode 36: LBU */
	case 37: /* Major opcode 37: LHU */
	case 38: /* Major opcode 38: LWR */
	case 39: /* Major opcode 39: LWU */
	case 40: /* Major opcode 40: SB */
	case 41: /* Major opcode 41: SH */
	case 42: /* Major opcode 42: SWL */
	case 43: /* Major opcode 43: SW */
	case 44: /* Major opcode 44: SDL */
	case 45: /* Major opcode 45: SDR */
	case 46: /* Major opcode 46: SWR */
	case 48: /* Major opcode 48: LL */
	case 49: /* Major opcode 49: LWC1 */
	case 53: /* Major opcode 53: LDC1 */
	case 55: /* Major opcode 55: LD */
	case 56: /* Major opcode 56: SC */
	case 57: /* Major opcode 57: SWC1 */
	case 61: /* Major opcode 61: SDC1 */
	case 63: /* Major opcode 63: SD */
		return true;
	default:
		return false;
	}
}

bool CanRaiseCop1Unusable(uint32_t op)
{
	switch ((op >> 26) & 0x3F) {
	case 17: /* Coprocessor 1 prefix */
	case 49: /* Major opcode 49: LWC1 */
	case 53: /* Major opcode 53: LDC1 */
	case 57: /* Major opcode 57: SWC1 */
	case 61: /* Major opcode 61: SDC1 */
		return true;
	default:
		return false;
	}
}

enum RoundingModeSource UsesRoundingMode(uint32_t op)
{
	switch ((op >> 26) & 0x3F) {
	case 17: /* Coprocessor 1 prefix */
		switch ((op >> 21) & 0x1F) {
		case 0: /* Coprocessor 1 opcode 0: MFC1 */
		case 1: /* Coprocessor 1 opcode 1: DMFC1 */
		case 2: /* Coprocessor 1 opcode 2: CFC1 */
		case 4: /* Coprocessor 1 opcode 4: MTC1 */
		case 5: /* Coprocessor 1 opcode 5: DMTC1 */
		case 6: /* Coprocessor 1 opcode 6: CTC1 */
		case 8: /* Coprocessor 1 opcode 8: Branch on C1 condition... */
			return RMS_NONE;
		case 16: /* Coprocessor 1 S-format opcodes */
			switch (op & 0x3F) {
			case 0: /* S-format opcode: ADD_S */
			case 1: /* S-format opcode: SUB_S */
			case 2: /* S-format opcode: MUL_S */
			case 3: /* S-format opcode: DIV_S */
			case 4: /* S-format opcode: SQRT_S */
				return RMS_N64;
			case 5: /* S-format opcode: ABS_S */
			case 6: /* S-format opcode: MOV_S */
			case 7: /* S-format opcode: NEG_S */
				/* These operations are exact. */
				return RMS_NONE;
			case 8: /* S-format opcode: ROUND_L_S */
			case 9: /* S-format opcode: TRUNC_L_S */
			case 10: /* S-format opcode: CEIL_L_S */
			case 11: /* S-format opcode: FLOOR_L_S */
			case 12: /* S-format opcode: ROUND_W_S */
			case 13: /* S-format opcode: TRUNC_W_S */
			case 14: /* S-format opcode: CEIL_W_S */
			case 15: /* S-format opcode: FLOOR_W_S */
				return RMS_OP;
			case 33: /* S-format opcode: CVT_D_S */
				/* This operation is exact. */
				return RMS_NONE;
			case 36: /* S-format opcode: CVT_W_S */
			case 37: /* S-format opcode: CVT_L_S */
				return RMS_N64;
			case 48: /* S-format opcode: C_F_S */
			case 49: /* S-format opcode: C_UN_S */
			case 50: /* S-format opcode: C_EQ_S */
			case 51: /* S-format opcode: C_UEQ_S */
			case 52: /* S-format opcode: C_OLT_S */
			case 53: /* S-format opcode: C_ULT_S */
			case 54: /* S-format opcode: C_OLE_S */
			case 55: /* S-format opcode: C_ULE_S */
			case 56: /* S-format opcode: C_SF_S */
			case 57: /* S-format opcode: C_NGLE_S */
			case 58: /* S-format opcode: C_SEQ_S */
			case 59: /* S-format opcode: C_NGL_S */
			case 60: /* S-format opcode: C_LT_S */
			case 61: /* S-format opcode: C_NGE_S */
			case 62: /* S-format opcode: C_LE_S */
			case 63: /* S-format opcode: C_NGT_S */
				return RMS_NONE;
			default: /* Coprocessor 1 S-format opcodes 16..32, 34..35, 38..47:
			            Reserved Instructions */
				return RMS_NONE;
			} /* switch (op & 0x3F) for Coprocessor 1 S-format opcodes */
			break;
		case 17: /* Coprocessor 1 D-format opcodes */
			switch (op & 0x3F) {
			case 0: /* D-format opcode: ADD_D */
			case 1: /* D-format opcode: SUB_D */
			case 2: /* D-format opcode: MUL_D */
			case 3: /* D-format opcode: DIV_D */
			case 4: /* D-format opcode: SQRT_D */
				return RMS_N64;
			case 5: /* D-format opcode: ABS_D */
			case 6: /* D-format opcode: MOV_D */
			case 7: /* D-format opcode: NEG_D */
				/* These operations are exact. */
				return RMS_NONE;
			case 8: /* D-format opcode: ROUND_L_D */
			case 9: /* D-format opcode: TRUNC_L_D */
			case 10: /* D-format opcode: CEIL_L_D */
			case 11: /* D-format opcode: FLOOR_L_D */
			case 12: /* D-format opcode: ROUND_W_D */
			case 13: /* D-format opcode: TRUNC_W_D */
			case 14: /* D-format opcode: CEIL_W_D */
			case 15: /* D-format opcode: FLOOR_W_D */
				return RMS_OP;
			case 32: /* D-format opcode: CVT_S_D */
			case 36: /* D-format opcode: CVT_W_D */
			case 37: /* D-format opcode: CVT_L_D */
				return RMS_N64;
			case 48: /* D-format opcode: C_F_D */
			case 49: /* D-format opcode: C_UN_D */
			case 50: /* D-format opcode: C_EQ_D */
			case 51: /* D-format opcode: C_UEQ_D */
			case 52: /* D-format opcode: C_OLT_D */
			case 53: /* D-format opcode: C_ULT_D */
			case 54: /* D-format opcode: C_OLE_D */
			case 55: /* D-format opcode: C_ULE_D */
			case 56: /* D-format opcode: C_SF_D */
			case 57: /* D-format opcode: C_NGLE_D */
			case 58: /* D-format opcode: C_SEQ_D */
			case 59: /* D-format opcode: C_NGL_D */
			case 60: /* D-format opcode: C_LT_D */
			case 61: /* D-format opcode: C_NGE_D */
			case 62: /* D-format opcode: C_LE_D */
			case 63: /* D-format opcode: C_NGT_D */
				return RMS_NONE;
			default: /* Coprocessor 1 D-format opcodes 16..31, 33..35, 38..47:
			            Reserved Instructions */
				return RMS_NONE;
			} /* switch (op & 0x3F) for Coprocessor 1 D-format opcodes */
			break;
		case 20: /* Coprocessor 1 W-format opcodes */
			switch (op & 0x3F) {
			case 32: /* W-format opcode: CVT_S_W */
				return RMS_N64;
			case 33: /* W-format opcode: CVT_D_W */
				/* This operation is exact. */
				return RMS_NONE;
			default: /* Coprocessor 1 W-format opcodes 0..31, 34..63:
			            Reserved Instructions */
				return RMS_NONE;
			}
			break;
		case 21: /* Coprocessor 1 L-format opcodes */
			switch (op & 0x3F) {
			case 32: /* L-format opcode: CVT_S_L */
			case 33: /* L-format opcode: CVT_D_L */
			default: /* Coprocessor 1 L-format opcodes 0..31, 34..63:
			            Reserved Instructions */
				return RMS_NONE;
			}
			break;
		default: /* Coprocessor 1 opcodes 3, 7, 9..15, 18..19, 22..31:
		            Reserved Instructions */
			return RMS_NONE;
		} /* switch ((op >> 21) & 0x1F) for the Coprocessor 1 prefix */
		break;
	default:
		return RMS_NONE;
	}
}
