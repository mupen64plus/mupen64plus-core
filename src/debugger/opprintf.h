/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - opprintf.h                                              *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2008 ZZT32                                              *
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

#ifndef __OPPRINTF_H__
#define __OPPRINTF_H__

/* Macros */
#define MR4KD_SPACING(x)     ((x) << 24)
#define MR4KD_SET_SPACE(x)   (mr4kd_conf |= (x) << 24)
#define MR4KD_GET_SPACE(x)   (mr4kd_conf >> 24 & 0xFF)
#define MR4KD_FLAG_SET(x)    (mr4kd_conf |=  (x))
#define MR4KD_FLAG_GET(x)    (mr4kd_conf &   (x))
#define MR4KD_FLAG_CLEAR(x)  (mr4kd_conf &= ~(x))

/* Disassembler flags */
enum
{
    /* Register display options */
    MR4KD_RTYPE0  = 0x00000001, /*  K0 */
    MR4KD_RTYPE1  = 0x00000002, /*  21 */
    MR4KD_RPREFIX = 0x00000004, /* $K0 */
    MR4KD_RLOWER  = 0x00000008, /*  k0 */

    /* Opcode display options */
    MR4KD_OLOWER  = 0x00000010, /* mfhi */

    /* Number display options */
    MR4KD_HLOWER  = 0x00000020, /* 0xffff */
};

/* Controlling functions (global) */
void mr4kd_flag_set   ( int flag  );
void mr4kd_flag_clear ( int flag  );
void mr4kd_spacing    ( int space );
int  mr4kd_flag_get   ( int flag  );

/* Controlling functions (app-wide) */
int mr4kd_sprintf ( char *dest, char *name, uint32 instruction, uint32 pc, char *fmt );

#endif /* __OPPRINTF_H__ */

