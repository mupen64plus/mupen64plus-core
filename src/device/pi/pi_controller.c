/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - pi_controller.c                                         *
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

#include "pi_controller.h"

#define M64P_CORE_PROTOTYPES 1
#include <stdint.h>
#include <string.h>

#include "api/callbacks.h"
#include "api/m64p_types.h"
#include "device/memory/memory.h"
#include "device/r4300/r4300_core.h"
#include "device/ri/rdram_detection_hack.h"
#include "device/ri/ri_controller.h"

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

enum
{
    /* PI_STATUS - read */
    PI_STATUS_DMA_BUSY  = 0x01,
    PI_STATUS_IO_BUSY   = 0x02,
    PI_STATUS_ERROR     = 0x04,

    /* PI_STATUS - write */
    PI_STATUS_RESET     = 0x01,
    PI_STATUS_CLR_INTR  = 0x02
};

static void dma_pi_read(struct pi_controller* pi)
{
    /* XXX: end of domain is wrong ? */
    if (pi->regs[PI_CART_ADDR_REG] >= 0x08000000 && pi->regs[PI_CART_ADDR_REG] < 0x08010000)
    {
        if (pi->use_flashram != 1)
        {
            dma_write_sram(pi);
            pi->use_flashram = -1;
        }
        else
        {
            dma_write_flashram(pi);
        }
    }
    else
    {
        DebugMessage(M64MSG_WARNING, "Unknown dma read in dma_pi_read()");
    }

    /* Mark DMA as busy */
    pi->regs[PI_STATUS_REG] |= PI_STATUS_DMA_BUSY;

    /* schedule end of dma interrupt event */
    cp0_update_count(pi->r4300);
    add_interrupt_event(&pi->r4300->cp0, PI_INT, 0x1000/*pi->regs[PI_RD_LEN_REG]*/); /* XXX: 0x1000 ??? */
}

static void dma_pi_write(struct pi_controller* pi)
{
    unsigned int longueur, i;
    uint32_t dram_address;
    uint32_t rom_address;
    uint8_t* dram;
    const uint8_t* rom;

    if (pi->regs[PI_CART_ADDR_REG] < 0x10000000)
    {
        /* XXX: end of domain is wrong ? */
        if (pi->regs[PI_CART_ADDR_REG] >= 0x08000000 && pi->regs[PI_CART_ADDR_REG] < 0x08010000)
        {
            if (pi->use_flashram != 1)
            {
                dma_read_sram(pi);
                pi->use_flashram = -1;
            }
            else
            {
                dma_read_flashram(pi);
            }
        }
        else if (pi->regs[PI_CART_ADDR_REG] >= 0x06000000 && pi->regs[PI_CART_ADDR_REG] < 0x08000000)
        {
        }
        else
        {
            DebugMessage(M64MSG_WARNING, "Unknown dma write 0x%" PRIX32 " in dma_pi_write()", pi->regs[PI_CART_ADDR_REG]);
        }

        /* mark DMA as busy */
        pi->regs[PI_STATUS_REG] |= PI_STATUS_DMA_BUSY;

        /* schedule end of dma interrupt event */
        cp0_update_count(pi->r4300);
        add_interrupt_event(&pi->r4300->cp0, PI_INT, /*pi->regs[PI_WR_LEN_REG]*/0x1000); /* XXX: 0x1000 ??? */

        return;
    }

    longueur = (pi->regs[PI_WR_LEN_REG] & 0xFFFFFE)+2;
    dram_address = pi->regs[PI_DRAM_ADDR_REG] & 0x7FFFFF;
    rom_address = (pi->regs[PI_CART_ADDR_REG] - 0x10000000) & 0x3ffffff;
    dram = (uint8_t*)pi->ri->rdram.dram;
    rom = pi->cart_rom.rom;

    if (rom_address + longueur < pi->cart_rom.rom_size)
    {
        for(i = 0; i < longueur; ++i)
        {
            dram[(dram_address+i)^S8] = rom[(rom_address+i)^S8];
        }
    }
    else
    {
        int32_t diff = pi->cart_rom.rom_size - rom_address;
        if (diff < 0) diff = 0;

        for (i = 0; i < diff; ++i)
        {
            dram[(dram_address+i)^S8] = rom[(rom_address+i)^S8];
        }
        for (; i < longueur; ++i)
        {
            dram[(dram_address+i)^S8] = 0;
        }
    }

    invalidate_r4300_cached_code(pi->r4300, 0x80000000 + dram_address, longueur);
    invalidate_r4300_cached_code(pi->r4300, 0xa0000000 + dram_address, longueur);

    /* HACK: monitor PI DMA to trigger RDRAM size detection
     * hack just before initial cart ROM loading. */
    if (pi->regs[PI_CART_ADDR_REG] == 0x10001000)
    {
        force_detected_rdram_size_hack(&pi->ri->rdram, pi->cic);
    }

    /* mark both DMA and IO as busy */
    pi->regs[PI_STATUS_REG] |=
        PI_STATUS_DMA_BUSY | PI_STATUS_IO_BUSY;

    /* schedule end of dma interrupt event */
    cp0_update_count(pi->r4300);
    add_interrupt_event(&pi->r4300->cp0, PI_INT, longueur/8);
}


