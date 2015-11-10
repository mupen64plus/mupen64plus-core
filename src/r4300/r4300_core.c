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
#include "trace_jit/mips-interp.h"
#include "r4300.h"
#include "recomp.h"

void init_r4300(struct r4300_core* r4300)
{
    init_mi(&r4300->mi);
}

int64_t* r4300_regs(void)
{
    return g_state.regs.gpr;
}

int64_t* r4300_mult_hi(void)
{
    return &g_state.regs.hi;
}

int64_t* r4300_mult_lo(void)
{
    return &g_state.regs.lo;
}

unsigned int* r4300_llbit(void)
{
    return &g_state.regs.ll_bit;
}

uint32_t* r4300_pc(void)
{
#ifdef NEW_DYNAREC
    return (r4300emu == CORE_DYNAREC)
        ? (uint32_t*)&pcaddr
        : &PC->addr;
#else
    return &PC->addr;
#endif
}

uint32_t* r4300_last_addr(void)
{
    return &last_addr;
}

unsigned int* r4300_next_interrupt(void)
{
    return &g_state.next_interrupt;
}

unsigned int get_r4300_emumode(void)
{
    return r4300emu;
}



void invalidate_r4300_cached_code(uint32_t address, size_t size)
{
    if (r4300emu != CORE_PURE_INTERPRETER)
    {
#if 0
#ifdef NEW_DYNAREC
        if (r4300emu == CORE_DYNAREC)
        {
            invalidate_cached_code_new_dynarec(address, size);
        }
        else
#endif
        {
            invalidate_cached_code_hacktarux(address, size);
        }
#else
		invalidate_cached_code_trace_jit(address, size);
#endif
    }
}

/* XXX: not really a good interface but it gets the job done... */
void savestates_load_set_pc(uint32_t pc)
{
#ifdef NEW_DYNAREC
    if (r4300emu == CORE_DYNAREC)
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
