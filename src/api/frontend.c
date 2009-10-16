/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus-core - api/frontend.c                                     *
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
                       
/* This file contains the Core front-end functions which will be exported
 * outside of the core library.
 */

#include <stdlib.h>

#include "m64p_types.h"
#include "callbacks.h"
#include "config.h"

EXPORT m64p_error CALL CoreStartup(int APIVersion, const char *ConfigPath, void *Context,
                                   void (*DebugCallback)(void *, int, const char *), void *Context2,
                                   void (*StateCallback)(void *, m64p_core_param, int))
{
  /* very first thing is to set the callback function for debug info */
  SetDebugCallback(DebugCallback, Context);

  /* next, start up the configuration handling code by loading and parsing the config file */
  if (ConfigInit() != M64ERR_SUCCESS)
      return M64ERR_INTERNAL;

  
  return M64ERR_INTERNAL;
}

EXPORT m64p_error CALL CoreShutdown(void)
{

  /* lastly, shut down the configuration code */
  ConfigShutdown();

  return M64ERR_INTERNAL;
}

EXPORT m64p_error CALL CoreAttachPlugin(m64p_plugin_type PluginType, m64p_dynlib_handle PluginLibHandle)
{
  return M64ERR_INTERNAL;
}

EXPORT m64p_error CALL CoreDetachPlugin(m64p_plugin_type PluginType)
{
  return M64ERR_INTERNAL;
}

EXPORT m64p_error CALL CoreDoCommand(m64p_command Command, int ParamInt, void *ParamPtr)
{
  return M64ERR_INTERNAL;
}

EXPORT m64p_error CALL CoreOverrideVidExt(m64p_video_extension_functions *VideoFunctionStruct)
{
  return M64ERR_INTERNAL;
}

EXPORT m64p_error CALL CoreAddCheat(const char *CheatName, m64p_cheat_code *CodeList, int NumCodes)
{
  return M64ERR_INTERNAL;
}

EXPORT m64p_error CALL CoreRemoveCheat(const char *CheatName)
{
  return M64ERR_INTERNAL;
}

EXPORT m64p_error CALL CoreRemoveAllCheats(void)
{
  return M64ERR_INTERNAL;
}


