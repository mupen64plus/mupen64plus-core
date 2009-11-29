/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus-core - api/debugger.c                                     *
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

#include "m64p_types.h"
#include "callbacks.h"
#include "debugger.h"

#include "debugger/dbg_types.h"
#include "debugger/dbg_breakpoints.h"
#include "debugger/dbg_decoder.h"
#include "debugger/debugger.h"
#include "r4300/r4300.h"

/* local variables */
static void (*callback_ui_init)(void) = NULL;
static void (*callback_ui_update)(unsigned int) = NULL;
static void (*callback_ui_vi)(void) = NULL;

/* global Functions for use by the Core */

void DebuggerCallback(eDbgCallbackType type, unsigned int param)
{
    if (type == DEBUG_UI_INIT)
    {
        if (callback_ui_init != NULL)
            (*callback_ui_init)();
    }
    else if (type == DEBUG_UI_UPDATE)
    {
        if (callback_ui_update != NULL)
            (*callback_ui_update)(param);
    }
    else if (type == DEBUG_UI_VI)
    {
        if (callback_ui_vi != NULL)
            (*callback_ui_vi)();
    }
}

/* exported functions for use by the front-end User Interface */
 
EXPORT m64p_error CALL DebugSetCallbacks(void (*dbg_frontend_init)(void), void (*dbg_frontend_update)(unsigned int pc), void (*dbg_frontend_vi)(void))
{
#ifdef DBG
    callback_ui_init = dbg_frontend_init;
    callback_ui_update = dbg_frontend_update;
    callback_ui_vi = dbg_frontend_vi;
    return M64ERR_SUCCESS;
#else
    return M64ERR_UNSUPPORTED;
#endif
}

EXPORT m64p_error CALL DebugSetRunState(int runstate)
{
#ifdef DBG
    run = runstate; /* in debugger/debugger.c */
    return M64ERR_SUCCESS;
#else
    return M64ERR_UNSUPPORTED;
#endif
}

EXPORT int CALL DebugGetState(m64p_dbg_state statenum)
{
#ifdef DBG
    switch (statenum)
    {
        case M64P_DBG_RUN_STATE:
            return run;
        case M64P_DBG_PREVIOUS_PC:
            return previousPC;
        case M64P_DBG_NUM_BREAKPOINTS:
            return g_NumBreakpoints;
        case M64P_DBG_CPU_DYNACORE:
            return r4300emu;
        case M64P_DBG_CPU_NEXT_INTERRUPT:
            return next_interupt;
        default:
            DebugMessage(M64MSG_WARNING, "Front-end bug: invalid m64p_dbg_state input in DebugGetState()");
            return 0;
    }
    return 0;
#else
    DebugMessage(M64MSG_ERROR, "Front-end bug: DebugGetState() called, but Debugger not supported in Core library");
    return 0;
#endif
}

EXPORT m64p_error CALL DebugStep(void)
{
#ifdef DBG
    if (!g_DebuggerActive)
        return M64ERR_INVALID_STATE;
    debugger_step(); /* in debugger/debugger.c */
    return M64ERR_SUCCESS;
#else
    return M64ERR_UNSUPPORTED;
#endif
}

EXPORT void CALL DebugDecodeOp(unsigned int instruction, char *op, char *args, int pc)
{
#ifdef DBG
    r4300_decode_op(instruction, op, args, pc);
#else
    DebugMessage(M64MSG_ERROR, "Front-end bug: DebugDecodeOp() called, but Debugger not supported in Core library");
    return;
#endif
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

