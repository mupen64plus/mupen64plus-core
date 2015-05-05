/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - input_plugin_compat.h                                   *
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

#ifndef M64P_PLUGIN_INPUT_PLUGIN_COMPAT_H
#define M64P_PLUGIN_INPUT_PLUGIN_COMPAT_H

#include "backends/controller_input_backend.h"
#include "backends/rumble_backend.h"

#include <stdint.h>

/* controller_input backend functions */

int input_plugin_is_connected(void* opaque);
enum pak_type input_plugin_detect_pak(void* opaque);
uint32_t input_plugin_get_input(void* opaque);

/* PIF data processing functions */

void input_plugin_read_controller(void* opaque,
    const uint8_t* tx, const uint8_t* tx_buf,
    uint8_t* rx, uint8_t* rx_buf);

void input_plugin_controller_command(void* opaque,
    uint8_t* tx, const uint8_t* tx_buf,
    const uint8_t* rx, const uint8_t* rx_buf);

/* Rumble backend function */

void input_plugin_rumble_exec(void* opaque, enum rumble_action action);

#endif
