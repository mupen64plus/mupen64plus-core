/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - 7zExtract.h                                             *
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

#ifndef __7Z_EXTRACT_H
#define __7Z_EXTRACT_H

#include "7zIn.h"

/*
  SzExtract extracts file from archive

  *outBuffer must be 0 before first call for each new archive. 

  Extracting cache:
    If you need to decompress more than one file, you can send 
    these values from previous call:
      *blockIndex, 
      *outBuffer, 
      *outBufferSize
    You can consider "*outBuffer" as cache of solid block. If your archive is solid, 
    it will increase decompression speed.

    If you use external function, you can declare these 3 cache variables 
    (blockIndex, outBuffer, outBufferSize) as static in that external function.

    Free *outBuffer and set *outBuffer to 0, if you want to flush cache.
*/

SZ_RESULT SzExtract(
    ISzInStream *inStream, 
    CArchiveDatabaseEx *db,
    UInt32 fileIndex,         /* index of file */
    UInt32 *blockIndex,       /* index of solid block */
    Byte **outBuffer,         /* pointer to pointer to output buffer (allocated with allocMain) */
    size_t *outBufferSize,    /* buffer size for output buffer */
    size_t *offset,           /* offset of stream for required file in *outBuffer */
    size_t *outSizeProcessed, /* size of file in *outBuffer */
    ISzAlloc *allocMain,
    ISzAlloc *allocTemp);

#endif

