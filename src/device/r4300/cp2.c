/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - cp2.c                                                   *
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

#include <stdint.h>
#include <string.h>

#include "cp0.h"
#include "cp1.h"
#include "cp2.h"

#include "new_dynarec/new_dynarec.h"

#define FCR31_FS_BIT UINT32_C(0x1000000)

void init_cp2(struct cp2* cp2, struct new_dynarec_hot_state* new_dynarec_hot_state)
{
#ifdef NEW_DYNAREC
    cp2->new_dynarec_hot_state = new_dynarec_hot_state;
#endif
}

void poweron_cp2(struct cp2* cp2)
{
    memset(cp2->regs, 0, 32 * sizeof(cp2->regs[0]));
    *r4300_cp2_fcr0(cp2) = UINT32_C(0x511);
    *r4300_cp2_fcr31(cp2) = 0;

    set_cp2_fpr_pointers(cp2, UINT32_C(0x34000000)); /* c0_status value at poweron */
#ifdef OSAL_SSE
    cp2->flush_mode = _MM_GET_FLUSH_ZERO_MODE();
#endif
    update_x86_rounding_mode((struct cp1*)cp2);
}


cp2_reg* r4300_cp2_regs(struct cp2* cp2)
{
    return cp2->regs;
}

float** r4300_cp2_regs_simple(struct cp2* cp2)
{
#ifndef NEW_DYNAREC
    /* New dynarec uses a different memory layout */
    return cp2->regs_simple;
#else
    return cp2->new_dynarec_hot_state->cp2_regs_simple;
#endif
}

double** r4300_cp2_regs_double(struct cp2* cp2)
{
#ifndef NEW_DYNAREC
    /* New dynarec uses a different memory layout */
    return cp2->regs_double;
#else
    return cp2->new_dynarec_hot_state->cp2_regs_double;
#endif
}

uint32_t* r4300_cp2_fcr0(struct cp2* cp2)
{
#ifndef NEW_DYNAREC
    /* New dynarec uses a different memory layout */
    return &cp2->fcr0;
#else
    return &cp2->new_dynarec_hot_state->cp2_fcr31;
#endif
}

uint32_t* r4300_cp2_fcr31(struct cp2* cp2)
{
#ifndef NEW_DYNAREC
    /* New dynarec uses a different memory layout */
    return &cp2->fcr31;
#else
    return &cp2->new_dynarec_hot_state->cp2_fcr31;
#endif
}

void set_cp2_fpr_pointers(struct cp2* cp2, uint32_t newStatus)
{
    int i;

    // update the FPR register pointers
    if ((newStatus & CP0_STATUS_FR) == 0)
    {
        for (i = 0; i < 32; i++)
        {
            (r4300_cp2_regs_simple(cp2))[i] = &cp2->regs[i & ~1].float32[i & 1];
            (r4300_cp2_regs_double(cp2))[i] = &cp2->regs[i & ~1].float64;
        }
    }
    else
    {
        for (i = 0; i < 32; i++)
        {
            (r4300_cp2_regs_simple(cp2))[i] = &cp2->regs[i].float32[0];
            (r4300_cp2_regs_double(cp2))[i] = &cp2->regs[i].float64;
        }
    }
}

