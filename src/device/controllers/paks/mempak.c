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

#include <stdint.h>
#include <string.h>


void format_mempak(uint8_t* mem)
{
    enum { MPK_PAGE_SIZE = 256 };
    size_t i;

    static const uint8_t page_0[MPK_PAGE_SIZE] =
    {
        /* Label area */
        0x81,0x01,0x02,0x03, 0x04,0x05,0x06,0x07, 0x08,0x09,0x0a,0x0b, 0x0c,0x0d,0x0e,0x0f,
        0x10,0x11,0x12,0x13, 0x14,0x15,0x16,0x17, 0x18,0x19,0x1a,0x1b, 0x1c,0x1d,0x1e,0x1f,
        /* Main ID area */
        0xff,0xff,0xff,0xff, 0x05,0x1a,0x5f,0x13, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff, 0xff,0xff,0x01,0xff, 0x66,0x25,0x99,0xcd,
        /* Unused */
        0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        /* ID area backup #1 */
        0xff,0xff,0xff,0xff, 0x05,0x1a,0x5f,0x13, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff, 0xff,0xff,0x01,0xff, 0x66,0x25,0x99,0xcd,
        /* ID area backup #2 */
        0xff,0xff,0xff,0xff, 0x05,0x1a,0x5f,0x13, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff, 0xff,0xff,0x01,0xff, 0x66,0x25,0x99,0xcd,
        /* Unused */
        0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        /* ID area backup #3 */
        0xff,0xff,0xff,0xff, 0x05,0x1a,0x5f,0x13, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff, 0xff,0xff,0x01,0xff, 0x66,0x25,0x99,0xcd,
        /* Unused */
        0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00
    };

    /* Fill Page 0 with pre-initialized content */
    memcpy(mem, page_0, MPK_PAGE_SIZE);

    /* Fill INODE page 1 and update it's checkum */
    size_t start_page = 5;
    memset(mem + 1*MPK_PAGE_SIZE, 0, 2*start_page);
    for (i = 1*MPK_PAGE_SIZE+2*start_page; i < 2*MPK_PAGE_SIZE; i += 2) {
        mem[i+0] = 0x00;
        mem[i+1] = 0x03;
    }
    mem[1*MPK_PAGE_SIZE + 1] = 0x71;

    /* Page 2 is identical to page 1 */
    memcpy(mem + 2*MPK_PAGE_SIZE, mem + 1*MPK_PAGE_SIZE, MPK_PAGE_SIZE);

    /* Remaining pages DIR+DATA (3...) are initialized with 0x00 */
    memset(mem + 3*MPK_PAGE_SIZE, 0, MEMPAK_SIZE - 3*MPK_PAGE_SIZE);
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
