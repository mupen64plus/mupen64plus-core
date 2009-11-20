/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - compare_core.h                                          *
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

#include <sys/stat.h>

#include "r4300.h"

#include "memory/memory.h"
#ifndef __WIN32__
#include "main/winlnxdefs.h"
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include "plugin/plugin.h"
#include "r4300/recomph.h"

static FILE *f;
static int pipe_opened = 0;
static long long int comp_reg[32];
extern unsigned int op;
extern unsigned int interp_addr;
static unsigned int old_op;

void display_error(char *txt)
{
   int i;
   unsigned int *comp_reg2 = (unsigned int *)comp_reg;
   printf("err: %6s  ", txt);
   if (r4300emu == CORE_PURE_INTERPRETER)
     {
    printf("addr:%x\t ", (int)interp_addr);
    if (!strcmp(txt, "PC")) printf("My PC: %x  Ref PC: %x\t ", (int)interp_addr, *(int*)&comp_reg[0]);
     }
   else
     {
    printf("addr:%x\t ", (int)PC->addr);
    if (!strcmp(txt, "PC")) printf("My PC: %x  Ref PC: %x\t ", (int)PC->addr, *(int*)&comp_reg[0]);
     }
   
   if (!strcmp(txt, "gpr"))
       {
      for (i=0; i<32; i++)
        {
           if (reg[i] != comp_reg[i])
         printf("My: reg[%d]=%llx\t Ref: reg[%d]=%llx\t ",
            i, reg[i], i, comp_reg[i]);
        }
       }
   if (!strcmp(txt, "cop0"))
       {
      for (i=0; i<32; i++)
        {
           if (reg_cop0[i] != comp_reg2[i])
         printf("My: reg_cop0[%d]=%x\t Ref: reg_cop0[%d]=%x\t ",
            i, (unsigned int)reg_cop0[i], i, (unsigned int)comp_reg2[i]);
        }
       }
   printf("\n");
   /*for (i=0; i<32; i++)
     {
    if (reg_cop0[i] != comp_reg[i])
      printf("reg_cop0[%d]=%llx != reg[%d]=%llx\n",
         i, reg_cop0[i], i, comp_reg[i]);
     }*/
   
   stop_it();
}

void check_input_sync(unsigned char *value)
{
    if (r4300emu == CORE_DYNAREC || r4300emu == CORE_PURE_INTERPRETER)
    {
        if (fread(value, 1, 4, f) != 4)
            stop_it();
    }
    else
    {
        if (fwrite(value, 1, 4, f) != 4)
            stop_it();
    }
}

void compare_core()
{
#ifndef __WIN32__   
   static int comparecnt = 0;
   int iFirst = 1;
   char errHead[128];
   sprintf(errHead, "Compare #%i  old_op: %x op: %x\n", comparecnt++, (int) old_op, (int) op);

   if (r4300emu == CORE_DYNAREC || r4300emu == CORE_PURE_INTERPRETER)
     {
    if (!pipe_opened)
      {
         mkfifo("compare_pipe", 0600);
         printf("Waiting to read pipe.\n");
         f = fopen("compare_pipe", "r");
         pipe_opened = 1;
      }
    
    if (fread(comp_reg, sizeof(int), 4, f) != 4)
        printf("compare_core: fread() failed");
    if (r4300emu == CORE_PURE_INTERPRETER)
      {
         if (memcmp(&interp_addr, comp_reg, 4))
         {
           if (iFirst) { printf("%s", errHead); iFirst = 0; }
           display_error("PC");
         }
      }
    else
      {
         if (memcmp(&PC->addr, comp_reg, 4))
         {
           if (iFirst) { printf("%s", errHead); iFirst = 0; }
           display_error("PC");
         }
      }
    if (fread (comp_reg, sizeof(long long int), 32, f) != 32)
        printf("compare_core: fread() failed");
    if (memcmp(reg, comp_reg, 32*sizeof(long long int)))
    {
      if (iFirst) { printf("%s", errHead); iFirst = 0; }
      display_error("gpr");
    }
    if (fread(comp_reg, sizeof(int), 32, f) != 32)
        printf("compare_core: fread() failed");
    if (memcmp(reg_cop0, comp_reg, 32*sizeof(int)))
    {
      if (iFirst) { printf("%s", errHead); iFirst = 0; }
      display_error("cop0");
      }
    if (fread (comp_reg, sizeof(long long int), 32, f) != 32)
        printf("compare_core: fread() failed");
    if (memcmp(reg_cop1_fgr_64, comp_reg, 32*sizeof(long long int)))
    {
      if (iFirst) { printf("%s", errHead); iFirst = 0; }
      display_error("cop1");
    }
    /*fread(comp_reg, 1, sizeof(int), f);
    if (memcmp(&rdram[0x31280/4], comp_reg, sizeof(int)))
      display_error("mem");*/
    /*fread (comp_reg, 4, 1, f);
    if (memcmp(&FCR31, comp_reg, 4))
      display_error();*/
    old_op = op;
     }
   else
     {
    if (!pipe_opened)
      {
         printf("Waiting to write pipe.\n");
         f = fopen("compare_pipe", "w");
         pipe_opened = 1;
      }
    
    if (fwrite(&PC->addr, sizeof(int), 4, f) != 4 ||
        fwrite(reg, sizeof(long long int), 32, f) != 32 ||
        fwrite(reg_cop0, sizeof(int), 32, f) != 32 ||
        fwrite(reg_cop1_fgr_64, sizeof(long long int), 32, f) != 32)
        printf("compare_core: write() failed");
    /*fwrite(&rdram[0x31280/4], 1, sizeof(int), f);
    fwrite(&FCR31, 4, 1, f);*/
     }
#endif
}

