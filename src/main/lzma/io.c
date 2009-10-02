/******************************************************************************

    LZMA decoder library with a zlib like API - lzma_FILE I/O functions

    Copyright (C) 1999-2005 Igor Pavlov (http://7-zip.org/)
    Copyright (C) 2005 Lasse Collin <lasse.collin@tukaani.org>
    Based on zlib.h and bzlib.h. FIXME

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

******************************************************************************/

#ifndef LZMADEC_NO_STDIO

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <errno.h>

/* Needed for pre-C99 systems that have SIZE_MAX in limits.h. */
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#define LZMADEC_NO_STDIO
#include "lzmadec.h"
#undef LZMADEC_NO_STDIO

#include "private.h"

#ifndef SIZE_MAX
#define SIZE_MAX (~(size_t)0)
#endif

#define LZMADEC_BUFSIZE (LZMA_IN_BUFFER_SIZE - LZMA_REQUIRED_IN_BUFFER_SIZE)

#define LZMADEC_IO_STATUS_OK        0
#define LZMADEC_IO_STATUS_EOF        1
#define LZMADEC_IO_STATUS_ERROR        2

typedef struct {
        lzmadec_stream strm;
        FILE *file;
        uint8_t buffer[LZMADEC_BUFSIZE];
        int_fast8_t status;
} lzmadec_FILE;


/****************************
  Opening and closing a file
 ****************************/

/* This is used by lzmadec_open() and lzmadec_dopen(). */
static lzmadec_FILE *
lzmadec_open_init (lzmadec_FILE *lfile)
{
        /* Check if the file was opened successfully */
        if (lfile->file == NULL) {
                int saved_errno = errno;
                free (lfile);
                errno = saved_errno;
                return NULL; /* Caller can read errno */
        }
        /* Initialize the decoder */
        lfile->strm.lzma_alloc = NULL;
        lfile->strm.lzma_free = NULL;
        lfile->strm.opaque = NULL;
        lfile->strm.avail_in = 0;
        lfile->strm.avail_out = 0;
        if (lzmadec_init (&lfile->strm) != LZMADEC_OK) {
                fclose (lfile->file);
                free (lfile);
                /* Set errno like fopen(2) (and malloc(3)) would set it: */
                errno = ENOMEM;
                return NULL; /* Caller can see faked malloc()'s errno */
        }
        /* Not yet at the end of the stream. */
        lfile->status = LZMADEC_IO_STATUS_OK;
        return lfile;
}

extern lzmadec_FILE *
lzmadec_open (const char *path)
{
        /* Allocate memory for the lzmadec_FILE */
        lzmadec_FILE *lfile = malloc (sizeof (lzmadec_FILE));
        if (lfile == NULL)
                return NULL;
        /* Open the file */
        lfile->file = fopen (path, "rb");
        /* The rest is shared with lzmadec_open() */
        return lzmadec_open_init (lfile);
}

extern lzmadec_FILE *
lzmadec_dopen (int fd)
{
        /* Allocate memory for the lzmadec_FILE */
        lzmadec_FILE *lfile = malloc (sizeof (lzmadec_FILE));
        if (lfile == NULL)
                return NULL;
        /* Open the file */
        lfile->file = fdopen (fd, "rb");
        /* The rest is shared with lzmadec_open() */
        return lzmadec_open_init (lfile);
}

extern int_fast8_t
lzmadec_close (lzmadec_FILE *lfile)
{
        /* Simple check that lfile looks like a valid lzmadec_FILE. */
        if (lfile == NULL || lfile->strm.state == NULL)
                return -1;
        lzmadec_end (&lfile->strm);
        fclose (lfile->file);
        lfile->file = NULL;
        free (lfile);
        return 0;
}


/****************
  Reading a file
 ****************/

extern ssize_t
lzmadec_read (lzmadec_FILE *lfile, uint8_t *buf, const size_t len)
{
        int_fast8_t ret;
        /* Simple check that lfile looks like a valid lzmadec_FILE. */
        if (lfile == NULL || lfile->strm.state == NULL)
                return -1;
        /* Check status */
        if (lfile->status == LZMADEC_IO_STATUS_ERROR)
                return -1;
        if (lfile->status == LZMADEC_IO_STATUS_EOF)
                return 0;
        /* The return value is ssize_t so we limit the maximum read size. */
        lfile->strm.avail_out = MIN (len, SIZE_MAX / 2 - 1);
        lfile->strm.next_out = buf;
        do {
                if (lfile->strm.avail_in == 0) {
                        lfile->strm.next_in = lfile->buffer;
                        lfile->strm.avail_in = fread (lfile->buffer,
                                        sizeof (uint8_t), LZMADEC_BUFSIZE,
                                        lfile->file);
                }
                ret = lzmadec_decode (&lfile->strm, lfile->strm.avail_in == 0);
        } while (lfile->strm.avail_out != 0 && ret == LZMADEC_OK);
        if (ret == LZMADEC_STREAM_END)
                lfile->status = LZMADEC_IO_STATUS_EOF;
        if (ret < 0)
                return -1; /* FIXME: errno? */
        return (len - lfile->strm.avail_out);
}

/* Read until '\n' or '\0' or at maximum of len bytes.
   Slow implementation, similar to what is in zlib. */
