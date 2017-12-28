/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - regcache.c                                              *
 *   Mupen64Plus homepage: https://mupen64plus.org/                        *
 *   Copyright (C) 2007 Richard Goedeken (Richard42)                       *
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

#include <stdint.h>
#include <stdio.h>

#include "api/callbacks.h"
#include "api/m64p_types.h"
#include "device/r4300/recomp.h"
#include "device/r4300/recomph.h"
#include "device/r4300/x86_64/assemble.h"
#include "device/r4300/x86_64/assemble_struct.h"
#include "regcache.h"

void init_cache(struct precomp_instr* start)
{
  int i;
  for (i=0; i<8; i++)
  {
    g_dev.r4300.regcache_state.reg_content[i] = NULL;
    g_dev.r4300.regcache_state.last_access[i] = NULL;
    g_dev.r4300.regcache_state.free_since[i] = start;
    g_dev.r4300.regcache_state.dirty[i] = 0;
    g_dev.r4300.regcache_state.is64bits[i] = 0;
  }
  g_dev.r4300.regcache_state.r0 = (unsigned long long *) r4300_regs(&g_dev.r4300);
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
    if (g_dev.r4300.regcache_state.last_access[i])
    {
      free_register(i);
    }
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
    long long x86addr = (long long) ((*g_dev.r4300.recomp.inst_pointer) + freestart);
    int mipsop = -5;
    if (fwrite(&mipsop, 1, 4, g_dev.r4300.recomp.pfProfile) != 4 || /* -5 = regcache flushing */
        fwrite(&x86addr, 1, sizeof(char *), g_dev.r4300.recomp.pfProfile) != sizeof(char *)) // write pointer to start of register cache flushing instructions
        DebugMessage(M64MSG_ERROR, "Error writing R4300 instruction address profiling data");
    x86addr = (long long) ((*g_dev.r4300.recomp.inst_pointer) + g_dev.r4300.recomp.code_length);
    if (fwrite(&g_dev.r4300.recomp.src, 1, 4, g_dev.r4300.recomp.pfProfile) != 4 || // write 4-byte MIPS opcode for current instruction
        fwrite(&x86addr, 1, sizeof(char *), g_dev.r4300.recomp.pfProfile) != sizeof(char *)) // write pointer to dynamically generated x86 code for this MIPS instruction
        DebugMessage(M64MSG_ERROR, "Error writing R4300 instruction address profiling data");
  }
#endif
}

static void simplify_access(void)
{
   int i;
   g_dev.r4300.recomp.dst->local_addr = g_dev.r4300.recomp.code_length;
   for(i=0; i<8; i++) g_dev.r4300.recomp.dst->reg_cache_infos.needed_registers[i] = NULL;
}

void free_registers_move_start(void)
{
  /* flush all dirty registers and clear needed_registers table */
  free_all_registers();

  /* now move the start of the new instruction down past the flushing instructions */
  simplify_access();

}

// this function frees a specific X86 GPR
void free_register(int reg)
{
  struct precomp_instr *last;
   
  if (g_dev.r4300.regcache_state.last_access[reg] != NULL)
    last = g_dev.r4300.regcache_state.last_access[reg]+1;
  else
    last = g_dev.r4300.regcache_state.free_since[reg];
   
  while (last <= g_dev.r4300.recomp.dst)
  {
    if (g_dev.r4300.regcache_state.last_access[reg] != NULL && g_dev.r4300.regcache_state.dirty[reg])
      last->reg_cache_infos.needed_registers[reg] = g_dev.r4300.regcache_state.reg_content[reg];
    else
      last->reg_cache_infos.needed_registers[reg] = NULL;
    last++;
  }
  if (g_dev.r4300.regcache_state.last_access[reg] == NULL) 
  {
    g_dev.r4300.regcache_state.free_since[reg] = g_dev.r4300.recomp.dst+1;
    return;
  }

  if (g_dev.r4300.regcache_state.dirty[reg]) 
  {
    if (g_dev.r4300.regcache_state.is64bits[reg])
    {
      mov_m64rel_xreg64((unsigned long long *) g_dev.r4300.regcache_state.reg_content[reg], reg);
    }
    else
    {
      movsxd_reg64_reg32(reg, reg);
      mov_m64rel_xreg64((unsigned long long *) g_dev.r4300.regcache_state.reg_content[reg], reg);
    }
  }

  g_dev.r4300.regcache_state.last_access[reg] = NULL;
  g_dev.r4300.regcache_state.free_since[reg] = g_dev.r4300.recomp.dst+1;
}

