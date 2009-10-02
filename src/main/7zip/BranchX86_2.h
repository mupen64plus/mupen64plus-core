/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - BranchX86_2.h                                           *
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

#ifndef __BRANCHX86_2_H
#define __BRANCHX86_2_H

#include "BranchTypes.h"

#define BCJ2_RESULT_OK 0
#define BCJ2_RESULT_DATA_ERROR 1

/*
Conditions:
  outSize <= FullOutputSize, 
  where FullOutputSize is full size of output stream of x86_2 filter.

If buf0 overlaps outBuf, there are two required conditions:
  1) (buf0 >= outBuf)
  2) (buf0 + size0 >= outBuf + FullOutputSize).
*/

int x86_2_Decode(
    const Byte *buf0, SizeT size0, 
    const Byte *buf1, SizeT size1, 
    const Byte *buf2, SizeT size2, 
    const Byte *buf3, SizeT size3, 
    Byte *outBuf, SizeT outSize);

#endif

