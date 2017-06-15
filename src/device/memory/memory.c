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
#include "device/r4300/r4300_core.h"

#ifdef DBG
#include <string.h>

#include "debugger/dbg_breakpoints.h"
#include "debugger/dbg_memory.h"
#include "debugger/dbg_types.h"
#endif

#include <stddef.h>
#include <stdint.h>

#ifdef DBG
void read_with_bp_checks(void* opaque, uint32_t address, uint32_t* value)
{
    struct r4300_core* r4300 = (struct r4300_core*)opaque;

    check_breakpoints_on_mem_access(*r4300_pc(r4300)-0x4, address, 4,
            M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_READ);

    mem_read32(&r4300->mem->saved_handlers[address >> 16], address, value);
}

void write_with_bp_checks(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
    struct r4300_core* r4300 = (struct r4300_core*)opaque;

    check_breakpoints_on_mem_access(*r4300_pc(r4300)-0x4, address, 4,
            M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_WRITE);

    mem_write32(&r4300->mem->saved_handlers[address >> 16], address, value, mask);
}

void activate_memory_break_read(struct memory* mem, uint32_t address)
{
    uint16_t region = address >> 16;
    struct mem_handler* dbg_handler = &mem->dbg_handler;
    struct mem_handler* handler = &mem->handlers[region];
    struct mem_handler* saved_handler = &mem->saved_handlers[region];

    if (saved_handler->read32 == NULL)
    {
        /* only change opaque value if memory_break_write is not active */
        if (saved_handler->write32 == NULL) {
            saved_handler->opaque = handler->opaque;
            handler->opaque = dbg_handler->opaque;
        }

        saved_handler->read32 = handler->read32;
        handler->read32 = dbg_handler->read32;
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
    struct mem_handler* dbg_handler = &mem->dbg_handler;
    struct mem_handler* handler = &mem->handlers[region];
    struct mem_handler* saved_handler = &mem->saved_handlers[region];

    if (saved_handler->write32 == NULL)
    {
        /* only change opaque value if memory_break_read is not active */
        if (saved_handler->read32 == NULL) {
            saved_handler->opaque = handler->opaque;
            handler->opaque = dbg_handler->opaque;
        }

        saved_handler->write32 = handler->write32;
        handler->write32 = dbg_handler->write32;
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
#else
void read_with_bp_checks(void* opaque, uint32_t address, uint32_t* value)
{
}

void write_with_bp_checks(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
}
#endif

void init_memory(struct memory* mem,
                 struct mem_mapping* mappings, size_t mappings_count,
                 void* base,
                 struct mem_handler* dbg_handler)
{
    size_t i, m;

#ifdef DBG
    memset(mem->saved_handlers, 0, 0x10000*sizeof(mem->saved_handlers[0]));
    memcpy(&mem->dbg_handler, dbg_handler, sizeof(*dbg_handler));
#endif

    mem->base = base;

    for(m = 0; m < mappings_count; ++m) {
        uint16_t begin = mappings[m].begin >> 16;
        uint16_t end   = mappings[m].end   >> 16;

        for(i = begin; i <= end; ++i) {
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
        mem->handlers[region].opaque = mem->dbg_handler.opaque;
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
        mem->handlers[region].read32 = mem->dbg_handler.read32;
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
        mem->handlers[region].write32 = mem->dbg_handler.write32;
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
