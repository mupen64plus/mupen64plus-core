/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - mips-simplify.h                                         *
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

#ifndef M64P_TRACE_JIT_MIPS_SIMPLIFY_H
#define M64P_TRACE_JIT_MIPS_SIMPLIFY_H

#include <stdint.h>

/*
 * Simplifies a single MIPS III opcode only as required by the MIPS
 * specification.
 *
 * This function transforms any opcode that would write into $0 into
 * [SLL $0, $0, 0], the canonical form of NOP, except [JALR $0, Rs],
 * which gets transformed into [JR Rs].
 *
 * This prevents all writes into register 0.
 */
extern uint32_t MandatorySimplifyOpcode(uint32_t op);

/*
 * Simplifies a single MIPS III opcode.
 *
 * Transformations are made on the opcode to make it easier to analyse.
 * These include:
 * a) transforming opcodes that would load a register with 0 into
 *    [OR Rd, $0, $0];
 * b) transforming opcodes that would load a register with a copy of the
 *    entirety of another into [OR Rd, Rs, $0];
 * c) transforming opcodes that would load a register with a copy of the lower
 *    32 bits of another, sign-extended to 64 bits, into [ADDU Rd, Rs, $0];
 * d) transforming opcodes that would load a register with a low constant
 *    (0x0000..0x7FFF) into [ORI Rt, $0, Imm16];
 * e) transforming jump opcodes that do not alter the control flow into
 *    [SLL $0, $0, 0], the canonical form of NOP. These include impossible
 *    branches ([BNE $x, $x, *], [BLTZ $0, *], [BGTZ $0, *]...) and branches
 *    that execute their delay slot followed by the instruction following the
 *    delay slot no matter what ([BNE $x, $y, +1], [BEQ $x, $y, +1], ...).
 *    Branch Likely opcodes are left alone, and so are Branch and Link opcodes
 *    due to them writing into $31 unconditionally.
 */
extern uint32_t SimplifyOpcode(uint32_t op);

#endif /* !M64P_TRACE_JIT_MIPS_SIMPLIFY_H */