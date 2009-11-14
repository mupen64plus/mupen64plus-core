/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - r4300.c                                                 *
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

#include <stdlib.h>
#include <string.h>

#include "r4300.h"
#include "ops.h"
#include "exception.h"
#include "interupt.h"
#include "macros.h"
#include "recomp.h"
#include "recomph.h"

#include "memory/memory.h"
#include "main/main.h"
#include "main/rom.h"

#ifdef DBG
#include "debugger/dbg_types.h"
#include "debugger/debugger.h"
#endif

unsigned int r4300emu = 0;
int no_compiled_jump = 0;
int stop, llbit, rompause;
long long int reg[32], hi, lo;
long long int local_rs, local_rt;
unsigned int reg_cop0[32];
int local_rs32, local_rt32;
unsigned int jump_target;
float *reg_cop1_simple[32];
double *reg_cop1_double[32];
long long int reg_cop1_fgr_64[32];
int FCR0, FCR31;
tlb tlb_e[32];
unsigned int delay_slot, skip_jump = 0, dyna_interp = 0, last_addr;
unsigned long long int debug_count = 0;
unsigned int next_interupt, CIC_Chip;
precomp_instr *PC;
char invalid_code[0x100000];

precomp_block *blocks[0x100000], *actual;
int rounding_mode = 0x33F, trunc_mode = 0xF3F, round_mode = 0x33F,
    ceil_mode = 0xB3F, floor_mode = 0x73F;
void (*code)();

/*#define check_memory() \
   if (!invalid_code[address>>12]) \
       invalid_code[address>>12] = 1;*/

#define check_memory() \
   if (!invalid_code[address>>12]) \
       if (blocks[address>>12]->block[(address&0xFFF)/4].ops != NOTCOMPILED) \
     invalid_code[address>>12] = 1;

void NI()
{
   printf("NI() @ %x\n", (int)PC->addr);
   printf("opcode not implemented : ");
   if (PC->addr >= 0xa4000000 && PC->addr < 0xa4001000)
     printf("%x:%x\n", (int)PC->addr, (int)SP_DMEM[(PC->addr-0xa4000000)/4]);
   else
     printf("%x:%x\n", (int)PC->addr, (int)rdram[(PC->addr-0x80000000)/4]);
   stop=1;
}

void RESERVED()
{
   printf("reserved opcode : ");
   if (PC->addr >= 0xa4000000 && PC->addr < 0xa4001000)
     printf("%x:%x\n", (int)PC->addr, (int)SP_DMEM[(PC->addr-0xa4000000)/4]);
   else
     printf("%x:%x\n", (int)PC->addr, (int)rdram[(PC->addr-0x80000000)/4]);
   stop=1;
}

void FIN_BLOCK()
{
   if (!delay_slot)
     {
    jump_to((PC-1)->addr+4);
/*#ifdef DBG
            if (debugger_mode) update_debugger(PC->addr);
#endif
Used by dynarec only, check should be unnecessary
*/
    PC->ops();
    if (r4300emu == CORE_DYNAREC) dyna_jump();
     }
   else
     {
    precomp_block *blk = actual;
    precomp_instr *inst = PC;
    jump_to((PC-1)->addr+4);
    
/*#ifdef DBG
            if (debugger_mode) update_debugger(PC->addr);
#endif
Used by dynarec only, check should be unnecessary
*/
    if (!skip_jump)
      {
         PC->ops();
         actual = blk;
         PC = inst+1;
      }
    else
      PC->ops();
    
    if (r4300emu == CORE_DYNAREC) dyna_jump();
     }
}

void J()
{
   PC++;
   delay_slot=1;
#ifdef DBG
            if (debugger_mode) update_debugger(PC->addr);
#endif
   PC->ops();
   update_count();
   delay_slot=0;
   if (!skip_jump)
     PC=actual->block+
     (((((PC-2)->f.j.inst_index<<2) | ((PC-1)->addr & 0xF0000000))-actual->start)>>2);
   last_addr = PC->addr;
   if (next_interupt <= Count) gen_interupt();
}

void J_OUT()
{
   jump_target = (PC->addr & 0xF0000000) | (PC->f.j.inst_index<<2);
   PC++;
   delay_slot=1;
#ifdef DBG
            if (debugger_mode) update_debugger(PC->addr);
#endif
   PC->ops();
   update_count();
   delay_slot=0;
   if (!skip_jump)
     jump_to(jump_target);
   last_addr = PC->addr;
   if (next_interupt <= Count) gen_interupt();
}

void J_IDLE()
{
   int skip;
   update_count();
   skip = next_interupt - Count;
   if (skip > 3) Count += (skip & 0xFFFFFFFC);
   else J();
}

void JAL()
{
   PC++;
   delay_slot=1;
#ifdef DBG
            if (debugger_mode) update_debugger(PC->addr);
#endif
   PC->ops();
   update_count();
   delay_slot=0;
   if (!skip_jump)
     {
    reg[31]=PC->addr;
    sign_extended(reg[31]);
    
    PC=actual->block+
      (((((PC-2)->f.j.inst_index<<2) | ((PC-1)->addr & 0xF0000000))-actual->start)>>2);
     }
   last_addr = PC->addr;
   if (next_interupt <= Count) gen_interupt();
}

void JAL_OUT()
{
   jump_target = (PC->addr & 0xF0000000) | (PC->f.j.inst_index<<2);
   PC++;
   delay_slot=1;
#ifdef DBG
            if (debugger_mode) update_debugger(PC->addr);
#endif
   PC->ops();
   update_count();
   delay_slot=0;
   if (!skip_jump)
     {
    reg[31]=PC->addr;
    sign_extended(reg[31]);
    
    jump_to(jump_target);
     }
   last_addr = PC->addr;
   if (next_interupt <= Count) gen_interupt();
}

void JAL_IDLE()
{
   int skip;
   update_count();
   skip = next_interupt - Count;
   if (skip > 3) Count += (skip & 0xFFFFFFFC);
   else JAL();
}

void BEQ()
{
   local_rs = irs;
   local_rt = irt;
   PC++;
   delay_slot=1;
#ifdef DBG
            if (debugger_mode) update_debugger(PC->addr);
#endif
   PC->ops();
   update_count();
   delay_slot=0;
   if (local_rs == local_rt && !skip_jump)
     PC += (PC-2)->f.i.immediate-1;
   last_addr = PC->addr;
   if (next_interupt <= Count) gen_interupt();
}

void BEQ_OUT()
{
   local_rs = irs;
   local_rt = irt;
   jump_target = (int)PC->f.i.immediate;
   PC++;
   delay_slot=1;
#ifdef DBG
            if (debugger_mode) update_debugger(PC->addr);
#endif
   PC->ops();
   update_count();
   delay_slot=0;
   if (!skip_jump && local_rs == local_rt)
     jump_to(PC->addr + ((jump_target-1)<<2));
   last_addr = PC->addr;
   if (next_interupt <= Count) gen_interupt();
}

