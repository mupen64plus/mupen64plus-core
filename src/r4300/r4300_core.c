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
#include "cp1_private.h"
#include "mi_controller.h"
#include "new_dynarec/new_dynarec.h"
#include "r4300.h"
#include "recomp.h"
#include "main/main.h"

#include <string.h>

void init_r4300(struct r4300_core* r4300, unsigned int count_per_op)
{
    init_cp0(&r4300->cp0, count_per_op);
}

void poweron_r4300(struct r4300_core* r4300)
{
    /* clear registers */
    memset(r4300->regs, 0, 32*sizeof(r4300->regs[0]));
    r4300->hi = 0;
    r4300->lo = 0;
    r4300->llbit = 0;

    r4300->delay_slot = 0;

    /* setup CP0 registers */
    poweron_cp0(&r4300->cp0);

    /* setup CP1 registers */
    memset(reg_cop1_fgr_64, 0, 32 * sizeof(reg_cop1_fgr_64[0]));
    FCR0 = UINT32_C(0x511);
    FCR31 = 0;
    set_fpr_pointers(r4300->cp0.regs[CP0_STATUS_REG]);
    update_x86_rounding_mode(FCR31);

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
    return (r4300emu == CORE_DYNAREC)
        ? (uint32_t*)&pcaddr
        : &PC->addr;
#else
    return &PC->addr;
#endif
}

unsigned int get_r4300_emumode(void)
{
    return r4300emu;
}



void invalidate_r4300_cached_code(uint32_t address, size_t size)
{
    if (r4300emu != CORE_PURE_INTERPRETER)
    {
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
