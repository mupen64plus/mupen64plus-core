/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - 7zHeader.h                                              *
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

#ifndef __7Z_HEADER_H
#define __7Z_HEADER_H

#include "Types.h"

#define k7zSignatureSize 6
extern Byte k7zSignature[k7zSignatureSize];

#define k7zMajorVersion 0

#define k7zStartHeaderSize 0x20

enum EIdEnum
{
  k7zIdEnd,
    
  k7zIdHeader,
    
  k7zIdArchiveProperties,
    
  k7zIdAdditionalStreamsInfo,
  k7zIdMainStreamsInfo,
  k7zIdFilesInfo,
  
  k7zIdPackInfo,
  k7zIdUnPackInfo,
  k7zIdSubStreamsInfo,
  
  k7zIdSize,
  k7zIdCRC,
  
  k7zIdFolder,
  
  k7zIdCodersUnPackSize,
  k7zIdNumUnPackStream,
  
  k7zIdEmptyStream,
  k7zIdEmptyFile,
  k7zIdAnti,
  
  k7zIdName,
  k7zIdCreationTime,
  k7zIdLastAccessTime,
  k7zIdLastWriteTime,
  k7zIdWinAttributes,
  k7zIdComment,
  
  k7zIdEncodedHeader,
  
  k7zIdStartPos
};

#endif

