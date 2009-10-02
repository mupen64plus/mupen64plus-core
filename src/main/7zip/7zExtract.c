/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - 7zExtract.c                                             *
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

#include "7zExtract.h"
#include "7zDecode.h"
#include "7zCrc.h"

SZ_RESULT SzExtract(
    ISzInStream *inStream, 
    CArchiveDatabaseEx *db,
    UInt32 fileIndex,
    UInt32 *blockIndex,
    Byte **outBuffer, 
    size_t *outBufferSize,
    size_t *offset, 
    size_t *outSizeProcessed, 
    ISzAlloc *allocMain,
    ISzAlloc *allocTemp)
{
  UInt32 folderIndex = db->FileIndexToFolderIndexMap[fileIndex];
  SZ_RESULT res = SZ_OK;
  *offset = 0;
  *outSizeProcessed = 0;
  if (folderIndex == (UInt32)-1)
  {
    allocMain->Free(*outBuffer);
    *blockIndex = folderIndex;
    *outBuffer = 0;
    *outBufferSize = 0;
    return SZ_OK;
  }

  if (*outBuffer == 0 || *blockIndex != folderIndex)
  {
    CFolder *folder = db->Database.Folders + folderIndex;
    CFileSize unPackSizeSpec = SzFolderGetUnPackSize(folder);
    size_t unPackSize = (size_t)unPackSizeSpec;
    CFileSize startOffset = SzArDbGetFolderStreamPos(db, folderIndex, 0);
    #ifndef _LZMA_IN_CB
    Byte *inBuffer = 0;
    size_t processedSize;
    CFileSize packSizeSpec;
    size_t packSize;
    RINOK(SzArDbGetFolderFullPackSize(db, folderIndex, &packSizeSpec));
    packSize = (size_t)packSizeSpec;
    if (packSize != packSizeSpec)
      return SZE_OUTOFMEMORY;
    #endif
    if (unPackSize != unPackSizeSpec)
      return SZE_OUTOFMEMORY;
    *blockIndex = folderIndex;
    allocMain->Free(*outBuffer);
    *outBuffer = 0;
    
    RINOK(inStream->Seek(inStream, startOffset));
    
    #ifndef _LZMA_IN_CB
    if (packSize != 0)
    {
      inBuffer = (Byte *)allocTemp->Alloc(packSize);
      if (inBuffer == 0)
        return SZE_OUTOFMEMORY;
    }
    res = inStream->Read(inStream, inBuffer, packSize, &processedSize);
    if (res == SZ_OK && processedSize != packSize)
      res = SZE_FAIL;
    #endif
    if (res == SZ_OK)
    {
      *outBufferSize = unPackSize;
      if (unPackSize != 0)
      {
        *outBuffer = (Byte *)allocMain->Alloc(unPackSize);
        if (*outBuffer == 0)
          res = SZE_OUTOFMEMORY;
      }
      if (res == SZ_OK)
      {
        res = SzDecode(db->Database.PackSizes + 
          db->FolderStartPackStreamIndex[folderIndex], folder, 
          #ifdef _LZMA_IN_CB
          inStream, startOffset, 
          #else
          inBuffer, 
          #endif
          *outBuffer, unPackSize, allocTemp);
        if (res == SZ_OK)
        {
          if (folder->UnPackCRCDefined)
          {
            if (CrcCalc(*outBuffer, unPackSize) != folder->UnPackCRC)
              res = SZE_CRC_ERROR;
          }
        }
      }
    }
    #ifndef _LZMA_IN_CB
    allocTemp->Free(inBuffer);
    #endif
  }
  if (res == SZ_OK)
  {
    UInt32 i; 
    CFileItem *fileItem = db->Database.Files + fileIndex;
    *offset = 0;
    for(i = db->FolderStartFileIndex[folderIndex]; i < fileIndex; i++)
      *offset += (UInt32)db->Database.Files[i].Size;
    *outSizeProcessed = (size_t)fileItem->Size;
    if (*offset + *outSizeProcessed > *outBufferSize)
      return SZE_FAIL;
    {
      if (fileItem->IsFileCRCDefined)
      {
        if (CrcCalc(*outBuffer + *offset, *outSizeProcessed) != fileItem->FileCRC)
          res = SZE_CRC_ERROR;
      }
    }
  }
  return res;
}

