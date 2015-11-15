/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - mips-simplify.c                                         *
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

#include "mips-parse.h"
#include "mips-simplify.h"
#include "mips-make.h"

uint32_t MandatorySimplifyOpcode(uint32_t op)
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
			if (RD_OF(op) == 0) return MAKE_NOP();
			break;
		case 9: /* SPECIAL opcode 9: JALR */
			if (RD_OF(op) == 0) return MAKE_JR(RS_OF(op));
			break;
		case 16: /* SPECIAL opcode 16: MFHI */
		case 18: /* SPECIAL opcode 18: MFLO */
		case 20: /* SPECIAL opcode 20: DSLLV */
		case 22: /* SPECIAL opcode 22: DSRLV */
		case 23: /* SPECIAL opcode 23: DSRAV */
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
		case 56: /* SPECIAL opcode 56: DSLL */
		case 58: /* SPECIAL opcode 58: DSRL */
		case 59: /* SPECIAL opcode 59: DSRA */
		case 60: /* SPECIAL opcode 60: DSLL32 */
		case 62: /* SPECIAL opcode 62: DSRL32 */
		case 63: /* SPECIAL opcode 63: DSRA32 */
			if (RD_OF(op) == 0) return MAKE_NOP();
			break;
		}  /* switch (op & 0x3F) for the SPECIAL prefix */
		break;
	case 8: /* Major opcode 8: ADDI */
	case 9: /* Major opcode 9: ADDIU */
	case 10: /* Major opcode 10: SLTI */
	case 11: /* Major opcode 11: SLTIU */
	case 12: /* Major opcode 12: ANDI */
	case 13: /* Major opcode 13: ORI */
	case 14: /* Major opcode 14: XORI */
	case 15: /* Major opcode 15: LUI */
		if (RT_OF(op) == 0) return MAKE_NOP();
		break;
	case 16: /* Coprocessor 0 prefix */
		switch ((op >> 21) & 0x1F) {
		case 0: /* Coprocessor 0 opcode 0: MFC0 */
			if (RT_OF(op) == 0) return MAKE_NOP();
			break;
		} /* switch ((op >> 21) & 0x1F) for the Coprocessor 0 prefix */
		break;
	case 17: /* Coprocessor 1 prefix */
		switch ((op >> 21) & 0x1F) {
		case 0: /* Coprocessor 1 opcode 0: MFC1 */
		case 1: /* Coprocessor 1 opcode 1: DMFC1 */
		case 2: /* Coprocessor 1 opcode 2: CFC1 */
			if (RT_OF(op) == 0) return MAKE_NOP();
			break;
		} /* switch ((op >> 21) & 0x1F) for the Coprocessor 1 prefix */
		break;
	case 24: /* Major opcode 24: DADDI */
	case 25: /* Major opcode 25: DADDIU */
		if (RT_OF(op) == 0) return MAKE_NOP();
		break;
	/* Some of the memory access opcodes below may need the side-effects to
	 * occur even if writing into $0. However, no actual game writes loaded
	 * data into $0 in the first place. */
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
	case 48: /* Major opcode 48: LL */
	case 52: /* Major opcode 52: LLD (Not implemented) */
	case 55: /* Major opcode 55: LD */
	case 56: /* Major opcode 56: SC */
	case 60: /* Major opcode 60: SCD (Not implemented) */
		if (RT_OF(op) == 0) return MAKE_NOP();
		break;
	} /* switch ((op >> 26) & 0x3F) */

	return op;
}

