/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - cp0.c                                                   *
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

#include <stdint.h>
#include <string.h>

#include "cp0.h"
#include "exception.h"
#include "main/main.h"
#include "new_dynarec/new_dynarec.h" /* for NEW_DYNAREC_ARM */
#include "recomp.h"

#ifdef COMPARE_CORE
#include "api/debugger.h"
#endif

#ifdef DBG
#include "debugger/dbg_debugger.h"
#include "debugger/dbg_types.h"
#endif

extern uint32_t g_dev_r4300_cp0_regs[32];
extern unsigned int g_dev_r4300_cp0_next_interrupt;

/* global functions */
void init_cp0(struct cp0* cp0, unsigned int count_per_op, struct new_dynarec_hot_state* new_dynarec_hot_state)
{
    cp0->count_per_op = count_per_op;
#if NEW_DYNAREC == NEW_DYNAREC_ARM
    cp0->new_dynarec_hot_state = new_dynarec_hot_state;
#endif
}

void poweron_cp0(struct cp0* cp0)
{
    uint32_t* cp0_regs;
    unsigned int* cp0_next_interrupt;

    cp0_regs = r4300_cp0_regs(cp0);
    cp0_next_interrupt = r4300_cp0_next_interrupt();

    memset(cp0_regs, 0, CP0_REGS_COUNT * sizeof(cp0_regs[0]));
    cp0_regs[CP0_RANDOM_REG] = UINT32_C(31);
    cp0_regs[CP0_STATUS_REG]= UINT32_C(0x34000000);
    cp0_regs[CP0_CONFIG_REG]= UINT32_C(0x6e463);
    cp0_regs[CP0_PREVID_REG] = UINT32_C(0xb00);
    cp0_regs[CP0_COUNT_REG] = UINT32_C(0x5000);
    cp0_regs[CP0_CAUSE_REG] = UINT32_C(0x5c);
    cp0_regs[CP0_CONTEXT_REG] = UINT32_C(0x7ffff0);
    cp0_regs[CP0_EPC_REG] = UINT32_C(0xffffffff);
    cp0_regs[CP0_BADVADDR_REG] = UINT32_C(0xffffffff);
    cp0_regs[CP0_ERROREPC_REG] = UINT32_C(0xffffffff);

    /* XXX: clarify what is done on poweron, in soft_reset and in execute... */
    cp0->interrupt_unsafe_state = 0;
    *cp0_next_interrupt = 0;
    cp0->special_done = 0;
    cp0->last_addr = UINT32_C(0xbfc00000);

    poweron_tlb(&cp0->tlb);
}


uint32_t* r4300_cp0_regs(struct cp0* cp0)
{
#if NEW_DYNAREC != NEW_DYNAREC_ARM
    return cp0->regs;
#else
/* ARM dynarec uses a different memory layout */
    return cp0->new_dynarec_hot_state->cp0_regs;
#endif
}

uint32_t* r4300_cp0_last_addr(void)
{
    return &g_dev.r4300.cp0.last_addr;
}

unsigned int* r4300_cp0_next_interrupt(void)
{
#if NEW_DYNAREC != NEW_DYNAREC_ARM
    return &g_dev.r4300.cp0.next_interrupt;
#else
/* ARM dynarec uses a different memory layout */
    return &g_dev_r4300_cp0_next_interrupt;
#endif
}


int check_cop1_unusable(struct r4300_core* r4300)
{
    uint32_t* cp0_regs = r4300_cp0_regs(&r4300->cp0);

    if (!(cp0_regs[CP0_STATUS_REG] & CP0_STATUS_CU1))
    {
        cp0_regs[CP0_CAUSE_REG] = CP0_CAUSE_EXCCODE_CPU | CP0_CAUSE_CE1;
        exception_general(r4300);
        return 1;
    }
    return 0;
}

void cp0_update_count(void)
{
    uint32_t* cp0_regs = r4300_cp0_regs(&g_dev.r4300.cp0);

#ifdef NEW_DYNAREC
    if (g_dev.r4300.emumode != EMUMODE_DYNAREC)
    {
#endif
        cp0_regs[CP0_COUNT_REG] += ((*r4300_pc(&g_dev.r4300) - g_dev.r4300.cp0.last_addr) >> 2) * g_dev.r4300.cp0.count_per_op;
        g_dev.r4300.cp0.last_addr = *r4300_pc(&g_dev.r4300);
#ifdef NEW_DYNAREC
    }
#endif

#ifdef COMPARE_CORE
   if (g_dev.r4300.delay_slot)
     CoreCompareCallback();
#endif
/*#ifdef DBG
   if (g_DebuggerActive && !g_dev.r4300.delay_slot) update_debugger(*r4300_pc(&g_dev.r4300));
#endif
*/
}