int lru_register(void)
{
   unsigned long long oldest_access = 0xFFFFFFFFFFFFFFFFULL;
   int i, reg = 0;
   for (i=0; i<8; i++)
     {
    if (i != ESP && (unsigned long long) g_dev.r4300.regcache_state.last_access[i] < oldest_access)
      {
         oldest_access = (unsigned long long) g_dev.r4300.regcache_state.last_access[i];
         reg = i;
      }
     }
   return reg;
}

int lru_base_register(void) /* EBP cannot be used as a base register for SIB addressing byte */
{
   unsigned long long oldest_access = 0xFFFFFFFFFFFFFFFFULL;
   int i, reg = 0;
   for (i=0; i<8; i++)
     {
    if (i != ESP && i != EBP && (unsigned long long) g_dev.r4300.regcache_state.last_access[i] < oldest_access)
      {
         oldest_access = (unsigned long long) g_dev.r4300.regcache_state.last_access[i];
         reg = i;
      }
     }
   return reg;
}

void set_register_state(int reg, unsigned int *addr, int _dirty, int _is64bits)
{
  if (addr == NULL)
    g_dev.r4300.regcache_state.last_access[reg] = NULL;
  else
    g_dev.r4300.regcache_state.last_access[reg] = g_dev.r4300.recomp.dst;
  g_dev.r4300.regcache_state.reg_content[reg] = (unsigned long long *) addr;
  g_dev.r4300.regcache_state.is64bits[reg] = _is64bits;
  g_dev.r4300.regcache_state.dirty[reg] = _dirty;
}

int lock_register(int reg)
{
   free_register(reg);
   g_dev.r4300.regcache_state.last_access[reg] = (struct precomp_instr *) 0xFFFFFFFFFFFFFFFFULL;
   g_dev.r4300.regcache_state.reg_content[reg] = NULL;
   return reg;
}

void unlock_register(int reg)
{
   g_dev.r4300.regcache_state.last_access[reg] = NULL;
}

// this function finds a register to put the data contained in addr,
// if there was another value before it's cleanly removed of the
// register cache. After that, the register number is returned.
// If data are already cached, the function only returns the register number
int allocate_register_32(unsigned int *addr)
{
  int reg = 0, i;
   
  // is it already cached ?
  if (addr != NULL)
  {
    for (i = 0; i < 8; i++)
    {
      if (g_dev.r4300.regcache_state.last_access[i] != NULL && (unsigned int *) g_dev.r4300.regcache_state.reg_content[i] == addr)
      {
        struct precomp_instr *last = g_dev.r4300.regcache_state.last_access[i]+1;

        while (last <= g_dev.r4300.recomp.dst)
        {
          last->reg_cache_infos.needed_registers[i] = g_dev.r4300.regcache_state.reg_content[i];
          last++;
        }
        g_dev.r4300.regcache_state.last_access[i] = g_dev.r4300.recomp.dst;
        g_dev.r4300.regcache_state.is64bits[i] = 0;
        return i;
      }
    }
  }

  // it's not cached, so take the least recently used register
  reg = lru_register();
   
  if (g_dev.r4300.regcache_state.last_access[reg])
    free_register(reg);
  else
  {
    while (g_dev.r4300.regcache_state.free_since[reg] <= g_dev.r4300.recomp.dst)
    {
      g_dev.r4300.regcache_state.free_since[reg]->reg_cache_infos.needed_registers[reg] = NULL;
      g_dev.r4300.regcache_state.free_since[reg]++;
    }
  }
   
  g_dev.r4300.regcache_state.last_access[reg] = g_dev.r4300.recomp.dst;
  g_dev.r4300.regcache_state.reg_content[reg] = (unsigned long long *) addr;
  g_dev.r4300.regcache_state.dirty[reg] = 0;
  g_dev.r4300.regcache_state.is64bits[reg] = 0;
   
  if (addr != NULL)
  {
    if (addr == (unsigned int *) g_dev.r4300.regcache_state.r0)
      xor_reg32_reg32(reg, reg);
    else
      mov_xreg32_m32rel(reg, addr);
  }

  return reg;
}

