/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - MIPS (Nintendo 64) arithmetic logic unit instructions   *
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

#ifndef M64P_TRACE_JIT_MIPS32_MIPS_ALU_H
#define M64P_TRACE_JIT_MIPS32_MIPS_ALU_H

#include "state.h"
#include "native-regcache.h"

extern enum TJEmitTraceResult mips32_emit_nop(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_sll(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_srl(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_sra(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_sllv(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_srlv(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_srav(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_dsllv(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_dsrlv(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_dsrav(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_addu(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_add(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_subu(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_sub(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_and(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_or(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_xor(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_nor(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_slt(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_sltu(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_daddu(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_dadd(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_dsubu(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_dsub(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_dsll(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_dsrl(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_dsra(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_dsll32(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_dsrl32(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_dsra32(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_addiu(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_addi(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_slti(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_sltiu(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_andi(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_ori(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_xori(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_lui(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_daddiu(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_daddi(struct mips32_state* state, struct mips32_reg_cache* cache);

#endif /* !M64P_TRACE_JIT_MIPS32_MIPS_ALU_H */
