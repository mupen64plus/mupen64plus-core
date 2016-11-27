/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - tlb.c                                                   *
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

#include "tlb.h"

#include "api/m64p_types.h"
#include "exception.h"
#include "main/main.h"
#include "main/rom.h"

#include <string.h>

void poweron_tlb(struct tlb* tlb)
{
    /* clear TLB entries */
    memset(tlb->entries, 0, 32 * sizeof(tlb->entries[0]));
    memset(tlb->LUT_r, 0, 0x100000 * sizeof(tlb->LUT_r[0]));
    memset(tlb->LUT_w, 0, 0x100000 * sizeof(tlb->LUT_w[0]));
}

void tlb_unmap(struct tlb_entry* entry)
{
    unsigned int i;

    /* FIXME! avoid g_dev usage */
    struct tlb* tlb = &g_dev.r4300.cp0.tlb;

    if (entry->v_even)
    {
        for (i=entry->start_even; i<entry->end_even; i += 0x1000)
            tlb->LUT_r[i>>12] = 0;
        if (entry->d_even)
            for (i=entry->start_even; i<entry->end_even; i += 0x1000)
                tlb->LUT_w[i>>12] = 0;
    }

    if (entry->v_odd)
    {
        for (i=entry->start_odd; i<entry->end_odd; i += 0x1000)
            tlb->LUT_r[i>>12] = 0;
        if (entry->d_odd)
            for (i=entry->start_odd; i<entry->end_odd; i += 0x1000)
                tlb->LUT_w[i>>12] = 0;
    }
}

void tlb_map(struct tlb_entry* entry)
{
    unsigned int i;

    /* FIXME! avoid g_dev usage */
    struct tlb* tlb = &g_dev.r4300.cp0.tlb;

    if (entry->v_even)
    {
        if (entry->start_even < entry->end_even &&
            !(entry->start_even >= 0x80000000 && entry->end_even < 0xC0000000) &&
            entry->phys_even < 0x20000000)
        {
            for (i=entry->start_even;i<entry->end_even;i+=0x1000)
                tlb->LUT_r[i>>12] = UINT32_C(0x80000000) | (entry->phys_even + (i - entry->start_even) + 0xFFF);
            if (entry->d_even)
                for (i=entry->start_even;i<entry->end_even;i+=0x1000)
                    tlb->LUT_w[i>>12] = UINT32_C(0x80000000) | (entry->phys_even + (i - entry->start_even) + 0xFFF);
        }
    }

    if (entry->v_odd)
    {
        if (entry->start_odd < entry->end_odd &&
            !(entry->start_odd >= 0x80000000 && entry->end_odd < 0xC0000000) &&
            entry->phys_odd < 0x20000000)
        {
            for (i=entry->start_odd;i<entry->end_odd;i+=0x1000)
                tlb->LUT_r[i>>12] = UINT32_C(0x80000000) | (entry->phys_odd + (i - entry->start_odd) + 0xFFF);
            if (entry->d_odd)
                for (i=entry->start_odd;i<entry->end_odd;i+=0x1000)
                    tlb->LUT_w[i>>12] = UINT32_C(0x80000000) | (entry->phys_odd + (i - entry->start_odd) + 0xFFF);
        }
    }
}

uint32_t virtual_to_physical_address(uint32_t addresse, int w)
{
    /* FIXME! avoid g_dev usage */
    struct tlb* tlb = &g_dev.r4300.cp0.tlb;

    if (addresse >= UINT32_C(0x7f000000) && addresse < UINT32_C(0x80000000) && isGoldeneyeRom)
    {
        /**************************************************
         GoldenEye 007 hack allows for use of TLB.
         Recoded by okaygo to support all US, J, and E ROMS.
        **************************************************/
        switch (ROM_HEADER.Country_code & UINT16_C(0xFF))
        {
        case 0x45:
            // U
            return UINT32_C(0xb0034b30) + (addresse & UINT32_C(0xFFFFFF));
            break;
        case 0x4A:
            // J
            return UINT32_C(0xb0034b70) + (addresse & UINT32_C(0xFFFFFF));
            break;
        case 0x50:
            // E
            return UINT32_C(0xb00329f0) + (addresse & UINT32_C(0xFFFFFF));
            break;
        default:
            // UNKNOWN COUNTRY CODE FOR GOLDENEYE USING AMERICAN VERSION HACK
            return UINT32_C(0xb0034b30) + (addresse & UINT32_C(0xFFFFFF));
            break;
        }
    }
    if (w == 1)
    {
        if (tlb->LUT_w[addresse>>12])
            return (tlb->LUT_w[addresse>>12] & UINT32_C(0xFFFFF000)) | (addresse & UINT32_C(0xFFF));
    }
    else
    {
        if (tlb->LUT_r[addresse>>12])
            return (tlb->LUT_r[addresse>>12] & UINT32_C(0xFFFFF000)) | (addresse & UINT32_C(0xFFF));
    }
    //printf("tlb exception !!! @ %x, %x, add:%x\n", addresse, w, g_dev.r4300.pc->addr);
    //getchar();
    TLB_refill_exception(addresse,w);
    //return 0x80000000;
    return 0x00000000;
}
