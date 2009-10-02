/******************************************************************************

    LZMA decoder library with a zlib like API

        * WARNING WARNING WARNING WARNING WARNING WARNING *
        *                                                 *
        * This library hasn't been maintained since 2005. *
        * This will be replaced by liblzma once it is     *
        * finished. liblzma will provide all the features *
        * of liblzmadec and a lot more.                   *
        *                                                 *
        * WARNING WARNING WARNING WARNING WARNING WARNING *

    Copyright (C) 1999-2005 Igor Pavlov (http://7-zip.org/)
    Copyright (C) 2005 Lasse Collin <lasse.collin@tukaani.org>
    Based on zlib.h and bzlib.h.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

******************************************************************************/


/*************************
  WARNING WARNING WARNING
  Comments about return
  codes etc. are not up
  to date.
 *************************/


#ifndef LZMADEC_H
#define LZMADEC_H

#ifdef __cplusplus
extern "C" {
#endif

/**********
  Includes
 **********/

#include <sys/types.h>
#include <inttypes.h>

/* Define LZMADEC_NO_STDIO to not include stdio.h and lzmadec_FILE functions. */
#ifndef LZMADEC_NO_STDIO
#include <stdio.h>
#endif


/*******************
  Defines/Constants
 *******************/

/* Size in bytes of the smallest possible LZMA encoded file */
#define LZMADEC_MINIMUM_SIZE 18

/* Return values */
#define LZMADEC_OK                0
#define LZMADEC_STREAM_END        1
#define LZMADEC_HEADER_ERROR    (-2)
#define LZMADEC_DATA_ERROR      (-3)
#define LZMADEC_MEM_ERROR       (-4)
#define LZMADEC_BUF_ERROR       (-5)
#define LZMADEC_SEQUENCE_ERROR  (-6)
/*
   LZMADEC_OK
   Operation succeeded or some progress has been made.

   LZMADEC_STREAM_END
   The end of the encoded data has been reached. Note that this is
   a possible return value even when finish_decoding == LZMADEC_RUN.

   LZMADEC_DATA_ERROR
   Something wrong with the input data.

   LZMADEC_MEM_ERROR
   The memory allocation function returned a NULL pointer. The same
   function can be called again with the same arguments to try again.

   LZMADEC_BUF_ERROR
   You should provide more input in next_in and set avail_in accordingly.
   The first call to lzmadec_decode() must provide at least 18 bytes of
   input data. Subsequent calls can any amount of data (or no data at all).
   Note that LZMADEC_BUF_ERROR is not fatal and decoding can continue by
   supplying more input data.
*/


/**********
  typedefs
 **********/

typedef struct {
        uint8_t *next_in;
        size_t avail_in;
        uint_fast64_t total_in;

        uint8_t *next_out;
        size_t avail_out;
        uint_fast64_t total_out;

        void *state; /* Internal state, not visible outside the library */

        void *(*lzma_alloc)(void *, size_t, size_t);
        void (*lzma_free)(void *, void *);
        void *opaque;
} lzmadec_stream;

typedef struct {
        uint_fast64_t uncompressed_size;
        uint_fast32_t dictionary_size;
        uint_fast32_t internal_data_size;
        uint_fast8_t is_streamed;
        uint_fast8_t pb;
        uint_fast8_t lp;
        uint_fast8_t lc;
} lzmadec_info;

#ifndef LZMADEC_NO_STDIO
typedef void lzmadec_FILE;
#endif


/*********************
  Single call decoding
 *********************/

extern int_fast8_t lzmadec_buffer (
        uint8_t *dest, size_t *dest_len,
        uint8_t *source, const size_t source_len);
/*
    Decode the data from source buffer to destination buffer with
    a single pass.

    Return values:
        LZMADEC_OK              Decoding successful
        LZMADEC_HEADER_ERROR    Invalid header
        LZMADEC_MEM_ERROR       Not enough memory
        LZMADEC_DATA_ERROR      Corrupted source data
        LZMADEC_BUF_ERROR       Destination buffer too small

    Equivalent in zlib: uncompress()
*/


/*********************
  Multi call decoding
 *********************/

extern int_fast8_t lzmadec_init (lzmadec_stream *strm);
/*
    Initialize the decoder.

    Return values:
        LZMADEC_OK
        LZMADEC_HEADER_ERROR
        LZMADEC_MEM_ERROR

    Equivalent in zlib: inflateInit()
*/

extern int_fast8_t
lzmadec_decode (lzmadec_stream *strm, const int_fast8_t finish_decoding);
/*
    The finish_decoding flag

    In contrast to zlib and bzlib, liblzmadec can detect the end of the
    compressed stream only with streamed LZMA data. Non-streamed data
    does not contain any end of stream marker and thus needs the
    finish_decoding flag to be set to decode the last bytes of the data.

    When the finish_decoding is zero,
    This is a sign to the decoder that even if avail_in == 0 happened to
    be true, there can still be more input data not passed to the library
    yet. It is safe to call lzmadec_decode with LZMADEC_RUN even if all
    the data has been passed to the library already; in that case there
    there will usually be bytes left in the internal output buffer.

    Set the finish_decoding to non-zero to sign the decoder that all
    the input has been given to it via next_in buffer. Once called with
    non-zero finish_decoding flag, it should not be unset or an error
    will be returned.

    If you can assure that (avail_in > 0) on every lzmadec_decode() call
    before all the data has been passed to the decoder library, the
    simplest way is to use (strm.avail_in == 0) as the finish_decoding
    value.

    Return values:
        LZMADEC_OK
        LZMADEC_STREAM_END
        LZMADEC_DATA_ERROR
        LZMADEC_HEADER_ERROR (only right after initialization)
        LZMADEC_MEM_ERROR (only right after initialization)

    Equivalent in zlib: inflate()
*/

int_fast8_t lzmadec_end (lzmadec_stream *strm);
/*
    Return values:
        LZMADEC_OK
        LZMADEC_STREAM_ERROR FIXME

    Equivalent in zlib: inflateEnd()
*/


/*************
  Information
 *************/

extern int_fast8_t lzmadec_buffer_info (
                lzmadec_info *info, const uint8_t *buffer, const size_t len);
/*
    Parse the header of a LZMA stream. The header size is
    13 bytes; make sure there is at least 13 bytes available
    in the buffer. Information about parsed header will be stored
    to *info.

    Most common uses for this function are checking
      - the uncompressed size of the file (if availabe)
      - how much RAM is needed to decompress the data.

        uncompressed_size   Uncompressed size of the data as bytes

        dictionary_size     Dictionary size as bytes; depends only on
                            settings used when compressing the data.

        internal_data_size  The amount of memory needed by liblzmadec
                            to decode the data excluding the dictionary
                            size. Note that this value depends not only
                            about the used compression settings but
                            also the implementation and/or compile time
                            settings; specifically sizeof(uint_fast16_t).

        is_streamed         Zero if the data is non-streamed LZMA, and
                            non-zero for streamed. This flag is set
                            simply by checking the size field.

        pb                  Number of pos bits; can be from 0 to 4.

        lp                  Number of literal pos bits; from 0 to 4.

        lc                  Number of literal context bits; from 0 to 8.

    To know how much memory is needed to compress a specific stream,
    add up dictionary_size and internal_data_size. Note that if the
    dictionary is extremely huge, the result might not fit in
    uint_fast32_t. ;-)

    WARNING: LZMA streams have no magic first bytes. All data
    that has 0x00 - 0xE1 as the first byte in the buffer will
    return LZMADEC_OK.

    Return values:
        LZMADEC_OK              All OK, the information was stored to *info.
        LZMADEC_BUF_ERROR       len is too small.
        LZMADEC_HEADER_ERROR    Invalid header data.
*/

extern const uint8_t *lzmadec_version (void);
/*
    Return a pointer to a statically allocated string containing the version
    number of the liblzmadec. The version number format is x.yy.z where
    x.yy is the version of LZMA SDK from http://7-zip.org/sdk.html, and x
    is

    Equivalent in zlib: zlibVersion()
*/


/**********
  File I/O
 **********/
#ifndef LZMADEC_NO_STDIO

extern lzmadec_FILE *lzmadec_open (const char *path);
extern lzmadec_FILE *lzmadec_dopen (int fd);
extern ssize_t lzmadec_read (lzmadec_FILE *file, uint8_t *buf, size_t len);
extern uint8_t *lzmadec_gets (lzmadec_FILE *file, uint8_t *buf, size_t len);
extern int lzmadec_getc (lzmadec_FILE *file);
extern int_fast8_t lzmadec_seek (lzmadec_FILE *file, off_t offset, int whence);
extern off_t lzmadec_tell (lzmadec_FILE *file);
extern int_fast8_t lzmadec_rewind (lzmadec_FILE *file);
extern int_fast8_t lzmadec_eof (lzmadec_FILE *file);
extern int_fast8_t lzmadec_close (lzmadec_FILE *file);
/* extern const char *lzmadec_error (lzmadec_FILE *file, int *errnum) */

#endif /* ifndef LZMADEC_NO_STDIO */

#ifdef __cplusplus
}
#endif

#endif /* ifndef LZMADEC_H */

