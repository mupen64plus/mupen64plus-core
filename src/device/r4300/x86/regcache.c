/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - regcache.c                                              *
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

#include "regcache.h"
#include "device/r4300/recomp.h"
#include "device/r4300/recomph.h"
#include "main/main.h"

void init_cache(struct precomp_instr* start)
{
   int i;
   for (i=0; i<8; i++)
     {
    g_dev.r4300.regcache_state.last_access[i] = NULL;
    g_dev.r4300.regcache_state.free_since[i] = start;
     }
     g_dev.r4300.regcache_state.r0 = (unsigned int*)r4300_regs(&g_dev.r4300);
}

void free_all_registers(void)
{
#if defined(PROFILE_R4300)
  int freestart = g_dev.r4300.recomp.code_length;
  int flushed = 0;
#endif

   int i;
   for (i=0; i<8; i++)
     {
#if defined(PROFILE_R4300)
    if (g_dev.r4300.regcache_state.last_access[i] && g_dev.r4300.regcache_state.dirty[i]) flushed = 1;
#endif
    if (g_dev.r4300.regcache_state.last_access[i]) free_register(i);
    else
      {
         while (g_dev.r4300.regcache_state.free_since[i] <= g_dev.r4300.recomp.dst)
           {
          g_dev.r4300.regcache_state.free_since[i]->reg_cache_infos.needed_registers[i] = NULL;
          g_dev.r4300.regcache_state.free_since[i]++;
           }
      }
     }

#if defined(PROFILE_R4300)
  if (flushed == 1)
  {
    long x86addr = (long) ((*g_dev.r4300.recomp.inst_pointer) + freestart);
    int mipsop = -5;
    fwrite(&mipsop, 1, 4, g_dev.r4300.recomp.pfProfile); /* -5 = regcache flushing */
    fwrite(&x86addr, 1, sizeof(char *), g_dev.r4300.recomp.pfProfile); // write pointer to start of register cache flushing instructions
    x86addr = (long) ((*g_dev.r4300.recomp.inst_pointer) + g_dev.r4300.recomp.code_length);
    fwrite(&g_dev.r4300.recomp.src, 1, 4, g_dev.r4300.recomp.pfProfile); // write 4-byte MIPS opcode for current instruction
    fwrite(&x86addr, 1, sizeof(char *), g_dev.r4300.recomp.pfProfile); // write pointer to dynamically generated x86 code for this MIPS instruction
  }
#endif
}

// this function frees a specific X86 GPR
void free_register(int reg)
{
   struct precomp_instr *last;
   
   if (g_dev.r4300.regcache_state.last_access[reg] != NULL &&
       g_dev.r4300.regcache_state.r64[reg] != -1 && (int)g_dev.r4300.regcache_state.reg_content[reg] != (int)g_dev.r4300.regcache_state.reg_content[g_dev.r4300.regcache_state.r64[reg]]-4)
     {
    free_register(g_dev.r4300.regcache_state.r64[reg]);
    return;
     }
   
   if (g_dev.r4300.regcache_state.last_access[reg] != NULL) last = g_dev.r4300.regcache_state.last_access[reg]+1;
   else last = g_dev.r4300.regcache_state.free_since[reg];
   
   while (last <= g_dev.r4300.recomp.dst)
     {
    if (g_dev.r4300.regcache_state.last_access[reg] != NULL && g_dev.r4300.regcache_state.dirty[reg])
      last->reg_cache_infos.needed_registers[reg] = g_dev.r4300.regcache_state.reg_content[reg];
    else
      last->reg_cache_infos.needed_registers[reg] = NULL;
    
    if (g_dev.r4300.regcache_state.last_access[reg] != NULL && g_dev.r4300.regcache_state.r64[reg] != -1)
      {
         if (g_dev.r4300.regcache_state.dirty[g_dev.r4300.regcache_state.r64[reg]])
           last->reg_cache_infos.needed_registers[g_dev.r4300.regcache_state.r64[reg]] = g_dev.r4300.regcache_state.reg_content[g_dev.r4300.regcache_state.r64[reg]];
         else
           last->reg_cache_infos.needed_registers[g_dev.r4300.regcache_state.r64[reg]] = NULL;
      }
    
    last++;
     }
   if (g_dev.r4300.regcache_state.last_access[reg] == NULL) 
     {
    g_dev.r4300.regcache_state.free_since[reg] = g_dev.r4300.recomp.dst+1;
    return;
     }
   
   if (g_dev.r4300.regcache_state.dirty[reg]) 
     {
    mov_m32_reg32(g_dev.r4300.regcache_state.reg_content[reg], reg);
    if (g_dev.r4300.regcache_state.r64[reg] == -1)
      {
         sar_reg32_imm8(reg, 31);
         mov_m32_reg32((unsigned int*)g_dev.r4300.regcache_state.reg_content[reg]+1, reg);
      }
    else mov_m32_reg32(g_dev.r4300.regcache_state.reg_content[g_dev.r4300.regcache_state.r64[reg]], g_dev.r4300.regcache_state.r64[reg]);
     }
   g_dev.r4300.regcache_state.last_access[reg] = NULL;
   g_dev.r4300.regcache_state.free_since[reg] = g_dev.r4300.recomp.dst+1;
   if (g_dev.r4300.regcache_state.r64[reg] != -1)
     {
    g_dev.r4300.regcache_state.last_access[g_dev.r4300.regcache_state.r64[reg]] = NULL;
    g_dev.r4300.regcache_state.free_since[g_dev.r4300.regcache_state.r64[reg]] = g_dev.r4300.recomp.dst+1;
     }
}

