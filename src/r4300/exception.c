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
#include "cp0_private.h"
#include "exception.h"
#include "memory/memory.h"
#include "r4300.h"
#include "r4300_core.h"
#include "recomp.h"
#include "recomph.h"
#include "tlb.h"

void TLB_refill_exception(uint32_t address, int w)
{
#if 0
   int usual_handler = 0, i;

   if (r4300emu != CORE_DYNAREC && w != 2) cp0_update_count();
   if (w == 1) g_state.regs.cp0[CP0_CAUSE_REG] = (UINT32_C(3) << 2);
   else g_state.regs.cp0[CP0_CAUSE_REG] = (UINT32_C(2) << 2);
   g_state.regs.cp0[CP0_BADVADDR_REG] = address;
   g_state.regs.cp0[CP0_CONTEXT_REG] = (g_state.regs.cp0[CP0_CONTEXT_REG] & UINT32_C(0xFF80000F)) | ((address >> 9) & UINT32_C(0x007FFFF0));
   g_state.regs.cp0[CP0_ENTRYHI_REG] = address & UINT32_C(0xFFFFE000);
   if (g_state.regs.cp0[CP0_STATUS_REG] & UINT32_C(0x2)) // Test de EXL
     {
    generic_jump_to(UINT32_C(0x80000180));
    if(g_state.delay_slot==1 || g_state.delay_slot==3) g_state.regs.cp0[CP0_CAUSE_REG] |= UINT32_C(0x80000000);
    else g_state.regs.cp0[CP0_CAUSE_REG] &= UINT32_C(0x7FFFFFFF);
     }
   else
     {
    if (r4300emu != CORE_PURE_INTERPRETER) 
      {
         if (w!=2)
           g_state.regs.cp0[CP0_EPC_REG] = PC->addr;
         else
           g_state.regs.cp0[CP0_EPC_REG] = address;
      }
    else g_state.regs.cp0[CP0_EPC_REG] = PC->addr;
         
    g_state.regs.cp0[CP0_CAUSE_REG] &= ~UINT32_C(0x80000000);
    g_state.regs.cp0[CP0_STATUS_REG] |= UINT32_C(0x2); //EXL=1
    
    if (address >= UINT32_C(0x80000000) && address < UINT32_C(0xc0000000))
      usual_handler = 1;
    for (i=0; i<32; i++)
      {
         if (/*tlb_e[i].v_even &&*/ address >= tlb_e[i].start_even &&
         address <= tlb_e[i].end_even)
           usual_handler = 1;
         if (/*tlb_e[i].v_odd &&*/ address >= tlb_e[i].start_odd &&
         address <= tlb_e[i].end_odd)
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
   if(g_state.delay_slot == 1 || g_state.delay_slot == 3)
     {
    g_state.regs.cp0[CP0_CAUSE_REG] |= UINT32_C(0x80000000);
    g_state.regs.cp0[CP0_EPC_REG]-=4;
     }
   else
     {
    g_state.regs.cp0[CP0_CAUSE_REG] &= UINT32_C(0x7FFFFFFF);
     }
   if(w != 2) g_state.regs.cp0[CP0_EPC_REG]-=4;
   
   last_addr = PC->addr;
   
   if (r4300emu == CORE_DYNAREC) 
     {
    dyna_jump();
    if (!dyna_interp) g_state.delay_slot = 0;
     }
   
   if (r4300emu != CORE_DYNAREC || dyna_interp)
     {
    dyna_interp = 0;
    if (g_state.delay_slot)
      {
         skip_jump = PC->addr;
         g_state.next_interrupt = 0;
      }
     }
#else
	int usual_handler = 0, i;

	if (r4300emu != CORE_DYNAREC && w != 2) cp0_update_count();
	if (w == 1) {
		g_state.regs.cp0[CP0_CAUSE_REG] = (UINT32_C(3) << 2);
	} else {
		g_state.regs.cp0[CP0_CAUSE_REG] = (UINT32_C(2) << 2);
	}
	g_state.regs.cp0[CP0_BADVADDR_REG] = address;
	g_state.regs.cp0[CP0_CONTEXT_REG] = (g_state.regs.cp0[CP0_CONTEXT_REG] & UINT32_C(0xFF80000F)) | ((address >> 9) & UINT32_C(0x007FFFF0));
	g_state.regs.cp0[CP0_ENTRYHI_REG] = address & UINT32_C(0xFFFFE000);
	if (g_state.regs.cp0[CP0_STATUS_REG] & UINT32_C(0x2)) { // Test de EXL
		generic_jump_to(UINT32_C(0x80000180));
		if(g_state.delay_slot) {
			g_state.regs.cp0[CP0_CAUSE_REG] |= UINT32_C(0x80000000);
		} else {
			g_state.regs.cp0[CP0_CAUSE_REG] &= UINT32_C(0x7FFFFFFF);
		}
	} else {
		if (r4300emu != CORE_PURE_INTERPRETER) {
			if (w != 2) {
				g_state.regs.cp0[CP0_EPC_REG] = PC->addr;
			} else {
				g_state.regs.cp0[CP0_EPC_REG] = address;
			}
		} else {
			g_state.regs.cp0[CP0_EPC_REG] = PC->addr;
		}

		g_state.regs.cp0[CP0_CAUSE_REG] &= ~UINT32_C(0x80000000);
		g_state.regs.cp0[CP0_STATUS_REG] |= UINT32_C(0x2); //EXL=1

		if ((address & UINT32_C(0xC0000000)) == UINT32_C(0x80000000)) {
			usual_handler = 1;
		} else {
			for (i = 0; i < 32; i++) {
				if (/*tlb_e[i].v_even &&*/ address >= tlb_e[i].start_even &&
				address <= tlb_e[i].end_even) {
					usual_handler = 1;
					break;
				}
				if (/*tlb_e[i].v_odd &&*/ address >= tlb_e[i].start_odd &&
				address <= tlb_e[i].end_odd) {
					usual_handler = 1;
					break;
				}
			}
		}
		if (usual_handler) {
			generic_jump_to(UINT32_C(0x80000180));
		} else {
			generic_jump_to(UINT32_C(0x80000000));
		}
	}

	if(g_state.delay_slot) {
		g_state.regs.cp0[CP0_CAUSE_REG] |= UINT32_C(0x80000000);
		g_state.regs.cp0[CP0_EPC_REG] -= 4;
	} else {
		g_state.regs.cp0[CP0_CAUSE_REG] &= UINT32_C(0x7FFFFFFF);
	}

	if(w != 2) g_state.regs.cp0[CP0_EPC_REG] -= 4;
	last_addr = PC->addr;

	if (g_state.delay_slot) {
		g_state.delay_slot = 0;
		skip_jump = 1;
	}
#endif
}

void exception_general(void)
{
#if 0
   cp0_update_count();
   g_state.regs.cp0[CP0_STATUS_REG] |= 2;
   
   g_state.regs.cp0[CP0_EPC_REG] = PC->addr;
   
   if(g_state.delay_slot==1 || g_state.delay_slot==3)
     {
    g_state.regs.cp0[CP0_CAUSE_REG] |= UINT32_C(0x80000000);
    g_state.regs.cp0[CP0_EPC_REG]-=4;
     }
   else
     {
    g_state.regs.cp0[CP0_CAUSE_REG] &= UINT32_C(0x7FFFFFFF);
     }
   generic_jump_to(UINT32_C(0x80000180));
   last_addr = PC->addr;
   if (r4300emu == CORE_DYNAREC)
     {
    dyna_jump();
    if (!dyna_interp) g_state.delay_slot = 0;
     }
   if (r4300emu != CORE_DYNAREC || dyna_interp)
     {
    dyna_interp = 0;
    if (g_state.delay_slot)
      {
         skip_jump = PC->addr;
         g_state.next_interrupt = 0;
      }
     }
#else
	cp0_update_count();

	g_state.regs.cp0[CP0_STATUS_REG] |= 2;
	g_state.regs.cp0[CP0_EPC_REG] = PC->addr;

	if(g_state.delay_slot) {
		g_state.regs.cp0[CP0_CAUSE_REG] |= UINT32_C(0x80000000);
		g_state.regs.cp0[CP0_EPC_REG]-=4;
	} else {
		g_state.regs.cp0[CP0_CAUSE_REG] &= UINT32_C(0x7FFFFFFF);
	}

	generic_jump_to(UINT32_C(0x80000180));
	last_addr = PC->addr;

	if (g_state.delay_slot) {
		g_state.delay_slot = 0;
		skip_jump = 1;
	}
#endif
}

