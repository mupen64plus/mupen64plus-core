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

#include <stdint.h>

#include "mempak.h"
#include "rumblepak.h"
#include "transferpak.h"

enum pak_type
{
    PAK_NONE,
    PAK_MEM,
    PAK_RUMBLE,
    PAK_TRANSFER
};

struct controller_input_backend;
struct storage_backend;

struct game_controller
{
    uint8_t status;

    struct controller_input_backend* cin;
    struct mempak mempak;
    struct rumblepak rumblepak;
    struct transferpak transferpak;
};


void init_game_controller(struct game_controller* cont,
    struct controller_input_backend* cin,
    struct storage_backend* mpk_storage,
    struct rumble_backend* rumble,
    struct gb_cart* gb_cart);

void poweron_game_controller(struct game_controller* cont);

void process_controller_command(void* opaque,
    const uint8_t* tx, const uint8_t* tx_buf,
    uint8_t* rx, uint8_t* rx_buf);

#endif
