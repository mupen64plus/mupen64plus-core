/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - memory.h                                                *
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

#ifndef M64P_DEVICE_MEMORY_MEMORY_H
#define M64P_DEVICE_MEMORY_MEMORY_H

#include <stddef.h>
#include <stdint.h>

typedef void (*read32fn)(void*,uint32_t,uint32_t*);
typedef void (*write32fn)(void*,uint32_t,uint32_t,uint32_t);

struct mem_handler
{
    void* opaque;
    read32fn read32;
    write32fn write32;
};

struct mem_mapping
{
    uint32_t begin;
    uint32_t end;       /* inclusive */
    int type;
    struct mem_handler handler;
};

struct memory
{
    struct mem_handler handlers[0x10000];
    void* base;

#ifdef DBG
    int memtype[0x10000];
    unsigned char bp_checks[0x10000];
    struct mem_handler saved_handlers[0x10000];
    struct mem_handler dbg_handler;
#endif
};

#ifndef M64P_BIG_ENDIAN
#if defined(__GNUC__) && (__GNUC__ > 4  || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3))
#define sl(x) __builtin_bswap32(x)
#else
#define sl(mot) \
( \
((mot & 0x000000FF) << 24) | \
((mot & 0x0000FF00) <<  8) | \
((mot & 0x00FF0000) >>  8) | \
((mot & 0xFF000000) >> 24) \
)
#endif
#define S8 3
#define S16 2
#define Sh16 1

#else

#define sl(mot) mot
#define S8 0
#define S16 0
#define Sh16 0

#endif

static void masked_write(uint32_t* dst, uint32_t value, uint32_t mask)
{
    *dst = (*dst & ~mask) | (value & mask);
}

void init_memory(struct memory* mem,
                 struct mem_mapping* mappings, size_t mappings_count,
                 void* base,
                 struct mem_handler* dbg_handler);

static const struct mem_handler* mem_get_handler(const struct memory* mem, uint32_t address)
{
    return &mem->handlers[address >> 16];
}

static void mem_read32(const struct mem_handler* handler, uint32_t address, uint32_t* value)
{
    handler->read32(handler->opaque, address, value);
}

static void mem_write32(const struct mem_handler* handler, uint32_t address, uint32_t value, uint32_t mask)
{
    handler->write32(handler->opaque, address, value, mask);
}

void map_region(struct memory* mem,
                uint16_t region,
                int type,
                const struct mem_handler* handler);

void read_with_bp_checks(void* opaque, uint32_t address, uint32_t* value);
void write_with_bp_checks(void* opaque, uint32_t address, uint32_t value, uint32_t mask);

#ifdef DBG
void activate_memory_break_read(struct memory* mem, uint32_t address);
void deactivate_memory_break_read(struct memory* mem, uint32_t address);
void activate_memory_break_write(struct memory* mem, uint32_t address);
void deactivate_memory_break_write(struct memory* mem, uint32_t address);
int get_memory_type(struct memory* mem, uint32_t address);
#endif

void* init_mem_base(void);
void release_mem_base(void* mem);

#endif

