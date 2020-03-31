/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - flashram.h                                              *
 *   Mupen64Plus homepage: https://mupen64plus.org/                        *
 *   Copyright (C) 2014 Bobby Smiles                                       *
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

#ifndef M64P_DEVICE_PI_FLASHRAM_H
#define M64P_DEVICE_PI_FLASHRAM_H

#include <stdint.h>

struct storage_backend_interface;

enum { FLASHRAM_SIZE = 0x20000 };

enum { FLASHRAM_TYPE_ID = 0x11118001 };

/* flashram manufacturer and device code */
enum {
    MX29L0000_ID = 0x00c20000,
    MX29L0001_ID = 0x00c20001,
    MX29L1100_ID = 0x00c2001e,
    MX29L1101_ID = 0x00c2001d,
    MN63F8MPN_ID = 0x003200f1,
};


enum flashram_mode
{
    FLASHRAM_MODE_READ_ARRAY,
    FLASHRAM_MODE_READ_SILICON_ID,
    FLASHRAM_MODE_STATUS,
    FLASHRAM_MODE_SECTOR_ERASE,
    FLASHRAM_MODE_CHIP_ERASE,
    FLASHRAM_MODE_PAGE_PROGRAM
};

struct flashram
{
    uint8_t page_buf[128];
    uint32_t silicon_id[2];
    uint32_t status; // supposedly only 8-bit
    uint16_t erase_page;
    enum flashram_mode mode;

    void* storage;
    const struct storage_backend_interface* istorage;
};

void init_flashram(struct flashram* flashram,
                   uint32_t flashram_id,
                   void* storage,
                   const struct storage_backend_interface* istorage);

void poweron_flashram(struct flashram* flashram);

void format_flashram(uint8_t* flash);

void read_flashram(void* opaque, uint32_t address, uint32_t* value);
void write_flashram(void* opaque, uint32_t address, uint32_t value, uint32_t mask);

unsigned int flashram_dma_write(void* opaque, uint8_t* dram, uint32_t dram_addr, uint32_t cart_addr, uint32_t length);
unsigned int flashram_dma_read(void* opaque, const uint8_t* dram, uint32_t dram_addr, uint32_t cart_addr, uint32_t length);

#endif
