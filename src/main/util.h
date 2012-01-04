/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - util.h                                                  *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
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

/** file utilities **/
int file_exists(const char *filename);
int copyfile(const char *src, const char *dest);
int read_from_file(const char *filename, void *data, size_t size);
int write_to_file(const char *filename, const void *data, size_t size);

/** linked list utilities **/
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
void list_node_move_front(list_t *list, list_node_t *node);
void list_node_move_back(list_t *list, list_node_t *node);
void *list_nth_node_data(list_t list, int n);
list_node_t *list_first_node(list_t list);
void *list_first_data(list_t list);
list_node_t *list_last_node(list_t list);
void *list_last_data(list_t list);
int list_empty(list_t list);
int list_length(list_t list);
list_node_t *list_find_node(list_t list, void *data);
char* dirfrompath(const char* string);

/* GUI utilities */
void countrycodestring(char countrycode, char *string);
void compressionstring(unsigned char compressiontype, char *string);
void imagestring(unsigned char imagetype, char *string);
void cicstring(unsigned char cic, char *string);
void rumblestring(unsigned char rumble, char *string);
void savestring(unsigned char savetype, char *string);
void playersstring(unsigned char players, char *string);

// cycles through each listnode in list setting curr_node to current node.
#define list_foreach(list, curr_node) \
    for((curr_node) = (list); (curr_node) != NULL; (curr_node) = (curr_node)->next)

/** string utilities **/
char *trim(char *str);
char *strnstrip(char* string, int size);
list_t tokenize_string(const char *string, const char* delim);

/** endianness macros **/
#define SWAP16(x) \
( \
((x & 0xFF00) >> 8) | \
((x & 0x00FF) << 8) \
)

#define SWAP32(x) \
( \
((x & 0x000000FF) << 24) | \
((x & 0x0000FF00) <<  8) | \
((x & 0x00FF0000) >>  8) | \
((x & 0xFF000000) >> 24) \
)

#define SWAP64(x) \
( \
((x & 0xFF00000000000000ULL) >> 56) | \
((x & 0x00FF000000000000ULL) >> 40) | \
((x & 0x0000FF0000000000ULL) >> 24) | \
((x & 0x000000FF00000000ULL) >> 8) | \
((x & 0x00000000FF000000ULL) << 8) | \
((x & 0x0000000000FF0000ULL) << 24) | \
((x & 0x000000000000FF00ULL) << 40) | \
((x & 0x00000000000000FFULL) << 56) \
)

#ifdef M64P_BIG_ENDIAN
#define BE16(x) (x)
#define BE32(x) (x)
#define BE64(x) (x)
#define LE16(x) SWAP16(x)
#define LE32(x) SWAP32(x)
#define LE64(x) SWAP64(x)
#else
#define BE16(x) SWAP16(x)
#define BE32(x) SWAP32(x)
#define BE64(x) SWAP64(x)
#define LE16(x) (x)
#define LE32(x) (x)
#define LE64(x) (x)
#endif

#ifdef __cplusplus
}
#endif

#endif // __UTIL_H__

