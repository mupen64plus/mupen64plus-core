/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - MIPS (native) endian definitions                        *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2015 Nebuleon <nebuleon.fumika@gmail.com>               *
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

#ifndef M64P_TRACE_JIT_MIPS32_NATIVE_ENDIAN_H
#define M64P_TRACE_JIT_MIPS32_NATIVE_ENDIAN_H

#include <stdint.h>

#if defined(__MIPSEB) && !defined(M64P_BIG_ENDIAN)
#  define M64P_BIG_ENDIAN
#endif

#if defined(M64P_BIG_ENDIAN)
#  define LO8_64_BYTES  7
#  define LO16_64_BYTES 6
#  define LO32_64_BYTES 4
#  define HI32_64_BYTES 0
#  define LO16_32_BYTES 2
#  define HI16_32_BYTES 0
#else
#  define LO8_64_BYTES  0
#  define LO16_64_BYTES 0
#  define LO32_64_BYTES 0
#  define HI32_64_BYTES 4
#  define LO16_32_BYTES 0
#  define HI16_32_BYTES 2
#endif

#define LO32_64(addr) ((uint32_t*) ((uintptr_t) (addr) + LO32_64_BYTES))
#define HI32_64(addr) ((uint32_t*) ((uintptr_t) (addr) + HI32_64_BYTES))
#define LO16_32(addr) ((uint16_t*) ((uintptr_t) (addr) + LO16_32_BYTES))
#define HI16_32(addr) ((uint16_t*) ((uintptr_t) (addr) + HI16_32_BYTES))

#endif /* !M64P_TRACE_JIT_MIPS32_NATIVE_ENDIAN_H */
