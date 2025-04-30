/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - ri_controller.h                                         *
 *   Mupen64Plus homepage: https://mupen64plus.org/                        *
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

#ifndef M64P_DEVICE_RCP_RI_RI_CONTROLLER_H
#define M64P_DEVICE_RCP_RI_RI_CONTROLLER_H

#include <stddef.h>
#include <stdint.h>

#include "osal/preproc.h"

struct rdram;

enum ri_registers
{
    RI_MODE_REG,
    RI_CONFIG_REG,
    RI_CURRENT_LOAD_REG,
    RI_SELECT_REG,
    RI_REFRESH_REG,
    RI_LATENCY_REG,
    RI_ERROR_REG,
    RI_WERROR_REG,
    RI_REGS_COUNT
};

struct ri_controller
{
    uint32_t regs[RI_REGS_COUNT];

    struct rdram* rdram;
};

static osal_inline uint32_t ri_reg(uint32_t address)
{
    return (address & 0x1f) >> 2;
}

static osal_inline uint32_t ri_address(uint32_t address)
{
    /* https://n64brew.dev/wiki/RDRAM_Interface#Memory_addressing */
    return (((address >> 20) == 0x03f)
        ? (((address & 0x3ff)))              | /* Adr[10:0]  */
           (((address >> 10) & 0x1ff) << 11) | /* Adr[19:11] */
           (((address >> 10) & 0x1ff) << 20)   /* Adr[28:20] */
        : (((address & 0x7ff)))              | /* Adr[10:0]  */
           (((address >> 11) & 0x1ff) << 11) | /* Adr[19:11] */
           (((address >> 20) & 0x03f) << 20)); /* Adr[28:20] */
}

static osal_inline uint16_t ri_address_to_id_field(uint32_t address, uint8_t swapfield)
{
    /* https://n64brew.dev/wiki/RDRAM#RDRAM_addressing */
    return (((swapfield  & ((address >> 11) & 0x1ff))) 
          | ((~swapfield & ((address >> 20) & 0x1ff)))); /* AdrS[28:20] */
}



void init_ri(struct ri_controller* ri, struct rdram* rdram);

void poweron_ri(struct ri_controller* ri);

void read_ri_regs(void* opaque, uint32_t address, uint32_t* value);
void write_ri_regs(void* opaque, uint32_t address, uint32_t value, uint32_t mask);

#endif
