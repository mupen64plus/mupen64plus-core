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
#include <string.h>
#include <stdio.h>

#include "api/m64p_types.h"
#include "osal/files.h"
#include "osal/strings.h"
#include "api_config.h"

/* local types */
#define SECTION_MAGIC 0xDBDC0580

typedef struct _config_var {
  char                  name[64];
  m64p_type             type;
  int                   val_int;
  double                val_double;
  char                 *val_string;
  char                 *comment;
  struct _config_var   *next;
  } config_var;

typedef struct _config_section {
  int                     magic;
  char                    name[64];
  struct _config_var     *first_var;
  struct _config_section *next;
  } config_section;

/* local variables */
static int      l_ConfigInit = 0;
config_section *l_SectionHead = NULL;

/* --------------- */
/* local functions */
/* --------------- */
static void strip_whitespace(char *string)
{
    char *start = string;
    char *end = string + strlen(string) - 1;

    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n')
        start++;

    while (end > string && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n'))
        end--;

    if (start > end)
    {
        string[0] = 0;
        return;
    }

    int newlen = end - start + 1;
    memmove(string, start, newlen);
    string[newlen] = 0;

    return;
}

static int is_numeric(const char *string)
{
    int dots = 0;

    if (*string == '-') string++;

    while (*string != 0)
    {
        char ch = *string++;
        if (ch >= '0' && ch <= '9')
            continue;
        else if (ch == '.' && dots == 0)
            dots++;
        else
            return 0;
    }

    return 1; /* true, input is numeric */
}

/* ----------------------------------------------------------- */
/* these functions are only to be used within the Core library */
/* ----------------------------------------------------------- */

m64p_error ConfigInit(void)
{
    if (l_ConfigInit)
        return M64ERR_ALREADY_INIT;
    l_ConfigInit = 1;

    /* get the full pathname to the config file and try to open it */
    const char *configpath = ConfigGetUserConfigPath();
    if (configpath == NULL)
        return M64ERR_FILES;

    char *filepath = (char *) malloc(strlen(configpath) + 32);
    if (filepath == NULL)
        return M64ERR_NO_MEMORY;

    strcpy(filepath, configpath);
    strcat(filepath, "mupen64plus.cfg");
    FILE *fPtr = fopen(filepath, "rb");
    if (fPtr == NULL)
    {
        DebugMessage(M64MSG_WARNING, "Couldn't open configuration file '%s'.  Using defaults.", filepath);
        free(filepath);
        return M64ERR_SUCCESS;
    }
    free(filepath);

    /* read the entire config file */
    fseek(fPtr, 0L, SEEK_END);
    long filelen = ftell(fPtr);
    fseek(fPtr, 0L, SEEK_SET);

    char *configtext = (char *) malloc(filelen + 16);
    if (configtext == NULL)
    {
        fclose(fPtr);
        return M64ERR_NO_MEMORY;
    }
    if (fread(configtext, 1, filelen, fPtr) != filelen)
    {
        free(configtext);
        fclose(fPtr);
        return M64ERR_FILES;
    }
    fclose(fPtr);

    /* parse the file data */
    config_section *current_section = NULL;
    char *line = configtext;
    char *end = configtext + filelen;
    char *lastcomment = NULL;
    *end = 0;
    while (line < end)
    {
        /* get the pointer to the next line, and null-terminate this line */
        char *nextline = strchr(line, '\n');
        if (nextline == NULL)
            nextline = end;
        *nextline++ = 0;
        /* strip the whitespace and handle comment */
        strip_whitespace(line);
        if (strlen(line) < 1)
        {
            line = nextline;
            continue;
        }
        if (line[0] == '#')
        {
            line++;
            strip_whitespace(line);
            lastcomment = line;
            line = nextline;
            continue;
        }
        /* handle section definition line */
        if (strlen(line) > 2 && line[0] == '[' && line[strlen(line)-1] == ']')
        {
            config_section *last_section = current_section;
            line++;
            line[strlen(line)-1] = 0;
            current_section = (config_section *) malloc(sizeof(config_section));
            if (current_section == NULL)
            {
                free(configtext);
                return M64ERR_INTERNAL;
            }
            current_section->magic = SECTION_MAGIC;
            strncpy(current_section->name, line, 63);
            current_section->name[63] = 0;
            current_section->first_var = NULL;
            current_section->next = NULL;
            lastcomment = NULL;
            if (last_section == NULL)
                l_SectionHead = current_section;
            else
                last_section->next = current_section;
            line = nextline;
            continue;
        }
        /* handle variable definition */
        char *pivot = strchr(line, '=');
        if (current_section == NULL || pivot == NULL)
        {
            line = nextline;
            continue;
        }
        char *varname = line;
        char *varvalue = pivot + 1;
        *pivot = 0;
        strip_whitespace(varname);
        strip_whitespace(varvalue);
        if (varvalue[0] == '"' && varvalue[strlen(varvalue)-1] == '"')
        {
            varvalue++;
            varvalue[strlen(varvalue)-1] = 0;
            ConfigSetDefaultString((m64p_handle) current_section, varname, varvalue, lastcomment);
        }
        else if (osal_insensitive_strcmp(varvalue, "false") == 0)
        {
            ConfigSetDefaultBool((m64p_handle) current_section, varname, 0, lastcomment);
        }
        else if (osal_insensitive_strcmp(varvalue, "true") == 0)
        {
            ConfigSetDefaultBool((m64p_handle) current_section, varname, 1, lastcomment);
        }
        else if (is_numeric(varvalue))
        {
            int val_int = (int) strtol(varvalue, NULL, 10);
            double val_float = strtod(varvalue, NULL);
            if ((val_float - val_int) != 0.0)
                ConfigSetDefaultFloat((m64p_handle) current_section, varname, (float) val_float, lastcomment);
            else
                ConfigSetDefaultInt((m64p_handle) current_section, varname, val_int, lastcomment);
        }
        lastcomment = NULL;
        line = nextline;
    }

    free(configtext);
    return M64ERR_SUCCESS;
}

m64p_error ConfigShutdown(void)
{
    if (!l_ConfigInit)
        return M64ERR_NOT_INIT;
    l_ConfigInit = 0;

}

/* -------------------------------------------------------------------------- */
/* these functions are exported outside of the Core as well as used within it */
/* -------------------------------------------------------------------------- */

EXPORT m64p_error CALL ConfigListSections(void *context, void (*SectionListCallback)(void * context, const char * SectionName))
{
  return M64ERR_INTERNAL;
}

EXPORT m64p_error CALL ConfigOpenSection(const char *SectionName, m64p_handle *ConfigSectionHandle)
{
  return M64ERR_INTERNAL;
}

EXPORT m64p_error CALL ConfigListParameters(m64p_handle ConfigSectionHandle, void *context, void (*ParameterListCallback)(void * context, const char *ParamName, m64p_type ParamType))
{
  return M64ERR_INTERNAL;
}

EXPORT m64p_error CALL ConfigSetParameter(m64p_handle ConfigSectionHandle, const char *ParamName, m64p_type ParamType, const void *ParamValue)
{
  return M64ERR_INTERNAL;
}

EXPORT m64p_error CALL ConfigGetParameter(m64p_handle ConfigSectionHandle, const char *ParamName, m64p_type ParamType, void *ParamValue, int MaxSize)
{
  return M64ERR_INTERNAL;
}

EXPORT const char * CALL ConfigGetParameterHelp(m64p_handle ConfigSectionHandle, const char *ParamName)
{
  return NULL;
}

EXPORT m64p_error CALL ConfigSetDefaultInt(m64p_handle ConfigSectionHandle, const char *ParamName, int ParamValue, const char *ParamHelp)
{
  return M64ERR_INTERNAL;
}

EXPORT m64p_error CALL ConfigSetDefaultFloat(m64p_handle ConfigSectionHandle, const char *ParamName, float ParamValue, const char *ParamHelp)
{
  return M64ERR_INTERNAL;
}

EXPORT m64p_error CALL ConfigSetDefaultBool(m64p_handle ConfigSectionHandle, const char *ParamName, int ParamValue, const char *ParamHelp)
{
  return M64ERR_INTERNAL;
}

EXPORT m64p_error CALL ConfigSetDefaultString(m64p_handle ConfigSectionHandle, const char *ParamName, const char * ParamValue, const char *ParamHelp)
{
  return M64ERR_INTERNAL;
}

EXPORT int CALL ConfigGetParamInt(m64p_handle ConfigSectionHandle, const char *ParamName)
{
  return 0;
}

EXPORT float CALL ConfigGetParamFloat(m64p_handle ConfigSectionHandle, const char *ParamName)
{
  return 0.0;
}

EXPORT int CALL ConfigGetParamBool(m64p_handle ConfigSectionHandle, const char *ParamName)
{
  return 0;
}

EXPORT const char * CALL ConfigGetParamString(m64p_handle ConfigSectionHandle, const char *ParamName)
{
  return NULL;
}

EXPORT const char * CALL ConfigGetSharedDataFilepath(const char *filename)
{
    /* check input parameter */
    if (filename == NULL) return NULL;

    /* try to get the SharedDataPath string variable in the Core configuration section */
    const char *configsharepath = NULL;
    m64p_handle CoreHandle = NULL;
    if (ConfigOpenSection("Core", &CoreHandle) == M64ERR_SUCCESS)
    {
        configsharepath = ConfigGetParamString(CoreHandle, "SharedDataPath");
    }

    return osal_get_shared_filepath(filename, configsharepath);
}

EXPORT const char * CALL ConfigGetUserConfigPath(void)
{
  return osal_get_user_configpath();
}

EXPORT const char * CALL ConfigGetUserDataPath(void)
{
  return osal_get_user_datapath();
}

EXPORT const char * CALL ConfigGetUserCachePath(void)
{
  return osal_get_user_cachepath();
}

