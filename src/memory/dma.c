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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "dma.h"
#include "memory.h"
#include "pif.h"

#include "api/m64p_types.h"
#define M64P_CORE_PROTOTYPES 1
#include "api/m64p_config.h"
#include "api/config.h"
#include "api/callbacks.h"
#include "main/main.h"
#include "main/rom.h"
#include "main/util.h"
#include "pi/pi_controller.h"
#include "pi/sram.h"
#include "pi/flashram.h"
#include "r4300/r4300.h"
#include "r4300/r4300_core.h"
#include "r4300/cached_interp.h"
#include "r4300/interupt.h"
#include "r4300/cp0.h"
#include "r4300/ops.h"
#include "r4300/new_dynarec/new_dynarec.h"


int delay_si = 0;

void dma_pi_read(void)
{
    if (g_pi.regs[PI_CART_ADDR_REG] >= 0x08000000
            && g_pi.regs[PI_CART_ADDR_REG] < 0x08010000)
    {
        if (g_pi.use_flashram != 1)
        {
            dma_write_sram(&g_pi);
            g_pi.use_flashram = -1;
        }
        else
        {
            dma_write_flashram(&g_pi);
        }
    }
    else
    {
        DebugMessage(M64MSG_WARNING, "Unknown dma read in dma_pi_read()");
    }

    g_pi.regs[PI_STATUS_REG] |= 1;
    update_count();
    add_interupt_event(PI_INT, 0x1000/*g_pi.regs[PI_RD_LEN_REG]*/);
}

void dma_pi_write(void)
{
    unsigned int longueur;
    int i;

    if (g_pi.regs[PI_CART_ADDR_REG] < 0x10000000)
    {
        if (g_pi.regs[PI_CART_ADDR_REG] >= 0x08000000
                && g_pi.regs[PI_CART_ADDR_REG] < 0x08010000)
        {
            if (g_pi.use_flashram != 1)
            {
                dma_read_sram(&g_pi);
                g_pi.use_flashram = -1;
            }
            else
            {
                dma_read_flashram(&g_pi);
            }
        }
        else if (g_pi.regs[PI_CART_ADDR_REG] >= 0x06000000
                 && g_pi.regs[PI_CART_ADDR_REG] < 0x08000000)
        {
        }
        else
        {
            DebugMessage(M64MSG_WARNING, "Unknown dma write 0x%x in dma_pi_write()", (int)g_pi.regs[PI_CART_ADDR_REG]);
        }

        g_pi.regs[PI_STATUS_REG] |= 1;
        update_count();
        add_interupt_event(PI_INT, /*g_pi.regs[PI_WR_LEN_REG]*/0x1000);

        return;
    }

    if (g_pi.regs[PI_CART_ADDR_REG] >= 0x1fc00000) // for paper mario
    {
        g_pi.regs[PI_STATUS_REG] |= 1;
        update_count();
        add_interupt_event(PI_INT, 0x1000);

        return;
    }

    longueur = (g_pi.regs[PI_WR_LEN_REG] & 0xFFFFFF)+1;
    i = (g_pi.regs[PI_CART_ADDR_REG]-0x10000000)&0x3FFFFFF;
    longueur = (i + (int) longueur) > g_rom_size ?
               (g_rom_size - i) : longueur;
    longueur = (g_pi.regs[PI_DRAM_ADDR_REG] + longueur) > 0x7FFFFF ?
               (0x7FFFFF - g_pi.regs[PI_DRAM_ADDR_REG]) : longueur;

    if (i>g_rom_size || g_pi.regs[PI_DRAM_ADDR_REG] > 0x7FFFFF)
    {
        g_pi.regs[PI_STATUS_REG] |= 3;
        update_count();
        add_interupt_event(PI_INT, longueur/8);

        return;
    }

    if (r4300emu != CORE_PURE_INTERPRETER)
    {
        for (i=0; i<(int)longueur; i++)
        {
            unsigned long rdram_address1 = g_pi.regs[PI_DRAM_ADDR_REG]+i+0x80000000;
            unsigned long rdram_address2 = g_pi.regs[PI_DRAM_ADDR_REG]+i+0xa0000000;
            ((unsigned char*)g_rdram)[(g_pi.regs[PI_DRAM_ADDR_REG]+i)^S8]=
                g_rom[(((g_pi.regs[PI_CART_ADDR_REG]-0x10000000)&0x3FFFFFF)+i)^S8];

            if (!invalid_code[rdram_address1>>12])
            {
                if (!blocks[rdram_address1>>12] ||
                    blocks[rdram_address1>>12]->block[(rdram_address1&0xFFF)/4].ops !=
                    current_instruction_table.NOTCOMPILED)
                {
                    invalid_code[rdram_address1>>12] = 1;
                }
#ifdef NEW_DYNAREC
                invalidate_block(rdram_address1>>12);
#endif
            }
            if (!invalid_code[rdram_address2>>12])
            {
                if (!blocks[rdram_address1>>12] ||
                    blocks[rdram_address2>>12]->block[(rdram_address2&0xFFF)/4].ops !=
                    current_instruction_table.NOTCOMPILED)
                {
                    invalid_code[rdram_address2>>12] = 1;
                }
            }
        }
    }
    else
    {
        for (i=0; i<(int)longueur; i++)
        {
            ((unsigned char*)g_rdram)[(g_pi.regs[PI_DRAM_ADDR_REG]+i)^S8]=
                g_rom[(((g_pi.regs[PI_CART_ADDR_REG]-0x10000000)&0x3FFFFFF)+i)^S8];
        }
    }

    // Set the RDRAM memory size when copying main ROM code
    // (This is just a convenient way to run this code once at the beginning)
    if (g_pi.regs[PI_CART_ADDR_REG] == 0x10001000)
    {
        switch (g_cic_type)
        {
        case CIC_X101:
        case CIC_X102:
        case CIC_X103:
        case CIC_X106:
        {
            if (ConfigGetParamInt(g_CoreConfig, "DisableExtraMem"))
            {
                g_rdram[0x318/4] = 0x400000;
            }
            else
            {
                g_rdram[0x318/4] = 0x800000;
            }
            break;
        }
        case CIC_X105:
        {
            if (ConfigGetParamInt(g_CoreConfig, "DisableExtraMem"))
            {
                g_rdram[0x3F0/4] = 0x400000;
            }
            else
            {
                g_rdram[0x3F0/4] = 0x800000;
            }
            break;
        }
        }
    }

    g_pi.regs[PI_STATUS_REG] |= 3;
    update_count();
    add_interupt_event(PI_INT, longueur/8);

    return;
}

