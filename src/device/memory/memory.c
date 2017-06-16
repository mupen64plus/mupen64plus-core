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

#ifdef DBG
#include <string.h>

#include "device/r4300/r4300_core.h"

#include "debugger/dbg_breakpoints.h"
#include "debugger/dbg_memory.h"
#include "debugger/dbg_types.h"
#endif

#include <stddef.h>
#include <stdint.h>

#ifdef DBG
enum
{
    BP_CHECK_READ  = 0x1,
    BP_CHECK_WRITE = 0x2,
};

void read_with_bp_checks(void* opaque, uint32_t address, uint32_t* value)
{
    struct r4300_core* r4300 = (struct r4300_core*)opaque;
    uint16_t region = address >> 16;

    /* only check bp if active */
    if (r4300->mem->bp_checks[region] & BP_CHECK_READ) {
        check_breakpoints_on_mem_access(*r4300_pc(r4300)-0x4, address, 4,
                M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_READ);
    }

    mem_read32(&r4300->mem->saved_handlers[region], address, value);
}

void write_with_bp_checks(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
    struct r4300_core* r4300 = (struct r4300_core*)opaque;
    uint16_t region = address >> 16;

    /* only check bp if active */
    if (r4300->mem->bp_checks[region] & BP_CHECK_WRITE) {
        check_breakpoints_on_mem_access(*r4300_pc(r4300)-0x4, address, 4,
                M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_WRITE);
    }

    mem_write32(&r4300->mem->saved_handlers[region], address, value, mask);
}

void activate_memory_break_read(struct memory* mem, uint32_t address)
{
    uint16_t region = address >> 16;
    struct mem_handler* dbg_handler = &mem->dbg_handler;
    struct mem_handler* handler = &mem->handlers[region];
    struct mem_handler* saved_handler = &mem->saved_handlers[region];
    unsigned char* bp_check = &mem->bp_checks[region];

    /* if neither read nor write bp is active, set dbg_handler */
    if (!(*bp_check & (BP_CHECK_READ | BP_CHECK_WRITE))) {
        *saved_handler = *handler;
        *handler = *dbg_handler;
    }

    /* activate bp read */
    *bp_check |= BP_CHECK_READ;
}

void deactivate_memory_break_read(struct memory* mem, uint32_t address)
{
    uint16_t region = address >> 16;
    struct mem_handler* handler = &mem->handlers[region];
    struct mem_handler* saved_handler = &mem->saved_handlers[region];
    unsigned char* bp_check = &mem->bp_checks[region];

    /* desactivate bp read */
    *bp_check &= ~BP_CHECK_READ;

    /* if neither read nor write bp is active, restore handler */
    if (!(*bp_check & (BP_CHECK_READ | BP_CHECK_WRITE))) {
        *handler = *saved_handler;
    }
}

void activate_memory_break_write(struct memory* mem, uint32_t address)
{
    uint16_t region = address >> 16;
    struct mem_handler* dbg_handler = &mem->dbg_handler;
    struct mem_handler* handler = &mem->handlers[region];
    struct mem_handler* saved_handler = &mem->saved_handlers[region];
    unsigned char* bp_check = &mem->bp_checks[region];

    /* if neither read nor write bp is active, set dbg_handler */
    if (!(*bp_check & (BP_CHECK_READ | BP_CHECK_WRITE))) {
        *saved_handler = *handler;
        *handler = *dbg_handler;
    }

    /* activate bp write */
    *bp_check |= BP_CHECK_WRITE;
}

void deactivate_memory_break_write(struct memory* mem, uint32_t address)
{
    uint16_t region = address >> 16;
    struct mem_handler* handler = &mem->handlers[region];
    struct mem_handler* saved_handler = &mem->saved_handlers[region];
    unsigned char* bp_check = &mem->bp_checks[region];

    /* desactivate bp write */
    *bp_check &= ~BP_CHECK_WRITE;

    /* if neither read nor write bp is active, restore handler */
    if (!(*bp_check & (BP_CHECK_READ | BP_CHECK_WRITE))) {
        *handler = *saved_handler;
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
    memset(mem->bp_checks, 0, 0x10000*sizeof(mem->bp_checks[0]));
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

void map_region(struct memory* mem,
                uint16_t region,
                int type,
                const struct mem_handler* handler)
{
#ifdef DBG
    /* set region type */
    mem->memtype[region] = type;

    /* set handler */
    if (lookup_breakpoint(((uint32_t)region << 16), 0x10000,
                          M64P_BKP_FLAG_ENABLED) != -1)
    {
        mem->saved_handlers[region] = *handler;
        mem->handlers[region] = mem->dbg_handler;
    }
    else
#endif
    {
        (void)type;
        mem->handlers[region] = *handler;
    }
}
