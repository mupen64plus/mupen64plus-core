/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - mempak.c                                                *
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

#include "mempak.h"

#include "backends/api/storage_backend.h"
#include "device/controllers/game_controller.h"
#include "main/util.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Serialized representation of ID Block
 * Only used to ease offsets/pointers computation
 * DO NOT DEREFERENCE
 */
#pragma pack(push, 1)
struct id_block_serialized {
    uint32_t serial[6];
    uint16_t device_id;
    uint8_t  banks;
    uint8_t  version;
    uint16_t sum;
    uint16_t isum;
};
#pragma pack(pop)
#if defined(static_assert)
static_assert(sizeof(struct id_block_serialized) == 32, "id_block_serialized must have a size of 32 bytes");
#endif

static void checksum_id_block(unsigned char* ptr,
            uint16_t* sum, uint16_t* isum)
{
    size_t i;
    uint16_t accu = 0;
    for (i = 0; i < offsetof(struct id_block_serialized, sum); i += 2) {
        accu += load_beu16((void*)&ptr[i]);
    }
    *sum  = accu;
    *isum = UINT16_C(0xfff2) - accu;
}

static uint8_t checksum_index_table(size_t count, unsigned char* ptr)
{
    unsigned sum = 0;
    while (count != 0) {
        sum += load_beu8((void*)ptr);
        ++ptr;
        --count;
    }

    return (uint8_t)sum;
}

static void serialize_id_block(unsigned char *ptr, const uint32_t serial[6], uint16_t device_id, uint8_t banks, uint8_t version) {

    size_t i;
    /* _ should never be dereferenced - it is only used to ease pointer/offsets computation */
    struct id_block_serialized* const _ = (struct id_block_serialized*)ptr;


    for (i = 0; i < 6; ++i) {
        store_beu32(serial[i], (void*)&_->serial[i]);
    }
    store_beu16(device_id, (void*)&_->device_id);
    store_beu8(banks, (void*)&_->banks);
    store_beu8(version, (void*)&_->version);

    uint16_t sum, isum;
    checksum_id_block(ptr, &sum, &isum);

    store_beu16(sum, (void*)&_->sum);
    store_beu16(isum, (void*)&_->isum);
}



void format_mempak(uint8_t* mem,
    const uint32_t serial[6],
    uint16_t device_id,
    uint8_t banks,
    uint8_t version)
{
    enum { MPK_PAGE_SIZE = 256 };

    uint8_t* const page_0 = mem + 0*MPK_PAGE_SIZE;
    uint8_t* const page_1 = mem + 1*MPK_PAGE_SIZE;
    uint8_t* const page_2 = mem + 2*MPK_PAGE_SIZE;
    uint8_t* const page_3 = mem + 3*MPK_PAGE_SIZE;

    /* Page 0 is divided in 8 x 32-byte blocks:
     * 0. reserved
     * 1. ID
     * 2. reserved
     * 3. ID backup #1
     * 4. ID backup #2
     * 5. reserved
     * 6. ID backup #3
     * 7. reserved
     */
    serialize_id_block(page_0 + 1*32, serial, device_id, banks, version);

    memset(page_0 + 0*32, 0, 32);
    memset(page_0 + 2*32, 0, 32);
    memset(page_0 + 5*32, 0, 32);
    memset(page_0 + 7*32, 0, 32);

    memcpy(page_0 + 3*32, page_0 + 1*32, 32);
    memcpy(page_0 + 4*32, page_0 + 1*32, 32);
    memcpy(page_0 + 6*32, page_0 + 1*32, 32);

    /* Page 1 holds the index table.
     * The first 5 inodes are reserved because the first 5 pages are reserved.
     * The first inode page index holds the checksum of the 123 normal nodes.
     * The remaining 123 pages are marked empty.
     */
    size_t start_page = 5;
    size_t last_page = 128;
    size_t i;
    memset(page_1, 0, 2*start_page);
    for(i = start_page; i < last_page; ++i) {
        store_beu16(UINT16_C(0x0003), page_1 + 2*i);
    }
    page_1[1] = checksum_index_table(2*(last_page-start_page), page_1 + 2*start_page);

    /* Page 2 is a backup of Page 1 */
    memcpy(page_2, page_1, MPK_PAGE_SIZE);

    /* Remaining pages DIR+DATA (3...) are initialized with 0x00 */
    memset(page_3, 0, MEMPAK_SIZE - 3*MPK_PAGE_SIZE);
}


void init_mempak(struct mempak* mpk,
                 void* storage,
                 const struct storage_backend_interface* istorage)
{
    mpk->storage = storage;
    mpk->istorage = istorage;
}

static void plug_mempak(void* pak)
{
}

static void unplug_mempak(void* pak)
{
}

static void read_mempak(void* pak, uint16_t address, uint8_t* data, size_t size)
{
    struct mempak* mpk = (struct mempak*)pak;

    if (address < 0x8000)
    {
        memcpy(data, mpk->istorage->data(mpk->storage) + address, size);
    }
    else
    {
        memset(data, 0x00, size);
    }
}

static void write_mempak(void* pak, uint16_t address, const uint8_t* data, size_t size)
{
    struct mempak* mpk = (struct mempak*)pak;

    if (address < 0x8000)
    {
        memcpy(mpk->istorage->data(mpk->storage) + address, data, size);
        mpk->istorage->save(mpk->storage, address, size);
    }
    else
    {
        /* do nothing */
    }
}

/* Memory pak definition */
const struct pak_interface g_imempak =
{
    "Memory pak",
    plug_mempak,
    unplug_mempak,
    read_mempak,
    write_mempak
};
