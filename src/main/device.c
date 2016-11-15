/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - device.c                                                *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2016 Bobby Smiles                                       *
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

#include "device.h"

#include "ai/ai_controller.h"
#include "memory/memory.h"
#include "pi/pi_controller.h"
#include "r4300/r4300_core.h"
#include "rdp/rdp_core.h"
#include "ri/ri_controller.h"
#include "rsp/rsp_core.h"
#include "si/si_controller.h"
#include "vi/vi_controller.h"


void init_device(struct device* dev,
    /* ai */
    struct audio_out_backend* aout,
    /* pi */
    uint8_t* rom, size_t rom_size,
    void* flashram_user_data, void (*flashram_save)(void*), uint8_t* flashram_data,
    void* sram_user_data, void (*sram_save)(void*), uint8_t* sram_data,
    /* ri */
    uint32_t* dram, size_t dram_size,
    /* si */
    struct controller_input_backend* cins,
    void* mpk_user_data[], void (*mpk_save[])(void*), uint8_t* mpk_data[],
    void* rpk_user_data[], void (*rpk_rumble[])(void*,enum rumble_action),
    void* eeprom_user_data, void (*eeprom_save)(void*), uint8_t* eeprom_data, size_t eeprom_size, uint16_t eeeprom_id,
    struct clock_backend* rtc,
    /* vi */
    unsigned int vi_clock, unsigned int expected_refresh_rate, unsigned int count_per_scanline, unsigned int alternate_timing)
{
    init_rdp(&dev->dp, &dev->r4300, &dev->sp, &dev->ri);
    init_rsp(&dev->sp, &dev->r4300, &dev->dp, &dev->ri);
    init_ai(&dev->ai, &dev->r4300, &dev->ri, &dev->vi, aout);
    init_pi(&dev->pi, rom, rom_size, flashram_user_data, flashram_save, flashram_data, sram_user_data, sram_save, sram_data, &dev->r4300, &dev->ri);
    init_ri(&dev->ri, dram, dram_size);
    init_si(&dev->si,
        cins,
        mpk_user_data, mpk_save, mpk_data,
        rpk_user_data, rpk_rumble,
        eeprom_user_data, eeprom_save, eeprom_data, eeprom_size, eeeprom_id,
        rtc,
        rom + 0x40,
        &dev->r4300, &dev->ri);
    init_vi(&dev->vi, vi_clock, expected_refresh_rate, count_per_scanline, alternate_timing, &dev->r4300);
}

void poweron_device(struct device* dev)
{
    poweron_r4300(&dev->r4300);
    poweron_rdp(&dev->dp);
    poweron_rsp(&dev->sp);
    poweron_ai(&dev->ai);
    poweron_pi(&dev->pi);
    poweron_ri(&dev->ri);
    poweron_si(&dev->si);
    poweron_vi(&dev->vi);
    poweron_memory();
}
