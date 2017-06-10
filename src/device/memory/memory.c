/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - memory.c                                                *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2002 Hacktarux                                          *
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

#include "memory.h"

#include "api/callbacks.h"
#include "api/m64p_types.h"
#include "device/ai/ai_controller.h"
#include "device/pi/pi_controller.h"
#include "device/r4300/new_dynarec/new_dynarec.h" /* for NEW_DYNAREC_ARM */
#include "device/r4300/r4300_core.h"
#include "device/rdp/rdp_core.h"
#include "device/ri/ri_controller.h"
#include "device/rsp/rsp_core.h"
#include "device/si/si_controller.h"
#include "device/vi/vi_controller.h"
#include "main/main.h"

#ifdef DBG
#include <string.h>

#include "debugger/dbg_breakpoints.h"
#include "debugger/dbg_memory.h"
#include "debugger/dbg_types.h"
#endif

#include <stddef.h>
#include <stdint.h>

static void read_open_bus(void* opaque, uint32_t address, uint32_t* value)
{
    *value = address & 0xffff;
    *value |= (*value << 16);
}

static void write_open_bus(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
}

#ifdef DBG
static void read32_with_bp_checks(void* opaque, uint32_t address, uint32_t* value)
{
    struct r4300_core* r4300 = (struct r4300_core*)opaque;

    check_breakpoints_on_mem_access(*r4300_pc(r4300)-0x4, address, 4,
            M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_READ);

    uint16_t region = address >> 16;
    r4300->mem->saved_read32[region](r4300->mem->saved_opaque[region], address, value);
}

static void write32_with_bp_checks(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
    struct r4300_core* r4300 = (struct r4300_core*)opaque;

    check_breakpoints_on_mem_access(*r4300_pc(r4300)-0x4, address, 4,
            M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_WRITE);

    uint16_t region = address >> 16;
    r4300->mem->saved_write32[region](r4300->mem->saved_opaque[region], address, value, mask);
}

void activate_memory_break_read(struct memory* mem, uint32_t address)
{
    uint16_t region = address >> 16;

    if (mem->saved_read32[region] == NULL)
    {
        /* only change opaque value if memory_break_write is not active */
        if (mem->saved_write32[region] == NULL) {
            mem->saved_opaque[region] = mem->opaque[region];
            mem->opaque[region] = &g_dev.r4300;
        }

        mem->saved_read32[region] = mem->read32[region];
        mem->read32[region] = read32_with_bp_checks;
    }
}

void deactivate_memory_break_read(struct memory* mem, uint32_t address)
{
    uint16_t region = address >> 16;

    if (mem->saved_read32[region] != NULL)
    {
        /* only restore opaque value if memory_break_write is not active */
        if (mem->saved_write32[region] == NULL) {
            mem->opaque[region] = mem->saved_opaque[region];
            mem->saved_opaque[region] = NULL;
        }

        mem->read32[region] = mem->saved_read32[region];
        mem->saved_read32[region] = NULL;
    }
}

void activate_memory_break_write(struct memory* mem, uint32_t address)
{
    uint16_t region = address >> 16;

    if (mem->saved_write32[region] == NULL)
    {
        /* only change opaque value if memory_break_read is not active */
        if (mem->saved_read32[region] == NULL) {
            mem->saved_opaque[region] = mem->opaque[region];
            mem->opaque[region] = &g_dev.r4300;
        }

        mem->saved_write32[region] = mem->write32[region];
        mem->write32[region] = write32_with_bp_checks;
    }
}

void deactivate_memory_break_write(struct memory* mem, uint32_t address)
{
    uint16_t region = address >> 16;

    if (mem->saved_write32[region] != NULL)
    {
        /* only restore opaque value if memory_break_read is not active */
        if (mem->saved_read32[region] == NULL) {
            mem->opaque[region] = mem->saved_opaque[region];
            mem->saved_opaque[region] = NULL;
        }

        mem->write32[region] = mem->saved_write32[region];
        mem->saved_write32[region] = NULL;
    }
}

int get_memory_type(struct memory* mem, uint32_t address)
{
    return mem->memtype[address >> 16];
}
#endif

#define R(x) read_ ## x
#define W(x) write_ ## x
#define RW(x) R(x), W(x)

