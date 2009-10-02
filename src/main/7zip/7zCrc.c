/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - 7zCrc.c                                                 *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2008 Tillin9                                            *
 *   7-Zip Copyright (C) 1999-2007 Igor Pavlov.                            *
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

#include "7zCrc.h"

#define kCrcPoly 0xEDB88320
UInt32 g_CrcTable[256];

void MY_FAST_CALL CrcGenerateTable(void)
{
  UInt32 i;
  for (i = 0; i < 256; i++)
  {
    UInt32 r = i;
    int j;
    for (j = 0; j < 8; j++)
      r = (r >> 1) ^ (kCrcPoly & ~((r & 1) - 1));
    g_CrcTable[i] = r;
  }
}

UInt32 MY_FAST_CALL CrcUpdate(UInt32 v, const void *data, size_t size)
{
  const Byte *p = (const Byte *)data;
  for (; size > 0 ; size--, p++) 
    v = CRC_UPDATE_BYTE(v, *p);
  return v;
}

UInt32 MY_FAST_CALL CrcCalc(const void *data, size_t size)
{
  return CrcUpdate(CRC_INIT_VAL, data, size) ^ 0xFFFFFFFF;
}

