/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - rumblepak.c                                             *
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

#include "rumblepak.h"

#include "backends/rumble_backend.h"

#include <string.h>


void init_rumblepak(struct rumblepak* rpk, struct rumble_backend* rumble)
{
    rpk->rumble = rumble;
}

void rumblepak_read_command(struct rumblepak* rpk, uint16_t address, uint8_t* data, size_t size)
{
    uint8_t value;

    if ((address >= 0x8000) && (address < 0x9000))
    {
        value = 0x80;
    }
    else
    {
        value = 0x00;
    }

    memset(data, value, size);
}

void rumblepak_write_command(struct rumblepak* rpk, uint16_t address, const uint8_t* data, size_t size)
{
    enum rumble_action action;

    if (address == 0xc000)
    {
        action = (*data == 0)
                ? RUMBLE_STOP
                : RUMBLE_START;

        rumble_exec(rpk->rumble, action);
    }
}

