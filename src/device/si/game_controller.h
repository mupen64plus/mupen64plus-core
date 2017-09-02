/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - game_controller.h                                       *
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

#ifndef M64P_DEVICE_SI_GAME_CONTROLLER_H
#define M64P_DEVICE_SI_GAME_CONTROLLER_H

#include <stddef.h>
#include <stdint.h>

struct controller_input_backend;

struct pak_interface
{
    const char* name;
    void (*plug)(void* opaque);
    void (*unplug)(void* opaque);
    void (*read)(void* opaque, uint16_t address, uint8_t* data, size_t size);
    void (*write)(void* opaque, uint16_t address, const uint8_t* data, size_t size);
};

struct game_controller
{
    uint8_t status;

    struct controller_input_backend* cin;
    void* pak;
    const struct pak_interface* ipak;
};

void init_game_controller(struct game_controller* cont,
    struct controller_input_backend* cin,
    void* pak, const struct pak_interface* ipak);

void poweron_game_controller(struct game_controller* cont);

void process_controller_command(void* opaque,
    const uint8_t* tx, const uint8_t* tx_buf,
    uint8_t* rx, uint8_t* rx_buf);

void standard_controller_reset(struct game_controller* cont);

#endif
