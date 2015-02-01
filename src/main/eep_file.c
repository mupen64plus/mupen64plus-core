/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - eep_file.c                                              *
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

#include "eep_file.h"

#include "api/m64p_types.h"
#include "api/callbacks.h"
#include "main/util.h"
#include "si/eeprom.h"

#include <stdlib.h>

void open_eep_file(struct eep_file* eep, const char* filename)
{
    /* ! Take ownership of filename ! */
    eep->filename = filename;

    /* try to load eep file content */
    switch(read_from_file(eep->filename, eep->eeprom, EEPROM_MAX_SIZE))
    {
    case file_open_error:
        /* if no prior file exists, provide default mempaks content */
        DebugMessage(M64MSG_VERBOSE, "couldn't open eeprom file '%s' for reading", eep->filename);
        format_eeprom(eep->eeprom, EEPROM_MAX_SIZE);
        break;
    case file_read_error:
        DebugMessage(M64MSG_WARNING, "failed to read eeprom file '%s'", eep->filename);
        break;
    default:
        break;
    }

    eep->touched = 0;
}

void close_eep_file(struct eep_file* eep)
{
    /* write back eep file content (if touched) */
    if (eep->touched != 0)
    {
        switch(write_to_file(eep->filename, eep->eeprom, EEPROM_MAX_SIZE))
        {
        case file_open_error:
            DebugMessage(M64MSG_WARNING, "couldn't open eeprom file '%s' for writing", eep->filename);
            break;
        case file_write_error:
            DebugMessage(M64MSG_WARNING, "failed to write eeprom file '%s'", eep->filename);
            break;
        default:
            break;
        }
    }

    free((void*)eep->filename);
}

uint8_t* eep_file_ptr(struct eep_file* eep)
{
    return eep->eeprom;
}

void touch_eep_file(void* opaque)
{
    struct eep_file* eep = (struct eep_file*)opaque;
    eep->touched = 1;
}

