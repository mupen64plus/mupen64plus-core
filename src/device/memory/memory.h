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

#include <stdint.h>

struct memory
{
    void (*readmem[0x10000])(void);
    void (*writemem[0x10000])(void);

#ifdef DBG
    int memtype[0x10000];
    void (*saved_readmem [0x10000])(void);
    void (*saved_writemem [0x10000])(void);
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

void poweron_memory(struct memory* mem);

void map_region(struct memory* mem,
                uint16_t region,
                int type,
                void (*read32)(void),
                void (*write32)(void));

/* XXX: cannot make them static because of dynarec + rdp fb */
void read_rdram(void);
void write_rdram(void);
void read_rdramFB(void);
void write_rdramFB(void);

/* Returns a pointer to a block of contiguous memory
 * Can access RDRAM, SP_DMEM, SP_IMEM and ROM, using TLB if necessary
 * Useful for getting fast access to a zone with executable code. */
uint32_t *fast_mem_access(uint32_t address);

#ifdef DBG
void activate_memory_break_read(struct memory* mem, uint32_t address);
void deactivate_memory_break_read(struct memory* mem, uint32_t address);
void activate_memory_break_write(struct memory* mem, uint32_t address);
void deactivate_memory_break_write(struct memory* mem, uint32_t address);
int get_memory_type(struct memory* mem, uint32_t address);
#endif

#endif

