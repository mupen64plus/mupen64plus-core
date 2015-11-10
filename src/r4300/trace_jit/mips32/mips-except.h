/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - MIPS (Nintendo 64) exception-causing instructions       *
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

#ifndef M64P_TRACE_JIT_MIPS32_MIPS_EXCEPT_H
#define M64P_TRACE_JIT_MIPS32_MIPS_EXCEPT_H

#include "state.h"
#include "native-regcache.h"

extern enum TJEmitTraceResult mips32_emit_ni(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_reserved(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_syscall(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_emit_teq(struct mips32_state* state, struct mips32_reg_cache* cache);

extern enum TJEmitTraceResult mips32_exception_writeback(struct mips32_state* state, struct mips32_reg_cache* cache, void* usual_path, uint32_t userdata);

extern enum TJEmitTraceResult mips32_raise_cop1_unusable(struct mips32_state* state, struct mips32_reg_cache* cache, void* usual_path, uint32_t userdata);

#endif /* !M64P_TRACE_JIT_MIPS32_MIPS_EXCEPT_H */
