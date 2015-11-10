/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - MIPS (Nintendo 64) branch and jump instructions         *
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

#ifndef M64P_TRACE_JIT_MIPS32_MIPS_BRANCH_H
#define M64P_TRACE_JIT_MIPS32_MIPS_BRANCH_H

#include "state.h"
#include "native-regcache.h"

extern enum TJEmitTraceResult mips32_emit_jr(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_jalr(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_bltz_idle(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_bltz(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_bgez_idle(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_bgez(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_bltzl_idle(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_bltzl(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_bgezl_idle(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_bgezl(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_bltzal_idle(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_bltzal(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_bgezal_idle(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_bgezal(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_bltzall_idle(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_bltzall(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_bgezall_idle(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_bgezall(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_j_idle(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_j(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_jal_idle(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_jal(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_beq_idle(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_beq(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_bne_idle(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_bne(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_blez_idle(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_blez(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_bgtz_idle(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_bgtz(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_bc1f_idle(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_bc1f(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_bc1t_idle(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_bc1t(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_bc1fl_idle(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_bc1fl(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_bc1tl_idle(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_bc1tl(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_beql_idle(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_beql(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_bnel_idle(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_bnel(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_blezl_idle(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_blezl(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_bgtzl_idle(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_bgtzl(struct mips32_state* state, struct mips32_reg_cache* cache);

#endif /* !M64P_TRACE_JIT_MIPS32_MIPS_BRANCH_H */
