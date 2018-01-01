/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - recomp.h                                                *
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

#ifndef M64P_DEVICE_R4300_RECOMP_H
#define M64P_DEVICE_R4300_RECOMP_H

#include <stddef.h>
#include <stdint.h>

#include "recomp_types.h"

struct r4300_core;

void recompile_block(struct r4300_core* r4300, const uint32_t* source, struct precomp_block* block, uint32_t func);
void init_block(struct r4300_core* r4300, struct precomp_block* block);
void free_block(struct r4300_core* r4300, struct precomp_block* block);
void recompile_opcode(struct r4300_core* r4300);
void dyna_jump(void);
void dyna_start(void (*code)(void));
void dyna_stop(struct r4300_core* r4300);
void *realloc_exec(void *ptr, size_t oldsize, size_t newsize);


void dynarec_jump_to_address(void);
void dynarec_exception_general(void);
int dynarec_check_cop1_unusable(void);
void dynarec_cp0_update_count(void);
void dynarec_gen_interrupt(void);
int dynarec_read_aligned_word(void);
int dynarec_write_aligned_word(void);
int dynarec_read_aligned_dword(void);
int dynarec_write_aligned_dword(void);


#if defined(PROFILE_R4300)
void profile_write_end_of_code_blocks(struct r4300_core* r4300);
#endif

#if defined(__x86_64__)
  #include "x86_64/assemble.h"
  #include "x86_64/regcache.h"
#else
  #include "x86/assemble.h"
  #include "x86/regcache.h"
#endif

#endif /* M64P_DEVICE_R4300_RECOMP_H */

