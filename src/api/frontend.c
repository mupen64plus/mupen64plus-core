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

#include "main/main.h"
#include "main/version.h"

EXPORT m64p_error CALL CoreStartup(int APIVersion, const char *ConfigPath, void *Context,
                                   void (*DebugCallback)(void *, int, const char *), void *Context2,
                                   void (*StateCallback)(void *, m64p_core_param, int))
{
    /* very first thing is to set the callback function for debug info */
    SetDebugCallback(DebugCallback, Context);

    /* check front-end's API version */
    if (APIVersion < MINIMUM_FRONTEND_API_VERSION)
    {
        DebugMessage(M64MSG_ERROR, "CoreStartup(): Front-end API version %i.%i.%i is below minimum supported %i.%i.%i\n",
                     VERSION_PRINTF_SPLIT(APIVersion), VERSION_PRINTF_SPLIT(MINIMUM_FRONTEND_API_VERSION));
        return M64ERR_INCOMPATIBLE;
    }

    /* next, start up the configuration handling code by loading and parsing the config file */
    if (ConfigInit(ConfigPath) != M64ERR_SUCCESS)
        return M64ERR_INTERNAL;

    /* set default configuration parameter values for Core */
    if (ConfigOpenSection("Core", &g_CoreConfig) != M64ERR_SUCCESS || g_CoreConfig == NULL)
        return M64ERR_INTERNAL;

    ConfigSetDefaultBool(g_CoreConfig, "Fullscreen", 0, "Use fullscreen mode if True, or windowed mode if False ");
    ConfigSetDefaultBool(g_CoreConfig, "OnScreenDisplay", 1, "Draw on-screen display if True, otherwise don't draw OSD");
    ConfigSetDefaultInt(g_CoreConfig, "R4300Emulator", 1, "Use Pure Interpreter if 0, Cached Interpreter if 1, or Dynamic Recompiler if 2 or more");
    ConfigSetDefaultBool(g_CoreConfig, "NoCompiledJump", 0, "Disable compiled jump commands in dynamic recompiler (should be set to False) ");
    ConfigSetDefaultBool(g_CoreConfig, "DisableExtraMem", 0, "Disable 4MB expansion RAM pack. May be necessary for some games");
    ConfigSetDefaultBool(g_CoreConfig, "AutoStateSlotIncrement", 0, "Increment the save state slot after each save operation");
    ConfigSetDefaultString(g_CoreConfig, "ScreenshotPath", "", "Path to directory where screenshots are saved. If this is blank, the default value of ${UserConfigPath}/screenshot will be used");
    ConfigSetDefaultString(g_CoreConfig, "SaveStatePath", "", "Path to directory where save states are saved. If this is blank, the default value of ${UserConfigPath}/save will be used");
    ConfigSetDefaultString(g_CoreConfig, "SharedDataPath", "", "Path to a directory to search when looking for shared data files");

    return M64ERR_SUCCESS;
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


