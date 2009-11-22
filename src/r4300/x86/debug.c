/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - debug.c                                                 *
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

#include <stdio.h>

#include "assemble.h"

#include "r4300/r4300.h"
#include "r4300/macros.h"
#include "r4300/recomph.h"

#include "memory/memory.h"

void debug(void)
{
#ifdef COMPARE_CORE
   compare_core();
#endif
}

//static void dyna_stop() {}

void stop_it(void)
{
   if (r4300emu == CORE_DYNAREC)
     {
    stop = 1;
    /**return_address = (unsigned int)dyna_stop;
    asm("mov return_address, %%esp \n"
        "ret                       \n"
        :
        :
        : "memory");*/
    /*int i;
    for (i=0; i<0x100000; i++)
      {
    if (((unsigned int)(EPC) >= blocks[i]->start)
    && ((unsigned int)(EPC) < blocks[i]->end))
           blocks[i]->code[blocks[i]->block[(EPC - blocks[i]->start)/4].local_addr] = 0xC3;
      }*/
     }
   else stop=1;
}

