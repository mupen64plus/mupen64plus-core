/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - mips-make.h                                             *
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

#ifndef M64P_TRACE_JIT_MIPS_MAKE_H
#define M64P_TRACE_JIT_MIPS_MAKE_H

#include <stdint.h>

#define MAKE_NOP()              0
#define MAKE_OR(Rd, Rs, Rt)     (UINT32_C(0x25) | ((Rd) << 11) | ((Rt) << 16) | ((Rs) << 21))
#define MAKE_ADDU(Rd, Rs, Rt)   (UINT32_C(0x21) | ((Rd) << 11) | ((Rt) << 16) | ((Rs) << 21))
#define MAKE_SUBU(Rd, Rs, Rt)   (UINT32_C(0x23) | ((Rd) << 11) | ((Rt) << 16) | ((Rs) << 21))
#define MAKE_JR(Rs)             (UINT32_C(0x8) | ((Rs) << 21))
#define MAKE_NOR(Rd, Rs, Rt)    (UINT32_C(0x27) | ((Rd) << 11) | ((Rt) << 16) | ((Rs) << 21))
#define MAKE_ORI(Rt, Rs, Imm)   (UINT32_C(0x34000000) | ((Rt) << 16) | ((Rs) << 21) | ((Imm) & 0xFFFF))
#define MAKE_ADDIU(Rt, Rs, Imm) (UINT32_C(0x24000000) | ((Rt) << 16) | ((Rs) << 21) | ((Imm) & 0xFFFF))
#define MAKE_XOR(Rd, Rs, Rt)    (UINT32_C(0x26) | ((Rd) << 11) | ((Rt) << 16) | ((Rs) << 21))
#define MAKE_SLL(Rd, Rt, Sa)    (UINT32_C(0) | ((Rd) << 11) | ((Rt) << 16) | ((Sa) << 6))
#define MAKE_SRL(Rd, Rt, Sa)    (UINT32_C(0x2) | ((Rd) << 11) | ((Rt) << 16) | ((Sa) << 6))
#define MAKE_SRA(Rd, Rt, Sa)    (UINT32_C(0x3) | ((Rd) << 11) | ((Rt) << 16) | ((Sa) << 6))
#define MAKE_DSLL(Rd, Rt, Sa)   (UINT32_C(0x38) | ((Rd) << 11) | ((Rt) << 16) | ((Sa) << 6))
#define MAKE_DSRL(Rd, Rt, Sa)   (UINT32_C(0x3A) | ((Rd) << 11) | ((Rt) << 16) | ((Sa) << 6))
#define MAKE_DSRA(Rd, Rt, Sa)   (UINT32_C(0x3B) | ((Rd) << 11) | ((Rt) << 16) | ((Sa) << 6))
#define MAKE_DSLL32(Rd, Rt, Sa) (UINT32_C(0x3C) | ((Rd) << 11) | ((Rt) << 16) | ((Sa) << 6))
#define MAKE_DSRL32(Rd, Rt, Sa) (UINT32_C(0x3E) | ((Rd) << 11) | ((Rt) << 16) | ((Sa) << 6))
#define MAKE_DSRA32(Rd, Rt, Sa) (UINT32_C(0x3F) | ((Rd) << 11) | ((Rt) << 16) | ((Sa) << 6))
#define MAKE_BGEZ(Rs, Imm)      (UINT32_C(0x04010000) | ((Rs) << 21) | ((Imm) & 0xFFFF))

#endif /* !M64P_TRACE_JIT_MIPS_MAKE_H */