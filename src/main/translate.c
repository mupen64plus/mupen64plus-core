/***************************************************************************
 translate.c - Handles multilanguage support
----------------------------------------------------------------------------
Began                : Sun Nov 17 2002
Copyright            : (C) 2002 by blight
Email                : blight@Ashitaka
****************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "api/config.h"
#include "translate.h"
#include "main.h"
#include "util.h"

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <iconv.h>
#include <errno.h>

// types
typedef struct
{
    char *orig; // original, english string
    char *trans;    // translated string to find entry
} translation_t;

typedef struct
{
    char *name;
    list_t translations;
} language_t;

// globals
static list_t g_LanguageList = NULL;
static language_t *g_Language = NULL;

// static functions
static language_t *
tr_load_language( const char *filename )
{
    FILE *f;
    language_t *lang;
    translation_t *trans;
    char line[2048];
    char iso[200];
    char *iso_start;
    size_t inbytes;
    char utf8[1024];
    char *utf8_start;
    size_t outbytes;
    char *p, *p2;
    iconv_t cd;
    int i;

    f = fopen( filename, "r" );
    if( !f )
        return NULL;

    if((cd = iconv_open("UTF-8", "ISO-8859-1")) == (iconv_t)-1)
    {
        fclose(f);
        perror(NULL);
        return NULL;
    }

    lang = (language_t *)malloc( sizeof( language_t ) );
    memset( lang, 0, sizeof( language_t ) );

    while( !feof( f ) )
    {
        if( !fgets( line, 2048, f ) )
            continue;

        trim( line );

        if( line[0] == ';' || line[0] == '\0' )
            continue;

        if( line[0] == '[' && line[strlen(line)-1] == ']' )
        {
            // convert language name to UTF-8
            strcpy(iso, line + 1);
            iso[strlen(iso)-1] = '\0';

            iso_start = iso;
            inbytes = strlen(iso);
            utf8_start = utf8;
            outbytes = 1024;

            memset(utf8, 0, sizeof(utf8));
            iconv(cd, &iso_start, &inbytes, &utf8_start, &outbytes);

            // store language name
            lang->name = malloc(strlen(utf8) + 1);
            strcpy(lang->name, utf8);
            continue;
        }

        p = strchr( line, '=' );
        if( p )
        {
            // replace "\\n" by '\n'
            while( (p2 = strstr( line, "\\n" )) )
            {
                *p2 = '\n'; // replace '\\' by '\n'
                p2++;
                for ( i = 0; i < strlen(p2); ++i )
                    p2[i] = p2[i+1];
            }

            p = strchr( line, '=' );    // line may have changed
            *p = '\0'; p++;
            trim( line );
            trim( p );

            if( strlen( line ) == 0 || strlen( p ) == 0 )
                continue;

            // convert translated string to UTF-8
            iso_start = p;
            inbytes = strlen(p);
            utf8_start = utf8;
            outbytes = 1024;

            memset(utf8, 0, sizeof(utf8));
            iconv(cd, &iso_start, &inbytes, &utf8_start, &outbytes);

            // save translation data
            trans = (translation_t *)malloc(sizeof(translation_t));
            trans->orig = strdup( line );
            trans->trans = malloc(strlen(utf8) + 1);
            strcpy(trans->trans, utf8);
           
            list_append(&(lang->translations), trans);
            continue;
        }
    }

    iconv_close(cd);
    fclose(f);
    return lang;
}

void
tr_delete_languages(void)
{
    language_t *lang;
    translation_t *trans;
    list_node_t *lang_node,
            *trans_node;

    // free current list
    list_foreach(g_LanguageList, lang_node)
    {
        lang = (language_t *)lang_node->data;
        list_foreach(lang->translations, trans_node)
        {
            trans = (translation_t *)trans_node->data;

            free(trans->orig);
            free(trans->trans);
            free(trans);
        }
        list_delete(&(lang->translations));
        free(lang->name);
        free(lang);
    }
    list_delete(&g_LanguageList);
}

// functions
static void
tr_load_languages( void )
{
    language_t *lang;
    char langdir[PATH_MAX], filename[PATH_MAX];
    const char *p;
    DIR *dir;
    struct dirent *de;

    // delete any existing list
    tr_delete_languages();

    // list languages
    //snprintf(langdir, PATH_MAX, "%slang/", get_installpath());
    /* fixme the configuration API needs to be extended to support searching through the shared data folder */
    return;
    langdir[PATH_MAX-1] = 0;
    dir = opendir(langdir);
    if(!dir)
        return;

    while((de = readdir(dir)))
    {
        p = strrchr(de->d_name, '.');
        if(!p)
            continue;
        if(strcmp(p, ".lng"))
            continue;

        snprintf(filename, PATH_MAX, "%s%s", langdir, de->d_name);
        lang = tr_load_language(filename);
        if(lang)
        {
            list_append(&g_LanguageList, lang);
        }
    }

    closedir(dir);
}

void tr_init(void)
{
    list_t langList;
    list_node_t *node;
    char *language;
    const char *confLang;

    // read in language file data
    tr_load_languages();

    // set language based on config file
    langList = tr_language_list();
    confLang = ConfigGetParamString(g_CoreConfig, "Language");
    list_foreach(langList, node)
    {
        language = (char *)node->data;
        if(!strcasecmp(language, confLang))
            tr_set_language(language);
    }
    // free language name list
    list_delete(&langList);
}

list_t
tr_language_list( void )
{
    language_t *lang;
    list_node_t *node;
    list_t list = NULL;

    list_foreach(g_LanguageList, node)
    {
        lang = (language_t *)node->data;
        list_append(&list, lang->name);
    }

    return list;
}

int
tr_set_language(const char *name)
{
    language_t *lang;
    list_node_t *node;

    list_foreach(g_LanguageList, node)
    {
        lang = (language_t *)node->data;
        if(!strcasecmp(name, lang->name))
        {
            g_Language = lang;
            return 0;
        }
    }

    g_Language = NULL;
    return -1;
}

const char *
tr( const char *text )
{
    list_node_t *node;
    translation_t *trans;
    const char *ret = text;

    if( g_Language )
    {
        list_foreach(g_Language->translations, node)
        {
            trans = (translation_t *)node->data;
            if(!strcasecmp(text, trans->orig))
            {
                ret = trans->trans;
                break;
            }
        }
    }

    return ret;
}