int lru_register(void)
{
   unsigned int oldest_access = 0xFFFFFFFF;
   int i, reg = 0;
   for (i=0; i<8; i++)
     {
    if (i != ESP && (unsigned int)g_dev.r4300.regcache_state.last_access[i] < oldest_access)
      {
         oldest_access = (int)g_dev.r4300.regcache_state.last_access[i];
         reg = i;
      }
     }
   return reg;
}

int lru_register_exc1(int exc1)
{
   unsigned int oldest_access = 0xFFFFFFFF;
   int i, reg = 0;
   for (i=0; i<8; i++)
     {
    if (i != ESP && i != exc1 && (unsigned int)g_dev.r4300.regcache_state.last_access[i] < oldest_access)
      {
         oldest_access = (int)g_dev.r4300.regcache_state.last_access[i];
         reg = i;
      }
     }
   return reg;
}

// this function finds a register to put the data contained in addr,
// if there was another value before it's cleanly removed of the
// register cache. After that, the register number is returned.
// If data are already cached, the function only returns the register number
int allocate_register(unsigned int *addr)
{
   unsigned int oldest_access = 0xFFFFFFFF;
   int reg = 0, i;
   
   // is it already cached ?
   if (addr != NULL)
     {
    for (i=0; i<8; i++)
      {
         if (g_dev.r4300.regcache_state.last_access[i] != NULL && g_dev.r4300.regcache_state.reg_content[i] == addr)
           {
          struct precomp_instr *last = g_dev.r4300.regcache_state.last_access[i]+1;
          
          while (last <= g_dev.r4300.recomp.dst)
            {
               last->reg_cache_infos.needed_registers[i] = g_dev.r4300.regcache_state.reg_content[i];
               last++;
            }
          g_dev.r4300.regcache_state.last_access[i] = g_dev.r4300.recomp.dst;
          if (g_dev.r4300.regcache_state.r64[i] != -1) 
            {
               last = g_dev.r4300.regcache_state.last_access[g_dev.r4300.regcache_state.r64[i]]+1;
               
               while (last <= g_dev.r4300.recomp.dst)
             {
                last->reg_cache_infos.needed_registers[g_dev.r4300.regcache_state.r64[i]] = g_dev.r4300.regcache_state.reg_content[g_dev.r4300.regcache_state.r64[i]];
                last++;
             }
               g_dev.r4300.regcache_state.last_access[g_dev.r4300.regcache_state.r64[i]] = g_dev.r4300.recomp.dst;
            }
          
          return i;
           }
      }
     }
   
   // if it's not cached, we take the least recently used register
   for (i=0; i<8; i++)
     {
    if (i != ESP && (unsigned int)g_dev.r4300.regcache_state.last_access[i] < oldest_access)
      {
         oldest_access = (int)g_dev.r4300.regcache_state.last_access[i];
         reg = i;
      }
     }
   
   if (g_dev.r4300.regcache_state.last_access[reg]) free_register(reg);
   else
     {
    while (g_dev.r4300.regcache_state.free_since[reg] <= g_dev.r4300.recomp.dst)
      {
         g_dev.r4300.regcache_state.free_since[reg]->reg_cache_infos.needed_registers[reg] = NULL;
         g_dev.r4300.regcache_state.free_since[reg]++;
      }
     }
   
   g_dev.r4300.regcache_state.last_access[reg] = g_dev.r4300.recomp.dst;
   g_dev.r4300.regcache_state.reg_content[reg] = addr;
   g_dev.r4300.regcache_state.dirty[reg] = 0;
   g_dev.r4300.regcache_state.r64[reg] = -1;
   
   if (addr != NULL)
     {
    if (addr == g_dev.r4300.regcache_state.r0 || addr == g_dev.r4300.regcache_state.r0+1)
      xor_reg32_reg32(reg, reg);
    else
      mov_reg32_m32(reg, addr);
     }
   
   return reg;
}