uint32_t SimplifyOpcode(uint32_t op)
{
	switch ((op >> 26) & 0x3F) {
	case 0: /* SPECIAL prefix */
		switch (op & 0x3F) {
		case 0: /* SPECIAL opcode 0: SLL */
		case 2: /* SPECIAL opcode 2: SRL */
		case 3: /* SPECIAL opcode 3: SRA */
			if (RT_OF(op) == 0) return MAKE_OR(RD_OF(op), 0, 0);
			if (SA_OF(op) == 0) return MAKE_ADDU(RD_OF(op), RT_OF(op), 0);
			break;
		case 4: /* SPECIAL opcode 4: SLLV */
		case 6: /* SPECIAL opcode 6: SRLV */
		case 7: /* SPECIAL opcode 7: SRAV */
			if (RT_OF(op) == 0) return MAKE_OR(RD_OF(op), 0, 0);
			if (RS_OF(op) == 0) return MAKE_ADDU(RD_OF(op), RT_OF(op), 0);
			break;
		case 20: /* SPECIAL opcode 20: DSLLV */
		case 22: /* SPECIAL opcode 22: DSRLV */
		case 23: /* SPECIAL opcode 23: DSRAV */
			if (RT_OF(op) == 0) return MAKE_OR(RD_OF(op), 0, 0);
			if (RS_OF(op) == 0) return SimplifyOpcode(MAKE_OR(RD_OF(op), RT_OF(op), 0));
			break;
		case 32: /* SPECIAL opcode 32: ADD */
		case 33: /* SPECIAL opcode 33: ADDU */
			if (RS_OF(op) == 0 && RT_OF(op) == 0) return MAKE_OR(RD_OF(op), 0, 0);
			if (RS_OF(op) == 0) return MAKE_ADDU(RD_OF(op), RT_OF(op), 0);
			if (RT_OF(op) == 0) return MAKE_ADDU(RD_OF(op), RS_OF(op), 0);
			break;
		case 34: /* SPECIAL opcode 34: SUB */
		case 35: /* SPECIAL opcode 35: SUBU */
			if (RS_OF(op) == RT_OF(op)) return MAKE_OR(RD_OF(op), 0, 0);
			if (RT_OF(op) == 0) return MAKE_ADDU(RD_OF(op), RS_OF(op), 0);
			break;
		case 36: /* SPECIAL opcode 36: AND */
			if (RS_OF(op) == RT_OF(op)) return SimplifyOpcode(MAKE_OR(RD_OF(op), RS_OF(op), 0));
			if (RS_OF(op) == 0 || RT_OF(op) == 0) return MAKE_OR(RD_OF(op), 0, 0);
			break;
		case 37: /* SPECIAL opcode 37: OR */
			/* at this point, Rd != 0 */
			if (RS_OF(op) == RT_OF(op)) {
				if (RS_OF(op) == 0) return op;
				return SimplifyOpcode(MAKE_OR(RD_OF(op), RS_OF(op), 0));
			}
			/* at this point, Rd != 0 && Rs != Rt */
			if (RS_OF(op) == 0) return SimplifyOpcode(MAKE_OR(RD_OF(op), RT_OF(op), 0));
			/* at this point, Rd != 0 && Rs != Rt, and $0 cannot be used for
			 * Rs alone */
			if (RD_OF(op) == RS_OF(op) && RT_OF(op) == 0) return MAKE_NOP();
			break;
		case 38: /* SPECIAL opcode 38: XOR */
			if (RS_OF(op) == RT_OF(op)) return MAKE_OR(RD_OF(op), 0, 0);
			if (RS_OF(op) == 0) return SimplifyOpcode(MAKE_OR(RD_OF(op), RT_OF(op), 0));
			if (RT_OF(op) == 0) return SimplifyOpcode(MAKE_OR(RD_OF(op), RS_OF(op), 0));
			break;
		case 39: /* SPECIAL opcode 39: NOR */
			if (RS_OF(op) == RT_OF(op)) return MAKE_NOR(RD_OF(op), RS_OF(op), 0);
			if (RS_OF(op) == 0) return MAKE_NOR(RD_OF(op), RT_OF(op), 0);
			break;
		case 42: /* SPECIAL opcode 42: SLT */
		case 43: /* SPECIAL opcode 43: SLTU */
			if (RS_OF(op) == RT_OF(op)) return MAKE_OR(RD_OF(op), 0, 0);
			break;
		case 44: /* SPECIAL opcode 44: DADD */
		case 45: /* SPECIAL opcode 45: DADDU */
			if (RS_OF(op) == 0) return SimplifyOpcode(MAKE_OR(RD_OF(op), RT_OF(op), 0));
			if (RT_OF(op) == 0) return SimplifyOpcode(MAKE_OR(RD_OF(op), RS_OF(op), 0));
			break;
		case 46: /* SPECIAL opcode 46: DSUB */
		case 47: /* SPECIAL opcode 47: DSUBU */
			if (RS_OF(op) == RT_OF(op)) return MAKE_OR(RD_OF(op), 0, 0);
			if (RT_OF(op) == 0) return SimplifyOpcode(MAKE_OR(RD_OF(op), RS_OF(op), 0));
			break;
		case 56: /* SPECIAL opcode 56: DSLL */
		case 58: /* SPECIAL opcode 58: DSRL */
		case 59: /* SPECIAL opcode 59: DSRA */
			if (RT_OF(op) == 0) return MAKE_OR(RD_OF(op), 0, 0);
			if (SA_OF(op) == 0) return SimplifyOpcode(MAKE_OR(RD_OF(op), RT_OF(op), 0));
			break;
		case 60: /* SPECIAL opcode 60: DSLL32 */
		case 62: /* SPECIAL opcode 62: DSRL32 */
		case 63: /* SPECIAL opcode 63: DSRA32 */
			if (RT_OF(op) == 0) return MAKE_OR(RD_OF(op), 0, 0);
			break;
		}  /* switch (op & 0x3F) for the SPECIAL prefix */
		break;
	case 1: /* REGIMM prefix */
		switch ((op >> 16) & 0x1F) {
		case 0: /* REGIMM opcode 0: BLTZ */
			if (IMM16S_OF(op) == 1) return MAKE_NOP();
			if (RS_OF(op) == 0) return MAKE_NOP();
			break;
		case 1: /* REGIMM opcode 1: BGEZ */
			if (IMM16S_OF(op) == 1) return MAKE_NOP();
			break;
		case 3: /* REGIMM opcode 3: BGEZL */
			/* Branch Likely doesn't matter for [BGEZL $0, ???]; turn it into
			 * a regular BGEZ by clearing bit 17. */
			if (RS_OF(op) == 0) return SimplifyOpcode(op & ~UINT32_C(0x00020000));
			break;
		case 19: /* REGIMM opcode 19: BGEZALL */
			/* Branch Likely doesn't matter for [BGEZALL $0, ???]; turn it into
			 * a regular BGEZAL by clearing bit 17. */
			if (RS_OF(op) == 0) return op & ~UINT32_C(0x00020000);
			break;
		} /* switch ((op >> 16) & 0x1F) for the REGIMM prefix */
		break;
	case 4: /* Major opcode 4: BEQ */
		if (IMM16S_OF(op) == 1) return MAKE_NOP();
		break;
	case 5: /* Major opcode 5: BNE */
		if (IMM16S_OF(op) == 1) return MAKE_NOP();
		if (RS_OF(op) == RT_OF(op)) return MAKE_NOP();
		break;
	case 6: /* Major opcode 6: BLEZ */
		if (IMM16S_OF(op) == 1) return MAKE_NOP();
		break;
	case 7: /* Major opcode 7: BGTZ */
		if (IMM16S_OF(op) == 1) return MAKE_NOP();
		if (RS_OF(op) == 0) return MAKE_NOP();
		break;
	case 8: /* Major opcode 8: ADDI */
	case 9: /* Major opcode 9: ADDIU */
		if (RS_OF(op) == 0) {
			if (IMM16S_OF(op) >= 0) return SimplifyOpcode(MAKE_ORI(RT_OF(op), 0, IMM16U_OF(op)));
			/* ADDIs from $0 cannot overflow, so make them ADDIUs in case
			 * Integer Overflow exceptions are implemented later. */
			return MAKE_ADDIU(RT_OF(op), 0, IMM16S_OF(op));
		}
		if (IMM16S_OF(op) == 0) return MAKE_ADDU(RT_OF(op), RS_OF(op), 0);
		break;
	case 10: /* Major opcode 10: SLTI */
		if (RS_OF(op) == 0) {
			/* 0 is not less than 0; 0 is not less (signed) than a negative */
			if (IMM16S_OF(op) <= 0) return MAKE_OR(RT_OF(op), 0, 0);
			/* 0 is less than a positive */
			return MAKE_ORI(RT_OF(op), 0, 1);
		}
		break;
	case 11: /* Major opcode 11: SLTIU */
		if (RS_OF(op) == 0) {
			/* 0 is not less than 0 */
			if (IMM16S_OF(op) == 0) return MAKE_OR(RT_OF(op), 0, 0);
			/* 0 is less (unsigned) than any other immediate:
			 * 0 < 0001, 0 < 7FFF, 0 < ...FFFF8000, 0 < ...FFFFFFFF */
			return MAKE_ORI(RT_OF(op), 0, 1);
		}
		break;
	case 12: /* Major opcode 12: ANDI */
		if (RS_OF(op) == 0 || IMM16U_OF(op) == 0) return MAKE_OR(RT_OF(op), 0, 0);
		break;
	case 13: /* Major opcode 13: ORI */
		if (IMM16U_OF(op) == 0) return SimplifyOpcode(MAKE_OR(RT_OF(op), RS_OF(op), 0));
		break;
	case 14: /* Major opcode 14: XORI */
		if (RS_OF(op) == 0) return MAKE_ORI(RT_OF(op), 0, IMM16U_OF(op));
		if (IMM16U_OF(op) == 0) return SimplifyOpcode(MAKE_XOR(RT_OF(op), RS_OF(op), 0));
		break;
	case 15: /* Major opcode 15: LUI */
		if (IMM16S_OF(op) == 0) return MAKE_OR(RT_OF(op), 0, 0);
		break;
	case 17: /* Coprocessor 1 prefix */
		switch ((op >> 21) & 0x1F) {
		case 8: /* Coprocessor 1 opcode 8: Branch on C1 condition... */
			switch ((op >> 16) & 0x3) {
			case 0: /* opcode 0: BC1F */
				if (IMM16S_OF(op) == 1) return MAKE_NOP();
				break;
			case 1: /* opcode 1: BC1T */
				if (IMM16S_OF(op) == 1) return MAKE_NOP();
				break;
			} /* switch ((op >> 16) & 0x3) for branches on C1 condition */
			break;
		} /* switch ((op >> 21) & 0x1F) for the Coprocessor 1 prefix */
		break;
	case 20: /* Major opcode 20: BEQL */
		/* Branch Likely doesn't matter for [BEQL $x, $x, ???]; turn it into
		 * a regular BEQ by clearing bit 30. */
		if (RS_OF(op) == RT_OF(op)) return SimplifyOpcode(op & ~UINT32_C(0x40000000));
		break;
	case 22: /* Major opcode 22: BLEZL */
		/* Branch Likely doesn't matter for [BLEZL $0, ???]; turn it into
		 * a regular BLEZ by clearing bit 30. */
		if (RS_OF(op) == 0) return SimplifyOpcode(op & ~UINT32_C(0x40000000));
		break;
	case 24: /* Major opcode 24: DADDI */
	case 25: /* Major opcode 25: DADDIU */
		if (RS_OF(op) == 0) return SimplifyOpcode(MAKE_ADDIU(RT_OF(op), 0, IMM16S_OF(op)));
		if (IMM16S_OF(op) == 0) return SimplifyOpcode(MAKE_OR(RT_OF(op), RS_OF(op), 0));
		break;
	} /* switch ((op >> 26) & 0x3F) */
	return op;
}