void dma_si_write(void)
{
    int i;

    if (g_si_regs[SI_PIF_ADDR_WR64B_REG] != 0x1FC007C0)
    {
        DebugMessage(M64MSG_ERROR, "dma_si_write(): unknown SI use");
        stop=1;
    }

    for (i = 0; i < PIF_RAM_SIZE; i += 4)
    {
        *((uint32_t*)(&g_pif_ram[i])) = sl(g_rdram[(g_si_regs[SI_DRAM_ADDR_REG]+i)/4]);
    }

    update_pif_write();
    update_count();

    if (delay_si) {
        add_interupt_event(SI_INT, /*0x100*/0x900);
    } else {
        g_r4300.mi.regs[MI_INTR_REG] |= 0x02; // SI
        g_si_regs[SI_STATUS_REG] |= 0x1000; // INTERRUPT
        check_interupt();
    }
}

void dma_si_read(void)
{
    int i;

    if (g_si_regs[SI_PIF_ADDR_RD64B_REG] != 0x1FC007C0)
    {
        DebugMessage(M64MSG_ERROR, "dma_si_read(): unknown SI use");
        stop=1;
    }

    update_pif_read();

    for (i = 0; i < PIF_RAM_SIZE; i += 4)
    {
        g_rdram[(g_si_regs[SI_DRAM_ADDR_REG]+i)/4] = sl(*(uint32_t*)(&g_pif_ram[i]));
    }

    update_count();

    if (delay_si) {
        add_interupt_event(SI_INT, /*0x100*/0x900);
    } else {
        g_r4300.mi.regs[MI_INTR_REG] |= 0x02; // SI
        g_si_regs[SI_STATUS_REG] |= 0x1000; // INTERRUPT
        check_interupt();
    }
}