// this function is similar to allocate_register except it loads
// a 64 bits value, and return the register number of the LSB part
int allocate_64_register1(unsigned int *addr)
{
   int reg1, reg2, i;
   
   // is it already cached as a 32 bits value ?
   for (i=0; i<8; i++)
     {
    if (g_dev.r4300.regcache_state.last_access[i] != NULL && g_dev.r4300.regcache_state.reg_content[i] == addr)
      {
         if (g_dev.r4300.regcache_state.r64[i] == -1)
           {
          allocate_register(addr);
          reg2 = allocate_register(g_dev.r4300.regcache_state.dirty[i] ? NULL : addr+1);
          g_dev.r4300.regcache_state.r64[i] = reg2;
          g_dev.r4300.regcache_state.r64[reg2] = i;
          
          if (g_dev.r4300.regcache_state.dirty[i])
            {
               g_dev.r4300.regcache_state.reg_content[reg2] = addr+1;
               g_dev.r4300.regcache_state.dirty[reg2] = 1;
               mov_reg32_reg32(reg2, i);
               sar_reg32_imm8(reg2, 31);
            }
          
          return i;
           }
      }
     }
   
   reg1 = allocate_register(addr);
   reg2 = allocate_register(addr+1);
   g_dev.r4300.regcache_state.r64[reg1] = reg2;
   g_dev.r4300.regcache_state.r64[reg2] = reg1;
   
   return reg1;
}

// this function is similar to allocate_register except it loads
// a 64 bits value, and return the register number of the MSB part
int allocate_64_register2(unsigned int *addr)
{
   int reg1, reg2, i;
   
   // is it already cached as a 32 bits value ?
   for (i=0; i<8; i++)
     {
    if (g_dev.r4300.regcache_state.last_access[i] != NULL && g_dev.r4300.regcache_state.reg_content[i] == addr)
      {
         if (g_dev.r4300.regcache_state.r64[i] == -1)
           {
          allocate_register(addr);
          reg2 = allocate_register(g_dev.r4300.regcache_state.dirty[i] ? NULL : addr+1);
          g_dev.r4300.regcache_state.r64[i] = reg2;
          g_dev.r4300.regcache_state.r64[reg2] = i;
          
          if (g_dev.r4300.regcache_state.dirty[i])
            {
               g_dev.r4300.regcache_state.reg_content[reg2] = addr+1;
               g_dev.r4300.regcache_state.dirty[reg2] = 1;
               mov_reg32_reg32(reg2, i);
               sar_reg32_imm8(reg2, 31);
            }
          
          return reg2;
           }
      }
     }
   
   reg1 = allocate_register(addr);
   reg2 = allocate_register(addr+1);
   g_dev.r4300.regcache_state.r64[reg1] = reg2;
   g_dev.r4300.regcache_state.r64[reg2] = reg1;
   
   return reg2;
}

// this function checks if the data located at addr are cached in a register
// and then, it returns 1  if it's a 64 bit value
//                      0  if it's a 32 bit value
//                      -1 if it's not cached
int is64(unsigned int *addr)
{
   int i;
   for (i=0; i<8; i++)
     {
    if (g_dev.r4300.regcache_state.last_access[i] != NULL && g_dev.r4300.regcache_state.reg_content[i] == addr)
      {
         if (g_dev.r4300.regcache_state.r64[i] == -1) return 0;
         return 1;
      }
     }
   return -1;
}

