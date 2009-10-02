/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - BranchX86.c                                             *
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

#include "BranchX86.h"

#define Test86MSByte(b) ((b) == 0 || (b) == 0xFF)

const Byte kMaskToAllowedStatus[8] = {1, 1, 1, 0, 1, 0, 0, 0};
const Byte kMaskToBitNumber[8] = {0, 1, 2, 2, 3, 3, 3, 3};

SizeT x86_Convert(Byte *buffer, SizeT endPos, UInt32 nowPos, UInt32 *prevMaskMix, int encoding)
{
  SizeT bufferPos = 0, prevPosT;
  UInt32 prevMask = *prevMaskMix & 0x7;
  if (endPos < 5)
    return 0;
  nowPos += 5;
  prevPosT = (SizeT)0 - 1;

  for(;;)
  {
    Byte *p = buffer + bufferPos;
    Byte *limit = buffer + endPos - 4;
    for (; p < limit; p++)
      if ((*p & 0xFE) == 0xE8)
        break;
    bufferPos = (SizeT)(p - buffer);
    if (p >= limit)
      break;
    prevPosT = bufferPos - prevPosT;
    if (prevPosT > 3)
      prevMask = 0;
    else
    {
      prevMask = (prevMask << ((int)prevPosT - 1)) & 0x7;
      if (prevMask != 0)
      {
        Byte b = p[4 - kMaskToBitNumber[prevMask]];
        if (!kMaskToAllowedStatus[prevMask] || Test86MSByte(b))
        {
          prevPosT = bufferPos;
          prevMask = ((prevMask << 1) & 0x7) | 1;
          bufferPos++;
          continue;
        }
      }
    }
    prevPosT = bufferPos;

    if (Test86MSByte(p[4]))
    {
      UInt32 src = ((UInt32)p[4] << 24) | ((UInt32)p[3] << 16) | ((UInt32)p[2] << 8) | ((UInt32)p[1]);
      UInt32 dest;
      for (;;)
      {
        Byte b;
        int index;
        if (encoding)
          dest = (nowPos + (UInt32)bufferPos) + src;
        else
          dest = src - (nowPos + (UInt32)bufferPos);
        if (prevMask == 0)
          break;
        index = kMaskToBitNumber[prevMask] * 8;
        b = (Byte)(dest >> (24 - index));
        if (!Test86MSByte(b))
          break;
        src = dest ^ ((1 << (32 - index)) - 1);
      }
      p[4] = (Byte)(~(((dest >> 24) & 1) - 1));
      p[3] = (Byte)(dest >> 16);
      p[2] = (Byte)(dest >> 8);
      p[1] = (Byte)dest;
      bufferPos += 5;
    }
    else
    {
      prevMask = ((prevMask << 1) & 0x7) | 1;
      bufferPos++;
    }
  }
  prevPosT = bufferPos - prevPosT;
  *prevMaskMix = ((prevPosT > 3) ? 0 : ((prevMask << ((int)prevPosT - 1)) & 0x7));
  return bufferPos;
}

