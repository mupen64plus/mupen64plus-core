/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - si_controller.h                                         *
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

#ifndef M64P_DEVICE_SI_SI_CONTROLLER_H
#define M64P_DEVICE_SI_SI_CONTROLLER_H

#include <stdint.h>

#include "pif.h"

struct mi_controller;
struct r4300_core;
struct ri_controller;
struct joybus_device_interface;

enum si_dma_dir
{
    SI_NO_DMA,
    SI_DMA_READ,
    SI_DMA_WRITE
};

enum si_registers
{
    SI_DRAM_ADDR_REG,
    SI_PIF_ADDR_RD64B_REG,
    SI_R2_REG, /* reserved */
    SI_R3_REG, /* reserved */
    SI_PIF_ADDR_WR64B_REG,
    SI_R5_REG, /* reserved */
    SI_STATUS_REG,
    SI_REGS_COUNT
};

struct si_controller
{
    uint32_t regs[SI_REGS_COUNT];
    unsigned char dma_dir;

    struct pif pif;

    struct mi_controller* mi;
    struct ri_controller* ri;

};

static uint32_t si_reg(uint32_t address)
{
    return (address & 0xffff) >> 2;
}


void init_si(struct si_controller* si,
             uint8_t* pif_base,
             void* jbds[PIF_CHANNELS_COUNT],
             const struct joybus_device_interface* ijbds[PIF_CHANNELS_COUNT],
             const uint8_t* ipl3,
             struct mi_controller* mi,
             struct r4300_core* r4300,
             struct ri_controller* ri);

void poweron_si(struct si_controller* si);

void read_si_regs(void* opaque, uint32_t address, uint32_t* value);
void write_si_regs(void* opaque, uint32_t address, uint32_t value, uint32_t mask);

void si_end_of_dma_event(void* opaque);

#endif
