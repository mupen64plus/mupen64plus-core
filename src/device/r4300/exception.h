/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - exception.h                                             *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2002 Hacktarux                                          *
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

#ifndef M64P_DEVICE_R4300_EXCEPTION_H
#define M64P_DEVICE_R4300_EXCEPTION_H

#include <stdint.h>
#include "device/r4300/r4300_core.h"

struct r4300_core;

void poweron_exception(struct cp0* cp0);
void add_exception_to_list(struct r4300_core* r4300);
void remove_exception_from_list(struct r4300_core* r4300);
void check_exception_list(struct r4300_core* r4300);
void clear_exception_list(struct r4300_core* r4300);
void TLB_refill_exception(struct r4300_core* r4300, uint32_t address, int w);
void exception_general(struct r4300_core* r4300);

#endif /* M64P_DEVICE_R4300_EXCEPTION_H */

