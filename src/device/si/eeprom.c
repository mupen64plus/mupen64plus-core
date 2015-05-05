/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - eeprom.c                                                *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
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

#include "eeprom.h"

#include <stdint.h>
#include <string.h>

#include "api/callbacks.h"
#include "api/m64p_types.h"
#include "backends/storage_backend.h"

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

enum { EEPROM_BLOCK_SIZE = 8 };

void format_eeprom(uint8_t* eeprom, size_t size)
{
    memset(eeprom, 0xff, size);
}


void init_eeprom(struct eeprom* eeprom,
    uint16_t type,
    struct storage_backend* storage)
{
    eeprom->type = type;
    eeprom->storage = storage;
}

void eeprom_read_block(struct eeprom* eeprom,
    uint8_t block, uint8_t* data)
{
    unsigned int address = block * EEPROM_BLOCK_SIZE;

    if (address < eeprom->storage->size)
    {
        memcpy(data, &eeprom->storage->data[address], EEPROM_BLOCK_SIZE);
    }
    else
    {
        DebugMessage(M64MSG_WARNING, "Invalid access to eeprom address=%04" PRIX16, address);
    }
}

void eeprom_write_block(struct eeprom* eeprom,
    uint8_t block, const uint8_t* data, uint8_t* status)
{
    unsigned int address = block * EEPROM_BLOCK_SIZE;

    if (address < eeprom->storage->size)
    {
        memcpy(&eeprom->storage->data[address], data, EEPROM_BLOCK_SIZE);
        storage_save(eeprom->storage);
        *status = 0x00;
    }
    else
    {
        DebugMessage(M64MSG_WARNING, "Invalid access to eeprom address=%04" PRIX16, address);
    }
}