void poweron_memory(struct memory* mem)
{
    int i;

#ifdef DBG
    memset(mem->saved_opaque, 0, 0x10000*sizeof(mem->saved_opaque[0]));
    memset(mem->saved_read32, 0, 0x10000*sizeof(mem->saved_read32[0]));
    memset(mem->saved_write32, 0, 0x10000*sizeof(mem->saved_write32[0]));
#endif

    /* clear mappings */
    for(i = 0; i < 0x10000; ++i)
    {
        map_region(mem, i, M64P_MEM_NOTHING, NULL, RW(open_bus));
    }

    /* map RDRAM */
    for(i = 0; i < /*0x40*/0x80; ++i)
    {
        map_region(mem, 0x0000+i, M64P_MEM_RDRAM, &g_dev.ri, RW(rdram_dram));
    }

    /* map RDRAM registers */
    map_region(mem, 0x03f0, M64P_MEM_RDRAMREG, &g_dev.ri, RW(rdram_regs));

    /* map RSP memory */
    map_region(mem, 0x0400, M64P_MEM_RSPMEM, &g_dev.sp, RW(rsp_mem));

    /* map RSP registers (1) */
    map_region(mem, 0x0404, M64P_MEM_RSPREG, &g_dev.sp, RW(rsp_regs));

    /* map RSP registers (2) */
    map_region(mem, 0x0408, M64P_MEM_RSP, &g_dev.sp, RW(rsp_regs2));

    /* map DPC registers */
    map_region(mem, 0x0410, M64P_MEM_DP, &g_dev.dp, RW(dpc_regs));

    /* map DPS registers */
    map_region(mem, 0x0420, M64P_MEM_DPS, &g_dev.dp, RW(dps_regs));

    /* map MI registers */
    map_region(mem, 0x0430, M64P_MEM_MI, &g_dev.r4300, RW(mi_regs));

    /* map VI registers */
    map_region(mem, 0x0440, M64P_MEM_VI, &g_dev.vi, RW(vi_regs));

    /* map AI registers */
    map_region(mem, 0x0450, M64P_MEM_AI, &g_dev.ai, RW(ai_regs));

    /* map PI registers */
    map_region(mem, 0x0460, M64P_MEM_PI, &g_dev.pi, RW(pi_regs));

    /* map RI registers */
    map_region(mem, 0x0470, M64P_MEM_RI, &g_dev.ri, RW(ri_regs));

    /* map SI registers */
    map_region(mem, 0x0480, M64P_MEM_SI, &g_dev.si, RW(si_regs));

    /* map flashram/sram */
    map_region(mem, 0x0800, M64P_MEM_FLASHRAMSTAT, &g_dev.pi, RW(flashram_status));
    map_region(mem, 0x0801, M64P_MEM_NOTHING, &g_dev.pi, RW(flashram_command));

    /* map cart ROM */
    for(i = 0; i < (g_dev.pi.cart_rom.rom_size >> 16); ++i)
    {
        map_region(mem, 0x1000+i, M64P_MEM_ROM, &g_dev.pi, RW(cart_rom));
    }

    /* map PIF RAM */
    map_region(mem, 0x1fc0, M64P_MEM_PIF, &g_dev.si, RW(pif_ram));
}

static void map_region_t(struct memory* mem, uint16_t region, int type)
{
#ifdef DBG
    mem->memtype[region] = type;
#else
    (void)region;
    (void)type;
#endif
}

static void map_region_o(struct memory* mem,
        uint16_t region,
        void* opaque)
{
#ifdef DBG
    if (lookup_breakpoint(((uint32_t)region << 16), 0x10000,
                          M64P_BKP_FLAG_ENABLED) != -1)
    {
        mem->saved_opaque[region] = opaque;
        mem->opaque[region] = &g_dev.r4300;
    }
    else
#endif
    {
        mem->opaque[region] = opaque;
    }
}

static void map_region_r(struct memory* mem,
        uint16_t region,
        read32fn read32)
{
#ifdef DBG
    if (lookup_breakpoint(((uint32_t)region << 16), 0x10000,
                          M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_READ) != -1)
    {
        mem->saved_read32[region] = read32;
        mem->read32[region] = read32_with_bp_checks;
    }
    else
#endif
    {
        mem->read32[region] = read32;
    }
}

static void map_region_w(struct memory* mem,
        uint16_t region,
        write32fn write32)
{
#ifdef DBG
    if (lookup_breakpoint(((uint32_t)region << 16), 0x10000,
                          M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_WRITE) != -1)
    {
        mem->saved_write32[region] = write32;
        mem->write32[region] = write32_with_bp_checks;
    }
    else
#endif
    {
        mem->write32[region] = write32;
    }
}

void map_region(struct memory* mem,
                uint16_t region,
                int type,
                void* opaque,
                read32fn read32,
                write32fn write32)
{
    map_region_t(mem, region, type);
    map_region_o(mem, region, opaque);
    map_region_r(mem, region, read32);
    map_region_w(mem, region, write32);
}

uint32_t *fast_mem_access(uint32_t address)
{
    /* This code is performance critical, specially on pure interpreter mode.
     * Removing error checking saves some time, but the emulator may crash. */

    if ((address & UINT32_C(0xc0000000)) != UINT32_C(0x80000000)) {
        address = virtual_to_physical_address(&g_dev.r4300, address, 2);
    }

    address &= UINT32_C(0x1ffffffc);

    if (address < RDRAM_MAX_SIZE)
        return (uint32_t*) ((uint8_t*) g_dev.ri.rdram.dram + address);
    else if (address >= UINT32_C(0x10000000))
        return (uint32_t*) ((uint8_t*) g_dev.pi.cart_rom.rom + (address - UINT32_C(0x10000000)));
    else if ((address & UINT32_C(0xffffe000)) == UINT32_C(0x04000000))
        return (uint32_t*) ((uint8_t*) g_dev.sp.mem + (address & UINT32_C(0x1ffc)));
    else
        return NULL;
}
