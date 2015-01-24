/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - game_controller.c                                       *
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

#include "game_controller.h"
#include "pif.h"

#include "plugin/plugin.h"

#ifdef COMPARE_CORE
#include "api/debugger.h"
#endif

#include <string.h>

static void read_controller_read_buttons(struct pif* pif, int channel, uint8_t* cmd)
{
    if (Controls[channel].Present)
    {
        BUTTONS keys;
        input.getKeys(channel, &keys);
        *((uint32_t*)(cmd + 3)) = keys.Value;

#ifdef COMPARE_CORE
        CoreCompareDataSync(4, cmd + 3);
#endif
    }
}

static void read_controller_read_pak(struct pif* pif, int channel, uint8_t* cmd)
{
    if (Controls[channel].Present)
    {
        if (Controls[channel].Plugin == PLUGIN_RAW)
            if (input.readController)
                input.readController(channel, cmd);
    }
}

static void read_controller_write_pak(struct pif* pif, int channel, uint8_t* cmd)
{
    if (Controls[channel].Present)
    {
        if (Controls[channel].Plugin == PLUGIN_RAW)
            if (input.readController)
                input.readController(channel, cmd);
    }
}


static void controller_status_command(struct pif* pif, int channel, uint8_t* cmd)
{
    if (cmd[1] & 0x80)
        return;

    if (!Controls[channel].Present)
    {
        cmd[1] |= 0x80;
        return;
    }

    cmd[3] = 0x05;
    cmd[4] = 0x00;

    switch(Controls[channel].Plugin)
    {
    case PLUGIN_MEMPAK:
    case PLUGIN_RAW:
        cmd[5] = 1;
        break;
    default:
        cmd[5] = 0;
    }
}

static void controller_read_buttons_command(struct pif* pif, int channel, uint8_t* cmd)
{
    if (!Controls[channel].Present)
        cmd[1] |= 0x80;

    /* NOTE: buttons reading is done in read_controller_read_buttons instead */
}

static void controller_read_pak_command(struct pif* pif, int channel, uint8_t* cmd)
{
    if (!Controls[channel].Present)
    {
        cmd[1] |= 0x80;
        return;
    }

    switch (Controls[channel].Plugin)
    {
    case PLUGIN_MEMPAK: mempak_read_command(&pif->controllers, channel, cmd); break;
    case PLUGIN_RAW:
        if (input.controllerCommand)
            input.controllerCommand(channel, cmd);
        break;
    default:
        memset(&cmd[5], 0, 0x20);
        cmd[0x25] = 0;
    }
}

static void controller_write_pak_command(struct pif* pif, int channel, uint8_t* cmd)
{
    if (!Controls[channel].Present)
    {
        cmd[1] |= 0x80;
        return;
    }

    switch (Controls[channel].Plugin)
    {
    case PLUGIN_MEMPAK: mempak_write_command(&pif->controllers, channel, cmd); break;
    case PLUGIN_RAW:
        if (input.controllerCommand)
            input.controllerCommand(channel, cmd);
        break;
    default:
        cmd[0x25] = pak_crc(&cmd[5]);
    }
}


void process_controller_command(struct pif* pif, int channel, uint8_t* cmd)
{
    switch (cmd[2])
    {
    case 0x00: // status
    case 0xFF: // reset
        controller_status_command(pif, channel, cmd); break;
    case 0x01: // read buttons
        controller_read_buttons_command(pif, channel, cmd); break;
    case 0x02: // read controller pak
        controller_read_pak_command(pif, channel, cmd); break;
    case 0x03: // write controller pak
        controller_write_pak_command(pif, channel, cmd); break;
        break;
    }
}

void read_controller(struct pif* pif, int channel, uint8_t* cmd)
{
    switch (cmd[2])
    {
    case 1: read_controller_read_buttons(pif, channel, cmd); break;
    case 2: read_controller_read_pak(pif, channel, cmd); break;
    case 3: read_controller_write_pak(pif, channel, cmd); break;
    }
}

uint8_t pak_crc(uint8_t* data)
{
    size_t i;
    uint8_t crc = 0;

    for(i = 0; i <= 0x20; ++i)
    {
        int mask;
        for (mask = 0x80; mask >= 1; mask >>= 1)
        {
            uint8_t xor_tap = (crc & 0x80) ? 0x85 : 0x00;
            crc <<= 1;
            if (i != 0x20 && (data[i] & mask)) crc |= 1;
            crc ^= xor_tap;
        }
    }
    return crc;
}

