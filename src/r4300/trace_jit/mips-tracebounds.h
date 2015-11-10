/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - mips-tracebounds.h                                      *
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

#ifndef M64P_TRACE_JIT_MIPS_TRACEBOUNDS_H
#define M64P_TRACE_JIT_MIPS_TRACEBOUNDS_H

#include <stddef.h>

/*
 * Makes a trace of simplified MIPS III instructions. See mips-simplify.h for
 * more details on the simplification process.
 *
 * The trace ends when Count opcodes have been included in it, or when an
 * opcode is encountered that does either of the following: (a) branch away,
 * conditionally or unconditionally; (b) raise an exception (i.e. SYSCALL)
 * or an interrupt, conditionally or unconditionally. If the opcode that would
 * have ended the trace is a branch with a delay slot, the delay slot is
 * included if the value of Count allows for it.
 *
 * In:
 *   Raw: A pointer to the first MIPS III opcode to be possibly included in
 *     the trace.
 *   Count: The number of 32-bit words, starting at *Raw, that can be included
 *     in the trace. This may end, for example, at a page boundary.
 * Out:
 *   Dest: A pointer to a writable native memory buffer to which simplified
 *     opcodes are written.
 * Returns:
 *   the number of simplified opcodes written starting at *Dest.
 */
extern size_t MakeTrace(uint32_t* Dest, const uint32_t* Raw, size_t Count);

#endif /* !M64P_TRACE_JIT_MIPS_TRACEBOUNDS_H */