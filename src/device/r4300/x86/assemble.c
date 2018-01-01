/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - assemble.c                                              *
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

#include <stdio.h>
#include <stdlib.h>

#include "api/callbacks.h"
#include "api/m64p_types.h"
#include "assemble.h"
#include "device/r4300/recomp.h"
#include "device/r4300/recomph.h"
#include "osal/preproc.h"

void init_assembler(void *block_jumps_table, int block_jumps_number, void *block_riprel_table, int block_riprel_number)
{
   if (block_jumps_table)
   {
     g_dev.r4300.jumps_table = (struct jump_table *) block_jumps_table;
     g_dev.r4300.jumps_number = block_jumps_number;
     g_dev.r4300.max_jumps_number = g_dev.r4300.jumps_number;
   }
   else
   {
     g_dev.r4300.jumps_table = (struct jump_table *) malloc(1000*sizeof(struct jump_table));
     g_dev.r4300.jumps_number = 0;
     g_dev.r4300.max_jumps_number = 1000;
   }
}

void free_assembler(void **block_jumps_table, int *block_jumps_number, void **block_riprel_table, int *block_riprel_number)
{
   *block_jumps_table = g_dev.r4300.jumps_table;
   *block_jumps_number = g_dev.r4300.jumps_number;
   *block_riprel_table = NULL;  /* RIP-relative addressing is only for x86-64 */
   *block_riprel_number = 0;
}

void add_jump(unsigned int pc_addr, unsigned int mi_addr)
{
   if (g_dev.r4300.jumps_number == g_dev.r4300.max_jumps_number)
   {
     g_dev.r4300.max_jumps_number += 1000;
     g_dev.r4300.jumps_table = (struct jump_table *) realloc(g_dev.r4300.jumps_table, g_dev.r4300.max_jumps_number*sizeof(struct jump_table));
   }
   g_dev.r4300.jumps_table[g_dev.r4300.jumps_number].pc_addr = pc_addr;
   g_dev.r4300.jumps_table[g_dev.r4300.jumps_number].mi_addr = mi_addr;
   g_dev.r4300.jumps_number++;
}

void passe2(struct precomp_instr *dest, int start, int end, struct precomp_block *block)
{
   unsigned int real_code_length, addr_dest;
   size_t i;
   build_wrappers(dest, start, end, block);
   real_code_length = g_dev.r4300.recomp.code_length;
   
   for (i=0; i < g_dev.r4300.jumps_number; i++)
   {
     g_dev.r4300.recomp.code_length = g_dev.r4300.jumps_table[i].pc_addr;
     if (dest[(g_dev.r4300.jumps_table[i].mi_addr - dest[0].addr)/4].reg_cache_infos.need_map)
     {
       addr_dest = (unsigned int)dest[(g_dev.r4300.jumps_table[i].mi_addr - dest[0].addr)/4].reg_cache_infos.jump_wrapper;
       put32(addr_dest-((unsigned int)block->code+g_dev.r4300.recomp.code_length)-4);
     }
     else
     {
       addr_dest = dest[(g_dev.r4300.jumps_table[i].mi_addr - dest[0].addr)/4].local_addr;
       put32(addr_dest-g_dev.r4300.recomp.code_length-4);
     }
   }
   g_dev.r4300.recomp.code_length = real_code_length;
}

void jump_start_rel8(void)
{
  g_dev.r4300.jump_start8 = g_dev.r4300.recomp.code_length;
}

void jump_start_rel32(void)
{
  g_dev.r4300.jump_start32 = g_dev.r4300.recomp.code_length;
}

void jump_end_rel8(void)
{
  unsigned int jump_end = g_dev.r4300.recomp.code_length;
  int jump_vec = jump_end - g_dev.r4300.jump_start8;

  if (jump_vec > 127 || jump_vec < -128)
  {
    DebugMessage(M64MSG_ERROR, "8-bit relative jump too long! From %x to %x", g_dev.r4300.jump_start8, jump_end);
    OSAL_BREAKPOINT_INTERRUPT;
  }

  g_dev.r4300.recomp.code_length = g_dev.r4300.jump_start8 - 1;
  put8(jump_vec);
  g_dev.r4300.recomp.code_length = jump_end;
}

void jump_end_rel32(void)
{
  unsigned int jump_end = g_dev.r4300.recomp.code_length;
  int jump_vec = jump_end - g_dev.r4300.jump_start32;

  g_dev.r4300.recomp.code_length = g_dev.r4300.jump_start32 - 4;
  put32(jump_vec);
  g_dev.r4300.recomp.code_length = jump_end;
}
