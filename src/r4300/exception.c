/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - exception.c                                             *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
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

#include "api/callbacks.h"
#include "api/m64p_types.h"
#include "exception.h"
#include "main/main.h"
#include "memory/memory.h"
#include "r4300.h"
#include "r4300_core.h"
#include "recomp.h"
#include "recomph.h"
#include "tlb.h"

void TLB_refill_exception(uint32_t address, int w)
{
   int usual_handler = 0, i;

   if (g_dev.r4300.emumode != EMUMODE_DYNAREC && w != 2) cp0_update_count();
   if (w == 1) g_dev.r4300.cp0.regs[CP0_CAUSE_REG] = CP0_CAUSE_EXCCODE_TLBS;
   else g_dev.r4300.cp0.regs[CP0_CAUSE_REG] = CP0_CAUSE_EXCCODE_TLBL;
   g_dev.r4300.cp0.regs[CP0_BADVADDR_REG] = address;
   g_dev.r4300.cp0.regs[CP0_CONTEXT_REG] = (g_dev.r4300.cp0.regs[CP0_CONTEXT_REG] & UINT32_C(0xFF80000F)) | ((address >> 9) & UINT32_C(0x007FFFF0));
   g_dev.r4300.cp0.regs[CP0_ENTRYHI_REG] = address & UINT32_C(0xFFFFE000);
   if (g_dev.r4300.cp0.regs[CP0_STATUS_REG] & CP0_STATUS_EXL)
     {
    generic_jump_to(UINT32_C(0x80000180));
    if(g_dev.r4300.delay_slot==1 || g_dev.r4300.delay_slot==3) g_dev.r4300.cp0.regs[CP0_CAUSE_REG] |= CP0_CAUSE_BD;
    else g_dev.r4300.cp0.regs[CP0_CAUSE_REG] &= ~CP0_CAUSE_BD;
     }
   else
     {
    if (g_dev.r4300.emumode != EMUMODE_PURE_INTERPRETER) 
      {
         if (w!=2)
           g_dev.r4300.cp0.regs[CP0_EPC_REG] = g_dev.r4300.pc->addr;
         else
           g_dev.r4300.cp0.regs[CP0_EPC_REG] = address;
      }
    else g_dev.r4300.cp0.regs[CP0_EPC_REG] = g_dev.r4300.pc->addr;
         
    g_dev.r4300.cp0.regs[CP0_CAUSE_REG] &= ~CP0_CAUSE_BD;
    g_dev.r4300.cp0.regs[CP0_STATUS_REG] |= CP0_STATUS_EXL;
    
    if (address >= UINT32_C(0x80000000) && address < UINT32_C(0xc0000000))
      usual_handler = 1;
    for (i=0; i<32; i++)
      {
         if (/*g_dev.r4300.cp0.tlb.entries[i].v_even &&*/ address >= g_dev.r4300.cp0.tlb.entries[i].start_even &&
         address <= g_dev.r4300.cp0.tlb.entries[i].end_even)
           usual_handler = 1;
         if (/*g_dev.r4300.cp0.tlb.entries[i].v_odd &&*/ address >= g_dev.r4300.cp0.tlb.entries[i].start_odd &&
         address <= g_dev.r4300.cp0.tlb.entries[i].end_odd)
           usual_handler = 1;
      }
    if (usual_handler)
      {
         generic_jump_to(UINT32_C(0x80000180));
      }
    else
      {
         generic_jump_to(UINT32_C(0x80000000));
      }
     }
   if(g_dev.r4300.delay_slot==1 || g_dev.r4300.delay_slot==3)
     {
    g_dev.r4300.cp0.regs[CP0_CAUSE_REG] |= CP0_CAUSE_BD;
    g_dev.r4300.cp0.regs[CP0_EPC_REG]-=4;
     }
   else
     {
    g_dev.r4300.cp0.regs[CP0_CAUSE_REG] &= ~CP0_CAUSE_BD;
     }
   if(w != 2) g_dev.r4300.cp0.regs[CP0_EPC_REG]-=4;
   
   g_dev.r4300.cp0.last_addr = g_dev.r4300.pc->addr;
   
   if (g_dev.r4300.emumode == EMUMODE_DYNAREC) 
     {
    dyna_jump();
    if (!g_dev.r4300.dyna_interp) g_dev.r4300.delay_slot = 0;
     }
   
   if (g_dev.r4300.emumode != EMUMODE_DYNAREC || g_dev.r4300.dyna_interp)
     {
    g_dev.r4300.dyna_interp = 0;
    if (g_dev.r4300.delay_slot)
      {
         g_dev.r4300.skip_jump = g_dev.r4300.pc->addr;
         g_dev.r4300.cp0.next_interrupt = 0;
      }
     }
}

void exception_general(void)
{
   cp0_update_count();
   g_dev.r4300.cp0.regs[CP0_STATUS_REG] |= CP0_STATUS_EXL;
   
   g_dev.r4300.cp0.regs[CP0_EPC_REG] = g_dev.r4300.pc->addr;
   
   if(g_dev.r4300.delay_slot==1 || g_dev.r4300.delay_slot==3)
     {
    g_dev.r4300.cp0.regs[CP0_CAUSE_REG] |= CP0_CAUSE_BD;
    g_dev.r4300.cp0.regs[CP0_EPC_REG]-=4;
     }
   else
     {
    g_dev.r4300.cp0.regs[CP0_CAUSE_REG] &= ~CP0_CAUSE_BD;
     }
   generic_jump_to(UINT32_C(0x80000180));
   g_dev.r4300.cp0.last_addr = g_dev.r4300.pc->addr;
   if (g_dev.r4300.emumode == EMUMODE_DYNAREC)
     {
    dyna_jump();
    if (!g_dev.r4300.dyna_interp) g_dev.r4300.delay_slot = 0;
     }
   if (g_dev.r4300.emumode != EMUMODE_DYNAREC || g_dev.r4300.dyna_interp)
     {
    g_dev.r4300.dyna_interp = 0;
    if (g_dev.r4300.delay_slot)
      {
         g_dev.r4300.skip_jump = g_dev.r4300.pc->addr;
         g_dev.r4300.cp0.next_interrupt = 0;
      }
     }
}

