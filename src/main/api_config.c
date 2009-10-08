/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus-core - api_config.c                                       *
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
                       
/* This file contains the Core config functions which will be exported
 * outside of the core library.
 */

#include <stdlib.h>

#include "../api/m64p_types.h"
 
EXPORT m64p_error CALL ConfigListSections(void *context, void (*SectionListCallback)(void * context, const char * SectionName))
{
  return M64ERR_INTERNAL;
}

EXPORT m64p_error CALL ConfigOpenSection(const char *SectionName, m64p_handle *ConfigSectionHandle)
{
  return M64ERR_INTERNAL;
}

EXPORT m64p_error CALL ConfigListParameters(m64p_handle *ConfigSectionHandle, void *context, void (*ParameterListCallback)(void * context, const char *ParamName, m64p_type ParamType))
{
  return M64ERR_INTERNAL;
}

EXPORT m64p_error CALL ConfigSetParameter(m64p_handle *ConfigSectionHandle, const char *ParamName, m64p_type ParamType, void *ParamValue)
{
  return M64ERR_INTERNAL;
}

EXPORT m64p_error CALL ConfigGetParameter(m64p_handle *ConfigSectionHandle, const char *ParamName, m64p_type ParamType, void *ParamValue, int MaxSize)
{
  return M64ERR_INTERNAL;
}

EXPORT const char * CALL ConfigGetParameterHelp(m64p_handle *ConfigSectionHandle, const char *ParamName)
{
  return NULL;
}

EXPORT m64p_error CALL ConfigSetDefaultInt(m64p_handle *ConfigSectionHandle, const char *ParamName, int ParamValue, const char *ParamHelp)
{
  return M64ERR_INTERNAL;
}

EXPORT m64p_error CALL ConfigSetDefaultBool(m64p_handle *ConfigSectionHandle, const char *ParamName, int ParamValue, const char *ParamHelp)
{
  return M64ERR_INTERNAL;
}

EXPORT m64p_error CALL ConfigSetDefaultString(m64p_handle *ConfigSectionHandle, const char *ParamName, const char * ParamValue, const char *ParamHelp)
{
  return M64ERR_INTERNAL;
}

EXPORT int CALL ConfigGetParamInt(m64p_handle *ConfigSectionHandle, const char *ParamName)
{
  return 0;
}

EXPORT int CALL ConfigGetParamBool(m64p_handle *ConfigSectionHandle, const char *ParamName)
{
  return 0;
}

EXPORT const char * CALL ConfigGetParamString(m64p_handle *ConfigSectionHandle, const char *ParamName)
{
  return NULL;
}

EXPORT const char * CALL ConfigGetSharedDataFilepath(const char *filename)
{
  return NULL;
}

EXPORT const char * CALL ConfigGetUserDataPath(void)
{
  return NULL;
}

EXPORT const char * CALL ConfigGetUserCachePath(void)
{
  return NULL;
}