void BEQ_IDLE()
{
   int skip;
   if (irs == irt)
     {
    update_count();
    skip = next_interupt - Count;
    if (skip > 3) Count += (skip & 0xFFFFFFFC);
    else BEQ();
     }
   else BEQ();
}

void BNE()
{
   local_rs = irs;
   local_rt = irt;
   PC++;
   delay_slot=1;
#ifdef DBG
            if (debugger_mode) update_debugger(PC->addr);
#endif
   PC->ops();
   update_count();
   delay_slot=0;
   if (local_rs != local_rt && !skip_jump)
     PC += (PC-2)->f.i.immediate-1;
   last_addr = PC->addr;
   if (next_interupt <= Count) gen_interupt();
}

void BNE_OUT()
{
   local_rs = irs;
   local_rt = irt;
   jump_target = (int)PC->f.i.immediate;
   PC++;
   delay_slot=1;
#ifdef DBG
            if (debugger_mode) update_debugger(PC->addr);
#endif
   PC->ops();
   update_count();
   delay_slot=0;
   if (!skip_jump && local_rs != local_rt)
     jump_to(PC->addr + ((jump_target-1)<<2));
   last_addr = PC->addr;
   if (next_interupt <= Count) gen_interupt();
}

void BNE_IDLE()
{
   int skip;
   if (irs != irt)
     {
    update_count();
    skip = next_interupt - Count;
    if (skip > 3) Count += (skip & 0xFFFFFFFC);
    else BNE();
     }
   else BNE();
}

void BLEZ()
{
   local_rs = irs;
   PC++;
   delay_slot=1;
#ifdef DBG
            if (debugger_mode) update_debugger(PC->addr);
#endif
   PC->ops();
   update_count();
   delay_slot=0;
   if (local_rs <= 0 && !skip_jump)
     PC += (PC-2)->f.i.immediate-1;
   last_addr = PC->addr;
   if (next_interupt <= Count) gen_interupt();
}

void BLEZ_OUT()
{
   local_rs = irs;
   jump_target = (int)PC->f.i.immediate;
   PC++;
   delay_slot=1;
#ifdef DBG
            if (debugger_mode) update_debugger(PC->addr);
#endif
   PC->ops();
   update_count();
   delay_slot=0;
   if (!skip_jump && local_rs <= 0)
     jump_to(PC->addr + ((jump_target-1)<<2));
   last_addr = PC->addr;
   if (next_interupt <= Count) gen_interupt();
}

void BLEZ_IDLE()
{
   int skip;
   if (irs <= irt)
     {
    update_count();
    skip = next_interupt - Count;
    if (skip > 3) Count += (skip & 0xFFFFFFFC);
    else BLEZ();
     }
   else BLEZ();
}

void BGTZ()
{
   local_rs = irs;
   PC++;
   delay_slot=1;
#ifdef DBG
            if (debugger_mode) update_debugger(PC->addr);
#endif
   PC->ops();
   update_count();
   delay_slot=0;
   if (local_rs > 0 && !skip_jump)
     PC += (PC-2)->f.i.immediate-1;
   last_addr = PC->addr;
   if (next_interupt <= Count) gen_interupt();
}

void BGTZ_OUT()
{
   local_rs = irs;
   jump_target = (int)PC->f.i.immediate;
   PC++;
   delay_slot=1;
#ifdef DBG
            if (debugger_mode) update_debugger(PC->addr);
#endif
   PC->ops();
   update_count();
   delay_slot=0;
   if (!skip_jump && local_rs > 0)
     jump_to(PC->addr + ((jump_target-1)<<2));
   last_addr = PC->addr;
   if (next_interupt <= Count) gen_interupt();
}

void BGTZ_IDLE()
{
   int skip;
   if (irs > irt)
     {
    update_count();
    skip = next_interupt - Count;
    if (skip > 3) Count += (skip & 0xFFFFFFFC);
    else BGTZ();
     }
   else BGTZ();
}

void ADDI()
{
   irt32 = irs32 + iimmediate;
   sign_extended(irt);
   PC++;
}

void ADDIU()
{
   irt32 = irs32 + iimmediate;
   sign_extended(irt);
   PC++;
}

void SLTI()
{
   if (irs < iimmediate) irt = 1;
   else irt = 0;
   PC++;
}

void SLTIU()
{
   if ((unsigned long long)irs < (unsigned long long)((long long)iimmediate))
     irt = 1;
   else irt = 0;
   PC++;
}

void ANDI()
{
   irt = irs & (unsigned short)iimmediate;
   PC++;
}

void ORI()
{
   irt = irs | (unsigned short)iimmediate;
   PC++;
}

void XORI()
{
   irt = irs ^ (unsigned short)iimmediate;
   PC++;
}

void LUI()
{
   irt32 = iimmediate << 16;
   sign_extended(irt);
   PC++;
}

void BEQL()
{
   if (irs == irt)
     {
    PC++;
    delay_slot=1;
#ifdef DBG
            if (debugger_mode) update_debugger(PC->addr);
#endif
    PC->ops();
    update_count();
    delay_slot=0;
    if(!skip_jump)
      PC += (PC-2)->f.i.immediate-1;
     }
   else 
     {
    PC+=2;
    update_count();
     }
   last_addr = PC->addr;
   if (next_interupt <= Count) gen_interupt();
}

void BEQL_OUT()
{
   if (irs == irt)
     {
    jump_target = (int)PC->f.i.immediate;
    PC++;
    delay_slot=1;
#ifdef DBG
            if (debugger_mode) update_debugger(PC->addr);
#endif
    PC->ops();
    update_count();
    delay_slot=0;
    if (!skip_jump)
      jump_to(PC->addr + ((jump_target-1)<<2));
     }
   else
     {
    PC+=2;
    update_count();
     }
   last_addr = PC->addr;
   if (next_interupt <= Count) gen_interupt();
}

void BEQL_IDLE()
{
   int skip;
   if (irs == irt)
     {
    update_count();
    skip = next_interupt - Count;
    if (skip > 3) Count += (skip & 0xFFFFFFFC);
    else BEQL();
     }
   else BEQL();
}

void BNEL()
{
   if (irs != irt)
     {
    PC++;
    delay_slot=1;
#ifdef DBG
            if (debugger_mode) update_debugger(PC->addr);
#endif
    PC->ops();
    update_count();
    delay_slot=0;
    if(!skip_jump)
      PC += (PC-2)->f.i.immediate-1;
     }
   else
     {
    PC+=2;
    update_count();
     }
   last_addr = PC->addr;
   if (next_interupt <= Count) gen_interupt();
}

void BNEL_OUT()
{
   if (irs != irt)
     {
    jump_target = (int)PC->f.i.immediate;
    PC++;
    delay_slot=1;
#ifdef DBG
            if (debugger_mode) update_debugger(PC->addr);
#endif
    PC->ops();
    update_count();
    delay_slot=0;
    if (!skip_jump)
      jump_to(PC->addr + ((jump_target-1)<<2));
     }
   else
     {
    PC+=2;
    update_count();
     }
   last_addr = PC->addr;
   if (next_interupt <= Count) gen_interupt();
}