// this function is similar to allocate_register except it loads
// a 64 bits value, and return the register number of the LSB part
int allocate_register_64(unsigned long long *addr)
{
  int reg, i;

  // is it already cached?
  if (addr != NULL)
  {
    for (i = 0; i < 8; i++)
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
        if (g_dev.r4300.regcache_state.is64bits[i] == 0)
        {
          movsxd_reg64_reg32(i, i);
          g_dev.r4300.regcache_state.is64bits[i] = 1;
        }
        return i;
      }
    }
  }

  // it's not cached, so take the least recently used register
  reg = lru_register();
   
  if (g_dev.r4300.regcache_state.last_access[reg])
    free_register(reg);
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
  g_dev.r4300.regcache_state.is64bits[reg] = 1;
   
  if (addr != NULL)
  {
    if (addr == g_dev.r4300.regcache_state.r0)
      xor_reg64_reg64(reg, reg);
    else
      mov_xreg64_m64rel(reg, addr);
  }

  return reg;
}

// this function checks if the data located at addr are cached in a register
// and then, it returns 1  if it's a 64 bit value
//                      0  if it's a 32 bit value
//                      -1 if it's not cached
int is64(unsigned int *addr)
{
  int i;
  for (i = 0; i < 8; i++)
  {
    if (g_dev.r4300.regcache_state.last_access[i] != NULL && g_dev.r4300.regcache_state.reg_content[i] == (unsigned long long *) addr)
    {
      return g_dev.r4300.regcache_state.is64bits[i];
    }
  }
  return -1;
}

int allocate_register_32_w(unsigned int *addr)
{
  int reg = 0, i;
   
  // is it already cached ?
  for (i = 0; i < 8; i++)
  {
    if (g_dev.r4300.regcache_state.last_access[i] != NULL && g_dev.r4300.regcache_state.reg_content[i] == (unsigned long long *) addr)
    {
      struct precomp_instr *last = g_dev.r4300.regcache_state.last_access[i] + 1;
         
      while (last <= g_dev.r4300.recomp.dst)
      {
        last->reg_cache_infos.needed_registers[i] = NULL;
        last++;
      }
      g_dev.r4300.regcache_state.last_access[i] = g_dev.r4300.recomp.dst;
      g_dev.r4300.regcache_state.dirty[i] = 1;
      g_dev.r4300.regcache_state.is64bits[i] = 0;
      return i;
    }
  }

  // it's not cached, so take the least recently used register
  reg = lru_register();
   
  if (g_dev.r4300.regcache_state.last_access[reg])
    free_register(reg);
  else
  {
    while (g_dev.r4300.regcache_state.free_since[reg] <= g_dev.r4300.recomp.dst)
    {
      g_dev.r4300.regcache_state.free_since[reg]->reg_cache_infos.needed_registers[reg] = NULL;
      g_dev.r4300.regcache_state.free_since[reg]++;
    }
  }
   
  g_dev.r4300.regcache_state.last_access[reg] = g_dev.r4300.recomp.dst;
  g_dev.r4300.regcache_state.reg_content[reg] = (unsigned long long *) addr;
  g_dev.r4300.regcache_state.dirty[reg] = 1;
  g_dev.r4300.regcache_state.is64bits[reg] = 0;

  return reg;
}

