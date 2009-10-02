/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - config.c                                                *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2002 Blight                                             *
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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

#include "config.h"
#include "main.h"
#include "util.h"
#include "translate.h"

typedef struct _SConfigValue
{
    char *key;      // key - string
    char *cValue;   // value - string
    int   iValue;   // value - integer
    int   bValue;   // value - bool
} SConfigValue;

typedef struct _SConfigSection
{
    char   *name;
    list_t  values;
} SConfigSection;

list_t m_config = NULL;         // list of config sections
SConfigSection *m_configSection = 0;    // currently selected section

/** helper funcs **/

static SConfigValue * config_findValue( const char *key )
{
    list_node_t *node;
    SConfigValue *val;

    if (!m_configSection)
        config_set_section("Default");

    list_foreach(m_configSection->values, node)
    {
        val = (SConfigValue *)node->data;

        if (strcasecmp(key, val->key) == 0)
            return val;
    }

    return NULL;
}

static SConfigSection * config_findSection( const char *section )
{
    list_node_t *node;
    SConfigSection *sec;

    list_foreach(m_config, node)
    {
        sec = (SConfigSection *)node->data;

        if (strcasecmp( section, sec->name ) == 0)
            return sec;
    }

    return NULL;
}


/** config API **/

int config_set_section( const char *section )
{
    SConfigSection *sec = config_findSection( section );

    if (!sec)
    {
        sec = malloc(sizeof(SConfigSection));
        if(!sec)
            return -1;

        sec->name = strdup( section );
        sec->values = NULL;
        list_append(&m_config, sec);
    }
    m_configSection = sec;

    return 0;
}

const char * config_get_string( const char *key, const char *def )
{
    SConfigValue *val = config_findValue( key );
    if (!val)
    {
        if (def != NULL)
            config_put_string(key, def);
        return def;
    }

    return val->cValue;
}

int config_get_number( const char *key, int def )
{
    SConfigValue *val = config_findValue( key );
    if (!val)
    {
        config_put_number(key, def);
        return def;
    }

    return val->iValue;
}

int config_get_bool( const char *key, int def )
{
    SConfigValue *val = config_findValue( key );
    if (!val)
    {
        config_put_bool(key, def);
        return def;
    }

    return val->bValue;
}


void config_put_string( const char *key, const char *value )
{
    SConfigValue *val = config_findValue( key );
    if (!val)
    {
        val = malloc(sizeof(SConfigValue));
        if(!val) return;
        memset(val, 0, sizeof(SConfigValue));
        val->key = strdup(key);
        list_append(&(m_configSection->values), val);
    }

    if (val->cValue)
        free( val->cValue );
    val->cValue = strdup( value );
    val->iValue = atoi(val->cValue);
    val->bValue = val->iValue;
    if (strcasecmp(val->cValue, "yes") == 0)
        val->bValue = 1;
    else if (strcasecmp(val->cValue, "true") == 0)
        val->bValue = 1;
}

void config_put_number( const char *key, int value )
{
    char buf[50];
    snprintf( buf, 50, "%d", value );
    config_put_string( key, buf );
}

void config_put_bool( const char *key, int value )
{
    config_put_string( key, (value != 0) ? ("true") : ("false") );
}

void config_read( void )
{
    FILE *f;
    char filename[PATH_MAX];
    char line[2048];
    char *p;
    int linelen;

    config_set_section( "Default" );

    snprintf( filename, PATH_MAX, "%smupen64plus.conf", get_configpath() );
    f = fopen( filename, "r" );
    if( f == NULL )
    {
        printf( "%s: %s\n", filename, strerror( errno ) );
        return;
    }

    while( !feof( f ) )
    {
        if( !fgets( line, 2048, f ) )
            break;

        trim( line );
        linelen = strlen( line );
        if (line[0] == '#')     // comment
            continue;

        if (line[0] == '[' && line[linelen-1] == ']')
        {
            line[linelen-1] = '\0';
            config_set_section( line+1 );
            continue;
        }

        p = strchr( line, '=' );
        if( !p )
            continue;

        *(p++) = '\0';
        trim( line );
        trim( p );
        config_put_string( line, p );
    }

    fclose( f );
}

void config_write( void )
{
    FILE *f;
    char filename[PATH_MAX];
    list_node_t *secNode,
            *valNode;
    SConfigSection *sec;
    SConfigValue *val;

    snprintf( filename, PATH_MAX, "%smupen64plus.conf", get_configpath() );
    f = fopen( filename, "w" );
    if( !f )
        return;

    // for each config section
    list_foreach(m_config, secNode)
    {
        sec = (SConfigSection *)secNode->data;

        fprintf(f, "[%s]\n", sec->name);

        // for each config value
        list_foreach(sec->values, valNode)
        {
            val = (SConfigValue *)valNode->data;
            fprintf(f, "%s = %s\n", val->key, val->cValue);
        }
        fprintf( f, "\n" );
    }

    fclose( f );
}

void config_delete(void)
{
    list_node_t *secNode,
            *valNode;
    SConfigSection *sec;
    SConfigValue *val;

    m_configSection = NULL;

    // for each config section
    list_foreach(m_config, secNode)
    {
        sec = (SConfigSection *)secNode->data;

        // for each config value
        list_foreach(sec->values, valNode)
        {
            val = (SConfigValue *)valNode->data;

            free(val->key);
            free(val->cValue);
            free(val);
        }
        free(sec->name);
        list_delete(&(sec->values));
        free(sec);
    }
    list_delete(&m_config);
}