void BNEL_IDLE()
{
   int skip;
   if (irs != irt)
     {
    update_count();
    skip = next_interupt - Count;
    if (skip > 3) Count += (skip & 0xFFFFFFFC);
    else BNEL();
     }
   else BNEL();
}

void BLEZL()
{
   if (irs <= 0)
     {
    PC++;
    delay_slot=1;
#ifdef DBG
            if (debugger_mode) update_debugger(PC->addr);
#endif
    PC->ops();
    update_count();
    delay_slot=0;
    if(!skip_jump)
      PC += (PC-2)->f.i.immediate-1;
     }
   else
     {
    PC+=2;
    update_count();
     }
   last_addr = PC->addr;
   if (next_interupt <= Count) gen_interupt();
}

void BLEZL_OUT()
{
   if (irs <= 0)
     {
    jump_target = (int)PC->f.i.immediate;
    PC++;
    delay_slot=1;
#ifdef DBG
            if (debugger_mode) update_debugger(PC->addr);
#endif
    PC->ops();
    update_count();
    delay_slot=0;
    if (!skip_jump)
      jump_to(PC->addr + ((jump_target-1)<<2));
     }
   else
     {
    PC+=2;
    update_count();
     }
   last_addr = PC->addr;
   if (next_interupt <= Count) gen_interupt();
}

void BLEZL_IDLE()
{
   int skip;
   if (irs <= irt)
     {
    update_count();
    skip = next_interupt - Count;
    if (skip > 3) Count += (skip & 0xFFFFFFFC);
    else BLEZL();
     }
   else BLEZL();
}

void BGTZL()
{
   if (irs > 0)
     {
    PC++;
    delay_slot=1;
#ifdef DBG
            if (debugger_mode) update_debugger(PC->addr);
#endif
    PC->ops();
    update_count();
    delay_slot=0;
    if(!skip_jump)
      PC += (PC-2)->f.i.immediate-1;
     }
   else
     {
    PC+=2;
    update_count();
     }
   last_addr = PC->addr;
   if (next_interupt <= Count) gen_interupt();
}

void BGTZL_OUT()
{
   if (irs > 0)
     {
    jump_target = (int)PC->f.i.immediate;
    PC++;
    delay_slot=1;
#ifdef DBG
            if (debugger_mode) update_debugger(PC->addr);
#endif
    PC->ops();
    update_count();
    delay_slot=0;
    if (!skip_jump)
      jump_to(PC->addr + ((jump_target-1)<<2));
     }
   else
     {
    PC+=2;
    update_count();
     }
   last_addr = PC->addr;
   if (next_interupt <= Count) gen_interupt();
}

void BGTZL_IDLE()
{
   int skip;
   if (irs > irt)
     {
    update_count();
    skip = next_interupt - Count;
    if (skip > 3) Count += (skip & 0xFFFFFFFC);
    else BGTZL();
     }
   else BGTZL();
}

void DADDI()
{
   irt = irs + iimmediate;
   PC++;
}

void DADDIU()
{
   irt = irs + iimmediate;
   PC++;
}

void LDL()
{
   unsigned long long int word = 0;
   PC++;
   switch ((lsaddr) & 7)
     {
      case 0:
    address = lsaddr;
    rdword = (unsigned long long *) &lsrt;
    read_dword_in_memory();
    break;
      case 1:
    address = (lsaddr) & 0xFFFFFFF8;
    rdword = &word;
    read_dword_in_memory();
    if(address)
      lsrt = (lsrt & 0xFF) | (word << 8);
    break;
      case 2:
    address = (lsaddr) & 0xFFFFFFF8;
    rdword = &word;
    read_dword_in_memory();
    if(address)
      lsrt = (lsrt & 0xFFFF) | (word << 16);
    break;
      case 3:
    address = (lsaddr) & 0xFFFFFFF8;
    rdword = &word;
    read_dword_in_memory();
    if(address)
      lsrt = (lsrt & 0xFFFFFF) | (word << 24);
    break;
      case 4:
    address = (lsaddr) & 0xFFFFFFF8;
    rdword = &word;
    read_dword_in_memory();
    if(address)
      lsrt = (lsrt & 0xFFFFFFFF) | (word << 32);
    break;
      case 5:
    address = (lsaddr) & 0xFFFFFFF8;
    rdword = &word;
    read_dword_in_memory();
    if(address)
      lsrt = (lsrt & 0xFFFFFFFFFFLL) | (word << 40);
    break;
      case 6:
    address = (lsaddr) & 0xFFFFFFF8;
    rdword = &word;
    read_dword_in_memory();
    if(address)
      lsrt = (lsrt & 0xFFFFFFFFFFFFLL) | (word << 48);
    break;
      case 7:
    address = (lsaddr) & 0xFFFFFFF8;
    rdword = &word;
    read_dword_in_memory();
    if(address)
      lsrt = (lsrt & 0xFFFFFFFFFFFFFFLL) | (word << 56);
    break;
     }
}

void LDR()
{
   unsigned long long int word = 0;
   PC++;
   switch ((lsaddr) & 7)
     {
      case 0:
    address = (lsaddr) & 0xFFFFFFF8;
    rdword = &word;
    read_dword_in_memory();
    if(address)
      lsrt = (lsrt & 0xFFFFFFFFFFFFFF00LL) | (word >> 56);
    break;
      case 1:
    address = (lsaddr) & 0xFFFFFFF8;
    rdword = &word;
    read_dword_in_memory();
    if(address)
      lsrt = (lsrt & 0xFFFFFFFFFFFF0000LL) | (word >> 48);
    break;
      case 2:
    address = (lsaddr) & 0xFFFFFFF8;
    rdword = &word;
    read_dword_in_memory();
    if(address)
      lsrt = (lsrt & 0xFFFFFFFFFF000000LL) | (word >> 40);
    break;
      case 3:
    address = (lsaddr) & 0xFFFFFFF8;
    rdword = &word;
    read_dword_in_memory();
    if(address)
      lsrt = (lsrt & 0xFFFFFFFF00000000LL) | (word >> 32);
    break;
      case 4:
    address = (lsaddr) & 0xFFFFFFF8;
    rdword = &word;
    read_dword_in_memory();
    if(address)
      lsrt = (lsrt & 0xFFFFFF0000000000LL) | (word >> 24);
    break;
      case 5:
    address = (lsaddr) & 0xFFFFFFF8;
    rdword = &word;
    read_dword_in_memory();
    if(address)
      lsrt = (lsrt & 0xFFFF000000000000LL) | (word >> 16);
    break;
      case 6:
    address = (lsaddr) & 0xFFFFFFF8;
    rdword = &word;
    read_dword_in_memory();
    if(address)
      lsrt = (lsrt & 0xFF00000000000000LL) | (word >> 8);
    break;
      case 7:
    address = (lsaddr) & 0xFFFFFFF8;
    rdword = (unsigned long long *) &lsrt;
    read_dword_in_memory();
    break;
     }
}