int allocate_register_64_w(unsigned long long *addr)
{
  int reg, i;

  // is it already cached?
  for (i = 0; i < 8; i++)
  {
    if (g_dev.r4300.regcache_state.last_access[i] != NULL && g_dev.r4300.regcache_state.reg_content[i] == addr)
    {
      struct precomp_instr *last = g_dev.r4300.regcache_state.last_access[i] + 1;

      while (last <= g_dev.r4300.recomp.dst)
      {
        last->reg_cache_infos.needed_registers[i] = NULL;
        last++;
      }
      g_dev.r4300.regcache_state.last_access[i] = g_dev.r4300.recomp.dst;
      g_dev.r4300.regcache_state.is64bits[i] = 1;
      g_dev.r4300.regcache_state.dirty[i] = 1;
      return i;
    }
  }

  // it's not cached, so take the least recently used register
  reg = lru_register();
   
  if (g_dev.r4300.regcache_state.last_access[reg])
    free_register(reg);
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
  g_dev.r4300.regcache_state.is64bits[reg] = 1;

  return reg;
}

void allocate_register_32_manually(int reg, unsigned int *addr)
{
  int i;

  /* check if we just happen to already have this r4300 reg cached in the requested x86 reg */
  if (g_dev.r4300.regcache_state.last_access[reg] != NULL && g_dev.r4300.regcache_state.reg_content[reg] == (unsigned long long *) addr)
  {
    struct precomp_instr *last = g_dev.r4300.regcache_state.last_access[reg] + 1;
    while (last <= g_dev.r4300.recomp.dst)
    {
      last->reg_cache_infos.needed_registers[reg] = g_dev.r4300.regcache_state.reg_content[reg];
      last++;
    }
    g_dev.r4300.regcache_state.last_access[reg] = g_dev.r4300.recomp.dst;
    /* we won't touch is64bits or dirty; the register returned is "read-only" */
    return;
  }

  /* otherwise free up the requested x86 register */
  if (g_dev.r4300.regcache_state.last_access[reg])
    free_register(reg);
  else
  {
    while (g_dev.r4300.regcache_state.free_since[reg] <= g_dev.r4300.recomp.dst)
    {
      g_dev.r4300.regcache_state.free_since[reg]->reg_cache_infos.needed_registers[reg] = NULL;
      g_dev.r4300.regcache_state.free_since[reg]++;
    }
  }

  /* if the r4300 register is already cached in a different x86 register, then copy it to the requested x86 register */
  for (i=0; i<8; i++)
  {
    if (g_dev.r4300.regcache_state.last_access[i] != NULL && g_dev.r4300.regcache_state.reg_content[i] == (unsigned long long *) addr)
    {
      struct precomp_instr *last = g_dev.r4300.regcache_state.last_access[i]+1;
      while (last <= g_dev.r4300.recomp.dst)
      {
        last->reg_cache_infos.needed_registers[i] = g_dev.r4300.regcache_state.reg_content[i];
        last++;
      }
      g_dev.r4300.regcache_state.last_access[i] = g_dev.r4300.recomp.dst;
      if (g_dev.r4300.regcache_state.is64bits[i])
        mov_reg64_reg64(reg, i);
      else
        mov_reg32_reg32(reg, i);
      g_dev.r4300.regcache_state.last_access[reg] = g_dev.r4300.recomp.dst;
      g_dev.r4300.regcache_state.is64bits[reg] = g_dev.r4300.regcache_state.is64bits[i];
      g_dev.r4300.regcache_state.dirty[reg] = g_dev.r4300.regcache_state.dirty[i];
      g_dev.r4300.regcache_state.reg_content[reg] = g_dev.r4300.regcache_state.reg_content[i];
      /* free the previous x86 register used to cache this r4300 register */
      g_dev.r4300.regcache_state.free_since[i] = g_dev.r4300.recomp.dst + 1;
      g_dev.r4300.regcache_state.last_access[i] = NULL;
      return;
    }
  }

  /* otherwise just load the 32-bit value into the requested register */
  g_dev.r4300.regcache_state.last_access[reg] = g_dev.r4300.recomp.dst;
  g_dev.r4300.regcache_state.reg_content[reg] = (unsigned long long *) addr;
  g_dev.r4300.regcache_state.dirty[reg] = 0;
  g_dev.r4300.regcache_state.is64bits[reg] = 0;
   
  if ((unsigned long long *) addr == g_dev.r4300.regcache_state.r0)
    xor_reg32_reg32(reg, reg);
  else
    mov_xreg32_m32rel(reg, addr);
}