int allocate_register_w(unsigned int *addr)
{
   unsigned int oldest_access = 0xFFFFFFFF;
   int reg = 0, i;
   
   // is it already cached ?
   for (i=0; i<8; i++)
     {
    if (g_dev.r4300.regcache_state.last_access[i] != NULL && g_dev.r4300.regcache_state.reg_content[i] == addr)
      {
         struct precomp_instr *last = g_dev.r4300.regcache_state.last_access[i]+1;
         
         while (last <= g_dev.r4300.recomp.dst)
           {
          last->reg_cache_infos.needed_registers[i] = NULL;
          last++;
           }
         g_dev.r4300.regcache_state.last_access[i] = g_dev.r4300.recomp.dst;
         g_dev.r4300.regcache_state.dirty[i] = 1;
         if (g_dev.r4300.regcache_state.r64[i] != -1)
           {
          last = g_dev.r4300.regcache_state.last_access[g_dev.r4300.regcache_state.r64[i]]+1;
          while (last <= g_dev.r4300.recomp.dst)
            {
               last->reg_cache_infos.needed_registers[g_dev.r4300.regcache_state.r64[i]] = NULL;
               last++;
            }
          g_dev.r4300.regcache_state.free_since[g_dev.r4300.regcache_state.r64[i]] = g_dev.r4300.recomp.dst+1;
          g_dev.r4300.regcache_state.last_access[g_dev.r4300.regcache_state.r64[i]] = NULL;
          g_dev.r4300.regcache_state.r64[i] = -1;
           }
         
         return i;
      }
     }
   
   // if it's not cached, we take the least recently used register
   for (i=0; i<8; i++)
     {
    if (i != ESP && (unsigned int)g_dev.r4300.regcache_state.last_access[i] < oldest_access)
      {
         oldest_access = (int)g_dev.r4300.regcache_state.last_access[i];
         reg = i;
      }
     }
   
   if (g_dev.r4300.regcache_state.last_access[reg]) free_register(reg);
   else
     {
    while (g_dev.r4300.regcache_state.free_since[reg] <= g_dev.r4300.recomp.dst)
      {
         g_dev.r4300.regcache_state.free_since[reg]->reg_cache_infos.needed_registers[reg] = NULL;
         g_dev.r4300.regcache_state.free_since[reg]++;
      }
     }
   
   g_dev.r4300.regcache_state.last_access[reg] = g_dev.r4300.recomp.dst;
   g_dev.r4300.regcache_state.reg_content[reg] = addr;
   g_dev.r4300.regcache_state.dirty[reg] = 1;
   g_dev.r4300.regcache_state.r64[reg] = -1;
   
   return reg;
}

int allocate_64_register1_w(unsigned int *addr)
{
   int reg1, reg2, i;
   
   // is it already cached as a 32 bits value ?
   for (i=0; i<8; i++)
     {
    if (g_dev.r4300.regcache_state.last_access[i] != NULL && g_dev.r4300.regcache_state.reg_content[i] == addr)
      {
         if (g_dev.r4300.regcache_state.r64[i] == -1)
           {
          allocate_register_w(addr);
          reg2 = lru_register();
          if (g_dev.r4300.regcache_state.last_access[reg2]) free_register(reg2);
          else
          {
            while (g_dev.r4300.regcache_state.free_since[reg2] <= g_dev.r4300.recomp.dst)
            {
              g_dev.r4300.regcache_state.free_since[reg2]->reg_cache_infos.needed_registers[reg2] = NULL;
              g_dev.r4300.regcache_state.free_since[reg2]++;
            }
          }
          g_dev.r4300.regcache_state.r64[i] = reg2;
          g_dev.r4300.regcache_state.r64[reg2] = i;
          g_dev.r4300.regcache_state.last_access[reg2] = g_dev.r4300.recomp.dst;
          
          g_dev.r4300.regcache_state.reg_content[reg2] = addr+1;
          g_dev.r4300.regcache_state.dirty[reg2] = 1;
          mov_reg32_reg32(reg2, i);
          sar_reg32_imm8(reg2, 31);
          
          return i;
           }
         else
           {
          g_dev.r4300.regcache_state.last_access[i] = g_dev.r4300.recomp.dst;
          g_dev.r4300.regcache_state.last_access[g_dev.r4300.regcache_state.r64[i]] = g_dev.r4300.recomp.dst;
          g_dev.r4300.regcache_state.dirty[i] = g_dev.r4300.regcache_state.dirty[g_dev.r4300.regcache_state.r64[i]] = 1;
          return i;
           }
      }
     }
   
   reg1 = allocate_register_w(addr);
   reg2 = lru_register();
   if (g_dev.r4300.regcache_state.last_access[reg2]) free_register(reg2);
   else
     {
    while (g_dev.r4300.regcache_state.free_since[reg2] <= g_dev.r4300.recomp.dst)
      {
         g_dev.r4300.regcache_state.free_since[reg2]->reg_cache_infos.needed_registers[reg2] = NULL;
         g_dev.r4300.regcache_state.free_since[reg2]++;
      }
     }
   g_dev.r4300.regcache_state.r64[reg1] = reg2;
   g_dev.r4300.regcache_state.r64[reg2] = reg1;
   g_dev.r4300.regcache_state.last_access[reg2] = g_dev.r4300.recomp.dst;
   g_dev.r4300.regcache_state.reg_content[reg2] = addr+1;
   g_dev.r4300.regcache_state.dirty[reg2] = 1;
   
   return reg1;
}