void LB()
{
   PC++;
   address = lsaddr;
   rdword = (unsigned long long *) &lsrt;
   read_byte_in_memory();
   if (address)
     sign_extendedb(lsrt);
}

void LH()
{
   PC++;
   address = lsaddr;
   rdword = (unsigned long long *) &lsrt;
   read_hword_in_memory();
   if (address)
     sign_extendedh(lsrt);
}

void LWL()
{
   unsigned long long int word = 0;
   PC++;
   switch ((lsaddr) & 3)
     {
      case 0:
    address = lsaddr;
    rdword = (unsigned long long *) &lsrt;
    read_word_in_memory();
    break;
      case 1:
    address = (lsaddr) & 0xFFFFFFFC;
    rdword = &word;
    read_word_in_memory();
    if(address)
      lsrt = (lsrt & 0xFF) | (word << 8);
    break;
      case 2:
    address = (lsaddr) & 0xFFFFFFFC;
    rdword = &word;
    read_word_in_memory();
    if(address)
      lsrt = (lsrt & 0xFFFF) | (word << 16);
    break;
      case 3:
    address = (lsaddr) & 0xFFFFFFFC;
    rdword = &word;
    read_word_in_memory();
    if(address)
      lsrt = (lsrt & 0xFFFFFF) | (word << 24);
    break;
     }
   if(address)
     sign_extended(lsrt);
}

void LW()
{
   PC++;
   address = lsaddr;
   rdword = (unsigned long long *) &lsrt;
   read_word_in_memory();
   if (address)
     sign_extended(lsrt);
}

void LBU()
{
   PC++;
   address = lsaddr;
   rdword = (unsigned long long *) &lsrt;
   read_byte_in_memory();
}

void LHU()
{
   PC++;
   address = lsaddr;
   rdword = (unsigned long long *) &lsrt;
   read_hword_in_memory();
}

void LWR()
{
   unsigned long long int word = 0;
   PC++;
   switch ((lsaddr) & 3)
     {
      case 0:
    address = (lsaddr) & 0xFFFFFFFC;
    rdword = &word;
    read_word_in_memory();
    if(address)
      lsrt = (lsrt & 0xFFFFFFFFFFFFFF00LL) | ((word >> 24) & 0xFF);
    break;
      case 1:
    address = (lsaddr) & 0xFFFFFFFC;
    rdword = &word;
    read_word_in_memory();
    if(address)
      lsrt = (lsrt & 0xFFFFFFFFFFFF0000LL) | ((word >> 16) & 0xFFFF);
    break;
      case 2:
    address = (lsaddr) & 0xFFFFFFFC;
    rdword = &word;
    read_word_in_memory();
    if(address)
      lsrt = (lsrt & 0xFFFFFFFFFF000000LL) | ((word >> 8) & 0XFFFFFF);
    break;
      case 3:
    address = (lsaddr) & 0xFFFFFFFC;
    rdword = (unsigned long long *) &lsrt;
    read_word_in_memory();
    if(address)
      sign_extended(lsrt);
     }
}

void LWU()
{
   PC++;
   address = lsaddr;
   rdword = (unsigned long long *) &lsrt;
   read_word_in_memory();
}

void SB()
{
   PC++;
   address = lsaddr;
   byte = (unsigned char)(lsrt & 0xFF);
   write_byte_in_memory();
   check_memory();
}

void SH()
{
   PC++;
   address = lsaddr;
   hword = (unsigned short)(lsrt & 0xFFFF);
   write_hword_in_memory();
   check_memory();
}

void SWL()
{
   unsigned long long int old_word = 0;
   PC++;
   switch ((lsaddr) & 3)
     {
      case 0:
    address = (lsaddr) & 0xFFFFFFFC;
    word = (unsigned int)lsrt;
    write_word_in_memory();
    check_memory();
    break;
      case 1:
    address = (lsaddr) & 0xFFFFFFFC;
    rdword = &old_word;
    read_word_in_memory();
    if(address)
      {
         word = ((unsigned int)lsrt >> 8) | (old_word & 0xFF000000);
         write_word_in_memory();
         check_memory();
      }
    break;
      case 2:
    address = (lsaddr) & 0xFFFFFFFC;
    rdword = &old_word;
    read_word_in_memory();
    if(address)
      {
         word = ((unsigned int)lsrt >> 16) | (old_word & 0xFFFF0000);
         write_word_in_memory();
         check_memory();
      }
    break;
      case 3:
    address = lsaddr;
    byte = (unsigned char)(lsrt >> 24);
    write_byte_in_memory();
    check_memory();
    break;
     }
}

void SW()
{
   PC++;
   address = lsaddr;
   word = (unsigned int)(lsrt & 0xFFFFFFFF);
   write_word_in_memory();
   check_memory();
}

void SDL()
{
   unsigned long long int old_word = 0;
   PC++;
   switch ((lsaddr) & 7)
     {
      case 0:
    address = (lsaddr) & 0xFFFFFFF8;
    dword = lsrt;
    write_dword_in_memory();
    check_memory();
    break;
      case 1:
    address = (lsaddr) & 0xFFFFFFF8;
    rdword = &old_word;
    read_dword_in_memory();
    if(address)
      {
         dword = ((unsigned long long)lsrt >> 8)|(old_word & 0xFF00000000000000LL);
         write_dword_in_memory();
         check_memory();
      }
    break;
      case 2:
    address = (lsaddr) & 0xFFFFFFF8;
    rdword = &old_word;
    read_dword_in_memory();
    if(address)
      {
         dword = ((unsigned long long)lsrt >> 16)|(old_word & 0xFFFF000000000000LL);
         write_dword_in_memory();
         check_memory();
      }
    break;
      case 3:
    address = (lsaddr) & 0xFFFFFFF8;
    rdword = &old_word;
    read_dword_in_memory();
    if(address)
      {
         dword = ((unsigned long long)lsrt >> 24)|(old_word & 0xFFFFFF0000000000LL);
         write_dword_in_memory();
         check_memory();
      }
    break;
      case 4:
    address = (lsaddr) & 0xFFFFFFF8;
    rdword = &old_word;
    read_dword_in_memory();
    if(address)
      {
         dword = ((unsigned long long)lsrt >> 32)|(old_word & 0xFFFFFFFF00000000LL);
         write_dword_in_memory();
         check_memory();
      }
    break;
      case 5:
    address = (lsaddr) & 0xFFFFFFF8;
    rdword = &old_word;
    read_dword_in_memory();
    if(address)
      {
         dword = ((unsigned long long)lsrt >> 40)|(old_word & 0xFFFFFFFFFF000000LL);
         write_dword_in_memory();
         check_memory();
      }
    break;
      case 6:
    address = (lsaddr) & 0xFFFFFFF8;
    rdword = &old_word;
    read_dword_in_memory();
    if(address)
      {
         dword = ((unsigned long long)lsrt >> 48)|(old_word & 0xFFFFFFFFFFFF0000LL);
         write_dword_in_memory();
         check_memory();
      }
    break;
      case 7:
    address = (lsaddr) & 0xFFFFFFF8;
    rdword = &old_word;
    read_dword_in_memory();
    if(address)
      {
         dword = ((unsigned long long)lsrt >> 56)|(old_word & 0xFFFFFFFFFFFFFF00LL);
         write_dword_in_memory();
         check_memory();
      }
    break;
     }
}

