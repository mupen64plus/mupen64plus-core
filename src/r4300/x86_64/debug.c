/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - debug.c                                                 *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2007 Richard Goedeken (Richard42)                       *
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

#include "../r4300.h"
#include "../macros.h"
#include "../recomph.h"

#include "../../memory/memory.h"

void debug(void)
{
#ifdef COMPARE_CORE
   compare_core();
#endif
   //if (Count > 0x8000000)
     //printf("PC->addr:%x:%x\n",(int)PC->addr, 
      /*(int)SP_DMEM[(PC->addr - 0xa4000000)/4]*/
      //(int)rdram[(PC->addr & 0xFFFFFF)/4]);
   //printf("count:%x\n", (int)(Count));
   /*if (debug_count + Count >= 0x80000000) 
     printf("debug : %x: %x\n", 
        (unsigned int)(PC->addr),
        (unsigned int)rdram[(PC->addr&0xFFFFFF)/4]);*/
   /*if (debug_count + Count >= 0x8000000) {
      printf("debug : %x\n", (unsigned int)(PC->addr));
      if (0x8018ddd8>actual->debut && 0x8018ddd8<actual->fin) {
     printf("ff: %x\n", //rdram[0x18ddd8/4]
           actual->code[actual->block[(0x8018ddd8-actual->debut)/4].local_addr]);
     getchar();
      }
   }*/
   //if (debug_count + Count >= 0x8000000) actual->code[(PC+1)->local_addr] = 0xC3;
   //if ((debug_count + Count) >= 0x5f66c82)
   //if ((debug_count + Count) >= 0x5f61bc0)
   /*if ((debug_count + Count) == 0xf203ae0)
     {
    int j;
    for (j=0; j<NBR_BLOCKS; j++)
      {
         if (aux[j].debut) {
        printf("deb:%x\n", aux[j].debut);
        printf("fin:%x\n", aux[j].fin);
        printf("valide:%x\n", aux[j].valide);
        getchar();
         }
      }
     }
   if ((debug_count + Count) >= 0xf203ae0)
     {
    int j;
    printf ("inst:%x\n", 
        (unsigned int)rdram[(PC->addr&0xFFFFFF)/4]);
    printf ("PC=%x\n", (unsigned int)((PC+1)->addr));
    for (j=0; j<16; j++)
      printf ("reg[%d]:%x%x       reg[%d]:%x%x\n",
          j,
          (unsigned int)(reg[j] >> 32),
          (unsigned int)reg[j],
          j+16,
          (unsigned int)(reg[j+16] >> 32),
          (unsigned int)reg[j+16]);
    printf("hi:%x%x        lo:%x%x\n",
           (unsigned int)(hi >> 32),
           (unsigned int)hi,
           (unsigned int)(lo >> 32),
           (unsigned int)lo);
    printf("aprs %d instructions soit %x\n", 
           (unsigned int)(debug_count+Count),
           (unsigned int)(debug_count+Count));
    getchar();  
     }*/
}

//static void dyna_stop() {}

void stop_it(void)
{
   if (dynacore)
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

