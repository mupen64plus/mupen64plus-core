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
#include "video.h"
#include "Graphics_1.3.h"

char pluginName[] = "No Video";
char *screenDirectory;
unsigned int last_good_ucode = -1;
void (*CheckInterrupts)( void );
char configdir[PATH_MAX] = {0};

#ifndef __LINUX__

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD dwReason, LPVOID lpvReserved)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {

    }
    return TRUE;
}
#else
void _init( void )
{
}
#endif // !__LINUX__

EXPORT void CALL CaptureScreen ( char * Directory )
{

}

EXPORT void CALL ChangeWindow (void)
{

}

EXPORT void CALL CloseDLL (void)
{
}

EXPORT void CALL DllAbout ( HWND hParent )
{

}

EXPORT void CALL DllConfig ( HWND hParent )
{

}

EXPORT void CALL DllTest ( HWND hParent )
{
}

EXPORT void CALL DrawScreen (void)
{
}

EXPORT void CALL GetDllInfo ( PLUGIN_INFO * PluginInfo )
{
    PluginInfo->Version = 0x103;
    PluginInfo->Type = PLUGIN_TYPE_GFX;
    strcpy( PluginInfo->Name, pluginName );
    PluginInfo->NormalMemory = FALSE;
    PluginInfo->MemoryBswaped = TRUE;
}


EXPORT BOOL CALL InitiateGFX (GFX_INFO Gfx_Info)
{
    return TRUE;
}

EXPORT void CALL MoveScreen (int xpos, int ypos)
{
}

EXPORT void CALL ProcessDList(void)
{

}

EXPORT void CALL ProcessRDPList(void)
{

}

EXPORT void CALL RomClosed (void)
{

}

EXPORT void CALL RomOpen (void)
{

}

EXPORT void CALL ShowCFB (void)
{

}

EXPORT void CALL UpdateScreen (void)
{

}

EXPORT void CALL ViStatusChanged (void)
{

}

EXPORT void CALL ViWidthChanged (void)
{

}


EXPORT void CALL ReadScreen (void **dest, int *width, int *height)
{

}

EXPORT void CALL SetConfigDir (char *configDir)
{

}