void SDR()
{
   unsigned long long int old_word = 0;
   PC++;
   switch ((lsaddr) & 7)
     {
      case 0:
    address = lsaddr;
    rdword = &old_word;
    read_dword_in_memory();
    if(address)
      {
         dword = (lsrt << 56) | (old_word & 0x00FFFFFFFFFFFFFFLL);
         write_dword_in_memory();
         check_memory();
      }
    break;
      case 1:
    address = (lsaddr) & 0xFFFFFFF8;
    rdword = &old_word;
    read_dword_in_memory();
    if(address)
      {
         dword = (lsrt << 48) | (old_word & 0x0000FFFFFFFFFFFFLL);
         write_dword_in_memory();
         check_memory();
      }
    break;
      case 2:
    address = (lsaddr) & 0xFFFFFFF8;
    rdword = &old_word;
    read_dword_in_memory();
    if(address)
      {
         dword = (lsrt << 40) | (old_word & 0x000000FFFFFFFFFFLL);
         write_dword_in_memory();
         check_memory();
      }
    break;
      case 3:
    address = (lsaddr) & 0xFFFFFFF8;
    rdword = &old_word;
    read_dword_in_memory();
    if(address)
      {
         dword = (lsrt << 32) | (old_word & 0x00000000FFFFFFFFLL);
         write_dword_in_memory();
         check_memory();
      }
    break;
      case 4:
    address = (lsaddr) & 0xFFFFFFF8;
    rdword = &old_word;
    read_dword_in_memory();
    if(address)
      {
         dword = (lsrt << 24) | (old_word & 0x0000000000FFFFFFLL);
         write_dword_in_memory();
         check_memory();
      }
    break;
      case 5:
    address = (lsaddr) & 0xFFFFFFF8;
    rdword = &old_word;
    read_dword_in_memory();
    if(address)
      {
         dword = (lsrt << 16) | (old_word & 0x000000000000FFFFLL);
         write_dword_in_memory();
         check_memory();
      }
    break;
      case 6:
    address = (lsaddr) & 0xFFFFFFF8;
    rdword = &old_word;
    read_dword_in_memory();
    if(address)
      {
         dword = (lsrt << 8) | (old_word & 0x00000000000000FFLL);
         write_dword_in_memory();
         check_memory();
      }
    break;
      case 7:
    address = (lsaddr) & 0xFFFFFFF8;
    dword = lsrt;
    write_dword_in_memory();
    check_memory();
    break;
     }
}

void SWR()
{
   unsigned long long int old_word = 0;
   PC++;
   switch ((lsaddr) & 3)
     {
      case 0:
    address = lsaddr;
    rdword = &old_word;
    read_word_in_memory();
    if(address)
      {
         word = ((unsigned int)lsrt << 24) | (old_word & 0x00FFFFFF);
         write_word_in_memory();
         check_memory();
      }
    break;
      case 1:
    address = (lsaddr) & 0xFFFFFFFC;
    rdword = &old_word;
    read_word_in_memory();
    if(address)
      {
         word = ((unsigned int)lsrt << 16) | (old_word & 0x0000FFFF);
         write_word_in_memory();
         check_memory();
      }
    break;
      case 2:
    address = (lsaddr) & 0xFFFFFFFC;
    rdword = &old_word;
    read_word_in_memory();
    if(address)
      {
         word = ((unsigned int)lsrt << 8) | (old_word & 0x000000FF);
         write_word_in_memory();
         check_memory();
      }
    break;
      case 3:
    address = (lsaddr) & 0xFFFFFFFC;
    word = (unsigned int)lsrt;
    write_word_in_memory();
    check_memory();
    break;
     }
}

void CACHE()
{
   PC++;
}

void LL()
{
   PC++;
   address = lsaddr;
   rdword = (unsigned long long *) &lsrt;
   read_word_in_memory();
   if (address)
     {
    sign_extended(lsrt);
    llbit = 1;
     }
}

void LWC1()
{  
   unsigned long long int temp;
   if (check_cop1_unusable()) return;
   PC++;
   address = lslfaddr;
   rdword = &temp;
   read_word_in_memory();
   if (address)
     *((int*)reg_cop1_simple[lslfft]) = *rdword;
}

void LDC1()
{
   if (check_cop1_unusable()) return;
   PC++;
   address = lslfaddr;
   rdword = (unsigned long long *)reg_cop1_double[lslfft];
   read_dword_in_memory();
}

void LD()
{
   PC++;
   address = lsaddr;
   rdword = (unsigned long long *) &lsrt;
   read_dword_in_memory();
}

void SC()
{
   /*PC++;
   printf("SC\n");
   if (llbit) {
      address = lsaddr;
      word = (unsigned int)(lsrt & 0xFFFFFFFF);
      write_word_in_memory();
   }
   lsrt = llbit;*/
   
   PC++;
   if(llbit)
     {
    address = lsaddr;
    word = (unsigned int)(lsrt & 0xFFFFFFFF);
    write_word_in_memory();
    check_memory();
    llbit = 0;
    lsrt = 1;
     }
   else
     {
    lsrt = 0;
     }
}

void SWC1()
{
   if (check_cop1_unusable()) return;
   PC++;
   address = lslfaddr;
   word = *((int*)reg_cop1_simple[lslfft]);
   write_word_in_memory();
   check_memory();
}

void SDC1()
{
   if (check_cop1_unusable()) return;
   PC++;
   address = lslfaddr;
   dword = *((unsigned long long*)reg_cop1_double[lslfft]);
   write_dword_in_memory();
   check_memory();
}

void SD()
{
   PC++;
   address = lsaddr;
   dword = lsrt;
   write_dword_in_memory();
   check_memory();
}

