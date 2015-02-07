/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - af_rtc.c                                                *
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

#include "af_rtc.h"

#include "api/m64p_types.h"
#include "api/callbacks.h"

#include <time.h>

static unsigned char byte2bcd(int n)
{
    n %= 100;
    return ((n / 10) << 4) | (n % 10);
}

void af_rtc_status_command(struct pif* pif, int channel, uint8_t* cmd)
{
    /* AF-RTC status query */
    cmd[3] = 0x00;
    cmd[4] = 0x10;
    cmd[5] = 0x00;
}

void af_rtc_read_command(struct pif* pif, int channel, uint8_t* cmd)
{
    time_t curtime_time;
    struct tm *curtime;

    /* read RTC block (cmd[3]: block number) */
    switch (cmd[3])
    {
    case 0:
        cmd[4] = 0x00;
        cmd[5] = 0x02;
        cmd[12] = 0x00;
        break;
    case 1:
        DebugMessage(M64MSG_ERROR, "AF-RTC read command: cannot read block 1");
        break;
    case 2:
        time(&curtime_time);
        curtime = localtime(&curtime_time);
        cmd[4] = byte2bcd(curtime->tm_sec);
        cmd[5] = byte2bcd(curtime->tm_min);
        cmd[6] = 0x80 + byte2bcd(curtime->tm_hour);
        cmd[7] = byte2bcd(curtime->tm_mday);
        cmd[8] = byte2bcd(curtime->tm_wday);
        cmd[9] = byte2bcd(curtime->tm_mon + 1);
        cmd[10] = byte2bcd(curtime->tm_year);
        cmd[11] = byte2bcd(curtime->tm_year / 100);
        cmd[12] = 0x00;	// status
        break;
    }
}

void af_rtc_write_command(struct pif* pif, int channel, uint8_t* cmd)
{
    /* write RTC block */
    DebugMessage(M64MSG_ERROR, "AF-RTC write command: not yet implemented");
}

