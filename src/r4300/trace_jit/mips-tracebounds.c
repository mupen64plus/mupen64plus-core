/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - mips-tracebounds.c                                      *
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

#include <stdint.h>

#include "../cp0.h"
#include "mips-analysis.h"
#include "mips-parse.h"
#include "mips-simplify.h"
#include "mips-tracebounds.h"

unsigned int IsTraceBoundary(uint32_t op)
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
			return 0;
		case 8: /* SPECIAL opcode 8: JR */
		case 9: /* SPECIAL opcode 9: JALR */
			return 2;
		case 12: /* SPECIAL opcode 12: SYSCALL */
		case 13: /* SPECIAL opcode 13: BREAK (Not implemented) */
			return 1;
		case 15: /* SPECIAL opcode 15: SYNC */
		case 16: /* SPECIAL opcode 16: MFHI */
		case 17: /* SPECIAL opcode 17: MTHI */
		case 18: /* SPECIAL opcode 18: MFLO */
		case 19: /* SPECIAL opcode 19: MTLO */
		case 20: /* SPECIAL opcode 20: DSLLV */
		case 22: /* SPECIAL opcode 22: DSRLV */
		case 23: /* SPECIAL opcode 23: DSRAV */
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
			return 0;
		case 48: /* SPECIAL opcode 48: TGE (Not implemented) */
		case 49: /* SPECIAL opcode 49: TGEU (Not implemented) */
		case 50: /* SPECIAL opcode 50: TLT (Not implemented) */
		case 51: /* SPECIAL opcode 51: TLTU (Not implemented) */
			return 1;
		case 52: /* SPECIAL opcode 52: TEQ */
			return 0;
		case 54: /* SPECIAL opcode 54: TNE (Not implemented) */
			return 1;
		case 56: /* SPECIAL opcode 56: DSLL */
		case 58: /* SPECIAL opcode 58: DSRL */
		case 59: /* SPECIAL opcode 59: DSRA */
		case 60: /* SPECIAL opcode 60: DSLL32 */
		case 62: /* SPECIAL opcode 62: DSRL32 */
		case 63: /* SPECIAL opcode 63: DSRA32 */
			return 0;
		default: /* SPECIAL opcodes 1, 5, 10, 11, 14, 21, 40, 41, 53, 55, 57,
		            61: Reserved Instructions */
			return 1;
		} /* switch (op & 0x3F) for the SPECIAL prefix */
		break;
	case 1: /* REGIMM prefix */
		switch ((op >> 16) & 0x1F) {
		case 0: /* REGIMM opcode 0: BLTZ */
		case 1: /* REGIMM opcode 1: BGEZ */
		case 2: /* REGIMM opcode 2: BLTZL */
		case 3: /* REGIMM opcode 3: BGEZL */
			return 2;
		case 8: /* REGIMM opcode 8: TGEI (Not implemented) */
		case 9: /* REGIMM opcode 9: TGEIU (Not implemented) */
		case 10: /* REGIMM opcode 10: TLTI (Not implemented) */
		case 11: /* REGIMM opcode 11: TLTIU (Not implemented) */
		case 12: /* REGIMM opcode 12: TEQI (Not implemented) */
		case 14: /* REGIMM opcode 14: TNEI (Not implemented) */
			return 1;
		case 16: /* REGIMM opcode 16: BLTZAL */
		case 17: /* REGIMM opcode 17: BGEZAL */
		case 18: /* REGIMM opcode 18: BLTZALL */
		case 19: /* REGIMM opcode 19: BGEZALL */
			return 2;
		default: /* REGIMM opcodes 4..7, 13, 15, 20..31:
		            Reserved Instructions */
			return 1;
		} /* switch ((op >> 16) & 0x1F) for the REGIMM prefix */
		break;
	case 2: /* Major opcode 2: J */
	case 3: /* Major opcode 3: JAL */
	case 4: /* Major opcode 4: BEQ */
	case 5: /* Major opcode 5: BNE */
	case 6: /* Major opcode 6: BLEZ */
	case 7: /* Major opcode 7: BGTZ */
		return 2;
	case 8: /* Major opcode 8: ADDI */
	case 9: /* Major opcode 9: ADDIU */
	case 10: /* Major opcode 10: SLTI */
	case 11: /* Major opcode 11: SLTIU */
	case 12: /* Major opcode 12: ANDI */
	case 13: /* Major opcode 13: ORI */
	case 14: /* Major opcode 14: XORI */
	case 15: /* Major opcode 15: LUI */
		return 0;
	case 16: /* Coprocessor 0 prefix */
		switch ((op >> 21) & 0x1F) {
		case 0: /* Coprocessor 0 opcode 0: MFC0 */
			if (RD_OF(op) == CP0_RANDOM_REG) return 1;
			else                             return 0;
		case 4: /* Coprocessor 0 opcode 4: MTC0 */
			switch (RD_OF(op)) {
			case CP0_COUNT_REG:
			case CP0_STATUS_REG:
			case CP0_COMPARE_REG:
				/* These may raise interrupts. */
				return 1;
			case CP0_INDEX_REG:
			case CP0_RANDOM_REG:
			case CP0_ENTRYLO0_REG:
			case CP0_ENTRYLO1_REG:
			case CP0_CONTEXT_REG:
			case CP0_PAGEMASK_REG:
			case CP0_WIRED_REG:
			case CP0_BADVADDR_REG:
			case CP0_ENTRYHI_REG:
			case CP0_CAUSE_REG:
			case CP0_EPC_REG:
			case CP0_PREVID_REG:
			case CP0_CONFIG_REG:
			case CP0_WATCHLO_REG:
			case CP0_WATCHHI_REG:
			case CP0_TAGLO_REG:
			case CP0_TAGHI_REG:
				return 0;
			default:
				return 1;
			}
		case 16: /* Coprocessor 0 opcode 16: TLB */
			switch (op & 0x3F) {
			case 1: /* TLB opcode 1: TLBR */
			case 2: /* TLB opcode 2: TLBWI */
			case 6: /* TLB opcode 6: TLBWR */
			case 8: /* TLB opcode 8: TLBP */
				return 0;
			case 24: /* TLB opcode 24: ERET */
				return 1;
			default: /* TLB sub-opcodes 0, 3..5, 7, 9..23, 25..63:
			            Reserved Instructions */
				return 1;
			} /* switch (op & 0x3F) for Coprocessor 0 TLB opcodes */
			break;
		default: /* Coprocessor 0 opcodes 1..3, 4..15, 17..31:
		            Reserved Instructions */
			return 1;
		} /* switch ((op >> 21) & 0x1F) for the Coprocessor 0 prefix */
		break;
	case 17: /* Coprocessor 1 prefix */
		switch ((op >> 21) & 0x1F) {
		case 0: /* Coprocessor 1 opcode 0: MFC1 */
		case 1: /* Coprocessor 1 opcode 1: DMFC1 */
		case 2: /* Coprocessor 1 opcode 2: CFC1 */
		case 4: /* Coprocessor 1 opcode 4: MTC1 */
		case 5: /* Coprocessor 1 opcode 5: DMTC1 */
		case 6: /* Coprocessor 1 opcode 6: CTC1 */
			return 0;
		case 8: /* Coprocessor 1 opcode 8: Branch on C1 condition... */
			return 2;
		case 16: /* Coprocessor 1 S-format opcodes */
			switch (op & 0x3F) {
			case 0: /* S-format opcode 0: ADD.S */
			case 1: /* S-format opcode 1: SUB.S */
			case 2: /* S-format opcode 2: MUL.S */
			case 3: /* S-format opcode 3: DIV.S */
			case 4: /* S-format opcode 4: SQRT.S */
			case 5: /* S-format opcode 5: ABS.S */
			case 6: /* S-format opcode 6: MOV.S */
			case 7: /* S-format opcode 7: NEG.S */
			case 8: /* S-format opcode 8: ROUND.L.S */
			case 9: /* S-format opcode 9: TRUNC.L.S */
			case 10: /* S-format opcode 10: CEIL.L.S */
			case 11: /* S-format opcode 11: FLOOR.L.S */
			case 12: /* S-format opcode 12: ROUND.W.S */
			case 13: /* S-format opcode 13: TRUNC.W.S */
			case 14: /* S-format opcode 14: CEIL.W.S */
			case 15: /* S-format opcode 15: FLOOR.W.S */
			case 33: /* S-format opcode 33: CVT.D.S */
			case 36: /* S-format opcode 36: CVT.W.S */
			case 37: /* S-format opcode 37: CVT.L.S */
			case 48: /* S-format opcode 48: C.F.S */
			case 49: /* S-format opcode 49: C.UN.S */
			case 50: /* S-format opcode 50: C.EQ.S */
			case 51: /* S-format opcode 51: C.UEQ.S */
			case 52: /* S-format opcode 52: C.OLT.S */
			case 53: /* S-format opcode 53: C.ULT.S */
			case 54: /* S-format opcode 54: C.OLE.S */
			case 55: /* S-format opcode 55: C.ULE.S */
			case 56: /* S-format opcode 56: C.SF.S */
			case 57: /* S-format opcode 57: C.NGLE.S */
			case 58: /* S-format opcode 58: C.SEQ.S */
			case 59: /* S-format opcode 59: C.NGL.S */
			case 60: /* S-format opcode 60: C.LT.S */
			case 61: /* S-format opcode 61: C.NGE.S */
			case 62: /* S-format opcode 62: C.LE.S */
			case 63: /* S-format opcode 63: C.NGT.S */
				return 0;
			default: /* Coprocessor 1 S-format opcodes 16..32, 34..35, 38..47:
			            Reserved Instructions */
				return 1;
			} /* switch (op & 0x3F) for Coprocessor 1 S-format opcodes */
			break;
		case 17: /* Coprocessor 1 D-format opcodes */
			switch (op & 0x3F) {
			case 0: /* D-format opcode 0: ADD.D */
			case 1: /* D-format opcode 1: SUB.D */
			case 2: /* D-format opcode 2: MUL.D */
			case 3: /* D-format opcode 3: DIV.D */
			case 4: /* D-format opcode 4: SQRT.D */
			case 5: /* D-format opcode 5: ABS.D */
			case 6: /* D-format opcode 6: MOV.D */
			case 7: /* D-format opcode 7: NEG.D */
			case 8: /* D-format opcode 8: ROUND.L.D */
			case 9: /* D-format opcode 9: TRUNC.L.D */
			case 10: /* D-format opcode 10: CEIL.L.D */
			case 11: /* D-format opcode 11: FLOOR.L.D */
			case 12: /* D-format opcode 12: ROUND.W.D */
			case 13: /* D-format opcode 13: TRUNC.W.D */
			case 14: /* D-format opcode 14: CEIL.W.D */
			case 15: /* D-format opcode 15: FLOOR.W.D */
			case 32: /* D-format opcode 32: CVT.S.D */
			case 36: /* D-format opcode 36: CVT.W.D */
			case 37: /* D-format opcode 37: CVT.L.D */
			case 48: /* D-format opcode 48: C.F.D */
			case 49: /* D-format opcode 49: C.UN.D */
			case 50: /* D-format opcode 50: C.EQ.D */
			case 51: /* D-format opcode 51: C.UEQ.D */
			case 52: /* D-format opcode 52: C.OLT.D */
			case 53: /* D-format opcode 53: C.ULT.D */
			case 54: /* D-format opcode 54: C.OLE.D */
			case 55: /* D-format opcode 55: C.ULE.D */
			case 56: /* D-format opcode 56: C.SF.D */
			case 57: /* D-format opcode 57: C.NGLE.D */
			case 58: /* D-format opcode 58: C.SEQ.D */
			case 59: /* D-format opcode 59: C.NGL.D */
			case 60: /* D-format opcode 60: C.LT.D */
			case 61: /* D-format opcode 61: C.NGE.D */
			case 62: /* D-format opcode 62: C.LE.D */
			case 63: /* D-format opcode 63: C.NGT.D */
				return 0;
			default: /* Coprocessor 1 D-format opcodes 16..31, 33..35, 38..47:
			            Reserved Instructions */
				return 1;
			} /* switch (op & 0x3F) for Coprocessor 1 D-format opcodes */
			break;
		case 20: /* Coprocessor 1 W-format opcodes */
			switch (op & 0x3F) {
			case 32: /* W-format opcode 32: CVT.S.W */
			case 33: /* W-format opcode 33: CVT.D.W */
				return 0;
			default: /* Coprocessor 1 W-format opcodes 0..31, 34..63:
			            Reserved Instructions */
				return 1;
			}
			break;
		case 21: /* Coprocessor 1 L-format opcodes */
			switch (op & 0x3F) {
			case 32: /* L-format opcode 32: CVT.S.L */
			case 33: /* L-format opcode 33: CVT.D.L */
				return 0;
			default: /* Coprocessor 1 L-format opcodes 0..31, 34..63:
			            Reserved Instructions */
				return 1;
			}
			break;
		default: /* Coprocessor 1 opcodes 3, 7, 9..15, 18..19, 22..31:
		            Reserved Instructions */
			return 1;
		} /* switch ((op >> 21) & 0x1F) for the Coprocessor 1 prefix */
		break;
	case 20: /* Major opcode 20: BEQL */
	case 21: /* Major opcode 21: BNEL */
	case 22: /* Major opcode 22: BLEZL */
	case 23: /* Major opcode 23: BGTZL */
		return 2;
	case 24: /* Major opcode 24: DADDI */
	case 25: /* Major opcode 25: DADDIU */
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
	case 47: /* Major opcode 47: CACHE */
	case 48: /* Major opcode 48: LL */
	case 49: /* Major opcode 49: LWC1 */
		return 0;
	case 52: /* Major opcode 52: LLD (Not implemented) */
		return 1;
	case 53: /* Major opcode 53: LDC1 */
	case 55: /* Major opcode 55: LD */
	case 56: /* Major opcode 56: SC */
	case 57: /* Major opcode 57: SWC1 */
		return 0;
	case 60: /* Major opcode 60: SCD (Not implemented) */
		return 1;
	case 61: /* Major opcode 61: SDC1 */
	case 63: /* Major opcode 63: SD */
		return 0;
	default: /* Major opcodes 18..19, 28..31, 50..51, 54, 58..59, 62:
	            Reserved Instructions */
		return 1;
	} /* switch ((op >> 26) & 0x3F) */
}

size_t MakeTrace(uint32_t* Dest, const uint32_t* Raw, size_t Count)
{
	unsigned int Left = 0;
	size_t Result = 0;
	struct ConstAnalysis CA;
	struct WidthAnalysis WA;
	InitConstAnalysis(&CA);
	InitWidthAnalysis(&WA);
	while (Count > 0 && Left != 1) {
		uint32_t op;
		if (Left == 2) {
			/* The previous iteration looked at a branch and now we're in its
			 * delay slot. */
			Left--;
		}
		op = MandatorySimplifyOpcode(*Raw++);
		op = SimplifyOpcode(op);
		op = UpdateConstAnalysis(&CA, op);
		op = UpdateWidthAnalysis(&WA, op);
		*Dest++ = op;
		Count--;
		Result++;
		if (Left == 0) {
			/* When do we need to end this trace?
			 * - 0: not right now;
			 * - 1: immediately after this opcode;
			 * - 2: after two opcodes, or one if Count == 1. */
			Left = IsTraceBoundary(op);
		}
	}
	return Result;
}