int allocate_64_register2_w(unsigned int *addr)
{
   int reg1, reg2, i;
   
   // is it already cached as a 32 bits value ?
   for (i=0; i<8; i++)
     {
    if (g_dev.r4300.regcache_state.last_access[i] != NULL && g_dev.r4300.regcache_state.reg_content[i] == addr)
      {
         if (g_dev.r4300.regcache_state.r64[i] == -1)
           {
          allocate_register_w(addr);
          reg2 = lru_register();
          if (g_dev.r4300.regcache_state.last_access[reg2]) free_register(reg2);
          else
          {
            while (g_dev.r4300.regcache_state.free_since[reg2] <= g_dev.r4300.recomp.dst)
            {
              g_dev.r4300.regcache_state.free_since[reg2]->reg_cache_infos.needed_registers[reg2] = NULL;
              g_dev.r4300.regcache_state.free_since[reg2]++;
            }
          }
          g_dev.r4300.regcache_state.r64[i] = reg2;
          g_dev.r4300.regcache_state.r64[reg2] = i;
          g_dev.r4300.regcache_state.last_access[reg2] = g_dev.r4300.recomp.dst;
          
          g_dev.r4300.regcache_state.reg_content[reg2] = addr+1;
          g_dev.r4300.regcache_state.dirty[reg2] = 1;
          mov_reg32_reg32(reg2, i);
          sar_reg32_imm8(reg2, 31);
          
          return reg2;
           }
         else
           {
          g_dev.r4300.regcache_state.last_access[i] = g_dev.r4300.recomp.dst;
          g_dev.r4300.regcache_state.last_access[g_dev.r4300.regcache_state.r64[i]] = g_dev.r4300.recomp.dst;
          g_dev.r4300.regcache_state.dirty[i] = g_dev.r4300.regcache_state.dirty[g_dev.r4300.regcache_state.r64[i]] = 1;
          return g_dev.r4300.regcache_state.r64[i];
           }
      }
     }
   
   reg1 = allocate_register_w(addr);
   reg2 = lru_register();
   if (g_dev.r4300.regcache_state.last_access[reg2]) free_register(reg2);
   else
     {
    while (g_dev.r4300.regcache_state.free_since[reg2] <= g_dev.r4300.recomp.dst)
      {
         g_dev.r4300.regcache_state.free_since[reg2]->reg_cache_infos.needed_registers[reg2] = NULL;
         g_dev.r4300.regcache_state.free_since[reg2]++;
      }
     }
   g_dev.r4300.regcache_state.r64[reg1] = reg2;
   g_dev.r4300.regcache_state.r64[reg2] = reg1;
   g_dev.r4300.regcache_state.last_access[reg2] = g_dev.r4300.recomp.dst;
   g_dev.r4300.regcache_state.reg_content[reg2] = addr+1;
   g_dev.r4300.regcache_state.dirty[reg2] = 1;
   
   return reg2;
}

void set_register_state(int reg, unsigned int *addr, int d)
{
   g_dev.r4300.regcache_state.last_access[reg] = g_dev.r4300.recomp.dst;
   g_dev.r4300.regcache_state.reg_content[reg] = addr;
   g_dev.r4300.regcache_state.r64[reg] = -1;
   g_dev.r4300.regcache_state.dirty[reg] = d;
}

void set_64_register_state(int reg1, int reg2, unsigned int *addr, int d)
{
   g_dev.r4300.regcache_state.last_access[reg1] = g_dev.r4300.recomp.dst;
   g_dev.r4300.regcache_state.last_access[reg2] = g_dev.r4300.recomp.dst;
   g_dev.r4300.regcache_state.reg_content[reg1] = addr;
   g_dev.r4300.regcache_state.reg_content[reg2] = addr+1;
   g_dev.r4300.regcache_state.r64[reg1] = reg2;
   g_dev.r4300.regcache_state.r64[reg2] = reg1;
   g_dev.r4300.regcache_state.dirty[reg1] = d;
   g_dev.r4300.regcache_state.dirty[reg2] = d;
}

