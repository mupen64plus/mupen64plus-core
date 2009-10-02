/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - 7zIn.h                                                  *
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

#ifndef __7Z_IN_H
#define __7Z_IN_H

#include "7zHeader.h"
#include "7zItem.h"
#include "7zAlloc.h"
 
typedef struct _CInArchiveInfo
{
  CFileSize StartPositionAfterHeader; 
  CFileSize DataStartPosition;
}CInArchiveInfo;

typedef struct _CArchiveDatabaseEx
{
  CArchiveDatabase Database;
  CInArchiveInfo ArchiveInfo;
  UInt32 *FolderStartPackStreamIndex;
  CFileSize *PackStreamStartPositions;
  UInt32 *FolderStartFileIndex;
  UInt32 *FileIndexToFolderIndexMap;
}CArchiveDatabaseEx;

void SzArDbExInit(CArchiveDatabaseEx *db);
void SzArDbExFree(CArchiveDatabaseEx *db, void (*freeFunc)(void *));
CFileSize SzArDbGetFolderStreamPos(CArchiveDatabaseEx *db, UInt32 folderIndex, UInt32 indexInFolder);
int SzArDbGetFolderFullPackSize(CArchiveDatabaseEx *db, UInt32 folderIndex, CFileSize *resSize);

typedef struct _ISzInStream
{
  #ifdef _LZMA_IN_CB
  SZ_RESULT (*Read)(
      void *object,           /* pointer to ISzInStream itself */
      void **buffer,          /* out: pointer to buffer with data */
      size_t maxRequiredSize, /* max required size to read */
      size_t *processedSize); /* real processed size. 
                                 processedSize can be less than maxRequiredSize.
                                 If processedSize == 0, then there are no more 
                                 bytes in stream. */
  #else
  SZ_RESULT (*Read)(void *object, void *buffer, size_t size, size_t *processedSize);
  #endif
  SZ_RESULT (*Seek)(void *object, CFileSize pos);
} ISzInStream;

 
int SzArchiveOpen(
    ISzInStream *inStream, 
    CArchiveDatabaseEx *db,
    ISzAlloc *allocMain, 
    ISzAlloc *allocTemp);
 
#endif

