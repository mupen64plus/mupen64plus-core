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

#ifndef M64P_DEVICE_DEVICE_H
#define M64P_DEVICE_DEVICE_H

#include <stddef.h>
#include <stdint.h>

#include "ai/ai_controller.h"
#include "memory/memory.h"
#include "pi/pi_controller.h"
#include "r4300/r4300_core.h"
#include "rdp/rdp_core.h"
#include "ri/ri_controller.h"
#include "rsp/rsp_core.h"
#include "si/si_controller.h"
#include "si/mempak.h"
#include "si/rumblepak.h"
#include "si/transferpak.h"
#include "vi/vi_controller.h"

struct audio_out_backend;
struct controller_input_backend;
struct clock_backend;
struct rumble_backend;
struct storage_backend;
struct pak_interface;

/* memory map constants */
#define MM_RDRAM_DRAM       UINT32_C(0x00000000)
#define MM_RDRAM_REGS       UINT32_C(0x03f00000)
#define MM_RSP_MEM          UINT32_C(0x04000000)
#define MM_RSP_REGS         UINT32_C(0x04040000)
#define MM_RSP_REGS2        UINT32_C(0x04080000)
#define MM_DPC_REGS         UINT32_C(0x04100000)
#define MM_DPS_REGS         UINT32_C(0x04200000)
#define MM_MI_REGS          UINT32_C(0x04300000)
#define MM_VI_REGS          UINT32_C(0x04400000)
#define MM_AI_REGS          UINT32_C(0x04500000)
#define MM_PI_REGS          UINT32_C(0x04600000)
#define MM_RI_REGS          UINT32_C(0x04700000)
#define MM_SI_REGS          UINT32_C(0x04800000)
#define MM_DD_REGS          UINT32_C(0x05000000) /* dom2 addr1 */
#define MM_DD_ROM           UINT32_C(0x06000000) /* dom1 addr1 */
#define MM_FLASHRAM_STATUS  UINT32_C(0x08000000) /* dom2 addr2 */
#define MM_FLASHRAM_COMMAND UINT32_C(0x08010000)
#define MM_CART_ROM         UINT32_C(0x10000000) /* dom1 addr2 */
#define MM_PIF_MEM          UINT32_C(0x1fc00000)

/* Device structure is a container for the n64 submodules
 * It contains all state related to the emulated system. */
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
    struct memory mem;

    struct mempak mempaks[GAME_CONTROLLERS_COUNT];
    struct rumblepak rumblepaks[GAME_CONTROLLERS_COUNT];
    struct transferpak transferpaks[GAME_CONTROLLERS_COUNT];
};

/* Setup device "static" properties.  */
void init_device(struct device* dev,
    /* memory */
    void* base,
    /* r4300 */
    unsigned int emumode,
    unsigned int count_per_op,
    int no_compiled_jump,
    int special_rom,
    /* ai */
    struct audio_out_backend* aout,
    /* pi */
    size_t rom_size,
    struct storage_backend* flashram_storage,
    struct storage_backend* sram_storage,
    /* ri */
    size_t dram_size,
    /* si */
    const struct pif_channel_device* pif_channel_devices,
    struct controller_input_backend* cins,
    void* paks[], const struct pak_interface* ipaks[],
    uint16_t eeprom_id, struct storage_backend* eeprom_storage,
    struct clock_backend* clock,
    /* vi */
    unsigned int vi_clock, unsigned int expected_refresh_rate);

/* Setup device such that it's state is
 * what it should be after power on.
 */
void poweron_device(struct device* dev);

/* Let device run.
 * To return from this function, a call to stop_device has to be made.
 */
void run_device(struct device* dev);

/* Terminate execution of running device.
 */
void stop_device(struct device* dev);

/* Schedule a hard reset on running device.
 * This is what model a poweroff/poweron action on the device.
 */
void hard_reset_device(struct device* dev);

/* Schedule a soft reset on runnning device.
 * This is what model a press on the device reset button.
 */
void soft_reset_device(struct device* dev);

#endif