void NOTCOMPILED()
{
#ifdef CORE_DBG
   printf("NOTCOMPILED: addr = %x ops = %lx\n", PC->addr, (long) PC->ops);
#endif

   if ((PC->addr>>16) == 0xa400)
     recompile_block((int *) SP_DMEM, blocks[0xa4000000>>12], PC->addr);
   else
     {
    unsigned int paddr = 0;
    if (PC->addr >= 0x80000000 && PC->addr < 0xc0000000)
      paddr = PC->addr;
    else
      paddr = virtual_to_physical_address(PC->addr, 2);
    if (paddr)
      {
         if ((paddr & 0x1FFFFFFF) >= 0x10000000)
           {
          recompile_block((int *) rom+((((paddr-(PC->addr-blocks[PC->addr>>12]->start)) & 0x1FFFFFFF) - 0x10000000)>>2),
                  blocks[PC->addr>>12], PC->addr);
           }
         else
           recompile_block((int *) rdram+(((paddr-(PC->addr-blocks[PC->addr>>12]->start)) & 0x1FFFFFFF)>>2),
                   blocks[PC->addr>>12], PC->addr);
      }
    else printf("not compiled exception\n");
     }
/*#ifdef DBG
            if (debugger_mode) update_debugger(PC->addr);
#endif
The preceeding update_debugger SHOULD be unnecessary since it should have been
called before NOTCOMPILED would have been executed
*/
   PC->ops();
   if (r4300emu == CORE_DYNAREC)
     dyna_jump();
}

void NOTCOMPILED2()
{
   NOTCOMPILED();
}

static inline unsigned int update_invalid_addr(unsigned int addr)
{
   if (addr >= 0x80000000 && addr < 0xa0000000)
     {
    if (invalid_code[addr>>12]) invalid_code[(addr+0x20000000)>>12] = 1;
    if (invalid_code[(addr+0x20000000)>>12]) invalid_code[addr>>12] = 1;
    return addr;
     }
   else if (addr >= 0xa0000000 && addr < 0xc0000000)
     {
    if (invalid_code[addr>>12]) invalid_code[(addr-0x20000000)>>12] = 1;
    if (invalid_code[(addr-0x20000000)>>12]) invalid_code[addr>>12] = 1;
    return addr;
     }
   else
     {
    unsigned int paddr = virtual_to_physical_address(addr, 2);
    if (paddr)
      {
         unsigned int beg_paddr = paddr - (addr - (addr&~0xFFF));
         update_invalid_addr(paddr);
         if (invalid_code[(beg_paddr+0x000)>>12]) invalid_code[addr>>12] = 1;
         if (invalid_code[(beg_paddr+0xFFC)>>12]) invalid_code[addr>>12] = 1;
         if (invalid_code[addr>>12]) invalid_code[(beg_paddr+0x000)>>12] = 1;
         if (invalid_code[addr>>12]) invalid_code[(beg_paddr+0xFFC)>>12] = 1;
      }
    return paddr;
     }
}

#define addr jump_to_address
unsigned int jump_to_address;
void jump_to_func()
{
   unsigned int paddr;
   if (skip_jump) return;
   paddr = update_invalid_addr(addr);
   if (!paddr) return;
   actual = blocks[addr>>12];
   if (invalid_code[addr>>12])
     {
    if (!blocks[addr>>12])
      {
         blocks[addr>>12] = malloc(sizeof(precomp_block));
         actual = blocks[addr>>12];
         blocks[addr>>12]->code = NULL;
         blocks[addr>>12]->block = NULL;
         blocks[addr>>12]->jumps_table = NULL;
         blocks[addr>>12]->riprel_table = NULL;
      }
    blocks[addr>>12]->start = addr & ~0xFFF;
    blocks[addr>>12]->end = (addr & ~0xFFF) + 0x1000;
    init_block((int *) rdram+(((paddr-(addr-blocks[addr>>12]->start)) & 0x1FFFFFFF)>>2),
           blocks[addr>>12]);
     }
   PC=actual->block+((addr-actual->start)>>2);
   
   if (r4300emu == CORE_DYNAREC) dyna_jump();
}
#undef addr

/* Refer to Figure 6-2 on page 155 and explanation on page B-11
   of MIPS R4000 Microprocessor User's Manual (Second Edition)
   by Joe Heinrich.
*/
void shuffle_fpr_data(int oldStatus, int newStatus)
{
#if defined(_BIG_ENDIAN)
    const int isBigEndian = 1;
#else
    const int isBigEndian = 0;
#endif

    if ((newStatus & 0x04000000) != (oldStatus & 0x04000000))
    {
        int i;
        int temp_fgr_32[32];

        // pack or unpack the FGR register data
        if (newStatus & 0x04000000)
        {   // switching into 64-bit mode
            // retrieve 32 FPR values from packed 32-bit FGR registers
            for (i = 0; i < 32; i++)
            {
                temp_fgr_32[i] = *((int *) &reg_cop1_fgr_64[i>>1] + ((i & 1) ^ isBigEndian));
            }
            // unpack them into 32 64-bit registers, taking the high 32-bits from their temporary place in the upper 16 FGRs
            for (i = 0; i < 32; i++)
            {
                int high32 = *((int *) &reg_cop1_fgr_64[(i>>1)+16] + (i & 1));
                *((int *) &reg_cop1_fgr_64[i] + isBigEndian)     = temp_fgr_32[i];
                *((int *) &reg_cop1_fgr_64[i] + (isBigEndian^1)) = high32;
            }
        }
        else
        {   // switching into 32-bit mode
            // retrieve the high 32 bits from each 64-bit FGR register and store in temp array
            for (i = 0; i < 32; i++)
            {
                temp_fgr_32[i] = *((int *) &reg_cop1_fgr_64[i] + (isBigEndian^1));
            }
            // take the low 32 bits from each register and pack them together into 64-bit pairs
            for (i = 0; i < 16; i++)
            {
                unsigned int least32 = *((unsigned int *) &reg_cop1_fgr_64[i*2] + isBigEndian);
                unsigned int most32 = *((unsigned int *) &reg_cop1_fgr_64[i*2+1] + isBigEndian);
                reg_cop1_fgr_64[i] = ((unsigned long long) most32 << 32) | (unsigned long long) least32;
            }
            // store the high bits in the upper 16 FGRs, which wont be accessible in 32-bit mode
            for (i = 0; i < 32; i++)
            {
                *((int *) &reg_cop1_fgr_64[(i>>1)+16] + (i & 1)) = temp_fgr_32[i];
            }
        }
    }
}

void set_fpr_pointers(int newStatus)
{
    int i;
#if defined(_BIG_ENDIAN)
    const int isBigEndian = 1;
#else
    const int isBigEndian = 0;
#endif

    // update the FPR register pointers
    if (newStatus & 0x04000000)
    {
        for (i = 0; i < 32; i++)
        {
            reg_cop1_double[i] = (double*) &reg_cop1_fgr_64[i];
            reg_cop1_simple[i] = ((float*) &reg_cop1_fgr_64[i]) + isBigEndian;
        }
    }
    else
    {
        for (i = 0; i < 32; i++)
        {
            reg_cop1_double[i] = (double*) &reg_cop1_fgr_64[i>>1];
            reg_cop1_simple[i] = ((float*) &reg_cop1_fgr_64[i>>1]) + ((i & 1) ^ isBigEndian);
        }
    }
}

