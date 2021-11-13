/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - is_viewer.h                                             *
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

#ifndef M64P_DEVICE_PI_IS_VIEWER_H
#define M64P_DEVICE_PI_IS_VIEWER_H

#include <stddef.h>
#include <stdint.h>

#define IS_BUFFER_SIZE 0x1000

struct is_viewer
{
    char data[IS_BUFFER_SIZE];
    char output_buffer[IS_BUFFER_SIZE];
    uint32_t buffer_pos;
};

void poweron_is_viewer(struct is_viewer* is_viewer);

void read_is_viewer(void* opaque, uint32_t address, uint32_t* value);
void write_is_viewer(void* opaque, uint32_t address, uint32_t value, uint32_t mask);

#endif
