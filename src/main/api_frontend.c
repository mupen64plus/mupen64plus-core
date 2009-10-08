/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus-core - api_frontend.c                                     *
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

#include "../api/m64p_types.h"

EXPORT m64p_error CALL CoreStartup(int APIVersion, const char *ConfigPath, void *Context,
                                   void (*DebugCallback)(void *Context, const char *message), void *Context2,
                                   void (*StateCallback)(void *Context2, m64p_core_param ParamChanged, int NewValue))
{
  return M64ERR_INTERNAL;
}

EXPORT m64p_error CALL CoreShutdown(void)
{
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