void force_32(int reg)
{
   if (g_dev.r4300.regcache_state.r64[reg] != -1)
     {
    struct precomp_instr *last = g_dev.r4300.regcache_state.last_access[reg]+1;
    
    while (last <= g_dev.r4300.recomp.dst)
      {
         if (g_dev.r4300.regcache_state.dirty[reg])
           last->reg_cache_infos.needed_registers[reg] = g_dev.r4300.regcache_state.reg_content[reg];
         else
           last->reg_cache_infos.needed_registers[reg] = NULL;
         
         if (g_dev.r4300.regcache_state.dirty[g_dev.r4300.regcache_state.r64[reg]])
           last->reg_cache_infos.needed_registers[g_dev.r4300.regcache_state.r64[reg]] = g_dev.r4300.regcache_state.reg_content[g_dev.r4300.regcache_state.r64[reg]];
         else
           last->reg_cache_infos.needed_registers[g_dev.r4300.regcache_state.r64[reg]] = NULL;
         
         last++;
      }
    
    if (g_dev.r4300.regcache_state.dirty[reg]) 
      {
         mov_m32_reg32(g_dev.r4300.regcache_state.reg_content[reg], reg);
         mov_m32_reg32(g_dev.r4300.regcache_state.reg_content[g_dev.r4300.regcache_state.r64[reg]], g_dev.r4300.regcache_state.r64[reg]);
         g_dev.r4300.regcache_state.dirty[reg] = 0;
      }
    g_dev.r4300.regcache_state.last_access[g_dev.r4300.regcache_state.r64[reg]] = NULL;
    g_dev.r4300.regcache_state.free_since[g_dev.r4300.regcache_state.r64[reg]] = g_dev.r4300.recomp.dst+1;
    g_dev.r4300.regcache_state.r64[reg] = -1;
     }
}

void allocate_register_manually(int reg, unsigned int *addr)
{
   int i;
   
   if (g_dev.r4300.regcache_state.last_access[reg] != NULL && g_dev.r4300.regcache_state.reg_content[reg] == addr)
     {
    struct precomp_instr *last = g_dev.r4300.regcache_state.last_access[reg]+1;
         
    while (last <= g_dev.r4300.recomp.dst)
      {
         last->reg_cache_infos.needed_registers[reg] = g_dev.r4300.regcache_state.reg_content[reg];
         last++;
      }
    g_dev.r4300.regcache_state.last_access[reg] = g_dev.r4300.recomp.dst;
    if (g_dev.r4300.regcache_state.r64[reg] != -1) 
      {
         last = g_dev.r4300.regcache_state.last_access[g_dev.r4300.regcache_state.r64[reg]]+1;
         
         while (last <= g_dev.r4300.recomp.dst)
           {
          last->reg_cache_infos.needed_registers[g_dev.r4300.regcache_state.r64[reg]] = g_dev.r4300.regcache_state.reg_content[g_dev.r4300.regcache_state.r64[reg]];
          last++;
           }
         g_dev.r4300.regcache_state.last_access[g_dev.r4300.regcache_state.r64[reg]] = g_dev.r4300.recomp.dst;
      }
    return;
     }
   
   if (g_dev.r4300.regcache_state.last_access[reg]) free_register(reg);
   else
     {
    while (g_dev.r4300.regcache_state.free_since[reg] <= g_dev.r4300.recomp.dst)
      {
         g_dev.r4300.regcache_state.free_since[reg]->reg_cache_infos.needed_registers[reg] = NULL;
         g_dev.r4300.regcache_state.free_since[reg]++;
      }
     }
   
   // is it already cached ?
   for (i=0; i<8; i++)
     {
    if (g_dev.r4300.regcache_state.last_access[i] != NULL && g_dev.r4300.regcache_state.reg_content[i] == addr)
      {
         struct precomp_instr *last = g_dev.r4300.regcache_state.last_access[i]+1;
         
         while (last <= g_dev.r4300.recomp.dst)
           {
          last->reg_cache_infos.needed_registers[i] = g_dev.r4300.regcache_state.reg_content[i];
          last++;
           }
         g_dev.r4300.regcache_state.last_access[i] = g_dev.r4300.recomp.dst;
         if (g_dev.r4300.regcache_state.r64[i] != -1) 
           {
          last = g_dev.r4300.regcache_state.last_access[g_dev.r4300.regcache_state.r64[i]]+1;
          
          while (last <= g_dev.r4300.recomp.dst)
            {
               last->reg_cache_infos.needed_registers[g_dev.r4300.regcache_state.r64[i]] = g_dev.r4300.regcache_state.reg_content[g_dev.r4300.regcache_state.r64[i]];
               last++;
            }
          g_dev.r4300.regcache_state.last_access[g_dev.r4300.regcache_state.r64[i]] = g_dev.r4300.recomp.dst;
           }
         
         mov_reg32_reg32(reg, i);
         g_dev.r4300.regcache_state.last_access[reg] = g_dev.r4300.recomp.dst;
         g_dev.r4300.regcache_state.r64[reg] = g_dev.r4300.regcache_state.r64[i];
         if (g_dev.r4300.regcache_state.r64[reg] != -1) g_dev.r4300.regcache_state.r64[g_dev.r4300.regcache_state.r64[reg]] = reg;
         g_dev.r4300.regcache_state.dirty[reg] = g_dev.r4300.regcache_state.dirty[i];
         g_dev.r4300.regcache_state.reg_content[reg] = g_dev.r4300.regcache_state.reg_content[i];
         g_dev.r4300.regcache_state.free_since[i] = g_dev.r4300.recomp.dst+1;
         g_dev.r4300.regcache_state.last_access[i] = NULL;
         
         return;
      }
     }
   
   g_dev.r4300.regcache_state.last_access[reg] = g_dev.r4300.recomp.dst;
   g_dev.r4300.regcache_state.reg_content[reg] = addr;
   g_dev.r4300.regcache_state.dirty[reg] = 0;
   g_dev.r4300.regcache_state.r64[reg] = -1;
   
   if (addr != NULL)
     {
    if (addr == g_dev.r4300.regcache_state.r0 || addr == g_dev.r4300.regcache_state.r0+1)
      xor_reg32_reg32(reg, reg);
    else
      mov_reg32_m32(reg, addr);
     }
}

