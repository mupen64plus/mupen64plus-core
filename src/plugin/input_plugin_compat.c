/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - input_plugin_compat.c                                   *
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

#include "input_plugin_compat.h"

#include "api/m64p_plugin.h"
#include "main/main.h"
#include "plugin.h"
#include "device/si/game_controller.h"
#include "backends/rumble_backend.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int input_plugin_is_connected(void* opaque)
{
    int control_id = *(int*)opaque;

    CONTROL* c = &Controls[control_id];

    return c->Present;
}

enum pak_type input_plugin_detect_pak(void* opaque)
{
    int control_id = *(int*)opaque;
    enum pak_type pak = PAK_NONE;

    CONTROL* c = &Controls[control_id];

    switch(c->Plugin)
    {
    case PLUGIN_NONE: pak = PAK_NONE; break;
    case PLUGIN_MEMPAK: pak = PAK_MEM; break;
    case PLUGIN_RUMBLE_PAK: pak = PAK_RUMBLE; break;
    case PLUGIN_TRANSFER_PAK: pak = PAK_TRANSFER; break;

    case PLUGIN_RAW:
        /* historically PLUGIN_RAW has been mostly (exclusively ?) used for rumble,
         * so we just reproduce that behavior */
        pak = PAK_RUMBLE; break;
    }

    /* XXX: Force transfer pak if core has loaded a gb cart for this controller */
    if (g_dev.si.pif.controllers[control_id].transferpak.gb_cart != NULL) {
        pak = PAK_TRANSFER;
    }

    return pak;
}

uint32_t input_plugin_get_input(void* opaque)
{
    int control_id = *(int*)opaque;

    BUTTONS keys = { 0 };

    if (input.getKeys) {
        input.getKeys(control_id, &keys);
    }

    return keys.Value;
}

void input_plugin_rumble_exec(void* opaque, enum rumble_action action)
{
    int control_id = *(int*)opaque;

    if (input.controllerCommand == NULL) {
        return;
    }

    static const uint8_t rumble_cmd_header[] =
    {
        0x23, 0x01, /* T=0x23, R=0x01 */
        0x03,       /* PIF_CMD_PAK_WRITE */
        0xc0, 0x1b, /* address=0xc000 | crc=0x1b */
    };

    uint8_t cmd[0x26];

    uint8_t rumble_data = (action == RUMBLE_START)
        ? 0x01
        : 0x00;

    /* build rumble command */
    memcpy(cmd, rumble_cmd_header, 5);
    memset(cmd + 5, rumble_data, 0x20);
    cmd[0x25] = 0; /* dummy data CRC */

    input.controllerCommand(control_id, cmd);
}


void input_plugin_read_controller(void* opaque,
    const uint8_t* tx, const uint8_t* tx_buf,
    uint8_t* rx, uint8_t* rx_buf)
{
    int control_id = *(int*)opaque;

    if (input.readController == NULL) {
        return;
    }

    /* UGLY: use negative offsets to get access to non-const tx pointer */
    input.readController(control_id, rx - 1);
}

void input_plugin_controller_command(void* opaque,
    uint8_t* tx, const uint8_t* tx_buf,
    const uint8_t* rx, const uint8_t* rx_buf)
{
    int control_id = *(int*)opaque;

    if (input.controllerCommand == NULL) {
        return;
    }

    input.controllerCommand(control_id, tx);
}

