/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - r4300_core.c                                            *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2014 Bobby Smiles                                       *
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

#include "r4300_core.h"

#include "cached_interp.h"
#include "mi_controller.h"
#include "new_dynarec/new_dynarec.h"
#include "r4300.h"
#include "recomp.h"
#include "main/main.h"

#include <string.h>

void init_r4300(struct r4300_core* r4300, unsigned int emumode, unsigned int count_per_op, int no_compiled_jump)
{
    r4300->emumode = emumode;
    init_cp0(&r4300->cp0, count_per_op);

    r4300->recomp.no_compiled_jump = no_compiled_jump;
}

void poweron_r4300(struct r4300_core* r4300)
{
    /* clear registers */
    memset(r4300->regs, 0, 32*sizeof(r4300->regs[0]));
    r4300->hi = 0;
    r4300->lo = 0;
    r4300->llbit = 0;

    r4300->pc = NULL;
    r4300->delay_slot = 0;
    r4300->local_rs = 0;
    r4300->skip_jump = 0;
    r4300->dyna_interp = 0;
    //r4300->current_instruction_table;
    r4300->reset_hard_job = 0;

    r4300->jumps_table = NULL;
    r4300->jumps_number = 0;
    r4300->max_jumps_number = 0;
    r4300->jump_start8 = 0;
    r4300->jump_start32 = 0;
#if defined(__x86_64__)
    r4300->riprel_table = NULL;
    r4300->riprel_number = 0;
    r4300->max_riprel_number = 0;
#endif

#if defined(__x86_64__)
    r4300->save_rsp = 0;
    r4300->save_rip = 0;
#else
    r4300->save_ebp = 0;
    r4300->save_ebx = 0;
    r4300->save_esi = 0;
    r4300->save_edi = 0;
    r4300->save_esp = 0;
    r4300->save_eip = 0;
#endif

    /* recomp init */
    r4300->recomp.fast_memory = 1;
    r4300->recomp.delay_slot_compiled = 0;

    r4300->branch_taken = 0;

    /* setup CP0 registers */
    poweron_cp0(&r4300->cp0);

    /* setup CP1 registers */
    poweron_cp1(&r4300->cp1);

    /* setup mi */
    poweron_mi(&r4300->mi);
}

int64_t* r4300_regs(void)
{
    return g_dev.r4300.regs;
}

int64_t* r4300_mult_hi(void)
{
    return &g_dev.r4300.hi;
}

int64_t* r4300_mult_lo(void)
{
    return &g_dev.r4300.lo;
}

unsigned int* r4300_llbit(void)
{
    return &g_dev.r4300.llbit;
}

uint32_t* r4300_pc(void)
{
#ifdef NEW_DYNAREC
    return (g_dev.r4300.emumode == EMUMODE_DYNAREC)
        ? (uint32_t*)&pcaddr
        : &g_dev.r4300.pc->addr;
#else
    return &g_dev.r4300.pc->addr;
#endif
}

unsigned int get_r4300_emumode(void)
{
    return g_dev.r4300.emumode;
}



void invalidate_r4300_cached_code(uint32_t address, size_t size)
{
    if (g_dev.r4300.emumode != EMUMODE_PURE_INTERPRETER)
    {
#ifdef NEW_DYNAREC
        if (g_dev.r4300.emumode == EMUMODE_DYNAREC)
        {
            invalidate_cached_code_new_dynarec(address, size);
        }
        else
#endif
        {
            invalidate_cached_code_hacktarux(address, size);
        }
    }
}

/* XXX: not really a good interface but it gets the job done... */
void savestates_load_set_pc(uint32_t pc)
{
#ifdef NEW_DYNAREC
    if (g_dev.r4300.emumode == EMUMODE_DYNAREC)
    {
        pcaddr = pc;
        pending_exception = 1;
        invalidate_all_pages();
    }
    else
#endif
    {
        generic_jump_to(pc);
        invalidate_r4300_cached_code(0,0);
    }
}
