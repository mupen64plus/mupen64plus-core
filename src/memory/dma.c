/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - dma.c                                                   *
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

#include "dma.h"
#include "memory.h"
#include "pif.h"

#include "api/m64p_types.h"
#include "api/callbacks.h"
#include "main/main.h"
#include "r4300/r4300.h"
#include "r4300/r4300_core.h"
#include "r4300/interupt.h"
#include "r4300/cp0.h"
#include "si/si_controller.h"


int delay_si = 0;

void dma_si_write(void)
{
    int i;

    if (g_si.regs[SI_PIF_ADDR_WR64B_REG] != 0x1FC007C0)
    {
        DebugMessage(M64MSG_ERROR, "dma_si_write(): unknown SI use");
        stop=1;
    }

    for (i = 0; i < PIF_RAM_SIZE; i += 4)
    {
        *((uint32_t*)(&g_pif_ram[i])) = sl(g_rdram[(g_si.regs[SI_DRAM_ADDR_REG]+i)/4]);
    }

    update_pif_write();
    update_count();

    if (delay_si) {
        add_interupt_event(SI_INT, /*0x100*/0x900);
    } else {
        g_r4300.mi.regs[MI_INTR_REG] |= 0x02; // SI
        g_si.regs[SI_STATUS_REG] |= 0x1000; // INTERRUPT
        check_interupt();
    }
}

void dma_si_read(void)
{
    int i;

    if (g_si.regs[SI_PIF_ADDR_RD64B_REG] != 0x1FC007C0)
    {
        DebugMessage(M64MSG_ERROR, "dma_si_read(): unknown SI use");
        stop=1;
    }

    update_pif_read();

    for (i = 0; i < PIF_RAM_SIZE; i += 4)
    {
        g_rdram[(g_si.regs[SI_DRAM_ADDR_REG]+i)/4] = sl(*(uint32_t*)(&g_pif_ram[i]));
    }

    update_count();

    if (delay_si) {
        add_interupt_event(SI_INT, /*0x100*/0x900);
    } else {
        g_r4300.mi.regs[MI_INTR_REG] |= 0x02; // SI
        g_si.regs[SI_STATUS_REG] |= 0x1000; // INTERRUPT
        check_interupt();
    }
}

