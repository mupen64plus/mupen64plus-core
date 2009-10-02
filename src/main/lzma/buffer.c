/******************************************************************************

    Decode the whole source buffer at once

    Copyright (C) 2005 Lasse Collin <lasse.collin@tukaani.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

******************************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include "lzmadec.h"
#include "private.h"

extern int_fast8_t
lzmadec_buffer (uint8_t *dest, size_t *dest_len,
        uint8_t *source, const size_t source_len)
{
        lzmadec_stream strm;
        int8_t ret;

        /* Initialize the decoder */
        strm.next_in = source;
        strm.avail_in = source_len;
        strm.next_out = dest;
        strm.avail_out = *dest_len;
        strm.lzma_alloc = NULL;
        strm.lzma_free = NULL;
        strm.opaque = NULL;
        ret = lzmadec_init (&strm);
        if (ret != LZMADEC_OK)
                return ret;

        /* Check that the destination buffer is big enough. With streamed
           LZMA data we can only hope it is big enough before starting
           the decoding process; if it is too small, we will return
           LZMADEC_BUF_ERROR after decoding dest_len bytes. */
        if (strm.avail_out
                        < ((lzmadec_state*)(strm.state))->uncompressed_size)
                return LZMADEC_BUF_ERROR; /* Too small destination buffer */

        /* Call the decoder. One pass is enough if everything is OK. */
        ret = lzmadec_decode (&strm, 1);

        /* Set *dest_len to amount of bytes actually decoded. */
        assert (*dest_len >= strm.avail_out);
        *dest_len -= strm.avail_out;

        /* Free the allocated memory no matter did the decoding
           go well or not. */
        lzmadec_end (&strm);

        /* Check the return value of lzmadec_decode() and return appropriate
           return value */
        switch (ret) {
                case LZMADEC_STREAM_END:
                        /* Everything has been decoded and put to
                           the destination buffer. */
                        return LZMADEC_OK;
                case LZMADEC_OK:
                        /* Decoding went fine so far but not all of the
                           uncompressed data did fit to the destination
                           buffer. This should happen only with streamed LZMA
                           data (otherwise liblzmadec might have a bug). */
                        assert (((lzmadec_state*)(strm.state))->streamed == 1);
                        return LZMADEC_BUF_ERROR;
                default:
                        assert (ret == LZMADEC_DATA_ERROR);
                        return LZMADEC_DATA_ERROR;
        }
}

