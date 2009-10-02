/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - 7zItem.c                                                *
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

#include "7zItem.h"
#include "7zAlloc.h"

void SzCoderInfoInit(CCoderInfo *coder)
{
  SzByteBufferInit(&coder->Properties);
}

void SzCoderInfoFree(CCoderInfo *coder, void (*freeFunc)(void *p))
{
  SzByteBufferFree(&coder->Properties, freeFunc);
  SzCoderInfoInit(coder);
}

void SzFolderInit(CFolder *folder)
{
  folder->NumCoders = 0;
  folder->Coders = 0;
  folder->NumBindPairs = 0;
  folder->BindPairs = 0;
  folder->NumPackStreams = 0;
  folder->PackStreams = 0;
  folder->UnPackSizes = 0;
  folder->UnPackCRCDefined = 0;
  folder->UnPackCRC = 0;
  folder->NumUnPackStreams = 0;
}

void SzFolderFree(CFolder *folder, void (*freeFunc)(void *p))
{
  UInt32 i;
  for (i = 0; i < folder->NumCoders; i++)
    SzCoderInfoFree(&folder->Coders[i], freeFunc);
  freeFunc(folder->Coders);
  freeFunc(folder->BindPairs);
  freeFunc(folder->PackStreams);
  freeFunc(folder->UnPackSizes);
  SzFolderInit(folder);
}

UInt32 SzFolderGetNumOutStreams(CFolder *folder)
{
  UInt32 result = 0;
  UInt32 i;
  for (i = 0; i < folder->NumCoders; i++)
    result += folder->Coders[i].NumOutStreams;
  return result;
}

int SzFolderFindBindPairForInStream(CFolder *folder, UInt32 inStreamIndex)
{
  UInt32 i;
  for(i = 0; i < folder->NumBindPairs; i++)
    if (folder->BindPairs[i].InIndex == inStreamIndex)
      return i;
  return -1;
}


int SzFolderFindBindPairForOutStream(CFolder *folder, UInt32 outStreamIndex)
{
  UInt32 i;
  for(i = 0; i < folder->NumBindPairs; i++)
    if (folder->BindPairs[i].OutIndex == outStreamIndex)
      return i;
  return -1;
}

CFileSize SzFolderGetUnPackSize(CFolder *folder)
{ 
  int i = (int)SzFolderGetNumOutStreams(folder);
  if (i == 0)
    return 0;
  for (i--; i >= 0; i--)
    if (SzFolderFindBindPairForOutStream(folder, i) < 0)
      return folder->UnPackSizes[i];
  /* throw 1; */
  return 0;
}

/*
int FindPackStreamArrayIndex(int inStreamIndex) const
{
  for(int i = 0; i < PackStreams.Size(); i++)
  if (PackStreams[i] == inStreamIndex)
    return i;
  return -1;
}
*/

void SzFileInit(CFileItem *fileItem)
{
  fileItem->IsFileCRCDefined = 0;
  fileItem->HasStream = 1;
  fileItem->IsDirectory = 0;
  fileItem->IsAnti = 0;
  fileItem->IsLastWriteTimeDefined = 0;
  fileItem->Name = 0;
}

void SzFileFree(CFileItem *fileItem, void (*freeFunc)(void *p))
{
  freeFunc(fileItem->Name);
  SzFileInit(fileItem);
}

void SzArchiveDatabaseInit(CArchiveDatabase *db)
{
  db->NumPackStreams = 0;
  db->PackSizes = 0;
  db->PackCRCsDefined = 0;
  db->PackCRCs = 0;
  db->NumFolders = 0;
  db->Folders = 0;
  db->NumFiles = 0;
  db->Files = 0;
}

void SzArchiveDatabaseFree(CArchiveDatabase *db, void (*freeFunc)(void *))
{
  UInt32 i;
  for (i = 0; i < db->NumFolders; i++)
    SzFolderFree(&db->Folders[i], freeFunc);
  for (i = 0; i < db->NumFiles; i++)
    SzFileFree(&db->Files[i], freeFunc);
  freeFunc(db->PackSizes);
  freeFunc(db->PackCRCsDefined);
  freeFunc(db->PackCRCs);
  freeFunc(db->Folders);
  freeFunc(db->Files);
  SzArchiveDatabaseInit(db);
}

