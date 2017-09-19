/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - mempak.h                                                *
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

#ifndef M64P_DEVICE_SI_MEMPAK_H
#define M64P_DEVICE_SI_MEMPAK_H

#include <stddef.h>
#include <stdint.h>

struct storage_backend;

struct mempak
{
    struct storage_backend* storage;
};

enum { MEMPAK_SIZE = 0x8000 };

void format_mempak(uint8_t* mempak);

void init_mempak(struct mempak* mpk, struct storage_backend* storage);

void mempak_read_command(struct mempak* mpk, uint16_t address, uint8_t* data, size_t size);
void mempak_write_command(struct mempak* mpk, uint16_t address, const uint8_t* data, size_t size);

#endif
