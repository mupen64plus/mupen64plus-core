/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - cop0.h                                                  *
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

#include "r4300.h"
#include "macros.h"
#include "ops.h"
#include "interupt.h"

void MFC0(void)
{
   switch(PC->f.r.nrd)
     {
      case 1:
    printf("lecture de Random\n");
    stop=1;
      default:
    rrt32 = reg_cop0[PC->f.r.nrd];
    sign_extended(rrt);
     }
   PC++;
}

void MTC0(void)
{
  switch(PC->f.r.nrd)
  {
    case 0:    // Index
      Index = rrt & 0x8000003F;
      if ((Index & 0x3F) > 31) 
      {
        printf ("il y a plus de 32 TLB\n");
        stop=1;
      }
      break;
    case 1:    // Random
      break;
    case 2:    // EntryLo0
      EntryLo0 = rrt & 0x3FFFFFFF;
      break;
    case 3:    // EntryLo1
      EntryLo1 = rrt & 0x3FFFFFFF;
      break;
    case 4:    // Context
      Context = (rrt & 0xFF800000) | (Context & 0x007FFFF0);
      break;
    case 5:    // PageMask
      PageMask = rrt & 0x01FFE000;
      break;
    case 6:    // Wired
      Wired = rrt;
      Random = 31;
      break;
    case 8:    // BadVAddr
      break;
    case 9:    // Count
      update_count();
      if (next_interupt <= Count) gen_interupt();
      debug_count += Count;
      translate_event_queue(rrt & 0xFFFFFFFF);
      Count = rrt & 0xFFFFFFFF;
      debug_count -= Count;
      break;
    case 10:   // EntryHi
      EntryHi = rrt & 0xFFFFE0FF;
      break;
    case 11:   // Compare
      update_count();
      remove_event(COMPARE_INT);
      add_interupt_event_count(COMPARE_INT, (unsigned int)rrt);
      Compare = rrt;
      Cause = Cause & 0xFFFF7FFF; //Timer interupt is clear
      break;
    case 12:   // Status
      if((rrt & 0x04000000) != (Status & 0x04000000))
      {
          shuffle_fpr_data(Status, rrt);
          set_fpr_pointers(rrt);
      }
      Status = rrt;
      PC++;
      check_interupt();
      update_count();
      if (next_interupt <= Count) gen_interupt();
      PC--;
      break;
    case 13:   // Cause
      if (rrt!=0)
      {
         printf("Write in Cause\n");
         stop = 1;
      }
      else Cause = rrt;
      break;
    case 14:   // EPC
      EPC = rrt;
      break;
    case 15:  // PRevID
      break;
    case 16:  // Config
      Config = rrt;
      break;
    case 18:  // WatchLo
      WatchLo = rrt & 0xFFFFFFFF;
      break;
    case 19:  // WatchHi
      WatchHi = rrt & 0xFFFFFFFF;
      break;
    case 27:  // CacheErr
      break;
    case 28:  // TagLo
      TagLo = rrt & 0x0FFFFFC0;
      break;
    case 29: // TagHi
      TagHi =0;
      break;
    default:
      printf("unknown mtc0 write : %d\n", PC->f.r.nrd);
      stop=1;
  }
  PC++;
}

