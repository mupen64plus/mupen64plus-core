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

#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))


static void read_open_bus(void* opaque, uint32_t address, uint32_t* value)
{
    *value = (address & 0xffff);
    *value |= (*value << 16);
}

static void write_open_bus(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
}


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
    struct storage_backend* mpk_storages,
    struct rumble_backend* rumbles,
    struct gb_cart* gb_carts,
    uint16_t eeprom_id, struct storage_backend* eeprom_storage,
    struct clock_backend* clock,
    /* vi */
    unsigned int vi_clock, unsigned int expected_refresh_rate)
{
    struct interrupt_handler interrupt_handlers[] = {
        { &dev->vi,        vi_vertical_interrupt_event }, /* VI */
        { &dev->r4300,     compare_int_handler         }, /* COMPARE */
        { &dev->r4300,     check_int_handler           }, /* CHECK */
        { &dev->si,        si_end_of_dma_event         }, /* SI */
        { &dev->pi,        pi_end_of_dma_event         }, /* PI */
        { &dev->r4300.cp0, special_int_handler         }, /* SPECIAL */
        { &dev->ai,        ai_end_of_dma_event         }, /* AI */
        { &dev->sp,        rsp_interrupt_event         }, /* SP */
        { &dev->dp,        rdp_interrupt_event         }, /* DP */
        { &dev->r4300,     hw2_int_handler             }, /* HW2 */
        { dev,             nmi_int_handler             }, /* NMI */
        { dev,             reset_hard_handler          }  /* reset_hard */
    };

    uint32_t ram_end = 0x00000000 + dram_size - 1;
    uint32_t rom_end = 0x10000000 + rom_size - 1;

#define R(x) read_ ## x
#define W(x) write_ ## x
#define RW(x) R(x), W(x)
    struct mem_mapping mappings[] = {
        /* clear mappings */
        { 0x00000000, 0xffffffff,  M64P_MEM_NOTHING,      { NULL,        RW(open_bus)         } },
        /* memory map */
        { 0x00000000, ram_end,     M64P_MEM_RDRAM,        { &dev->ri,    RW(rdram_dram)       } },
        { 0x03f00000, 0x03f0ffff,  M64P_MEM_RDRAMREG,     { &dev->ri,    RW(rdram_regs)       } },
        { 0x04000000, 0x0400ffff,  M64P_MEM_RSPMEM,       { &dev->sp,    RW(rsp_mem)          } },
        { 0x04040000, 0x0404ffff,  M64P_MEM_RSPREG,       { &dev->sp,    RW(rsp_regs)         } },
        { 0x04080000, 0x0408ffff,  M64P_MEM_RSP,          { &dev->sp,    RW(rsp_regs2)        } },
        { 0x04100000, 0x0410ffff,  M64P_MEM_DP,           { &dev->dp,    RW(dpc_regs)         } },
        { 0x04200000, 0x0420ffff,  M64P_MEM_DPS,          { &dev->dp,    RW(dps_regs)         } },
        { 0x04300000, 0x0430ffff,  M64P_MEM_MI,           { &dev->r4300, RW(mi_regs)          } },
        { 0x04400000, 0x0440ffff,  M64P_MEM_VI,           { &dev->vi,    RW(vi_regs)          } },
        { 0x04500000, 0x0450ffff,  M64P_MEM_AI,           { &dev->ai,    RW(ai_regs)          } },
        { 0x04600000, 0x0460ffff,  M64P_MEM_PI,           { &dev->pi,    RW(pi_regs)          } },
        { 0x04700000, 0x0470ffff,  M64P_MEM_RI,           { &dev->ri,    RW(ri_regs)          } },
        { 0x04800000, 0x0480ffff,  M64P_MEM_SI,           { &dev->si,    RW(si_regs)          } },
        { 0x08000000, 0x0800ffff,  M64P_MEM_FLASHRAMSTAT, { &dev->pi,    RW(flashram_status)  } },
        { 0x08010000, 0x0801ffff,  M64P_MEM_NOTHING,      { &dev->pi,    RW(flashram_command) } },
        { 0x10000000, rom_end,     M64P_MEM_ROM,          { &dev->pi,    RW(cart_rom)         } },
        { 0x1fc00000, 0x1fc0ffff,  M64P_MEM_PIF,          { &dev->si,    RW(pif_ram)          } }
    };
#undef R
#undef W
#undef RW

    init_memory(&dev->mem, mappings, ARRAY_SIZE(mappings), base);
    init_r4300(&dev->r4300, &dev->mem, &dev->ri, interrupt_handlers,
            emumode, count_per_op, no_compiled_jump, special_rom);
    init_rdp(&dev->dp, &dev->r4300, &dev->sp, &dev->ri);
    init_rsp(&dev->sp, (uint32_t*)((uint8_t*)base + 0x04000000), &dev->r4300, &dev->dp, &dev->ri);
    init_ai(&dev->ai, &dev->r4300, &dev->ri, &dev->vi, aout);
    init_pi(&dev->pi, (uint8_t*)base + 0x10000000, rom_size, flashram_storage, sram_storage, &dev->r4300, &dev->ri, &dev->si.pif.cic);
    init_ri(&dev->ri, (uint32_t*)((uint8_t*)base + 0x00000000), dram_size);
    init_si(&dev->si,
        (uint8_t*)base + 0x1fc00000,
        pif_channel_devices,
        cins,
        mpk_storages,
        rumbles,
        gb_carts,
        eeprom_id, eeprom_storage,
        clock,
        (uint8_t*)base + 0x10000040,
        &dev->r4300, &dev->ri);
    init_vi(&dev->vi, vi_clock, expected_refresh_rate, &dev->r4300);
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

    /* XXX: somewhat cheating to put it here but not really other option.
     * Proper fix would probably trigerring the first vi
     * when VI_CONTROL_REG[1:0] is set to non zero value */
    add_interrupt_event_count(&dev->r4300.cp0, VI_INT, dev->vi.next_vi);
}

void run_device(struct device* dev)
{
    /* device execution is driven by the r4300 */
    run_r4300(&dev->r4300);
}

void stop_device(struct device* dev)
{
    /* set stop flag so that r4300 execution will be stopped at next interrupt */
    *r4300_stop(&dev->r4300) = 1;
}

void hard_reset_device(struct device* dev)
{
    /* set reset hard flag so reset_hard will be called at next interrupt */
    dev->r4300.reset_hard_job = 1;
}

void soft_reset_device(struct device* dev)
{
    /* schedule HW2 interrupt now and an NMI after 1/2 seconds */
    add_interrupt_event(&dev->r4300.cp0, HW2_INT, 0);
    add_interrupt_event(&dev->r4300.cp0, NMI_INT, 50000000);
}
