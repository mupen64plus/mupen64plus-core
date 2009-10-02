/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - 7zBuffer.h                                              *
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

#include "7zBuffer.h"
#include "7zAlloc.h"

void SzByteBufferInit(CSzByteBuffer *buffer)
{
  buffer->Capacity = 0;
  buffer->Items = 0;
}

int SzByteBufferCreate(CSzByteBuffer *buffer, size_t newCapacity, void * (*allocFunc)(size_t size))
{
  buffer->Capacity = newCapacity;
  if (newCapacity == 0)
  {
    buffer->Items = 0;
    return 1;
  }
  buffer->Items = (Byte *)allocFunc(newCapacity);
  return (buffer->Items != 0);
}

void SzByteBufferFree(CSzByteBuffer *buffer, void (*freeFunc)(void *))
{
  freeFunc(buffer->Items);
  buffer->Items = 0;
  buffer->Capacity = 0;
}