void allocate_register_32_manually_w(int reg, unsigned int *addr)
{
  int i;

  /* check if we just happen to already have this r4300 reg cached in the requested x86 reg */
  if (g_dev.r4300.regcache_state.last_access[reg] != NULL && g_dev.r4300.regcache_state.reg_content[reg] == (unsigned long long *) addr)
  {
    struct precomp_instr *last = g_dev.r4300.regcache_state.last_access[reg]+1;
    while (last <= g_dev.r4300.recomp.dst)
    {
      last->reg_cache_infos.needed_registers[reg] = NULL;
      last++;
    }
    g_dev.r4300.regcache_state.last_access[reg] = g_dev.r4300.recomp.dst;
    g_dev.r4300.regcache_state.is64bits[reg] = 0;
    g_dev.r4300.regcache_state.dirty[reg] = 1;
    return;
  }
   
  /* otherwise free up the requested x86 register */
  if (g_dev.r4300.regcache_state.last_access[reg])
    free_register(reg);
  else
  {
    while (g_dev.r4300.regcache_state.free_since[reg] <= g_dev.r4300.recomp.dst)
    {
      g_dev.r4300.regcache_state.free_since[reg]->reg_cache_infos.needed_registers[reg] = NULL;
      g_dev.r4300.regcache_state.free_since[reg]++;
    }
  }

  /* if the r4300 register is already cached in a different x86 register, then free it and bind to the requested x86 register */
  for (i = 0; i < 8; i++)
  {
    if (g_dev.r4300.regcache_state.last_access[i] != NULL && g_dev.r4300.regcache_state.reg_content[i] == (unsigned long long *) addr)
    {
      struct precomp_instr *last = g_dev.r4300.regcache_state.last_access[i] + 1;
      while (last <= g_dev.r4300.recomp.dst)
      {
        last->reg_cache_infos.needed_registers[i] = NULL;
        last++;
      }
      g_dev.r4300.regcache_state.last_access[reg] = g_dev.r4300.recomp.dst;
      g_dev.r4300.regcache_state.reg_content[reg] = g_dev.r4300.regcache_state.reg_content[i];
      g_dev.r4300.regcache_state.dirty[reg] = 1;
      g_dev.r4300.regcache_state.is64bits[reg] = 0;
      /* free the previous x86 register used to cache this r4300 register */
      g_dev.r4300.regcache_state.free_since[i] = g_dev.r4300.recomp.dst+1;
      g_dev.r4300.regcache_state.last_access[i] = NULL;
      return;
    }
  }
   
  /* otherwise just set up the requested register as 32-bit */
  g_dev.r4300.regcache_state.last_access[reg] = g_dev.r4300.recomp.dst;
  g_dev.r4300.regcache_state.reg_content[reg] = (unsigned long long *) addr;
  g_dev.r4300.regcache_state.dirty[reg] = 1;
  g_dev.r4300.regcache_state.is64bits[reg] = 0;
}


