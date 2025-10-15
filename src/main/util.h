/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - util.h                                                  *
 *   Mupen64Plus homepage: https://mupen64plus.org/                        *
 *   Copyright (C) 2020 Richard42                                          *
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

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "osal/preproc.h"

#if defined(__GNUC__)
#define ATTR_FMT(fmtpos, attrpos) __attribute__ ((format (printf, fmtpos, attrpos)))
#else
#define ATTR_FMT(fmtpos, attrpos)
#endif

/**********************
     File utilities
 **********************/

typedef enum _file_status
{
    file_ok,
    file_open_error,
    file_read_error,
    file_write_error,
    file_size_error
} file_status_t;

/** read_from_file
 *    opens a file and reads the specified number of bytes.
 *    returns zero on success, nonzero on failure
 */
file_status_t read_from_file(const char *filename, void *data, size_t size);

/** write_to_file
 *    opens a file and writes the specified number of bytes.
 *    returns zero on success, nonzero on failure
 */
file_status_t write_to_file(const char *filename, const void *data, size_t size);

/** write_chunk_to_file
 *    opens a file, seek to offset and writes the specified number of bytes.
 *    returns zero on success, nonzero on failure
 */
file_status_t write_chunk_to_file(const char *filename, const void *data, size_t size, size_t offset);

/** load_file
 *    load the file content into a newly allocated buffer.
 *    returns zero on success, nonzero on failure
 */
file_status_t load_file(const char* filename, void** buffer, size_t* size);

/** get_file_size
 *     get file size.
 *     returns zero on success, nonzero on failure
 */
file_status_t get_file_size(const char* filename, size_t* size);

/**********************
   Byte swap utilities
 **********************/
#ifdef _MSC_VER
#include <stdlib.h>
#endif

/* GCC has also byte swap intrinsics (__builtin_bswap32, etc.), but they were
 * added in relatively recent versions. In addition, GCC can detect the byte
 * swap code and optimize it with a high enough optimization level. */

static osal_inline unsigned short m64p_swap16(unsigned short x)
{
    #ifdef _MSC_VER
    return _byteswap_ushort(x);
    #else
    return ((x & 0x00FF) << 8) |
           ((x & 0xFF00) >> 8);
    #endif
}

static osal_inline unsigned int m64p_swap32(unsigned int x)
{
    #ifdef _MSC_VER
    return _byteswap_ulong(x); // long is always 32-bit in Windows
    #else
    return ((x & 0x000000FF) << 24) |
           ((x & 0x0000FF00) << 8) |
           ((x & 0x00FF0000) >> 8) |
           ((x & 0xFF000000) >> 24);
    #endif
}

static osal_inline unsigned long long int m64p_swap64(unsigned long long int x)
{
    #ifdef _MSC_VER
    return _byteswap_uint64(x);
    #else
    return ((x & 0x00000000000000FFULL) << 56) |
           ((x & 0x000000000000FF00ULL) << 40) |
           ((x & 0x0000000000FF0000ULL) << 24) |
           ((x & 0x00000000FF000000ULL) << 8) |
           ((x & 0x000000FF00000000ULL) >> 8) |
           ((x & 0x0000FF0000000000ULL) >> 24) |
           ((x & 0x00FF000000000000ULL) >> 40) |
           ((x & 0xFF00000000000000ULL) >> 56);
    #endif
}

#ifdef M64P_BIG_ENDIAN
#define big16(x) (x)
#define big32(x) (x)
#define big64(x) (x)
#define little16(x) m64p_swap16(x)
#define little32(x) m64p_swap32(x)
#define little64(x) m64p_swap64(x)
#else
#define big16(x) m64p_swap16(x)
#define big32(x) m64p_swap32(x)
#define big64(x) m64p_swap64(x)
#define little16(x) (x)
#define little32(x) (x)
#define little64(x) (x)
#endif

/* Byte swaps, converts to little endian or converts to big endian a buffer,
 * containing 'count' elements, each of size 'length'. */