extern uint8_t *
lzmadec_gets (lzmadec_FILE *lfile, uint8_t *buf, size_t len)
{
        int_fast8_t ret;
        uint8_t *buf_start = buf;
        /* Sanity checks */
        if (buf == NULL || len < 1)
                return NULL;
        if (lfile == NULL || lfile->strm.state == NULL)
                return NULL;
        /* Read byte by byte (sloooow) and stop when 1) buf is full
           2) end of file 3) '\n' or '\0' is found. */
        while (--len > 0) {
                ret = lzmadec_read (lfile, buf, 1);
                if (ret != 1) {
                        /* Error checking: 1) decoding error or 2) end of file
                           and no characters were read. */
                        if (ret < 0 || buf == buf_start)
                                return NULL;
                        break;
                }
                if (*buf == '\0')
                        return buf_start;
                if (*buf++ == '\n')
                        break;
        }
        *buf = '\0';
        return buf_start;
}

extern int
lzmadec_getc (lzmadec_FILE *lfile)
{
        uint8_t c;
        if (lzmadec_read (lfile, &c, 1) == 0)
                return -1;
        return (int)(c);
}


/*******
  Other
 *******/

extern off_t
lzmadec_tell (lzmadec_FILE *lfile)
{
        /* Simple check that lfile looks like a valid lzmadec_FILE. */
        if (lfile == NULL || lfile->strm.state == NULL)
                return -1;
        return (off_t)(lfile->strm.total_out);
}

extern int_fast8_t
lzmadec_eof (lzmadec_FILE *lfile)
{
        /* Simple check that lfile looks like a valid lzmadec_FILE. */
        if (lfile == NULL || lfile->strm.state == NULL)
                return -1;
        return lfile->status == LZMADEC_IO_STATUS_EOF;
}

extern int_fast8_t
lzmadec_rewind (lzmadec_FILE *lfile)
{
        /* Simple check that lfile looks like a valid lzmadec_FILE. */
        if (lfile == NULL || lfile->strm.state == NULL)
                return -1;
        /* Rewinding is done by closing the old lzmadec_stream
           and reinitializing it. */
        if (lzmadec_end (&lfile->strm) != LZMADEC_OK) {
                lfile->status = LZMADEC_IO_STATUS_ERROR;
                return -1;
        }
        rewind (lfile->file);
        if (lzmadec_init (&lfile->strm) != LZMADEC_OK) {
                lfile->status = LZMADEC_IO_STATUS_ERROR;
                return -1;
        }
        lfile->status = LZMADEC_IO_STATUS_OK;
        return 0;
}

extern off_t
lzmadec_seek (lzmadec_FILE *lfile, off_t offset, int whence)
{
        off_t oldpos = (off_t)(lfile->strm.total_out);
        off_t newpos;
        /* Simple check that lfile looks like a valid lzmadec_FILE. */
        if (lfile == NULL || lfile->strm.state == NULL)
                return -1;
        /* Get the new absolute position. */
        switch (whence) {
                case SEEK_SET:
                        /* Absolute position must be >= 0. */
                        if (offset < 0)
                                return -1;
                        newpos = offset;
                        break;
                case SEEK_CUR:
                        /* Need to be careful to avoid integer overflows. */
                        if ((offset < 0 && (off_t)(-1 * offset) > oldpos)
                                        ||
                                        (offset > 0 && (off_t)(offset) + oldpos
                                        < oldpos))
                                return (off_t)(-1);
                        newpos = (off_t)(lfile->strm.total_out) + offset;
                        break;
                case SEEK_END:
                        /* zlib doesn't support SEEK_END. However, liblzmadec
                           provides this as a way to find out uncompressed
                           size of a streamed file (streamed files don't have
                           uncompressed size in their header). */
                        newpos = -1;
                        break;
                default:
                        /* Invalid whence */
                        errno = EINVAL;
                        return -1;
        }
        /* Seeking with a valid whence value always clears
           the end of file indicator. */
        lfile->status = LZMADEC_IO_STATUS_OK;
        /* If the new absolute position is backward from current position,
           we need to rewind and uncompress from the beginning of the file.
           This is usually slow and thus not recommended. */
        if (whence != SEEK_END && newpos < oldpos) {
                if (lzmadec_rewind (lfile))
                        return -1;
                oldpos = 0;
                assert (lfile->strm.total_out == 0);
        }
        /* Maybe we are lucky and don't need to seek at all. ;-) */
        if (newpos == oldpos)
                return oldpos;
        assert (newpos > oldpos || newpos == -1);
        /* Read as many bytes as needed to reach the requested position. */
        {
                /* strm.next_out cannot be NULL so use a temporary buffer. */
                uint8_t buf[LZMADEC_BUFSIZE];
                size_t req_size;
                ssize_t got_size;
                while (newpos > oldpos || newpos == -1) {
                        req_size = MIN (LZMADEC_BUFSIZE, newpos - oldpos);
                        got_size = lzmadec_read (lfile, buf, req_size);
                        if (got_size != (ssize_t)(req_size)) {
                                if (got_size < 0) {
                                        return -1; /* Stream error */
                                } else {
                                        /* End of stream */
                                        newpos = oldpos + got_size;
                                        break;
                                }
                        }
                        oldpos += got_size;
                };
        }
        assert (newpos == oldpos);
        assert ((off_t)(lfile->strm.total_out) == newpos);
        return newpos;
}

#endif /* ifndef LZMADEC_NO_STDIO */

