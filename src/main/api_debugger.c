/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus-core - api_debugger.c                                     *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2009 Richard Goedeken                                   *
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
                       
/* This file contains the Core debugger functions which will be exported
 * outside of the core library.
 */
   
#include <stdlib.h>

#include "api/m64p_types.h"
 
EXPORT m64p_error CALL DebugSetCallbacks(void (*dbg_frontend_init)(void), void (*dbg_frontend_update)(unsigned int pc), void (*dbg_frontend_vi)(void))
{
  return M64ERR_INTERNAL;
}

EXPORT m64p_error CALL DebugSetRunState(int runstate)
{
  return M64ERR_INTERNAL;
}

EXPORT int CALL DebugGetState(m64p_dbg_state statenum)
{
  return 0;
}

EXPORT m64p_error CALL DebugStep(void)
{
  return M64ERR_INTERNAL;
}

EXPORT void CALL DebugDecodeOp(unsigned int instruction, char *op, char *args, int pc)
{
  return;
}

EXPORT void * CALL DebugMemGetRecompInfo(m64p_dbg_mem_info recomp_type, unsigned int address, int index)
{
  return NULL;
}

EXPORT int CALL DebugMemGetMemInfo(m64p_dbg_mem_info mem_info_type, unsigned int address)
{
  return 0;
}

EXPORT void * CALL DebugMemGetPointer(m64p_dbg_memptr_type mem_ptr_type)
{
  return NULL;
}

EXPORT unsigned long long CALL DebugMemRead64(unsigned int address)
{
  return 0;
}

EXPORT unsigned int CALL DebugMemRead32(unsigned int address)
{
  return 0;
}

EXPORT unsigned short CALL DebugMemRead16(unsigned int address)
{
  return 0;
}

EXPORT unsigned char CALL DebugMemRead8(unsigned int address)
{
  return 0;
}

EXPORT void CALL DebugMemWrite64(unsigned int address, unsigned long long value)
{
  return;
}

EXPORT void CALL DebugMemWrite32(unsigned int address, unsigned int value)
{
  return;
}

EXPORT void CALL DebugMemWrite16(unsigned int address, unsigned short value)
{
  return;
}

EXPORT void CALL DebugMemWrite8(unsigned int address, unsigned char value)
{
  return;
}

EXPORT void * CALL DebugGetCPUDataPtr(m64p_dbg_cpu_data cpu_data_type)
{
  return NULL;
}

EXPORT int CALL DebugBreakpointLookup(unsigned int address, unsigned int size, unsigned int flags)
{
  return -1;
}

EXPORT int CALL DebugBreakpointCommand(m64p_dbg_bkp_command command, unsigned int index, void *ptr)
{
  return -1;
}

