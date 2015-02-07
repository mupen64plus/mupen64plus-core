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
#include "pif.h"

#include "api/m64p_types.h"
#include "api/callbacks.h"

#include "main/main.h"
#include "main/rom.h"
#include "main/util.h"

#include <stdlib.h>
#include <string.h>

static char *get_eeprom_path(void)
{
    return formatstr("%s%s.eep", get_savesrampath(), ROM_SETTINGS.goodname);
}

static void eeprom_format(void* eeprom)
{
    memset(eeprom, 0xff, EEPROM_MAX_SIZE);
}

static void eeprom_read_file(void* eeprom)
{
    char *filename = get_eeprom_path();

    switch (read_from_file(filename, eeprom, EEPROM_MAX_SIZE))
    {
        case file_open_error:
            DebugMessage(M64MSG_VERBOSE, "couldn't open eeprom file '%s' for reading", filename);
            eeprom_format(eeprom);
            break;
        case file_read_error:
            DebugMessage(M64MSG_WARNING, "fread() failed for 2kb eeprom file '%s'", filename);
            break;
        default: break;
    }

    free(filename);
}

static void eeprom_write_file(void* eeprom)
{
    char *filename = get_eeprom_path();

    switch (write_to_file(filename, eeprom, EEPROM_MAX_SIZE))
    {
        case file_open_error:
            DebugMessage(M64MSG_WARNING, "couldn't open eeprom file '%s' for writing", filename);
            break;
        case file_write_error:
            DebugMessage(M64MSG_WARNING, "fwrite() failed for 2kb eeprom file '%s'", filename);
            break;
        default: break;
    }

    free(filename);
}


void eeprom_status_command(struct pif* pif, int channel, uint8_t* cmd)
{
    /* check size */
    if (cmd[1] != 3)
    {
        cmd[1] |= 0x40;
        if ((cmd[1] & 3) > 0)
            cmd[3] = 0;
        if ((cmd[1] & 3) > 1)
            cmd[4] = (ROM_SETTINGS.savetype != EEPROM_16KB) ? 0x80 : 0xc0;
        if ((cmd[1] & 3) > 2)
            cmd[5] = 0;
    }
    else
    {
        cmd[3] = 0;
        cmd[4] = (ROM_SETTINGS.savetype != EEPROM_16KB) ? 0x80 : 0xc0;
        cmd[5] = 0;
    }
}

void eeprom_read_command(struct pif* pif, int channel, uint8_t* cmd)
{
    /* read 8-byte block */
    eeprom_read_file(pif->eeprom);
    memcpy(&cmd[4], pif->eeprom + cmd[3]*8, 8);
}

void eeprom_write_command(struct pif* pif, int channel, uint8_t* cmd)
{
    /* write 8-byte block */
    eeprom_read_file(pif->eeprom);
    memcpy(pif->eeprom + cmd[3]*8, &cmd[4], 8);
    eeprom_write_file(pif->eeprom);
}

