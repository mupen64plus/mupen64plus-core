/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus-core - api_common.c                                       *
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

/* This file contains the Core common functions which will be exported
 * outside of the core library.
 */

#include <stdlib.h>

#include "api/m64p_types.h"
#include "version.h"

EXPORT m64p_error CALL PluginGetVersion(m64p_plugin_type *PluginType, int *PluginVersion, int *APIVersion, const char **PluginNamePtr, int *Capabilities)
{
    /* set version info */
    if (PluginType != NULL)
        *PluginType = M64PLUGIN_CORE;

    if (PluginVersion != NULL)
        *PluginVersion = MUPEN_CORE_VERSION;

    if (APIVersion != NULL)
        *APIVersion = MUPEN_API_VERSION;
    
    if (PluginNamePtr != NULL)
        *PluginNamePtr = MUPEN_CORE_NAME;

    if (Capabilities != NULL)
    {
        *Capabilities = 0;
#if defined(DBG)
         *Capabilities |= M64CAPS_DEBUGGER;
#endif
#if defined(DYNAREC)
         *Capabilities |= M64CAPS_DYNAREC;
#endif         
    }
                    
    return M64ERR_SUCCESS;
}

EXPORT const char * CALL CoreErrorMessage(m64p_error ReturnCode)
{
  return NULL;
}

