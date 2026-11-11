/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - sram.c                                                  *
 *   Mupen64Plus homepage: https://mupen64plus.org/                        *
 *   Copyright (C) 2014 Bobby Smiles                                       *
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

#include "sram.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "backends/api/storage_backend.h"
#include "device/memory/memory.h"
#include "device/rdram/rdram.h"

/* The 256 Kbit SRAM decodes 15 address lines and is mirrored through the
 * remainder of the save domain. */
#define SRAM_ADDR_MASK ((uint32_t)SRAM_SIZE - 1)

static size_t sram_dma_length(const struct sram* sram,
                              uint32_t dram_addr, uint32_t length)
{
    if (dram_addr >= sram->rdram->dram_size) {
        return 0;
    }

    if (length > sram->rdram->dram_size - dram_addr) {
        return sram->rdram->dram_size - dram_addr;
    }

    return length;
}

void format_sram(uint8_t* mem)
{
    memset(mem, 0xff, SRAM_SIZE);
}

void init_sram(struct sram* sram,
               void* storage, const struct storage_backend_interface* istorage, struct rdram* rdram)
{
    sram->storage = storage;
    sram->istorage = istorage;
    sram->rdram = rdram;
}

unsigned int sram_dma_read(void* opaque, const uint8_t* dram, uint32_t dram_addr, uint32_t cart_addr, uint32_t length)
{
    size_t i;
    size_t transfer_length;
    struct sram* sram = (struct sram*)opaque;
    uint8_t* mem = sram->istorage->data(sram->storage);

    cart_addr &= SRAM_ADDR_MASK;
    transfer_length = sram_dma_length(sram, dram_addr, length);

    for (i = 0; i < transfer_length; ++i) {
        mem[((cart_addr+i) & SRAM_ADDR_MASK)^S8] = dram[(dram_addr+i)^S8];
    }

    if (transfer_length > SRAM_SIZE - cart_addr) {
        sram->istorage->save(sram->storage, 0, SRAM_SIZE);
    }
    else if (transfer_length != 0) {
        sram->istorage->save(sram->storage, cart_addr, transfer_length);
    }

    return /* length / 8 */0x1000;
}

unsigned int sram_dma_write(void* opaque, uint8_t* dram, uint32_t dram_addr, uint32_t cart_addr, uint32_t length)
{
    size_t i;
    size_t transfer_length;
    struct sram* sram = (struct sram*)opaque;
    const uint8_t* mem = sram->istorage->data(sram->storage);

    cart_addr &= SRAM_ADDR_MASK;
    transfer_length = sram_dma_length(sram, dram_addr, length);

    for (i = 0; i < transfer_length; ++i) {
        dram[(dram_addr+i)^S8] = mem[((cart_addr+i) & SRAM_ADDR_MASK)^S8];
    }

    return /* length / 8 */0x1000;
}

void read_sram(void* opaque, uint32_t address, uint32_t* value)
{
    struct sram* sram = (struct sram*)opaque;
    const uint8_t* mem = sram->istorage->data(sram->storage);

    address &= SRAM_ADDR_MASK;

    *value = *(uint32_t*)(mem + address);
}

void write_sram(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
    struct sram* sram = (struct sram*)opaque;
    uint8_t* mem = sram->istorage->data(sram->storage);

    address &= SRAM_ADDR_MASK;

    masked_write((uint32_t*)(mem + address), value, mask);

    sram->istorage->save(sram->storage, address, sizeof(value));
}
