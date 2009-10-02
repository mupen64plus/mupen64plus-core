/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - 7zMain.c                                                *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2008 Tillin9                                            *
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

#include "7zMain.h"

#ifdef _LZMA_IN_CB

#define kBufferSize (1 << 12)
Byte g_Buffer[kBufferSize];

SZ_RESULT SzFileReadImp(void *object, void **buffer, size_t maxRequiredSize, size_t *processedSize)
{
  CFileInStream *s = (CFileInStream *)object;
  size_t processedSizeLoc;
  if (maxRequiredSize > kBufferSize)
    maxRequiredSize = kBufferSize;
  processedSizeLoc = fread(g_Buffer, 1, maxRequiredSize, s->File);
  *buffer = g_Buffer;
  if (processedSize != 0)
    *processedSize = processedSizeLoc;
  return SZ_OK;
}

#else

SZ_RESULT SzFileReadImp(void *object, void *buffer, size_t size, size_t *processedSize)
{
  CFileInStream *s = (CFileInStream *)object;
  size_t processedSizeLoc = fread(buffer, 1, size, s->File);
  if (processedSize != 0)
    *processedSize = processedSizeLoc;
  return SZ_OK;
}

#endif

SZ_RESULT SzFileSeekImp(void *object, CFileSize pos)
{
  CFileInStream *s = (CFileInStream *)object;

  int res = fseek(s->File, (long)pos, SEEK_SET);
  if (res == 0)
    return SZ_OK;
  return SZE_FAIL;
}


