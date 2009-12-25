/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus-core - osal/files_unix.c                                  *
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
                       
/* This file contains the definitions for the unix-specific file handling
 * functions
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <direct.h>
#include <shlobj.h>

#include "files.h"
#include "api/m64p_types.h"
#include "api/callbacks.h"

/* definitions for system directories to search when looking for shared data files */
#if defined(SHAREDIR)
  #define XSTR(S) STR(S) /* this wacky preprocessor thing is necessary to generate a quote-enclosed */
  #define STR(S) #S      /* copy of the SHAREDIR macro, which is defined by the makefile via gcc -DSHAREDIR="..." */
  static const int   datasearchdirs = 2;
  static const char *datasearchpath[2] = { XSTR(SHAREDIR), ".\\" };
  #undef STR
  #undef XSTR
#else
  static const int   datasearchdirs = 1;
  static const char *datasearchpath[1] = { ".\\" };
#endif

/* local functions */
static int search_dir_file(char *destpath, const char *path, const char *filename)
{
    struct _stat fileinfo;

    /* sanity check to start */
    if (destpath == NULL || path == NULL || filename == NULL)
        return 1;

    /* build the full filepath */
    strcpy(destpath, path);
    if (destpath[strlen(destpath)-1] != '\\')
        strcat(destpath, "\\");
    strcat(destpath, filename);

    /* test for a valid file */
    if (_stat(destpath, &fileinfo) != 0)
        return 2;
    if ((fileinfo.st_mode & _S_IFREG) == 0)
        return 3;

    /* success - file exists and is a regular file */
    return 0;
}

/* global functions */

int osal_mkdirp(const char *dirpath, int mode)
{
    struct _stat fileinfo;
    size_t dirpathlen = strlen(dirpath);
    char *currpath = _strdup(dirpath);

    /* first, break the path into pieces by replacing all of the slashes with NULL chars */
    while (strlen(currpath) > 1)
    {
        char *lastslash = strrchr(currpath, '\\');
        if (lastslash == NULL)
            break;
        *lastslash = 0;
    }
    
    /* then re-assemble the path from left to right until we get to a directory that doesn't exist */
    while (strlen(currpath) < dirpathlen)
    {
        if (strlen(currpath) > 0 && _stat(currpath, &fileinfo) != 0)
            break;
        currpath[strlen(currpath)] = '\\';
    }

    /* then walk up the path chain, creating directories along the way */
    do
    {
        if (_stat(currpath, &fileinfo) != 0)
        {
            if (_mkdir(currpath) != 0)
            {
                free(currpath);
                return 1;        /* mkdir failed */
            }
        }
        if (strlen(currpath) == dirpathlen)
            break;
        else
            currpath[strlen(currpath)] = '\\';
    } while (1);
    
    free(currpath);        
    return 0;
}

const char * osal_get_shared_filepath(const char *filename, const char *firstsearch, const char *secondsearch)
{
    static char retpath[_MAX_PATH];
    int i;

    /* if caller gave us any directories to search, then look there first */
    if (firstsearch != NULL && search_dir_file(retpath, firstsearch, filename) == 0)
        return retpath;
    if (secondsearch != NULL && search_dir_file(retpath, secondsearch, filename) == 0)
        return retpath;

    /* otherwise check our standard paths */
    for (i = 0; i < datasearchdirs; i++)
    {
        if (search_dir_file(retpath, datasearchpath[i], filename) == 0)
            return retpath;
    }

    /* we couldn't find the file */
    return NULL;
}

const char * osal_get_user_configpath(void)
{
    static char chHomePath[MAX_PATH];
    LPITEMIDLIST pidl;
    LPMALLOC pMalloc;
    struct _stat fileinfo;

    // Get item ID list for the path of user's personal directory
    HRESULT hr = SHGetSpecialFolderLocation(NULL, CSIDL_APPDATA, &pidl);
    // get the path in a char string
    SHGetPathFromIDList(pidl, chHomePath);
    // do a bunch of crap just to free some memory
    hr = SHGetMalloc(&pMalloc);
    pMalloc->lpVtbl->Free(pMalloc, pidl);
    pMalloc->lpVtbl->Release(pMalloc);

    // tack on 'mupen64plus'
    if (chHomePath[strlen(chHomePath)-1] != '\\')
        strcat(chHomePath, "\\");
    strcat(chHomePath, "mupen64plus\\");

    // if this directory doesn't exist, then make it
    if (_stat(chHomePath, &fileinfo) != 0)
    {
        osal_mkdirp(chHomePath, 0);
        if (_stat(chHomePath, &fileinfo) == 0)
            return chHomePath;
    }

    /* otherwise we are in trouble */
    DebugMessage(M64MSG_ERROR, "Failed to get configuration directory; $HOME is undefined or invalid.");
    return NULL;
}

const char * osal_get_user_datapath(void)
{
    // in windows, these are all the same
    return osal_get_user_configpath();
}

const char * osal_get_user_cachepath(void)
{
    // in windows, these are all the same
    return osal_get_user_configpath();
}


