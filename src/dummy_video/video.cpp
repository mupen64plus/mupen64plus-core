/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - video.cpp                                               *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2008 John Chadwick (NMN)                                *
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

#include <limits.h>
#include <string.h>

#ifdef WIN32
#include <windows.h>
#else
# include "../main/winlnxdefs.h"
#endif

#include "Graphics_1.3.h"

static char pluginName[] = "No Video";
static char *screenDirectory;
static unsigned int last_good_ucode = -1;
static char configdir[PATH_MAX] = {0};

void (*CheckInterrupts)( void );


EXPORT void CALL dummyvideo_CaptureScreen ( char * Directory )
{

}

EXPORT void CALL dummyvideo_ChangeWindow (void)
{

}

EXPORT void CALL dummyvideo_CloseDLL (void)
{
}

EXPORT void CALL dummyvideo_DllAbout ( HWND hParent )
{

}

EXPORT void CALL dummyvideo_DllConfig ( HWND hParent )
{

}

EXPORT void CALL dummyvideo_DllTest ( HWND hParent )
{
}

EXPORT void CALL dummyvideo_DrawScreen (void)
{
}

EXPORT void CALL dummyvideo_GetDllInfo ( PLUGIN_INFO * PluginInfo )
{
    PluginInfo->Version = 0x103;
    PluginInfo->Type = PLUGIN_TYPE_GFX;
    strcpy( PluginInfo->Name, pluginName );
    PluginInfo->NormalMemory = FALSE;
    PluginInfo->MemoryBswaped = TRUE;
}


EXPORT BOOL CALL dummyvideo_InitiateGFX (GFX_INFO Gfx_Info)
{
    return TRUE;
}

EXPORT void CALL dummyvideo_MoveScreen (int xpos, int ypos)
{
}

EXPORT void CALL dummyvideo_ProcessDList(void)
{

}

EXPORT void CALL dummyvideo_ProcessRDPList(void)
{

}

EXPORT void CALL dummyvideo_RomClosed (void)
{

}

EXPORT void CALL dummyvideo_RomOpen (void)
{

}

EXPORT void CALL dummyvideo_ShowCFB (void)
{

}

EXPORT void CALL dummyvideo_UpdateScreen (void)
{

}

EXPORT void CALL dummyvideo_ViStatusChanged (void)
{

}

EXPORT void CALL dummyvideo_ViWidthChanged (void)
{

}

EXPORT void CALL dummyvideo_ReadScreen (void **dest, int *width, int *height)
{

}

EXPORT void CALL dummyvideo_SetConfigDir (char *configDir)
{

}

