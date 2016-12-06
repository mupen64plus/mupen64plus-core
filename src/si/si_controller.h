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

#ifndef SI_SI_CONTROLLER_H
#define SI_SI_CONTROLLER_H

#include <stdint.h>

#include "pif.h"

struct r4300_core;
struct ri_controller;
struct controller_input_backend;
struct rumble_backend;
struct storage_backend;

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

    struct pif pif;

    struct r4300_core* r4300;
    struct ri_controller* ri;
};

static uint32_t si_reg(uint32_t address)
{
    return (address & 0xffff) >> 2;
}


void init_si(struct si_controller* si,
             struct controller_input_backend* cins,
             uint8_t* mpk_data[],
             struct storage_backend* mpk_storages,
             struct rumble_backend* rumbles,
             uint8_t* eeprom_data,
             size_t eeprom_size,
             uint16_t eeprom_id,
             struct storage_backend* eeprom_storage,
             struct clock_backend* rtc,
             const uint8_t* ipl3,
             struct r4300_core* r4300,
             struct ri_controller* ri);

void poweron_si(struct si_controller* si);

int read_si_regs(void* opaque, uint32_t address, uint32_t* value);
int write_si_regs(void* opaque, uint32_t address, uint32_t value, uint32_t mask);

void si_end_of_dma_event(struct si_controller* si);

#endif
