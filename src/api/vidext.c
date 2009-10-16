/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus-core - api/vidext.c                                       *
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
                       
/* This file contains the Core video extension functions which will be exported
 * outside of the core library.
 */

#include <stdlib.h>

#include "m64p_types.h"
 
EXPORT m64p_error CALL VidExt_Init(void)
{
  return M64ERR_INTERNAL;
}

EXPORT m64p_error CALL VidExt_Quit(void)
{
  return M64ERR_INTERNAL;
}

EXPORT m64p_error CALL VidExt_ListFullscreenModes(m64p_2d_size *SizeArray, int *NumSizes)
{
  return M64ERR_INTERNAL;
}

EXPORT m64p_error CALL VidExt_SetVideoMode(int Width, int Height, int BitsPerPixel, m64p_video_mode ScreenMode)
{
  return M64ERR_INTERNAL;
}

EXPORT m64p_error CALL VidExt_SetCaption(const char *Title)
{
  return M64ERR_INTERNAL;
}

EXPORT m64p_error CALL VidExt_ToggleFullScreen(void)
{
  return M64ERR_INTERNAL;
}

EXPORT void * CALL VidExt_GL_GetProcAddress(const char* Proc)
{
  return NULL;
}

EXPORT m64p_error CALL VidExt_GL_SetAttribute(m64p_GLattr Attr, int Value)
{
  return M64ERR_INTERNAL;
}

EXPORT m64p_error CALL VidExt_GL_SwapBuffers(void)
{
  return M64ERR_INTERNAL;
}

