/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - af_rtc.h                                                *
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

#ifndef M64P_SI_AF_RTC_H
#define M64P_SI_AF_RTC_H

#include <stdint.h>

struct clock_backend;

struct af_rtc
{
    struct clock_backend* rtc;
};

void init_af_rtc(struct af_rtc* afrtc,
                 struct clock_backend* rtc);

void af_rtc_status_command(struct af_rtc* afrtc, uint8_t* cmd);
void af_rtc_read_command(struct af_rtc* afrtc, uint8_t* cmd);
void af_rtc_write_command(struct af_rtc* afrtc, uint8_t* cmd);

#endif
