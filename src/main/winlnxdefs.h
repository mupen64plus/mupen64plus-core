/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - winlnxdefs.h                                            *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2002 Hacktarux                                          *
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

#ifndef WINLNXDEFS_H
#define WINLNXDEFS_H

#ifdef __WIN32__
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <winbase.h>
# define sleep(x) Sleep(x*1000)
#else
typedef unsigned int BOOL;
typedef unsigned int DWORD;
typedef unsigned short WORD;
typedef unsigned char BYTE;
typedef unsigned int UINT;
typedef unsigned long long DWORD64;

typedef short SHORT;

typedef int __int32;

typedef int HINSTANCE;
typedef int HWND;
typedef int WPARAM;
typedef int LPARAM;
typedef void* LPVOID;

#define __declspec(dllexport) __attribute__((visibility("default")))
#define _cdecl
#define __stdcall
#define WINAPI

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#endif // __WIN32__
#endif // WINLNXDEFS_H

