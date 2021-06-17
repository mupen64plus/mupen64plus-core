/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - flashram.c                                              *
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

#include "flashram.h"

#include <string.h>

#include "api/callbacks.h"
#include "api/m64p_types.h"
#include "backends/api/storage_backend.h"
#include "device/memory/memory.h"

#define __STDC_FORMAT_MACROS
#include <inttypes.h>


static void flashram_command(struct flashram* flashram, uint32_t command)
{
    unsigned int i;
    unsigned int offset;
    uint8_t* mem = flashram->istorage->data(flashram->storage);

    switch (command & 0xff000000)
    {
    case 0x3c000000:
        /* set chip erase mode */
        flashram->mode = FLASHRAM_MODE_CHIP_ERASE;
        break;

    case 0x4b000000:
        /* set sector erase mode, set erase sector */
        flashram->mode = FLASHRAM_MODE_SECTOR_ERASE;
        flashram->erase_page = (command & 0xffff);
        break;

    case 0x78000000:
        /* set erase busy flag */
        flashram->status |= 0x02;

        /* do chip/sector erase */
        if (flashram->mode == FLASHRAM_MODE_SECTOR_ERASE) {
            offset = (flashram->erase_page & 0xff80) * 128;
            memset(mem + offset, 0xff, 128*128);
            flashram->istorage->save(flashram->storage, offset, 128*128);
        }
        else if (flashram->mode == FLASHRAM_MODE_CHIP_ERASE){
            memset(mem, 0xff, FLASHRAM_SIZE);
            flashram->istorage->save(flashram->storage, 0, FLASHRAM_SIZE);
        }
        else {
            DebugMessage(M64MSG_WARNING, "Unexpected erase command (mode=%x)", flashram->mode);
        }

        /* clear erase busy flag, set erase success flag, transition to status mode */
        flashram->status &= ~UINT32_C(0x02);
        flashram->status |= 0x08;
        flashram->mode = FLASHRAM_MODE_STATUS;
        break;

    case 0xa5000000:
        /* set program busy flag */
        flashram->status |= 0x01;

        /* program selected page */
        offset = (command & 0xffff) * 128;
        for (i = 0; i < 128; ++i) {
            mem[(offset+i)^S8] = flashram->page_buf[i];
        }
        flashram->istorage->save(flashram->storage, offset, 128);

        /* clear program busy flag, set program success flag, transition to status mode */
        flashram->status &= ~UINT32_C(0x01);
        flashram->status |= 0x04;
        flashram->mode = FLASHRAM_MODE_STATUS;
        break;

    case 0xb4000000:
        /* set page program mode */
        flashram->mode = FLASHRAM_MODE_PAGE_PROGRAM;
        break;

    case 0xd2000000:
        /* set status mode */
        flashram->mode = FLASHRAM_MODE_STATUS;
        break;

    case 0xe1000000:
        /* set silicon_id mode */
        flashram->mode = FLASHRAM_MODE_READ_SILICON_ID;
        break;

    case 0xf0000000:
        /* set read mode */
        flashram->mode = FLASHRAM_MODE_READ_ARRAY;
        break;

    default:
        DebugMessage(M64MSG_WARNING, "unknown flashram command: %" PRIX32, command);
    }
}


void init_flashram(struct flashram* flashram,
                   uint32_t flashram_id,
                   void* storage, const struct storage_backend_interface* istorage)
{
    flashram->silicon_id[0] = FLASHRAM_TYPE_ID;
    flashram->silicon_id[1] = flashram_id;
    flashram->storage = storage;
    flashram->istorage = istorage;
}

void poweron_flashram(struct flashram* flashram)
{
    flashram->mode = FLASHRAM_MODE_READ_ARRAY;
    flashram->status = 0x00;
    flashram->erase_page = 0;
    memset(flashram->page_buf, 0xff, 128);
}

void format_flashram(uint8_t* flash)
{
    memset(flash, 0xff, FLASHRAM_SIZE);
}

