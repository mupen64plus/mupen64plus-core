/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - is_viewer.c                                             *
 *   Mupen64Plus homepage: https://mupen64plus.org/                        *
 *   Copyright (C) 2021 loganmc10                                          *
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

#include "is_viewer.h"

#define M64P_CORE_PROTOTYPES 1
#include "api/callbacks.h"
#include "main/util.h"

#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <string.h>

#define IS_ADDR_MASK UINT32_C(0x00000fff)

void poweron_is_viewer(struct is_viewer* is_viewer)
{
    memset(is_viewer->data, 0, 0x1000);
    memset(is_viewer->output_buffer, 0, 0x200);
    is_viewer->buffer_pos = 0;
}

void read_is_viewer(void* opaque, uint32_t address, uint32_t* value)
{
    struct is_viewer* is_viewer = (struct is_viewer*)opaque;
    address &= IS_ADDR_MASK;
    memcpy(value, &is_viewer->data[address], 4);
    *value = m64p_swap32(*value);
}

void write_is_viewer(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
    struct is_viewer* is_viewer = (struct is_viewer*)opaque;
    address &= IS_ADDR_MASK;
    uint32_t word = value & mask;
    if (address == 0x14)
    {
        if (word > 0)
        {
            memcpy(&is_viewer->output_buffer[is_viewer->buffer_pos], &is_viewer->data[0x20], word);
            is_viewer->buffer_pos += word;
            char* newline = memchr(is_viewer->output_buffer, '\n', is_viewer->buffer_pos);
            if (newline)
            {
                *newline = '\0';
                DebugMessage(M64MSG_INFO, "IS64: %s", is_viewer->output_buffer);
                memset(is_viewer->output_buffer, 0, is_viewer->buffer_pos);
                is_viewer->buffer_pos = 0;
            }
        }
    }
    else
    {
        word = m64p_swap32(word);
        memcpy(&is_viewer->data[address], &word, sizeof(word));
    }
}
