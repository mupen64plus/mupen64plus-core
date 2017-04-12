/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - cp1.c                                                   *
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
#include "cp1.h"

#include "new_dynarec/new_dynarec.h" /* for NEW_DYNAREC_ARM */

#include "main/main.h"

extern float* g_dev_r4300_cp1_regs_simple[32];
extern double* g_dev_r4300_cp1_regs_double[32];
extern uint32_t g_dev_r4300_cp1_fcr0;
extern uint32_t g_dev_r4300_cp1_fcr31;

void poweron_cp1(struct cp1* cp1)
{
    memset(cp1->regs, 0, 32 * sizeof(cp1->regs[0]));
    *r4300_cp1_fcr0() = UINT32_C(0x511);
    *r4300_cp1_fcr31() = 0;

    set_fpr_pointers(UINT32_C(0x34000000)); /* c0_status value at poweron */
    update_x86_rounding_mode(*r4300_cp1_fcr31());
}


int64_t* r4300_cp1_regs(struct cp1* cp1)
{
    return cp1->regs;
}

float** r4300_cp1_regs_simple(void)
{
#if NEW_DYNAREC != NEW_DYNAREC_ARM
/* ARM dynarec uses a different memory layout */
    return g_dev.r4300.cp1.regs_simple;
#else
    return g_dev_r4300_cp1_regs_simple;
#endif
}

double** r4300_cp1_regs_double(void)
{
#if NEW_DYNAREC != NEW_DYNAREC_ARM
/* ARM dynarec uses a different memory layout */
    return g_dev.r4300.cp1.regs_double;
#else
    return g_dev_r4300_cp1_regs_double;
#endif
}

uint32_t* r4300_cp1_fcr0(void)
{
#if NEW_DYNAREC != NEW_DYNAREC_ARM
/* ARM dynarec uses a different memory layout */
    return &g_dev.r4300.cp1.fcr0;
#else
    return &g_dev_r4300_cp1_fcr0;
#endif
}

uint32_t* r4300_cp1_fcr31(void)
{
#if NEW_DYNAREC != NEW_DYNAREC_ARM
/* ARM dynarec uses a different memory layout */
    return &g_dev.r4300.cp1.fcr31;
#else
    return &g_dev_r4300_cp1_fcr31;
#endif
}



/* Refer to Figure 6-2 on page 155 and explanation on page B-11
   of MIPS R4000 Microprocessor User's Manual (Second Edition)
   by Joe Heinrich.
*/
void shuffle_fpr_data(uint32_t oldStatus, uint32_t newStatus)
{
#if defined(M64P_BIG_ENDIAN)
    const int isBigEndian = 1;
#else
    const int isBigEndian = 0;
#endif

    if ((newStatus & CP0_STATUS_FR) != (oldStatus & CP0_STATUS_FR))
    {
        int i;
        int32_t temp_fgr_32[32];

        // pack or unpack the FGR register data
        if (newStatus & CP0_STATUS_FR)
        {   // switching into 64-bit mode
            // retrieve 32 FPR values from packed 32-bit FGR registers
            for (i = 0; i < 32; i++)
            {
                temp_fgr_32[i] = *((int32_t *) &g_dev.r4300.cp1.regs[i>>1] + ((i & 1) ^ isBigEndian));
            }
            // unpack them into 32 64-bit registers, taking the high 32-bits from their temporary place in the upper 16 FGRs
            for (i = 0; i < 32; i++)
            {
                int32_t high32 = *((int32_t *) &g_dev.r4300.cp1.regs[(i>>1)+16] + (i & 1));
                *((int32_t *) &g_dev.r4300.cp1.regs[i] + isBigEndian)     = temp_fgr_32[i];
                *((int32_t *) &g_dev.r4300.cp1.regs[i] + (isBigEndian^1)) = high32;
            }
        }
        else
        {   // switching into 32-bit mode
            // retrieve the high 32 bits from each 64-bit FGR register and store in temp array
            for (i = 0; i < 32; i++)
            {
                temp_fgr_32[i] = *((int32_t *) &g_dev.r4300.cp1.regs[i] + (isBigEndian^1));
            }
            // take the low 32 bits from each register and pack them together into 64-bit pairs
            for (i = 0; i < 16; i++)
            {
                uint32_t least32 = *((uint32_t *) &g_dev.r4300.cp1.regs[i*2] + isBigEndian);
                uint32_t most32 = *((uint32_t *) &g_dev.r4300.cp1.regs[i*2+1] + isBigEndian);
                g_dev.r4300.cp1.regs[i] = ((uint64_t) most32 << 32) | (uint64_t) least32;
            }
            // store the high bits in the upper 16 FGRs, which wont be accessible in 32-bit mode
            for (i = 0; i < 32; i++)
            {
                *((int32_t *) &g_dev.r4300.cp1.regs[(i>>1)+16] + (i & 1)) = temp_fgr_32[i];
            }
        }
    }
}

void set_fpr_pointers(uint32_t newStatus)
{
    int i;
#if defined(M64P_BIG_ENDIAN)
    const int isBigEndian = 1;
#else
    const int isBigEndian = 0;
#endif

    // update the FPR register pointers
    if (newStatus & CP0_STATUS_FR)
    {
        for (i = 0; i < 32; i++)
        {
            (r4300_cp1_regs_double())[i] = (double*) &g_dev.r4300.cp1.regs[i];
            (r4300_cp1_regs_simple())[i] = ((float*) &g_dev.r4300.cp1.regs[i]) + isBigEndian;
        }
    }
    else
    {
        for (i = 0; i < 32; i++)
        {
            (r4300_cp1_regs_double())[i] = (double*) &g_dev.r4300.cp1.regs[i>>1];
            (r4300_cp1_regs_simple())[i] = ((float*) &g_dev.r4300.cp1.regs[i>>1]) + ((i & 1) ^ isBigEndian);
        }
    }
}

/* XXX: This shouldn't really be here, but rounding_mode is used by the
 * Hacktarux JIT and updated by CTC1 and saved states. Figure out a better
 * place for this. */
void update_x86_rounding_mode(uint32_t fcr31)
{
    switch (fcr31 & 3)
    {
    case 0: /* Round to nearest, or to even if equidistant */
        g_dev.r4300.cp1.rounding_mode = UINT32_C(0x33F);
        break;
    case 1: /* Truncate (toward 0) */
        g_dev.r4300.cp1.rounding_mode = UINT32_C(0xF3F);
        break;
    case 2: /* Round up (toward +Inf) */
        g_dev.r4300.cp1.rounding_mode = UINT32_C(0xB3F);
        break;
    case 3: /* Round down (toward -Inf) */
        g_dev.r4300.cp1.rounding_mode = UINT32_C(0x73F);
        break;
    }
}
