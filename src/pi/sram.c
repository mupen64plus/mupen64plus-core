/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - sram.c                                                  *
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

#include "sram.h"
#include "pi_controller.h"

#include "api/m64p_types.h"
#include "api/callbacks.h"

#include "main/main.h"
#include "main/rom.h"
#include "main/util.h"

#include "memory/memory.h"

#include "ri/ri_controller.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>


static char *get_sram_path(void)
{
    return formatstr("%s%s.sra", get_savesrampath(), ROM_SETTINGS.goodname);
}

static void sram_format(uint8_t* sram)
{
    memset(sram, 0, SRAM_SIZE);
}

static void sram_read_file(uint8_t* sram)
{
    char *filename = get_sram_path();

    sram_format(sram);
    switch (read_from_file(filename, sram, SRAM_SIZE))
    {
        case file_open_error:
            DebugMessage(M64MSG_VERBOSE, "couldn't open sram file '%s' for reading", filename);
            sram_format(sram);
            break;
        case file_read_error:
            DebugMessage(M64MSG_WARNING, "fread() failed on 32kb read from sram file '%s'", filename);
            sram_format(sram);
            break;
        default: break;
    }

    free(filename);
}

static void sram_write_file(uint8_t* sram)
{
    char *filename = get_sram_path();

    switch (write_to_file(filename, sram, SRAM_SIZE))
    {
        case file_open_error:
            DebugMessage(M64MSG_WARNING, "couldn't open sram file '%s' for writing.", filename);
            break;
        case file_write_error:
            DebugMessage(M64MSG_WARNING, "fwrite() failed on 32kb write to sram file '%s'", filename);
            break;
        default: break;
    }

    free(filename);
}


void dma_write_sram(struct pi_controller* pi)
{
    size_t i;
    size_t length = (pi->regs[PI_RD_LEN_REG] & 0xffffff) + 1;

    uint8_t* sram = pi->sram;
    uint8_t* dram = (uint8_t*)pi->ri->rdram.dram;
    uint32_t cart_addr = pi->regs[PI_CART_ADDR_REG] - 0x08000000;
    uint32_t dram_addr = pi->regs[PI_DRAM_ADDR_REG];

    sram_read_file(sram);

    for(i = 0; i < length; ++i)
        sram[(cart_addr+i)^S8] = dram[(dram_addr+i)^S8];

    sram_write_file(sram);
}

void dma_read_sram(struct pi_controller* pi)
{
    size_t i;
    size_t length = (pi->regs[PI_WR_LEN_REG] & 0xffffff) + 1;

    uint8_t* sram = pi->sram;
    uint8_t* dram = (uint8_t*)pi->ri->rdram.dram;
    uint32_t cart_addr = (pi->regs[PI_CART_ADDR_REG] - 0x08000000) & 0xffff;
    uint32_t dram_addr = pi->regs[PI_DRAM_ADDR_REG];

    sram_read_file(sram);

    for(i = 0; i < length; ++i)
        dram[(dram_addr+i)^S8] = sram[(cart_addr+i)^S8];
}

