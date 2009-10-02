/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - BranchTypes.h                                           *
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

#ifndef __BRANCHTYPES_H
#define __BRANCHTYPES_H

#include <zconf.h>

#ifndef _7ZIP_UINT16_DEFINED
#define _7ZIP_UINT16_DEFINED
typedef unsigned short UInt16;
#endif 

#ifndef _7ZIP_UINT32_DEFINED
#define _7ZIP_UINT32_DEFINED
#ifdef _LZMA_UINT32_IS_ULONG
typedef unsigned long UInt32;
#else
typedef unsigned int UInt32;
#endif
#endif

#ifndef _7ZIP_UINT64_DEFINED
#define _7ZIP_UINT64_DEFINED
#ifdef _SZ_NO_INT_64
typedef unsigned long UInt64;
#else
#if defined(_MSC_VER) || defined(__BORLANDC__)
typedef unsigned __int64 UInt64;
#else
typedef unsigned long long int UInt64;
#endif
#endif
#endif

/* #define _LZMA_NO_SYSTEM_SIZE_T */
/* You can use it, if you don't want <stddef.h> */

#ifndef _7ZIP_SIZET_DEFINED
#define _7ZIP_SIZET_DEFINED
#ifdef _LZMA_NO_SYSTEM_SIZE_T
typedef UInt32 SizeT;
#else
#include <stddef.h>
typedef size_t SizeT;
#endif
#endif

#endif