void allocate_register_manually_w(int reg, unsigned int *addr, int load)
{
   int i;
   
   if (g_dev.r4300.regcache_state.last_access[reg] != NULL && g_dev.r4300.regcache_state.reg_content[reg] == addr)
     {
    struct precomp_instr *last = g_dev.r4300.regcache_state.last_access[reg]+1;
         
    while (last <= g_dev.r4300.recomp.dst)
      {
         last->reg_cache_infos.needed_registers[reg] = g_dev.r4300.regcache_state.reg_content[reg];
         last++;
      }
    g_dev.r4300.regcache_state.last_access[reg] = g_dev.r4300.recomp.dst;
    
    if (g_dev.r4300.regcache_state.r64[reg] != -1) 
      {
         last = g_dev.r4300.regcache_state.last_access[g_dev.r4300.regcache_state.r64[reg]]+1;
         
         while (last <= g_dev.r4300.recomp.dst)
           {
          last->reg_cache_infos.needed_registers[g_dev.r4300.regcache_state.r64[reg]] = g_dev.r4300.regcache_state.reg_content[g_dev.r4300.regcache_state.r64[reg]];
          last++;
           }
         g_dev.r4300.regcache_state.last_access[g_dev.r4300.regcache_state.r64[reg]] = NULL;
         g_dev.r4300.regcache_state.free_since[g_dev.r4300.regcache_state.r64[reg]] = g_dev.r4300.recomp.dst+1;
         g_dev.r4300.regcache_state.r64[reg] = -1;
      }
    g_dev.r4300.regcache_state.dirty[reg] = 1;
    return;
     }
   
   if (g_dev.r4300.regcache_state.last_access[reg]) free_register(reg);
   else
     {
    while (g_dev.r4300.regcache_state.free_since[reg] <= g_dev.r4300.recomp.dst)
      {
         g_dev.r4300.regcache_state.free_since[reg]->reg_cache_infos.needed_registers[reg] = NULL;
         g_dev.r4300.regcache_state.free_since[reg]++;
      }
     }
   
   // is it already cached ?
   for (i=0; i<8; i++)
     {
    if (g_dev.r4300.regcache_state.last_access[i] != NULL && g_dev.r4300.regcache_state.reg_content[i] == addr)
      {
         struct precomp_instr *last = g_dev.r4300.regcache_state.last_access[i]+1;
         
         while (last <= g_dev.r4300.recomp.dst)
           {
          last->reg_cache_infos.needed_registers[i] = g_dev.r4300.regcache_state.reg_content[i];
          last++;
           }
         g_dev.r4300.regcache_state.last_access[i] = g_dev.r4300.recomp.dst;
         if (g_dev.r4300.regcache_state.r64[i] != -1)
           {
          last = g_dev.r4300.regcache_state.last_access[g_dev.r4300.regcache_state.r64[i]]+1;
          while (last <= g_dev.r4300.recomp.dst)
            {
               last->reg_cache_infos.needed_registers[g_dev.r4300.regcache_state.r64[i]] = NULL;
               last++;
            }
          g_dev.r4300.regcache_state.free_since[g_dev.r4300.regcache_state.r64[i]] = g_dev.r4300.recomp.dst+1;
          g_dev.r4300.regcache_state.last_access[g_dev.r4300.regcache_state.r64[i]] = NULL;
          g_dev.r4300.regcache_state.r64[i] = -1;
           }
         
         if (load)
           mov_reg32_reg32(reg, i);
         g_dev.r4300.regcache_state.last_access[reg] = g_dev.r4300.recomp.dst;
         g_dev.r4300.regcache_state.dirty[reg] = 1;
         g_dev.r4300.regcache_state.r64[reg] = -1;
         g_dev.r4300.regcache_state.reg_content[reg] = g_dev.r4300.regcache_state.reg_content[i];
         g_dev.r4300.regcache_state.free_since[i] = g_dev.r4300.recomp.dst+1;
         g_dev.r4300.regcache_state.last_access[i] = NULL;
         
         return;
      }
     }
   
   g_dev.r4300.regcache_state.last_access[reg] = g_dev.r4300.recomp.dst;
   g_dev.r4300.regcache_state.reg_content[reg] = addr;
   g_dev.r4300.regcache_state.dirty[reg] = 1;
   g_dev.r4300.regcache_state.r64[reg] = -1;
   
   if (addr != NULL && load)
     {
    if (addr == g_dev.r4300.regcache_state.r0 || addr == g_dev.r4300.regcache_state.r0+1)
      xor_reg32_reg32(reg, reg);
    else
      mov_reg32_m32(reg, addr);
     }
}