void read_flashram(void* opaque, uint32_t address, uint32_t* value)
{
    struct flashram* flashram = (struct flashram*)opaque;

    if ((address & 0x1ffff) == 0x00000 && flashram->mode == FLASHRAM_MODE_STATUS) {
        /* read Status register */
        *value = flashram->status;
    }
    else if ((address & 0x1ffff) == 0x0000 && flashram->mode == FLASHRAM_MODE_READ_ARRAY) {
        /* flashram MMIO read are not supported except for the "dummy" read @0x0000 done before DMA.
         * returns a "dummy" value. */
        *value = 0;
    }
    else {
        /* other accesses are not implemented */
        DebugMessage(M64MSG_WARNING, "unknown Flashram read IO (mode=%x) @%08x", flashram->mode, address);
    }
}

void write_flashram(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
    struct flashram* flashram = (struct flashram*)opaque;

    if ((address & 0x1ffff) == 0x00000 && flashram->mode == FLASHRAM_MODE_STATUS) {
        /* clear/set Status register */
        flashram->status = (value & mask) & 0xff;
    }
    else if ((address & 0x1ffff) == 0x10000) {
        /* set command */
        flashram_command(flashram, value & mask);
    }
    else {
        /* other accesses are not implemented */
        DebugMessage(M64MSG_WARNING, "unknown Flashram write IO (mode=%x) @%08x <- %08x & %08x", flashram->mode, address, value, mask);
    }
}


unsigned int flashram_dma_write(void* opaque, uint8_t* dram, uint32_t dram_addr, uint32_t cart_addr, uint32_t length)
{
    size_t i;
    struct flashram* flashram = (struct flashram*)opaque;
    const uint8_t* mem = flashram->istorage->data(flashram->storage);

    if ((cart_addr & 0x1ffff) == 0x00000 && length == 8 && flashram->mode == FLASHRAM_MODE_READ_SILICON_ID) {
        /* read Silicon ID using DMA */
        ((uint32_t*)dram)[dram_addr/4+0] = flashram->silicon_id[0];
        ((uint32_t*)dram)[dram_addr/4+1] = flashram->silicon_id[1];
    }
    else if ((cart_addr & 0x1ffff) < 0x10000 && flashram->mode == FLASHRAM_MODE_READ_ARRAY) {

        /* adjust flashram address before starting DMA. */
        if (flashram->silicon_id[1] == MX29L1100_ID
            || flashram->silicon_id[1] == MX29L0000_ID
            || flashram->silicon_id[1] == MX29L0001_ID) {
            /* "old" flash needs special address adjusting */
            cart_addr = (cart_addr & 0xffff) * 2;
        }
        else {
            /* "new" flash doesn't require special address adjusting at DMA start. */
            cart_addr &= 0xffff;
        }

        /* do actual DMA */
        for(i = 0; i < length; ++i) {
            dram[(dram_addr+i)^S8] = mem[(cart_addr+i)^S8];
        }
    }
    else {
        /* other accesses are not implemented */
        DebugMessage(M64MSG_WARNING, "unknown Flashram DMA Write (mode=%x) @%08x <- %08x length=%08x",
            flashram->mode, dram_addr, cart_addr, length);
    }

    return /* length / 8 */0x1000;
}

unsigned int flashram_dma_read(void* opaque, const uint8_t* dram, uint32_t dram_addr, uint32_t cart_addr, uint32_t length)
{
    struct flashram* flashram = (struct flashram*)opaque;
    unsigned int i;

    if ((cart_addr & 0x1ffff) == 0x00000 && length == 128 && flashram->mode == FLASHRAM_MODE_PAGE_PROGRAM) {
        /* load page buf using DMA */
        for(i = 0; i < length; ++i) {
            flashram->page_buf[i] = dram[(dram_addr+i)^S8];
        }
    }
    else {
        /* other accesses are not implemented */
        DebugMessage(M64MSG_WARNING, "unknown Flashram DMA Read (mode=%x) @%08x <- %08x length=%08x",
            flashram->mode, cart_addr, dram_addr, length);
    }

    return /* length / 8 */0x1000;
}

