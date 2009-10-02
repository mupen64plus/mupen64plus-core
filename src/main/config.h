/***************************************************************************
 config.h - Handles the configuration files
----------------------------------------------------------------------------
Began                : Fri Nov 8 2002
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

#ifndef __CONFIG_H__
#define __CONFIG_H__

#define CORE_INTERPRETER        (0)
#define CORE_DYNAREC            (1)
#define CORE_PURE_INTERPRETER       (2)

void config_read( void );
void config_write( void );
void config_delete( void ); // Deletes config structures in memory (doesn't delete conf file)

int config_set_section( const char *section );

const char *config_get_string( const char *key, const char *def );
int config_get_number( const char *key, int def );
int config_get_bool( const char *key, int def );

void config_put_string( const char *key, const char *value );
void config_put_number( const char *key, int value );
void config_put_bool( const char *key, int value );

#endif // __CONFIG_H__