void init_pi(struct pi_controller* pi,
             uint8_t* rom, size_t rom_size,
             struct storage_backend* flashram_storage,
             struct storage_backend* sram_storage,
             struct r4300_core* r4300,
             struct ri_controller* ri,
             const struct cic* cic)
{
    init_cart_rom(&pi->cart_rom, rom, rom_size);
    init_flashram(&pi->flashram, flashram_storage);
    init_sram(&pi->sram, sram_storage);

    pi->use_flashram = 0;

    pi->r4300 = r4300;
    pi->ri = ri;

    pi->cic = cic;
}

void poweron_pi(struct pi_controller* pi)
{
    memset(pi->regs, 0, PI_REGS_COUNT*sizeof(uint32_t));

    poweron_cart_rom(&pi->cart_rom);
    poweron_flashram(&pi->flashram);
}

int read_pi_regs(void* opaque, uint32_t address, uint32_t* value)
{
    struct pi_controller* pi = (struct pi_controller*)opaque;
    uint32_t reg = pi_reg(address);

    *value = pi->regs[reg];

    return 0;
}

int write_pi_regs(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
    struct pi_controller* pi = (struct pi_controller*)opaque;
    uint32_t reg = pi_reg(address);

    switch (reg)
    {
    case PI_RD_LEN_REG:
        masked_write(&pi->regs[PI_RD_LEN_REG], value, mask);
        dma_pi_read(pi);
        return 0;

    case PI_WR_LEN_REG:
        masked_write(&pi->regs[PI_WR_LEN_REG], value, mask);
        dma_pi_write(pi);
        return 0;

    case PI_STATUS_REG:
        if (value & mask & 2)
            clear_rcp_interrupt(pi->r4300, MI_INTR_PI);
        return 0;

    case PI_BSD_DOM1_LAT_REG:
    case PI_BSD_DOM1_PWD_REG:
    case PI_BSD_DOM1_PGS_REG:
    case PI_BSD_DOM1_RLS_REG:
    case PI_BSD_DOM2_LAT_REG:
    case PI_BSD_DOM2_PWD_REG:
    case PI_BSD_DOM2_PGS_REG:
    case PI_BSD_DOM2_RLS_REG:
        masked_write(&pi->regs[reg], value & 0xff, mask);
        return 0;
    }

    masked_write(&pi->regs[reg], value, mask);

    return 0;
}

void pi_end_of_dma_event(void* opaque)
{
    struct pi_controller* pi = (struct pi_controller*)opaque;
    pi->regs[PI_STATUS_REG] &= ~(PI_STATUS_DMA_BUSY | PI_STATUS_IO_BUSY);
    raise_rcp_interrupt(pi->r4300, MI_INTR_PI);
}
