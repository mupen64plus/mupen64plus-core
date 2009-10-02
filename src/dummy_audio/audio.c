/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - audio.h                                                 *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2008 Richard42                                          *
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

#include <stdio.h>

#include "../main/winlnxdefs.h"

#include "Audio_1.1.h"

static AUDIO_INFO AudioInfo;


EXPORT void CALL
AiDacrateChanged( int SystemType )
{
}

EXPORT void CALL
AiLenChanged( void )
{
}

EXPORT DWORD CALL
AiReadLength( void )
{
    return 0;
}

EXPORT void CALL
AiUpdate( BOOL Wait )
{
}

EXPORT void CALL
CloseDLL( void )
{
}

EXPORT void CALL
DllAbout( HWND hParent )
{
    printf ("No Audio Plugin\n" );
}

EXPORT void CALL
DllConfig ( HWND hParent )
{
}

EXPORT void CALL
DllTest ( HWND hParent )
{
}

EXPORT void CALL
GetDllInfo( PLUGIN_INFO * PluginInfo )
{
    PluginInfo->Version = 0x0101;
    PluginInfo->Type    = PLUGIN_TYPE_AUDIO;
    sprintf(PluginInfo->Name,"No Audio");
    PluginInfo->NormalMemory  = TRUE;
    PluginInfo->MemoryBswaped = TRUE;
}

EXPORT BOOL CALL
InitiateAudio( AUDIO_INFO Audio_Info )
{
    AudioInfo = Audio_Info;
    return TRUE;
}

EXPORT void CALL RomOpen(void)
{
}

EXPORT void CALL
RomClosed( void )
{
}

EXPORT void CALL
ProcessAList( void )
{
}

