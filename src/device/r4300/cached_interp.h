/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - cached_interp.h                                         *
 *   Mupen64Plus homepage: https://mupen64plus.org/                        *
 *   Copyright (C) 2002 Hacktarux                                          *
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

#ifndef M64P_DEVICE_R4300_CACHED_INTERP_H
#define M64P_DEVICE_R4300_CACHED_INTERP_H

#include <stddef.h>
#include <stdint.h>

#include "ops.h"

struct r4300_core;

extern const struct cpu_instruction_table cached_interpreter_table;

void init_blocks(struct r4300_core* r4300);
void free_blocks(struct r4300_core* r4300);

void invalidate_cached_code_hacktarux(struct r4300_core* r4300, uint32_t address, size_t size);

void run_cached_interpreter(struct r4300_core* r4300);

/* Jumps to the given address. This is for the cached interpreter / dynarec. */
void cached_interpreter_dynarec_jump_to(struct r4300_core* r4300, uint32_t address);

#endif /* M64P_DEVICE_R4300_CACHED_INTERP_H */