void swap_buffer(void *buffer, size_t length, size_t count);
void to_little_endian_buffer(void *buffer, size_t length, size_t count);
void to_big_endian_buffer(void *buffer, size_t length, size_t count);


/* Simple serialization primitives,
 * Loosely modeled after N2827 <stdbit.h> proposal.
 */
uint8_t load_beu8(const unsigned char *ptr);
uint16_t load_beu16(const unsigned char *ptr);
uint32_t load_beu32(const unsigned char *ptr);
uint64_t load_beu64(const unsigned char *ptr);

uint8_t load_leu8(const unsigned char *ptr);
uint16_t load_leu16(const unsigned char *ptr);
uint32_t load_leu32(const unsigned char *ptr);
uint64_t load_leu64(const unsigned char *ptr);

void store_beu8(uint8_t value, unsigned char *ptr);
void store_beu16(uint16_t value, unsigned char *ptr);
void store_beu32(uint32_t value, unsigned char *ptr);
void store_beu64(uint64_t value, unsigned char *ptr);

void store_leu8(uint8_t value, unsigned char *ptr);
void store_leu16(uint16_t value, unsigned char *ptr);
void store_leu32(uint32_t value, unsigned char *ptr);
void store_leu64(uint64_t value, unsigned char *ptr);


/**********************
    Random utilities
 **********************/

struct xoshiro256pp_state { uint64_t s[4]; };

struct xoshiro256pp_state xoshiro256pp_seed(uint64_t seed);

uint64_t xoshiro256pp_next(struct xoshiro256pp_state* s);

#ifndef min
#define min(a, b) ((a < b) ? a : b)
#endif
#ifndef max
#define max(a, b) ((a < b) ? b : a)
#endif

/**********************
     GUI utilities
 **********************/
void countrycodestring(uint16_t countrycode, char *string);
void imagestring(unsigned char imagetype, char *string);

/**********************
     Path utilities
 **********************/

/* Extracts the full file name (with extension) from a path string.
 * Returns the same string, advanced until the file name. */
const char* namefrompath(const char* path);

/* Creates a path string by joining two path strings.
 * The given path strings may or may not start or end with a path separator.
 * Returns a malloc'd string with the resulting path. */
char* combinepath(const char* first, const char *second);

/**********************
    String utilities
 **********************/

/* strpbrk_reverse
 * Looks for an instance of ANY of the characters in 'needles' in 'haystack',
 * starting from the end of 'haystack'. Returns a pointer to the last position
 * of some character on 'needles' on 'haystack'. If not found, returns NULL.
 */
char* strpbrk_reverse(const char* needles, char* haystack, size_t haystack_len);

/** trim
 *    Removes leading and trailing whitespace from str. Function modifies str
 *    and also returns modified string.
 */
char *trim(char *str);

 /* Replaces all occurences of any char in chars with r in string.
  * returns amount of replaced chars
  */
int string_replace_chars(char *str, const char *chars, const char r);

/* Converts an string to an integer.
 * Returns 1 on success, 0 on failure. 'result' is undefined on failure.
 *
 * The following conditions cause this function to fail:
 * - Empty string
 * - Leading characters (including whitespace)
 * - Trailing characters (including whitespace)
 * - Overflow or underflow.
 */
int string_to_int(const char *str, int *result);

/* Converts an string of hexadecimal characters to a byte array.
 * 'output_size' is the number of bytes (hex digraphs) to convert.
 * Returns 1 on success, 0 on failure. 'output' is undefined on failure. */
int parse_hex(const char *str, unsigned char *output, size_t output_size);

/* Formats an string, using the same syntax as printf.
 * Returns the result in a malloc'd string. */
char* formatstr(const char* fmt, ...) ATTR_FMT(1, 2);

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

/* Convert text in Shift-JIS (code page 932) to UTF-8
 */
void ShiftJis2UTF8(const unsigned char *pccInput, unsigned char *pucOutput, int outputLength);

#endif // __UTIL_H__

