/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - Configuration for the MIPS32 Trace JIT                  *
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

#ifndef M64P_TRACE_JIT_MIPS32_CONFIG_H
#define M64P_TRACE_JIT_MIPS32_CONFIG_H

#define ARCH_TRACE_JIT_INIT mips32_trace_jit_init
#define ARCH_TRACE_JIT_EXIT mips32_trace_jit_exit

#define ARCH_EMIT_TRACE mips32_emit_trace
#define ARCH_JIT_ENTRY mips32_jit_entry
#define ARCH_JIT_ENTRY_IS_GENERATED

#endif /* !M64P_TRACE_JIT_MIPS32_CONFIG_H */