int check_cop1_unusable()
{
   if (!(Status & 0x20000000))
     {
    Cause = (11 << 2) | 0x10000000;
    exception_general();
    return 1;
     }
   return 0;
}

void update_count()
{
   if (r4300emu == CORE_PURE_INTERPRETER)
     {
    Count = Count + (interp_addr - last_addr)/2;
    last_addr = interp_addr;
     }
   else
     {
    if (PC->addr < last_addr)
      {
         printf("PC->addr < last_addr\n");
      }
    Count = Count + (PC->addr - last_addr)/2;
    last_addr = PC->addr;
     }
#ifdef COMPARE_CORE
   if (delay_slot)
     compare_core();
#endif
/*#ifdef DBG
   if (debugger_mode && !delay_slot) update_debugger(PC->addr);
#endif
*/
}

void init_blocks()
{
   int i;
   for (i=0; i<0x100000; i++)
     {
    invalid_code[i] = 1;
    blocks[i] = NULL;
     }
   blocks[0xa4000000>>12] = malloc(sizeof(precomp_block));
   invalid_code[0xa4000000>>12] = 1;
   blocks[0xa4000000>>12]->code = NULL;
   blocks[0xa4000000>>12]->block = NULL;
   blocks[0xa4000000>>12]->jumps_table = NULL;
   blocks[0xa4000000>>12]->riprel_table = NULL;
   blocks[0xa4000000>>12]->start = 0xa4000000;
   blocks[0xa4000000>>12]->end = 0xa4001000;
   actual=blocks[0xa4000000>>12];
   init_block((int *) SP_DMEM, blocks[0xa4000000>>12]);
   PC=actual->block+(0x40/4);
/*#ifdef DBG //should only be needed by dynamic recompiler
   if (debugger_mode) // debugger shows initial state (before 1st instruction).
     update_debugger(PC->addr);
#endif*/
}

/* this hard reset function simulates the boot-up state of the R4300 CPU */
void r4300_reset_hard()
{
    unsigned int i;

    // clear r4300 registers and TLB entries
    for (i = 0; i < 32; i++)
    {
        reg[i]=0;
        reg_cop0[i]=0;
        reg_cop1_fgr_64[i]=0;

        // --------------tlb------------------------
        tlb_e[i].mask=0;
        tlb_e[i].vpn2=0;
        tlb_e[i].g=0;
        tlb_e[i].asid=0;
        tlb_e[i].pfn_even=0;
        tlb_e[i].c_even=0;
        tlb_e[i].d_even=0;
        tlb_e[i].v_even=0;
        tlb_e[i].pfn_odd=0;
        tlb_e[i].c_odd=0;
        tlb_e[i].d_odd=0;
        tlb_e[i].v_odd=0;
        tlb_e[i].r=0;
        //tlb_e[i].check_parity_mask=0x1000;

        tlb_e[i].start_even=0;
        tlb_e[i].end_even=0;
        tlb_e[i].phys_even=0;
        tlb_e[i].start_odd=0;
        tlb_e[i].end_odd=0;
        tlb_e[i].phys_odd=0;
    }
    for (i=0; i<0x100000; i++)
    {
        tlb_LUT_r[i] = 0;
        tlb_LUT_w[i] = 0;
    }
    llbit=0;
    hi=0;
    lo=0;
    FCR0=0x511;
    FCR31=0;

    // set COP0 registers
    Random = 31;
    Status= 0x34000000;
    set_fpr_pointers(Status);
    Config= 0x6e463;
    PRevID = 0xb00;
    Count = 0x5000;
    Cause = 0x5C;
    Context = 0x7FFFF0;
    EPC = 0xFFFFFFFF;
    BadVAddr = 0xFFFFFFFF;
    ErrorEPC = 0xFFFFFFFF;
   
    rounding_mode = 0x33F;
}

/* this soft reset function simulates the actions of the PIF ROM, which may vary by region */
void r4300_reset_soft()
{
    long long CRC = 0;
    unsigned int i;

    // copy boot code from ROM to SP_DMEM
    memcpy((char *)SP_DMEM+0x40, rom+0x40, 0xFC0);

   // the following values are extracted from the pj64 source code
   // thanks to Zilmar and Jabo
   
   reg[6] = 0xFFFFFFFFA4001F0CLL;
   reg[7] = 0xFFFFFFFFA4001F08LL;
   reg[8] = 0x00000000000000C0LL;
   reg[10]= 0x0000000000000040LL;
   reg[11]= 0xFFFFFFFFA4000040LL;
   reg[29]= 0xFFFFFFFFA4001FF0LL;
   
    // figure out which ROM type is loaded
   for (i = 0x40/4; i < (0x1000/4); i++)
     CRC += SP_DMEM[i];
   switch(CRC) {
    case 0x000000D0027FDF31LL:
    case 0x000000CFFB631223LL:
      CIC_Chip = 1;
      break;
    case 0x000000D057C85244LL:
      CIC_Chip = 2;
      break;
    case 0x000000D6497E414BLL:
      CIC_Chip = 3;
      break;
    case 0x0000011A49F60E96LL:
      CIC_Chip = 5;
      break;
    case 0x000000D6D5BE5580LL:
      CIC_Chip = 6;
      break;
    default:
      CIC_Chip = 2;
   }

   switch(ROM_HEADER->Country_code&0xFF)
     {
      case 0x44:
      case 0x46:
      case 0x49:
      case 0x50:
      case 0x53:
      case 0x55:
      case 0x58:
      case 0x59:
    switch (CIC_Chip) {
     case 2:
       reg[5] = 0xFFFFFFFFC0F1D859LL;
       reg[14]= 0x000000002DE108EALL;
       break;
     case 3:
       reg[5] = 0xFFFFFFFFD4646273LL;
       reg[14]= 0x000000001AF99984LL;
       break;
     case 5:
       SP_IMEM[1] = 0xBDA807FC;
       reg[5] = 0xFFFFFFFFDECAAAD1LL;
       reg[14]= 0x000000000CF85C13LL;
       reg[24]= 0x0000000000000002LL;
       break;
     case 6:
       reg[5] = 0xFFFFFFFFB04DC903LL;
       reg[14]= 0x000000001AF99984LL;
       reg[24]= 0x0000000000000002LL;
       break;
    }
    reg[23]= 0x0000000000000006LL;
    reg[31]= 0xFFFFFFFFA4001554LL;
    break;
      case 0x37:
      case 0x41:
      case 0x45:
      case 0x4A:
      default:
    switch (CIC_Chip) {
     case 2:
       reg[5] = 0xFFFFFFFFC95973D5LL;
       reg[14]= 0x000000002449A366LL;
       break;
     case 3:
       reg[5] = 0xFFFFFFFF95315A28LL;
       reg[14]= 0x000000005BACA1DFLL;
       break;
     case 5:
       SP_IMEM[1] = 0x8DA807FC;
       reg[5] = 0x000000005493FB9ALL;
       reg[14]= 0xFFFFFFFFC2C20384LL;
       break;
     case 6:
       reg[5] = 0xFFFFFFFFE067221FLL;
       reg[14]= 0x000000005CD2B70FLL;
       break;
    }
    reg[20]= 0x0000000000000001LL;
    reg[24]= 0x0000000000000003LL;
    reg[31]= 0xFFFFFFFFA4001550LL;
     }
   switch (CIC_Chip) {
    case 1:
      reg[22]= 0x000000000000003FLL;
      break;
    case 2:
      reg[1] = 0x0000000000000001LL;
      reg[2] = 0x000000000EBDA536LL;
      reg[3] = 0x000000000EBDA536LL;
      reg[4] = 0x000000000000A536LL;
      reg[12]= 0xFFFFFFFFED10D0B3LL;
      reg[13]= 0x000000001402A4CCLL;
      reg[15]= 0x000000003103E121LL;
      reg[22]= 0x000000000000003FLL;
      reg[25]= 0xFFFFFFFF9DEBB54FLL;
      break;
    case 3:
      reg[1] = 0x0000000000000001LL;
      reg[2] = 0x0000000049A5EE96LL;
      reg[3] = 0x0000000049A5EE96LL;
      reg[4] = 0x000000000000EE96LL;
      reg[12]= 0xFFFFFFFFCE9DFBF7LL;
      reg[13]= 0xFFFFFFFFCE9DFBF7LL;
      reg[15]= 0x0000000018B63D28LL;
      reg[22]= 0x0000000000000078LL;
      reg[25]= 0xFFFFFFFF825B21C9LL;
      break;
    case 5:
      SP_IMEM[0] = 0x3C0DBFC0;
      SP_IMEM[2] = 0x25AD07C0;
      SP_IMEM[3] = 0x31080080;
      SP_IMEM[4] = 0x5500FFFC;
      SP_IMEM[5] = 0x3C0DBFC0;
      SP_IMEM[6] = 0x8DA80024;
      SP_IMEM[7] = 0x3C0BB000;
      reg[2] = 0xFFFFFFFFF58B0FBFLL;
      reg[3] = 0xFFFFFFFFF58B0FBFLL;
      reg[4] = 0x0000000000000FBFLL;
      reg[12]= 0xFFFFFFFF9651F81ELL;
      reg[13]= 0x000000002D42AAC5LL;
      reg[15]= 0x0000000056584D60LL;
      reg[22]= 0x0000000000000091LL;
      reg[25]= 0xFFFFFFFFCDCE565FLL;
      break;
    case 6:
      reg[2] = 0xFFFFFFFFA95930A4LL;
      reg[3] = 0xFFFFFFFFA95930A4LL;
      reg[4] = 0x00000000000030A4LL;
      reg[12]= 0xFFFFFFFFBCB59510LL;
      reg[13]= 0xFFFFFFFFBCB59510LL;
      reg[15]= 0x000000007A3C07F4LL;
      reg[22]= 0x0000000000000085LL;
      reg[25]= 0x00000000465E3F72LL;
      break;
   }

}

