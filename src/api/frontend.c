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
#include <string.h>

#include "m64p_types.h"
#include "callbacks.h"
#include "config.h"

#include "main/main.h"
#include "main/rom.h"
#include "main/version.h"
#include "plugin/plugin.h"

/* some local state variables */
static int l_CoreInit = 0;
static int l_ROMOpen = 0;
static int l_EmuRunning = 0;

/* functions exported outside of libmupen64plus to front-end application */
EXPORT m64p_error CALL CoreStartup(int APIVersion, const char *ConfigPath, const char *DataPath, void *Context,
                                   void (*DebugCallback)(void *, int, const char *), void *Context2,
                                   void (*StateCallback)(void *, m64p_core_param, int))
{
    if (l_CoreInit)
        return M64ERR_ALREADY_INIT;

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
    if (ConfigInit(ConfigPath, DataPath) != M64ERR_SUCCESS)
        return M64ERR_INTERNAL;

    /* set default configuration parameter values for Core */
    if (ConfigOpenSection("Core", &g_CoreConfig) != M64ERR_SUCCESS || g_CoreConfig == NULL)
        return M64ERR_INTERNAL;
    main_set_core_defaults();

    /* set up multi-language support */
    tr_init();

    /* The ROM database contains MD5 hashes, goodnames, and some game-specific parameters */
    romdatabase_open();

    l_CoreInit = 1;
    return M64ERR_SUCCESS;
}

EXPORT m64p_error CALL CoreShutdown(void)
{
    if (!l_CoreInit)
        return M64ERR_NOT_INIT;

    /* close down some core sub-systems */
    cheat_delete_all(); /* fixme: should this be somewhere else? */
    romdatabase_close();
    tr_delete_languages();

    /* lastly, shut down the configuration code */
    ConfigShutdown();

    l_CoreInit = 0;
    return M64ERR_SUCCESS;
}

EXPORT m64p_error CALL CoreAttachPlugin(m64p_plugin_type PluginType, m64p_dynlib_handle PluginLibHandle)
{
    m64p_error rval;

    if (!l_CoreInit)
        return M64ERR_NOT_INIT;
    if (g_EmulatorRunning || !l_ROMOpen)
        return M64ERR_INVALID_STATE;

    rval = plugin_connect(PluginType, PluginLibHandle);
    if (rval != M64ERR_SUCCESS)
        return rval;

    rval = plugin_start(PluginType);
    if (rval != M64ERR_SUCCESS)
        return rval;

    return M64ERR_SUCCESS;
}

EXPORT m64p_error CALL CoreDetachPlugin(m64p_plugin_type PluginType)
{
    if (!l_CoreInit)
        return M64ERR_NOT_INIT;
    if (g_EmulatorRunning)
        return M64ERR_INVALID_STATE;

    return plugin_connect(PluginType, NULL);
}

EXPORT m64p_error CALL CoreDoCommand(m64p_command Command, int ParamInt, void *ParamPtr)
{
    m64p_error rval;

    if (!l_CoreInit)
        return M64ERR_NOT_INIT;

    switch(Command)
    {
        case M64CMD_NOP:
            return M64ERR_SUCCESS;
        case M64CMD_ROM_OPEN:
            if (g_EmulatorRunning || l_ROMOpen)
                return M64ERR_INVALID_STATE;
            if (ParamPtr == NULL || ParamInt < 4096)
                return M64ERR_INPUT_ASSERT;
            rval = open_rom((const char *) ParamPtr, ParamInt);
            if (rval == M64ERR_SUCCESS)
                l_ROMOpen = 1;
            return rval;
        case M64CMD_ROM_CLOSE:
            if (g_EmulatorRunning || !l_ROMOpen)
                return M64ERR_INVALID_STATE;
            l_ROMOpen = 0;
            return close_rom();
        case M64CMD_ROM_GET_HEADER:
            if (!l_ROMOpen || ROM_HEADER == NULL)
                return M64ERR_INVALID_STATE;
            if (ParamPtr == NULL)
                return M64ERR_INPUT_ASSERT;
            if (sizeof(rom_header) < ParamInt)
                ParamInt = sizeof(rom_header);
            memcpy(ParamPtr, ROM_HEADER, ParamInt);
            return M64ERR_SUCCESS;
        case M64CMD_ROM_GET_SETTINGS:
            if (!l_ROMOpen)
                return M64ERR_INVALID_STATE;
            if (ParamPtr == NULL)
                return M64ERR_INPUT_ASSERT;
            if (sizeof(rom_settings) < ParamInt)
                ParamInt = sizeof(rom_settings);
            memcpy(ParamPtr, &ROM_SETTINGS, ParamInt);
            return M64ERR_SUCCESS;
        case M64CMD_EXECUTE:
            if (g_EmulatorRunning || !l_ROMOpen)
                return M64ERR_INVALID_STATE;
            /* print out plugin-related warning messages */
            plugin_check();
            /* the main_run() function will not return until the player has quit the game */
            main_run();
            return M64ERR_SUCCESS;
        case M64CMD_STOP:
        case M64CMD_PAUSE:
        case M64CMD_RESUME:
        case M64CMD_CORE_STATE_QUERY:
        case M64CMD_SEND_SDL_KEYDOWN:
        case M64CMD_SEND_SDL_KEYUP:
        case M64CMD_SET_FRAME_CALLBACK:
        default:
            return M64ERR_INPUT_INVALID;
    }

    return M64ERR_INTERNAL;
}

EXPORT m64p_error CALL CoreOverrideVidExt(m64p_video_extension_functions *VideoFunctionStruct)
{
    if (!l_CoreInit)
        return M64ERR_NOT_INIT;

    return M64ERR_INTERNAL;
}

EXPORT m64p_error CALL CoreAddCheat(const char *CheatName, m64p_cheat_code *CodeList, int NumCodes)
{
    if (!l_CoreInit)
        return M64ERR_NOT_INIT;

    return M64ERR_INTERNAL;
}

EXPORT m64p_error CALL CoreRemoveCheat(const char *CheatName)
{
    if (!l_CoreInit)
        return M64ERR_NOT_INIT;

    return M64ERR_INTERNAL;
}

EXPORT m64p_error CALL CoreRemoveAllCheats(void)
{
    if (!l_CoreInit)
        return M64ERR_NOT_INIT;

    return M64ERR_INTERNAL;
}


