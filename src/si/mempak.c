/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - mempak.c                                                *
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

#include "mempak.h"
#include "game_controller.h"

#include "api/m64p_types.h"
#include "api/callbacks.h"

#include "main/main.h"
#include "main/rom.h"
#include "main/util.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static char *get_mempaks_path(void)
{
    return formatstr("%s%s.mpk", get_savesrampath(), ROM_SETTINGS.goodname);
}

static void mempaks_format(uint8_t(*mempaks)[MEMPAK_SIZE])
{
    static const uint8_t init[] =
    {
        0x81,0x01,0x02,0x03, 0x04,0x05,0x06,0x07, 0x08,0x09,0x0a,0x0b, 0x0c,0x0d,0x0e,0x0f,
        0x10,0x11,0x12,0x13, 0x14,0x15,0x16,0x17, 0x18,0x19,0x1a,0x1b, 0x1c,0x1d,0x1e,0x1f,
        0xff,0xff,0xff,0xff, 0x05,0x1a,0x5f,0x13, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff, 0xff,0xff,0x01,0xff, 0x66,0x25,0x99,0xcd,
        0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        0xff,0xff,0xff,0xff, 0x05,0x1a,0x5f,0x13, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff, 0xff,0xff,0x01,0xff, 0x66,0x25,0x99,0xcd,
        0xff,0xff,0xff,0xff, 0x05,0x1a,0x5f,0x13, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff, 0xff,0xff,0x01,0xff, 0x66,0x25,0x99,0xcd,
        0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        0xff,0xff,0xff,0xff, 0x05,0x1a,0x5f,0x13, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff, 0xff,0xff,0x01,0xff, 0x66,0x25,0x99,0xcd,
        0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        0x00,0x71,0x00,0x03, 0x00,0x03,0x00,0x03, 0x00,0x03,0x00,0x03, 0x00,0x03,0x00,0x03
    };
    int i,j;
    for (i=0; i<MEMPAK_COUNT; i++)
    {
        for (j=0; j<MEMPAK_SIZE; j+=2)
        {
            mempaks[i][j] = 0;
            mempaks[i][j+1] = 0x03;
        }
        memcpy(mempaks[i], init, 272);
    }
}

static void mempaks_read_file(uint8_t(*mempaks)[MEMPAK_SIZE])
{
    char *filename = get_mempaks_path();

    switch (read_from_file(filename, mempaks, MEMPAK_COUNT*MEMPAK_SIZE))
    {
        case file_open_error:
            DebugMessage(M64MSG_VERBOSE, "couldn't open memory pak file '%s' for reading", filename);
            mempaks_format(mempaks);
            break;
        case file_read_error:
            DebugMessage(M64MSG_WARNING, "fread() failed for 128kb memory pak file '%s'", filename);
            break;
        default: break;
    }

    free(filename);
}

static void mempaks_write_file(uint8_t(*mempaks)[MEMPAK_SIZE])
{
    char *filename = get_mempaks_path();

    switch (write_to_file(filename, mempaks, MEMPAK_COUNT*MEMPAK_SIZE))
    {
        case file_open_error:
            DebugMessage(M64MSG_WARNING, "couldn't open memory pak file '%s' for writing", filename);
            break;
        case file_write_error:
            DebugMessage(M64MSG_WARNING, "fwrite() failed for 128kb memory pak file '%s'", filename);
            break;
        default: break;
    }

    free(filename);
}

void mempak_read_command(struct game_controllers* controllers, int channel, uint8_t* cmd)
{
    /* address is in fact an offset (11bit) | CRC (5 bits) */
    uint16_t address = (cmd[3] << 8) | cmd[4];

    if (address == 0x8001)
    {
        memset(&cmd[5], 0, 0x20);
        cmd[0x25] = pak_crc(&cmd[5]);
    }
    else
    {
        address &= 0xFFE0;
        if (address <= 0x7FE0)
        {
            mempaks_read_file(controllers->mempaks);
            memcpy(&cmd[5], &controllers->mempaks[channel][address], 0x20);
        }
        else
        {
            memset(&cmd[5], 0, 0x20);
        }
        cmd[0x25] = pak_crc(&cmd[5]);
    }
}

void mempak_write_command(struct game_controllers* controllers, int channel, uint8_t* cmd)
{
    /* address is in fact an offset (11bit) | CRC (5 bits) */
    uint16_t address = (cmd[3] << 8) | cmd[4];

    if (address == 0x8001)
    {
        cmd[0x25] = pak_crc(&cmd[5]);
    }
    else
    {
        address &= 0xFFE0;
        if (address <= 0x7FE0)
        {
            mempaks_read_file(controllers->mempaks);
            memcpy(&controllers->mempaks[channel][address], &cmd[5], 0x20);
            mempaks_write_file(controllers->mempaks);
        }
        cmd[0x25] = pak_crc(&cmd[5]);
    }
}
