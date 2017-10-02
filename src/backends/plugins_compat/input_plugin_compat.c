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

#include "plugins_compat.h"

#include "api/m64p_plugin.h"
#include "backends/api/controller_input_backend.h"
#include "backends/api/rumble_backend.h"
#include "plugin/plugin.h"

#include "main/main.h"

#include <stdint.h>
#include <string.h>

/* XXX: this is an abuse of the Zilmar Spec normally this value is reserved */
enum {
    PAK_SWITCH_BUTTON = 0x4000,
    GB_CART_SWITCH_BUTTON = 0x8000
};

/* Pak switching delay
 * If you put too low value,
 * some games (for instance Perfect Dark) won't be able to detect the pak change
 * causing incorrect pak accesses */
enum { PAK_SWITCH_DELAY = 20 };
enum { GB_CART_SWITCH_DELAY = 20 };

static int is_button_released(uint32_t input, uint32_t last_input, uint32_t mask)
{
    return ((input & mask) == 0)
        && ((last_input & mask) != 0);
}

static uint32_t input_plugin_get_input(void* opaque)
{
    static uint32_t last_values[GAME_CONTROLLERS_COUNT] = { 0 };
    static unsigned int pak_switch_delays[GAME_CONTROLLERS_COUNT] = { 0 };
    static unsigned int gb_switch_delays[GAME_CONTROLLERS_COUNT] = { 0 };

    int control_id = *(int*)opaque;

    BUTTONS keys = { 0 };

    if (input.getKeys) {
        input.getKeys(control_id, &keys);
    }

    /* disconnect current pak (if any) immediately after "pak switch" button is released */
    if (is_button_released(keys.Value, last_values[control_id], PAK_SWITCH_BUTTON)) {
        change_pak(&g_dev.controllers[control_id], NULL, NULL);
        pak_switch_delays[control_id] = PAK_SWITCH_DELAY;
    }

    /* switch to next pak after switch delay has expired */
    if (pak_switch_delays[control_id] > 0 && --pak_switch_delays[control_id] == 0) {
        main_switch_pak(control_id);
    }

    /* disconnect current GB cart (if any) immediately after "GB cart switch" button is released */
    if (is_button_released(keys.Value, last_values[control_id], GB_CART_SWITCH_BUTTON)) {
        change_gb_cart(&g_dev.transferpaks[control_id], NULL);
        gb_switch_delays[control_id] = GB_CART_SWITCH_DELAY;
    }

    /* switch to new GB cart after switch delay has expired */
    if (gb_switch_delays[control_id] > 0 && --gb_switch_delays[control_id] == 0) {
        main_change_gb_cart(control_id);
    }

    last_values[control_id] = keys.Value;

    return keys.Value;
}

const struct controller_input_backend_interface
    g_icontroller_input_backend_plugin_compat =
{
    input_plugin_get_input
};


static void input_plugin_rumble_exec(void* opaque, enum rumble_action action)
{
    int control_id = *(int*)opaque;

    if (input.controllerCommand == NULL) {
        return;
    }

    static const uint8_t rumble_cmd_header[] =
    {
        0x23, 0x01, /* T=0x23, R=0x01 */
        JCMD_PAK_WRITE,
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

const struct rumble_backend_interface
    g_irumble_backend_plugin_compat =
{
    input_plugin_rumble_exec
};


static void input_plugin_read_controller(void* opaque,
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

const struct joybus_device_interface
    g_ijoybus_device_plugin_compat =
{
    NULL,
    input_plugin_read_controller,
    input_plugin_controller_command,
};
