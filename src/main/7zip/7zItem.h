/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - 7zItem.h                                                *
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

#ifndef __7Z_ITEM_H
#define __7Z_ITEM_H

#include "7zMethodID.h"
#include "7zHeader.h"
#include "7zBuffer.h"

typedef struct _CCoderInfo
{
  UInt32 NumInStreams;
  UInt32 NumOutStreams;
  CMethodID MethodID;
  CSzByteBuffer Properties;
}CCoderInfo;

void SzCoderInfoInit(CCoderInfo *coder);
void SzCoderInfoFree(CCoderInfo *coder, void (*freeFunc)(void *p));

typedef struct _CBindPair
{
  UInt32 InIndex;
  UInt32 OutIndex;
}CBindPair;

typedef struct _CFolder
{
  UInt32 NumCoders;
  CCoderInfo *Coders;
  UInt32 NumBindPairs;
  CBindPair *BindPairs;
  UInt32 NumPackStreams; 
  UInt32 *PackStreams;
  CFileSize *UnPackSizes;
  int UnPackCRCDefined;
  UInt32 UnPackCRC;

  UInt32 NumUnPackStreams;
}CFolder;

void SzFolderInit(CFolder *folder);
CFileSize SzFolderGetUnPackSize(CFolder *folder);
int SzFolderFindBindPairForInStream(CFolder *folder, UInt32 inStreamIndex);
UInt32 SzFolderGetNumOutStreams(CFolder *folder);
CFileSize SzFolderGetUnPackSize(CFolder *folder);

typedef struct _CArchiveFileTime
{
  UInt32 Low;
  UInt32 High;
} CArchiveFileTime;

typedef struct _CFileItem
{
  CArchiveFileTime LastWriteTime;
  /*
  CFileSize StartPos;
  UInt32 Attributes; 
  */
  CFileSize Size;
  UInt32 FileCRC;
  char *Name;

  Byte IsFileCRCDefined;
  Byte HasStream;
  Byte IsDirectory;
  Byte IsAnti;
  Byte IsLastWriteTimeDefined;
  /*
  int AreAttributesDefined;
  int IsLastWriteTimeDefined;
  int IsStartPosDefined;
  */
}CFileItem;

void SzFileInit(CFileItem *fileItem);

typedef struct _CArchiveDatabase
{
  UInt32 NumPackStreams;
  CFileSize *PackSizes;
  Byte *PackCRCsDefined;
  UInt32 *PackCRCs;
  UInt32 NumFolders;
  CFolder *Folders;
  UInt32 NumFiles;
  CFileItem *Files;
}CArchiveDatabase;

void SzArchiveDatabaseInit(CArchiveDatabase *db);
void SzArchiveDatabaseFree(CArchiveDatabase *db, void (*freeFunc)(void *));


#endif