void r4300_execute()
{
    unsigned int i;

    debug_count = 0;
    printf("Starting r4300 emulator\n");

    delay_slot=0;
    stop = 0;
    rompause = 0;

    /* clear instruction counters */
#if defined(COUNT_INSTR)
    for (i = 0; i < 131; i++)
        instr_count[i] = 0;
#endif

    last_addr = 0xa4000040;
    next_interupt = 624999;
    init_interupt();
    
    if (r4300emu == CORE_INTERPRETER)
    {
        printf ("R4300 Core mode: Cached Interpreter\n");
        init_blocks();
        last_addr = PC->addr;
        while (!stop)
        {
#ifdef COMPARE_CORE
            if (PC->ops == FIN_BLOCK && 
            (PC->addr < 0x80000000 || PC->addr >= 0xc0000000))
            virtual_to_physical_address(PC->addr, 2);
            compare_core();
#endif
#ifdef DBG
            if (debugger_mode) update_debugger(PC->addr);
#endif
            PC->ops();
        }
    }
#if defined(DYNAREC)
    else if (r4300emu >= 2)
    {
        r4300emu = CORE_DYNAREC;
        printf ("R4300 Core mode: Dynamic Recompiler\n");
        init_blocks();
        code = (void *)(actual->code+(actual->block[0x40/4].local_addr));
        dyna_start(code);
        PC++;
#if defined(PROFILE_R4300)
        pfProfile = fopen("instructionaddrs.dat", "ab");
        for (i=0; i<0x100000; i++)
            if (invalid_code[i] == 0 && blocks[i] != NULL && blocks[i]->code != NULL && blocks[i]->block != NULL)
            {
                unsigned char *x86addr;
                int mipsop;
                // store final code length for this block
                mipsop = -1; /* -1 == end of x86 code block */
                x86addr = blocks[i]->code + blocks[i]->code_length;
                fwrite(&mipsop, 1, 4, pfProfile);
                fwrite(&x86addr, 1, sizeof(char *), pfProfile);
            }
        fclose(pfProfile);
        pfProfile = NULL;
#endif
    }
#endif
    else
    {
        printf ("R4300 Core mode: Pure Interpreter\n");
        r4300emu = CORE_PURE_INTERPRETER;
        pure_interpreter();
    }
    debug_count+= Count;
    printf("R4300 core finished.\n");
    for (i=0; i<0x100000; i++)
    {
        if (blocks[i])
        {
            if (blocks[i]->block) { free(blocks[i]->block); blocks[i]->block = NULL; }
            if (blocks[i]->code) { free(blocks[i]->code); blocks[i]->code = NULL; }
            if (blocks[i]->jumps_table) { free(blocks[i]->jumps_table); blocks[i]->jumps_table = NULL; }
            if (blocks[i]->riprel_table) { free(blocks[i]->riprel_table); blocks[i]->riprel_table = NULL; }
            free(blocks[i]);
            blocks[i] = NULL;
        }
    }
    if (r4300emu == CORE_PURE_INTERPRETER) free(PC);

    /* print instruction counts */
#if defined(COUNT_INSTR)
    if (r4300emu == CORE_DYNAREC)
    {
        unsigned int iTypeCount[11] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        unsigned int iTotal = 0;
        printf("Instruction counters:\n");
        for (i = 0; i < 131; i++)
        {
            printf("%8s: %08i  ", instr_name[i], instr_count[i]);
            if (i % 5 == 4) printf("\n");
            iTypeCount[instr_type[i]] += instr_count[i];
            iTotal += instr_count[i];
        }
        printf("\nInstruction type summary (total instructions = %i)\n", iTotal);
        for (i = 0; i < 11; i++)
        {
            printf("%20s: %04.1f%% (%i)\n", instr_typename[i], (float) iTypeCount[i] * 100.0 / iTotal, iTypeCount[i]);
        }
    }
#endif
}

