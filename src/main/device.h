/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - device.h                                                *
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

#ifndef M64P_MAIN_DEVICE_H
#define M64P_MAIN_DEVICE_H

#include <stddef.h>
#include <stdint.h>

#include "ai/ai_controller.h"
#include "pi/pi_controller.h"
#include "r4300/r4300_core.h"
#include "rdp/rdp_core.h"
#include "ri/ri_controller.h"
#include "rsp/rsp_core.h"
#include "si/si_controller.h"
#include "vi/vi_controller.h"

/* Device structure is a container for the n64 submodules
 * FIXME: should also include memory submodule, but not possible atm
 * because of new_dynarec.
 */
struct device
{
    struct r4300_core r4300;
    struct rdp_core dp;
    struct rsp_core sp;
    struct ai_controller ai;
    struct pi_controller pi;
    struct ri_controller ri;
    struct si_controller si;
    struct vi_controller vi;
};

/* Setup device "static" properties.  */
void init_device(struct device* dev,
    /* ai */
    void * ai_user_data, void (*ai_set_audio_format)(void*,unsigned int, unsigned int), void (*ai_push_audio_samples)(void*,const void*,size_t),
    /* pi */
    uint8_t* rom, size_t rom_size,
    void* flashram_user_data, void (*flashram_save)(void*), uint8_t* flashram_data,
    void* sram_user_data, void (*sram_save)(void*), uint8_t* sram_data,
    /* ri */
    uint32_t* dram, size_t dram_size,
    /* si */
    void* cont_user_data[], int (*cont_is_connected[])(void*,enum pak_type*), uint32_t (*cont_get_input[])(void*),
    void* mpk_user_data[], void (*mpk_save[])(void*), uint8_t* mpk_data[],
    void* rpk_user_data[], void (*rpk_rumble[])(void*,enum rumble_action),
    void* eeprom_user_data, void (*eeprom_save)(void*), uint8_t* eeprom_data, size_t eeprom_size, uint16_t eeeprom_id,
    void* af_rtc_user_data, const struct tm* (*af_rtc_get_time)(void*),
    /* vi */
    unsigned int vi_clock, unsigned int expected_refresh_rate, unsigned int count_per_scanline, unsigned int alternate_timing);

/* Setup device such that it's state is
 * what it should be after power on.
 */
void poweron_device(struct device* dev);

#endif