// 0x48 0x83 0xEC 0x8                     sub rsp, byte 8
// 0x48 0xA1           0xXXXXXXXXXXXXXXXX mov rax, qword (&code start)
// 0x48 0x05                   0xXXXXXXXX add rax, dword (local_addr)
// 0x48 0x89 0x04 0x24                    mov [rsp], rax
// 0x48 0xB8           0xXXXXXXXXXXXXXXXX mov rax, &reg[0]
// 0x48 0x8B (reg<<3)|0x80     0xXXXXXXXX mov rdi, [rax + XXXXXXXX]
// 0x48 0x8B (reg<<3)|0x80     0xXXXXXXXX mov rsi, [rax + XXXXXXXX]
// 0x48 0x8B (reg<<3)|0x80     0xXXXXXXXX mov rbp, [rax + XXXXXXXX]
// 0x48 0x8B (reg<<3)|0x80     0xXXXXXXXX mov rdx, [rax + XXXXXXXX]
// 0x48 0x8B (reg<<3)|0x80     0xXXXXXXXX mov rcx, [rax + XXXXXXXX]
// 0x48 0x8B (reg<<3)|0x80     0xXXXXXXXX mov rbx, [rax + XXXXXXXX]
// 0x48 0x8B (reg<<3)|0x80     0xXXXXXXXX mov rax, [rax + XXXXXXXX]
// 0xC3 ret
// total : 84 bytes

static void build_wrapper(struct precomp_instr *instr, unsigned char* pCode, struct precomp_block* block)
{
   int i;

#if defined(PROFILE_R4300)
   long long x86addr = (long long) pCode;
   int mipsop = -4;
   if (fwrite(&mipsop, 1, 4, g_dev.r4300.recomp.pfProfile) != 4 || // write 4-byte MIPS opcode
       fwrite(&x86addr, 1, sizeof(char *), g_dev.r4300.recomp.pfProfile) != sizeof(char *)) // write pointer to dynamically generated x86 code for this MIPS instruction
       DebugMessage(M64MSG_ERROR, "Error writing R4300 instruction address profiling data");
#endif

   *pCode++ = 0x48;
   *pCode++ = 0x83;
   *pCode++ = 0xEC;
   *pCode++ = 0x08;
   
   *pCode++ = 0x48;
   *pCode++ = 0xA1;
   *((unsigned long long *) pCode) = (unsigned long long) (&block->code);
   pCode += 8;
   
   *pCode++ = 0x48;
   *pCode++ = 0x05;
   *((unsigned int *) pCode) = (unsigned int) instr->local_addr;
   pCode += 4;
   
   *pCode++ = 0x48;
   *pCode++ = 0x89;
   *pCode++ = 0x04;
   *pCode++ = 0x24;

   *pCode++ = 0x48;
   *pCode++ = 0xB8;
   *((unsigned long long *) pCode) = (unsigned long long) &r4300_regs(&g_dev.r4300)[0];
   pCode += 8;

   for (i=7; i>=0; i--)
   {
     long long riprel;
     if (instr->reg_cache_infos.needed_registers[i] != NULL)
     {
       *pCode++ = 0x48;
       *pCode++ = 0x8B;
       *pCode++ = 0x80 | (i << 3);
       riprel = (long long) ((unsigned char *) instr->reg_cache_infos.needed_registers[i] - (unsigned char *) &r4300_regs(&g_dev.r4300)[0]);
       *((int *) pCode) = (int) riprel;
       pCode += 4;
       if (riprel >= 0x7fffffffLL || riprel < -0x80000000LL)
       {
         DebugMessage(M64MSG_ERROR, "build_wrapper error: regs[%i] offset too big for relative address from %p to %p",
                i, (&r4300_regs(&g_dev.r4300)[0]), instr->reg_cache_infos.needed_registers[i]);
         OSAL_BREAKPOINT_INTERRUPT;
       }
     }
   }
   *pCode++ = 0xC3;
}

void build_wrappers(struct precomp_instr *instr, int start, int end, struct precomp_block* block)
{
   int i, reg;
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

