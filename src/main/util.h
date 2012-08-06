/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - util.h                                                  *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2012 CasualJames                                        *
 *   Copyright (C) 2002 Hacktarux                                          *
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

#ifndef __UTIL_H__
#define __UTIL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>

/**********************
     File utilities
 **********************/

typedef enum _file_status
{
    file_ok,
    file_open_error,
    file_read_error,
    file_write_error
} file_status_t;

/** read_from_file
 *    opens a file and reads the specified number of bytes.
 *    returns zero on success, nonzero on failure
 */
file_status_t read_from_file(const char *filename, void *data, size_t size);

/** write_to_file
 *    opens a file and writes the specified number of bytes.
 *    returns zero on sucess, nonzero on failure
 */ 
file_status_t write_to_file(const char *filename, const void *data, size_t size);

/**********************
  Linked list utilities
 **********************/
typedef struct _list_node {
    void *data;
    struct _list_node *prev;
    struct _list_node *next;
} list_node_t;

typedef list_node_t * list_t;

list_node_t *list_prepend(list_t *list, void *data);
list_node_t *list_append(list_t *list, void *data);
void list_node_delete(list_t *list, list_node_t *node);
void list_delete(list_t *list);
list_node_t *list_find_node(list_t list, void *data);

// cycles through each listnode in list setting curr_node to current node.
#define list_foreach(list, curr_node) \
    for((curr_node) = (list); (curr_node) != NULL; (curr_node) = (curr_node)->next)

/**********************
     GUI utilities
 **********************/
void countrycodestring(char countrycode, char *string);
void imagestring(unsigned char imagetype, char *string);

/**********************
     Path utilities
 **********************/

/* Extracts the directory string (part before the file name) from a path string.
 * Returns a malloc'd string with the directory string.
 * If there's no directory string in the path, returns a malloc'd empty string.
 * (This is done so that path = dirfrompath(path) + namefrompath(path)). */
char* dirfrompath(const char* path);

/* Extracts the full file name (with extension) from a path string.
 * Returns a malloc'd string with the file name. */
char* namefrompath(const char* path);

/* Creates a path string by joining two path strings.
 * The given path strings may or may not start or end with a path separator.
 * Returns a malloc'd string with the resulting path. */
char* combinepath(const char* first, const char *second);

/**********************
    String utilities
 **********************/

/** trim
 *    Removes leading and trailing whitespace from str. Function modifies str
 *    and also returns modified string.
 */
char *trim(char *str);

/* Formats an string, using the same syntax as printf.
 * Returns the result in a malloc'd string. */
char* formatstr(const char* fmt, ...);

typedef enum _ini_line_type
{
    INI_BLANK,
    INI_COMMENT,
    INI_SECTION,
    INI_PROPERTY,
    INI_TRASH
} ini_line_type;

typedef struct _ini_line
{
    ini_line_type type;
    char *name;
    char *value;
} ini_line;

/* Parses the INI file line pointer by 'lineptr'.
 * The first line pointed by 'lineptr' may be modifed.
 * 'lineptr' will point to the next line after this function runs.
 *
 * Returns a ini_line structure with information about the line.
 * For INI_COMMENT, the value field contains the comment.
 * For INI_SECTION, the name field contains the section name.
 * For INI_PROPERTY, the name and value fields contain the property parameters.
 * The line type is INI_BLANK if the line is blank or invalid.
 *
 * The name and value fields (if any) of ini_line point to 'lineptr'
 * (so their lifetime is associated to that of 'lineptr').
 */
ini_line ini_parse_line(char **lineptr);

#ifdef __cplusplus
}
#endif

#endif // __UTIL_H__

