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

#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))

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

    const struct mem_handler* handler = &r4300->mem->saved_handlers[address >> 16];

    handler->read32(handler->opaque, address, value);
}

static void write32_with_bp_checks(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
    struct r4300_core* r4300 = (struct r4300_core*)opaque;

    check_breakpoints_on_mem_access(*r4300_pc(r4300)-0x4, address, 4,
            M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_WRITE);

    const struct mem_handler* handler = &r4300->mem->saved_handlers[address >> 16];

    handler->write32(handler->opaque, address, value, mask);
}

void activate_memory_break_read(struct memory* mem, uint32_t address)
{
    uint16_t region = address >> 16;
    struct mem_handler* handler = &mem->handlers[region];
    struct mem_handler* saved_handler = &mem->saved_handlers[region];

    if (saved_handler->read32 == NULL)
    {
        /* only change opaque value if memory_break_write is not active */
        if (saved_handler->write32 == NULL) {
            saved_handler->opaque = handler->opaque;
            handler->opaque = &g_dev.r4300;
        }

        saved_handler->read32 = handler->read32;
        handler->read32 = read32_with_bp_checks;
    }
}

void deactivate_memory_break_read(struct memory* mem, uint32_t address)
{
    uint16_t region = address >> 16;
    struct mem_handler* handler = &mem->handlers[region];
    struct mem_handler* saved_handler = &mem->saved_handlers[region];

    if (saved_handler->read32 != NULL)
    {
        /* only restore opaque value if memory_break_write is not active */
        if (saved_handler->write32 == NULL) {
            handler->opaque = saved_handler->opaque;
            saved_handler->opaque = NULL;
        }

        handler->read32 = saved_handler->read32;
        saved_handler->read32 = NULL;
    }
}

void activate_memory_break_write(struct memory* mem, uint32_t address)
{
    uint16_t region = address >> 16;
    struct mem_handler* handler = &mem->handlers[region];
    struct mem_handler* saved_handler = &mem->saved_handlers[region];

    if (saved_handler->write32 == NULL)
    {
        /* only change opaque value if memory_break_read is not active */
        if (saved_handler->read32 == NULL) {
            saved_handler->opaque = handler->opaque;
            handler->opaque = &g_dev.r4300;
        }

        saved_handler->write32 = handler->write32;
        handler->write32 = write32_with_bp_checks;
    }
}

void deactivate_memory_break_write(struct memory* mem, uint32_t address)
{
    uint16_t region = address >> 16;
    struct mem_handler* handler = &mem->handlers[region];
    struct mem_handler* saved_handler = &mem->saved_handlers[region];

    if (saved_handler->write32 != NULL)
    {
        /* only restore opaque value if memory_break_read is not active */
        if (saved_handler->read32 == NULL) {
            handler->opaque = saved_handler->opaque;
            saved_handler->opaque = NULL;
        }

        handler->write32 = saved_handler->write32;
        saved_handler->write32 = NULL;
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
    size_t i, m;
    uint16_t ram_end = 0x0000 + (g_dev.ri.rdram.dram_size >> 16) - 1;
    uint16_t rom_end = 0x1000 + (g_dev.pi.cart_rom.rom_size >> 16) - 1;

    struct
    {
        uint16_t begin;
        uint16_t end;
        int type;
        struct mem_handler handler;
    } mappings[] =
    {
        /* clear mappings */
        { 0x0000, 0xffff,  M64P_MEM_NOTHING,      { NULL,         RW(open_bus)          } },
        /* memory map */
        { 0x0000, ram_end, M64P_MEM_RDRAM,        { &g_dev.ri,    RW(rdram_dram)       } },
        { 0x03f0, 0x03f0,  M64P_MEM_RDRAMREG,     { &g_dev.ri,    RW(rdram_regs)       } },
        { 0x0400, 0x0400,  M64P_MEM_RSPMEM,       { &g_dev.sp,    RW(rsp_mem)          } },
        { 0x0404, 0x0404,  M64P_MEM_RSPREG,       { &g_dev.sp,    RW(rsp_regs)         } },
        { 0x0408, 0x0408,  M64P_MEM_RSP,          { &g_dev.sp,    RW(rsp_regs2)        } },
        { 0x0410, 0x0410,  M64P_MEM_DP,           { &g_dev.dp,    RW(dpc_regs)         } },
        { 0x0420, 0x0420,  M64P_MEM_DPS,          { &g_dev.dp,    RW(dps_regs)         } },
        { 0x0430, 0x0430,  M64P_MEM_MI,           { &g_dev.r4300, RW(mi_regs)          } },
        { 0x0440, 0x0440,  M64P_MEM_VI,           { &g_dev.vi,    RW(vi_regs)          } },
        { 0x0450, 0x0450,  M64P_MEM_AI,           { &g_dev.ai,    RW(ai_regs)          } },
        { 0x0460, 0x0460,  M64P_MEM_PI,           { &g_dev.pi,    RW(pi_regs)          } },
        { 0x0470, 0x0470,  M64P_MEM_RI,           { &g_dev.ri,    RW(ri_regs)          } },
        { 0x0480, 0x0480,  M64P_MEM_SI,           { &g_dev.si,    RW(si_regs)          } },
        { 0x0800, 0x0800,  M64P_MEM_FLASHRAMSTAT, { &g_dev.pi,    RW(flashram_status)  } },
        { 0x0801, 0x0801,  M64P_MEM_NOTHING,      { &g_dev.pi,    RW(flashram_command) } },
        { 0x1000, rom_end, M64P_MEM_ROM,          { &g_dev.pi,    RW(cart_rom)         } },
        { 0x1fc0, 0x1fc0,  M64P_MEM_PIF,          { &g_dev.si,    RW(pif_ram)          } }
    };

#ifdef DBG
    memset(mem->saved_handlers, 0, 0x10000*sizeof(mem->saved_handlers[0]));
#endif

    for(m = 0; m < ARRAY_SIZE(mappings); ++m) {
        for(i = mappings[m].begin; i <= mappings[m].end; ++i) {
            map_region(mem, i, mappings[m].type, &mappings[m].handler);
        }
    }
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
        mem->saved_handlers[region].opaque = opaque;
        mem->handlers[region].opaque = &g_dev.r4300;
    }
    else
#endif
    {
        mem->handlers[region].opaque = opaque;
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
        mem->saved_handlers[region].read32 = read32;
        mem->handlers[region].read32 = read32_with_bp_checks;
    }
    else
#endif
    {
        mem->handlers[region].read32 = read32;
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
        mem->saved_handlers[region].write32 = write32;
        mem->handlers[region].write32 = write32_with_bp_checks;
    }
    else
#endif
    {
        mem->handlers[region].write32 = write32;
    }
}

void map_region(struct memory* mem,
                uint16_t region,
                int type,
                const struct mem_handler* handler)
{
    map_region_t(mem, region, type);
    map_region_o(mem, region, handler->opaque);
    map_region_r(mem, region, handler->read32);
    map_region_w(mem, region, handler->write32);
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