// 0x81 0xEC 0x4 0x0 0x0 0x0  sub esp, 4
// 0xA1            0xXXXXXXXX mov eax, XXXXXXXX (&code start)
// 0x05            0xXXXXXXXX add eax, XXXXXXXX (local_addr)
// 0x89 0x04 0x24             mov [esp], eax
// 0x8B (reg<<3)|5 0xXXXXXXXX mov eax, [XXXXXXXX]
// 0x8B (reg<<3)|5 0xXXXXXXXX mov ebx, [XXXXXXXX]
// 0x8B (reg<<3)|5 0xXXXXXXXX mov ecx, [XXXXXXXX]
// 0x8B (reg<<3)|5 0xXXXXXXXX mov edx, [XXXXXXXX]
// 0x8B (reg<<3)|5 0xXXXXXXXX mov ebp, [XXXXXXXX]
// 0x8B (reg<<3)|5 0xXXXXXXXX mov esi, [XXXXXXXX]
// 0x8B (reg<<3)|5 0xXXXXXXXX mov edi, [XXXXXXXX]
// 0xC3 ret
// total : 62 bytes
static void build_wrapper(struct precomp_instr *instr, unsigned char* code, struct precomp_block* block)
{
   int i;
   int j=0;

#if defined(PROFILE_R4300)
   long x86addr = (long) code;
   int mipsop = -4;
   fwrite(&mipsop, 1, 4, g_dev.r4300.recomp.pfProfile); // write 4-byte MIPS opcode
   fwrite(&x86addr, 1, sizeof(char *), g_dev.r4300.recomp.pfProfile); // write pointer to dynamically generated x86 code for this MIPS instruction
#endif
   
   code[j++] = 0x81;
   code[j++] = 0xEC;
   code[j++] = 0x04;
   code[j++] = 0x00;
   code[j++] = 0x00;
   code[j++] = 0x00;
   
   code[j++] = 0xA1;
   *((unsigned int*)&code[j]) = (unsigned int)(&block->code);
   j+=4;
   
   code[j++] = 0x05;
   *((unsigned int*)&code[j]) = (unsigned int)instr->local_addr;
   j+=4;
   
   code[j++] = 0x89;
   code[j++] = 0x04;
   code[j++] = 0x24;
   
   for (i=0; i<8; i++)
     {
    if (instr->reg_cache_infos.needed_registers[i] != NULL)
      {
         code[j++] = 0x8B;
         code[j++] = (i << 3) | 5;
         *((unsigned int*)&code[j]) =
                 (unsigned int)instr->reg_cache_infos.needed_registers[i];
         j+=4;
      }
     }
   
   code[j++] = 0xC3;
}

void build_wrappers(struct precomp_instr *instr, int start, int end, struct precomp_block* block)
{
   int i, reg;;
   for (i=start; i<end; i++)
     {
    instr[i].reg_cache_infos.need_map = 0;
    for (reg=0; reg<8; reg++)
      {
         if (instr[i].reg_cache_infos.needed_registers[reg] != NULL)
           {
          instr[i].reg_cache_infos.need_map = 1;
          build_wrapper(&instr[i], instr[i].reg_cache_infos.jump_wrapper, block);
          break;
           }
      }
     }
}

void simplify_access(void)
{
   int i;
   g_dev.r4300.recomp.dst->local_addr = g_dev.r4300.recomp.code_length;
   for(i=0; i<8; i++) g_dev.r4300.recomp.dst->reg_cache_infos.needed_registers[i] = NULL;
}

