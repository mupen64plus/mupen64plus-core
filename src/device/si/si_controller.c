/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - si_controller.c                                         *
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

#include "si_controller.h"

#include <string.h>

#include "api/callbacks.h"
#include "api/m64p_types.h"
#include "device/memory/memory.h"
#include "device/r4300/r4300_core.h"
#include "device/ri/ri_controller.h"

enum
{
    /* SI_STATUS - read */
    SI_STATUS_DMA_BUSY  = 0x0001,
    SI_STATUS_RD_BUSY   = 0x0002,
    SI_STATUS_DMA_ERROR = 0x0008,
    SI_STATUS_INTERRUPT = 0x1000,
};

void init_si(struct si_controller* si,
             uint8_t* pif_base,
             void* jbds[PIF_CHANNELS_COUNT],
             const struct joybus_device_interface* ijbds[PIF_CHANNELS_COUNT],
             const uint8_t* ipl3,
             struct r4300_core* r4300,
             struct ri_controller* ri)
{
    si->r4300 = r4300;
    si->ri = ri;

    init_pif(&si->pif,
        pif_base,
        jbds, ijbds,
        ipl3,
        r4300);
}

void poweron_si(struct si_controller* si)
{
    memset(si->regs, 0, SI_REGS_COUNT*sizeof(uint32_t));
    si->dma_dir = SI_NO_DMA;

    poweron_pif(&si->pif);
}


void read_si_regs(void* opaque, uint32_t address, uint32_t* value)
{
    struct si_controller* si = (struct si_controller*)opaque;
    uint32_t reg = si_reg(address);

    *value = si->regs[reg];
}

void write_si_regs(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
    struct si_controller* si = (struct si_controller*)opaque;
    uint32_t reg = si_reg(address);

    switch (reg)
    {
    case SI_DRAM_ADDR_REG:
        masked_write(&si->regs[SI_DRAM_ADDR_REG], value, mask);
        break;

    case SI_PIF_ADDR_RD64B_REG:
    case SI_PIF_ADDR_WR64B_REG:
        masked_write(&si->regs[reg], value, mask);

        if ((si->regs[reg] & 0x1fffffff) != 0x1fc007c0) {
            DebugMessage(M64MSG_ERROR, "Unknown SI DMA PIF address: %08x", si->regs[reg]);
            return;
        }

        /* if DMA already busy, error, and ignore request */
        if (si->regs[SI_STATUS_REG] & SI_STATUS_DMA_BUSY) {
            si->regs[SI_STATUS_REG] |= SI_STATUS_DMA_ERROR;
            return;
        }

        /* rd64b (1) -> dma_read (1), wr64b (4) -> dma_write (2) */
        si->dma_dir = (reg == SI_PIF_ADDR_RD64B_REG)
            ? SI_DMA_READ
            : SI_DMA_WRITE;
            //(enum si_dma_dir)(1 + (reg >> 2));
        si->regs[SI_STATUS_REG] |= SI_STATUS_DMA_BUSY;

        cp0_update_count(si->r4300);
        add_interrupt_event(&si->r4300->cp0, SI_INT, 0x900 + add_random_interrupt_time(si->r4300));
        break;

    case SI_STATUS_REG:
        /* clear si interrupt */
        si->regs[SI_STATUS_REG] &= ~SI_STATUS_INTERRUPT;
        clear_rcp_interrupt(si->r4300, MI_INTR_SI);
        break;
    }
}

void si_end_of_dma_event(void* opaque)
{
    struct si_controller* si = (struct si_controller*)opaque;
    size_t i;
    /* DRAM address must be word-aligned */
    uint32_t dram_addr = si->regs[SI_DRAM_ADDR_REG] & UINT32_C(0xFFFFFFFC);

    uint32_t* pif_ram = (uint32_t*)si->pif.ram;
    uint32_t* dram = (uint32_t*)(&si->ri->rdram.dram[rdram_dram_address(dram_addr)]);

    /* DRAM -> PIF : copy data to PIF RAM and let start the PIF processing */
    if (si->dma_dir == SI_DMA_WRITE) {
        for(i = 0; i < (PIF_RAM_SIZE / 4); ++i) {
            pif_ram[i] = sl(dram[i]);
        }

        process_pif_ram(si);
    }
    /* PIF -> DRAM : gather results from PIF and copy to RDRAM */
    else if (si->dma_dir == SI_DMA_READ) {
        update_pif_ram(si);

        for(i = 0; i < (PIF_RAM_SIZE / 4); ++i) {
            dram[i] = sl(pif_ram[i]);
        }
    } else {
        /* shouldn't happen except potentially when loading an old savestate */
    }

    /* end DMA */
    si->dma_dir = SI_NO_DMA;
    si->regs[SI_STATUS_REG] &= ~SI_STATUS_DMA_BUSY;

    /* raise si interrupt */
    si->regs[SI_STATUS_REG] |= SI_STATUS_INTERRUPT;
    raise_rcp_interrupt(si->r4300, MI_INTR_SI);
}

