/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus-core - api/config.c                                       *
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

#include "m64p_types.h"
#include "config.h"
#include "callbacks.h"

#include "osal/files.h"
#include "osal/strings.h"

/* local types */
#define MUPEN64PLUS_CFG_NAME "mupen64plus.cfg"

#define SECTION_MAGIC 0xDBDC0580

typedef struct _config_var {
  char                  name[64];
  m64p_type             type;
  int                   val_int;
  float                 val_float;
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

static config_var *find_section_var(config_section *section, const char *ParamName)
{
    /* walk through the linked list of variables in the section */
    config_var *curr_var = section->first_var;
    while (curr_var != NULL)
    {
        if (osal_insensitive_strcmp(ParamName, curr_var->name) == 0)
        {
            return curr_var;
        }
        curr_var = curr_var->next;
    }

    /* couldn't find this configuration parameter */
    return NULL;
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
    strcat(filepath, MUPEN64PLUS_CFG_NAME);
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
            float val_float = (float) strtod(varvalue, NULL);
            if ((val_float - val_int) != 0.0)
                ConfigSetDefaultFloat((m64p_handle) current_section, varname, val_float, lastcomment);
            else
                ConfigSetDefaultInt((m64p_handle) current_section, varname, val_int, lastcomment);
        }
        else
        {
            DebugMessage(M64MSG_WARNING, "Invalid config file parameter: %s=%s", varname, varvalue);
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

    /* free all of the malloc'd blocks */
    config_section *curr_section = l_SectionHead;
    while (curr_section != NULL)
    {
        config_section *next_section = curr_section->next;
        /* delete all the variables in this section */
        config_var *curr_var = curr_section->first_var;
        while (curr_var != NULL)
        {
            config_var *next_var = curr_var->next;
            if (curr_var->val_string != NULL)
                free(curr_var->val_string);
            if (curr_var->comment != NULL)
                free(curr_var->comment);
            free(curr_var);
            curr_var = next_var;
        }
        /* delete the section itself */
        free(curr_section);
        curr_section = next_section;
    }

    l_SectionHead = NULL;
    return M64ERR_SUCCESS;
}

/* ------------------------------------------------ */
/* Selector functions, exported outside of the Core */
/* ------------------------------------------------ */

EXPORT m64p_error CALL ConfigListSections(void *context, void (*SectionListCallback)(void * context, const char * SectionName))
{
    if (!l_ConfigInit)
        return M64ERR_NOT_INIT;
    if (SectionListCallback == NULL)
        return M64ERR_INPUT_ASSERT;

    /* just walk through the section list, making a callback for each section name */
    config_section *curr_section = l_SectionHead;
    while (curr_section != NULL)
    {
        (*SectionListCallback)(context, curr_section->name);
        curr_section = curr_section->next;
    }

    return M64ERR_SUCCESS;
}

EXPORT m64p_error CALL ConfigOpenSection(const char *SectionName, m64p_handle *ConfigSectionHandle)
{
    if (!l_ConfigInit)
        return M64ERR_NOT_INIT;
    if (SectionName == NULL || ConfigSectionHandle == NULL)
        return M64ERR_INPUT_ASSERT;

    /* walk through the section list, looking for a case-insensitive name match */
    config_section *curr_section = l_SectionHead;
    while (curr_section != NULL)
    {
        if (osal_insensitive_strcmp(SectionName, curr_section->name) == 0)
        {
            *ConfigSectionHandle = curr_section;
            return M64ERR_SUCCESS;
        }
        curr_section = curr_section->next;
    }

    /* didn't find the section */
    *ConfigSectionHandle = NULL;
    return M64ERR_INPUT_NOT_FOUND;
}

EXPORT m64p_error CALL ConfigListParameters(m64p_handle ConfigSectionHandle, void *context, void (*ParameterListCallback)(void * context, const char *ParamName, m64p_type ParamType))
{
    if (!l_ConfigInit)
        return M64ERR_NOT_INIT;
    if (ConfigSectionHandle == NULL || ParameterListCallback == NULL)
        return M64ERR_INPUT_ASSERT;

    config_section *section = (config_section *) ConfigSectionHandle;
    if (section->magic != SECTION_MAGIC)
        return M64ERR_INPUT_INVALID;

    /* walk through this section's parameter list, making a callback for each parameter */
    config_var *curr_var = section->first_var;
    while (curr_var != NULL)
    {
        (*ParameterListCallback)(context, curr_var->name, curr_var->type);
        curr_var = curr_var->next;
    }

  return M64ERR_SUCCESS;
}

EXPORT m64p_error CALL ConfigSaveFile(void)
{
    if (!l_ConfigInit)
        return M64ERR_NOT_INIT;

    /* get the full pathname to the config file and try to open it */
    const char *configpath = ConfigGetUserConfigPath();
    if (configpath == NULL)
        return M64ERR_FILES;

    char *filepath = (char *) malloc(strlen(configpath) + 32);
    if (filepath == NULL)
        return M64ERR_NO_MEMORY;

    strcpy(filepath, configpath);
    strcat(filepath, MUPEN64PLUS_CFG_NAME);
    FILE *fPtr = fopen(filepath, "wb"); 
    if (fPtr == NULL)
    {
        DebugMessage(M64MSG_ERROR, "Couldn't open configuration file '%s' for writing.", filepath);
        free(filepath);
        return M64ERR_FILES;
    }
    free(filepath);

    /* write out header */
    fprintf(fPtr, "# Mupen64Plus Configuration File\n");
    fprintf(fPtr, "# This file is automatically read and written by the Mupen64Plus Core library\n\n");

    /* write out all of the config parameters */
    config_section *curr_section = l_SectionHead;
    while (curr_section != NULL)
    {
        fprintf(fPtr, "[%s]\n", curr_section->name);
        config_var *curr_var = curr_section->first_var;
        while (curr_var != NULL)
        {
            if (curr_var->comment != NULL && strlen(curr_var->comment) > 0)
                fprintf(fPtr, "# %s", curr_var->comment);
            else if (curr_var->type == M64TYPE_INT)
                fprintf(fPtr, "%s = %i\n", curr_var->name, curr_var->val_int);
            else if (curr_var->type == M64TYPE_FLOAT)
                fprintf(fPtr, "%s = %f\n", curr_var->name, curr_var->val_float);
            else if (curr_var->type == M64TYPE_BOOL && curr_var->val_int)
                fprintf(fPtr, "%s = True\n", curr_var->name);
            else if (curr_var->type == M64TYPE_BOOL && !curr_var->val_int)
                fprintf(fPtr, "%s = False\n", curr_var->name);
            else if (curr_var->type == M64TYPE_STRING && curr_var->val_string != NULL)
                fprintf(fPtr, "%s = \"%s\"\n", curr_var->name, curr_var->val_string);
            curr_var = curr_var->next;
        }
        fprintf(fPtr, "\n");
        curr_section = curr_section->next;
    }

    fclose(fPtr);
    return M64ERR_SUCCESS;
}

/* ------------------------------------------------------- */
/* Generic Get/Set functions, exported outside of the Core */
/* ------------------------------------------------------- */

EXPORT m64p_error CALL ConfigSetParameter(m64p_handle ConfigSectionHandle, const char *ParamName, m64p_type ParamType, const void *ParamValue)
{
    /* check input conditions */
    if (!l_ConfigInit)
        return M64ERR_NOT_INIT;
    if (ConfigSectionHandle == NULL || ParamName == NULL || ParamValue == NULL || (int) ParamType < 1 || (int) ParamType > 4)
        return M64ERR_INPUT_ASSERT;

    config_section *section = (config_section *) ConfigSectionHandle;
    if (section->magic != SECTION_MAGIC)
        return M64ERR_INPUT_INVALID;

    /* if this parameter doesn't already exist, then create it and add it to the section */
    config_var *var = find_section_var(section, ParamName);
    if (var == NULL)
    {
        var = (config_var *) malloc(sizeof(config_var));
        if (var == NULL)
            return M64ERR_NO_MEMORY;
        strncpy(var->name, ParamName, 63);
        var->name[63] = 0;
        var->type = M64TYPE_INT;
        var->val_int = 0;
        var->val_string = NULL;
        var->comment = NULL;
        var->next = section->first_var;
        section->first_var = var;
    }

    /* set this parameter's value */
    var->type = ParamType;
    switch(ParamType)
    {
        case M64TYPE_INT:
            var->val_int = *((int *) ParamValue);
            break;
        case M64TYPE_FLOAT:
            var->val_float = *((float *) ParamValue);
            break;
        case M64TYPE_BOOL:
            var->val_int = (*((int *) ParamValue) != 0);
            break;
        case M64TYPE_STRING:
            if (var->val_string != NULL)
                free(var->val_string);
            var->val_string = malloc(strlen((char *) ParamValue) + 1);
            if (var->val_string == NULL)
                return M64ERR_NO_MEMORY;
            memcpy(var->val_string, ParamValue, strlen((char *) ParamValue) + 1);
            break;
        default:
            /* this is logically impossible because of the ParamType check at the top of this function */
            break;
    }

    return M64ERR_SUCCESS;
}

EXPORT m64p_error CALL ConfigGetParameter(m64p_handle ConfigSectionHandle, const char *ParamName, m64p_type ParamType, void *ParamValue, int MaxSize)
{
    /* check input conditions */
    if (!l_ConfigInit)
        return M64ERR_NOT_INIT;
    if (ConfigSectionHandle == NULL || ParamName == NULL || ParamValue == NULL || (int) ParamType < 1 || (int) ParamType > 4)
        return M64ERR_INPUT_ASSERT;

    config_section *section = (config_section *) ConfigSectionHandle;
    if (section->magic != SECTION_MAGIC)
        return M64ERR_INPUT_INVALID;

    /* if this parameter doesn't already exist, return an error */
    config_var *var = find_section_var(section, ParamName);
    if (var == NULL)
        return M64ERR_INPUT_NOT_FOUND;

    /* call the specific Get function to translate the parameter to the desired type */
    switch(ParamType)
    {
        case M64TYPE_INT:
            *((int *) ParamValue) = ConfigGetParamInt(ConfigSectionHandle, ParamName);
            break;
        case M64TYPE_FLOAT:
            *((float *) ParamValue) = ConfigGetParamFloat(ConfigSectionHandle, ParamName);
            break;
        case M64TYPE_BOOL:
            *((int *) ParamValue) = ConfigGetParamBool(ConfigSectionHandle, ParamName);
            break;
        case M64TYPE_STRING:
        {
            const char *string = ConfigGetParamString(ConfigSectionHandle, ParamName);
            strncpy(ParamValue, string, MaxSize);
            *((char *) ParamValue + MaxSize - 1) = 0;
            break;
        }
        default:
            /* this is logically impossible because of the ParamType check at the top of this function */
            break;
    }

    return M64ERR_SUCCESS;
}

EXPORT const char * CALL ConfigGetParameterHelp(m64p_handle ConfigSectionHandle, const char *ParamName)
{
    /* check input conditions */
    if (!l_ConfigInit || ConfigSectionHandle == NULL || ParamName == NULL)
        return NULL;

    config_section *section = (config_section *) ConfigSectionHandle;
    if (section->magic != SECTION_MAGIC)
        return NULL;

    /* if this parameter doesn't exist, return an error */
    config_var *var = find_section_var(section, ParamName);
    if (var == NULL)
        return NULL;

    return var->comment;
}

/* ------------------------------------------------------- */
/* Special Get/Set functions, exported outside of the Core */
/* ------------------------------------------------------- */

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

/* ------------------------------------------------------ */
/* OS Abstraction functions, exported outside of the Core */
/* ------------------------------------------------------ */

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

