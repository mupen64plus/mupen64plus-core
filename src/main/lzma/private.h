/******************************************************************************

    Internal defines and typedefs for liblzmadec

    Copyright (C) 1999-2005 Igor Pavlov (http://7-zip.org/)
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


/***********
  Constants
 ***********/

/* uint16_t would be enough for CProb. uint_fast16_t will give a little
   extra speed but wastes memory. On 32-bit architechture the amount
   of wasted memory is usually only a few kilobytes but the theoretical
   maximum is about 1.5 megabytes (4.5 on 64-bit).
   
   Update: Now we always use uint32_t, since uint_fast16_t can be 64-bit
   on 64-bit systems, which is bad for CPU's cache. */
#define CProb uint32_t

#define LZMA_BASE_SIZE 1846
#define LZMA_LIT_SIZE 768
#define LZMA_IN_BUFFER_SIZE 4096

#define LZMA_MINIMUM_COMPRESSED_FILE_SIZE 18

/* Decoder status */
#define LZMADEC_STATUS_UNINITIALIZED 0
#define LZMADEC_STATUS_RUNNING       1
#define LZMADEC_STATUS_FINISHING     2
#define LZMADEC_STATUS_STREAM_END    3
#define LZMADEC_STATUS_ERROR       (-1)


#define LZMA_NUM_TOP_BITS 24
#define LZMA_TOP_VALUE ((uint32_t)1 << LZMA_NUM_TOP_BITS)

#define LZMA_NUM_BIT_MODEL_TOTAL_BITS 11
#define LZMA_BIT_MODEL_TOTAL (1 << LZMA_NUM_BIT_MODEL_TOTAL_BITS)
#define LZMA_NUM_MOVE_BITS 5

#define LZMA_NUM_POS_BITS_MAX 4
#define LZMA_NUM_POS_STATES_MAX (1 << LZMA_NUM_POS_BITS_MAX)

#define LZMA_LEN_NUM_LOW_BITS 3
#define LZMA_LEN_NUM_LOW_SYMBOLS (1 << LZMA_LEN_NUM_LOW_BITS)
#define LZMA_LEN_NUM_MID_BITS 3
#define LZMA_LEN_NUM_MID_SYMBOLS (1 << LZMA_LEN_NUM_MID_BITS)
#define LZMA_LEN_NUM_HIGH_BITS 8
#define LZMA_LEN_NUM_HIGH_SYMBOLS (1 << LZMA_LEN_NUM_HIGH_BITS)

#define LZMA_LEN_CHOICE 0
#define LZMA_LEN_CHOICE2 (LZMA_LEN_CHOICE + 1)
#define LZMA_LEN_LOW (LZMA_LEN_CHOICE2 + 1)
#define LZMA_LEN_MID (LZMA_LEN_LOW + (LZMA_NUM_POS_STATES_MAX << LZMA_LEN_NUM_LOW_BITS))
#define LZMA_LEN_HIGH (LZMA_LEN_MID + (LZMA_NUM_POS_STATES_MAX << LZMA_LEN_NUM_MID_BITS))
#define LZMA_NUM_LEN_PROBS (LZMA_LEN_HIGH + LZMA_LEN_NUM_HIGH_SYMBOLS)

#define LZMA_NUM_STATES 12
#define LZMA_NUM_LIT_STATES 7

#define LZMA_START_POS_MODEL_INDEX 4
#define LZMA_END_POS_MODEL_INDEX 14
#define LZMA_NUM_FULL_DISTANCES (1 << (LZMA_END_POS_MODEL_INDEX >> 1))

#define LZMA_NUM_POS_SLOT_BITS 6
#define LZMA_NUM_LEN_TO_POS_STATES 4

#define LZMA_NUM_ALIGN_BITS 4
#define LZMA_ALIGN_TABLE_SIZE (1 << LZMA_NUM_ALIGN_BITS)

#define LZMA_MATCH_MIN_LEN 2
#define LZMA_IS_MATCH 0
#define LZMA_IS_REP (LZMA_IS_MATCH + (LZMA_NUM_STATES << LZMA_NUM_POS_BITS_MAX))
#define LZMA_IS_REP_G0 (LZMA_IS_REP + LZMA_NUM_STATES)
#define LZMA_IS_REP_G1 (LZMA_IS_REP_G0 + LZMA_NUM_STATES)
#define LZMA_IS_REP_G2 (LZMA_IS_REP_G1 + LZMA_NUM_STATES)
#define LZMA_IS_REP0_LONG (LZMA_IS_REP_G2 + LZMA_NUM_STATES)
#define LZMA_POS_SLOT (LZMA_IS_REP0_LONG + (LZMA_NUM_STATES << LZMA_NUM_POS_BITS_MAX))
#define LZMA_SPEC_POS (LZMA_POS_SLOT + (LZMA_NUM_LEN_TO_POS_STATES << LZMA_NUM_POS_SLOT_BITS))
#define LZMA_ALIGN (LZMA_SPEC_POS + LZMA_NUM_FULL_DISTANCES - LZMA_END_POS_MODEL_INDEX)
#define LZMA_LEN_CODER (LZMA_ALIGN + LZMA_ALIGN_TABLE_SIZE)
#define LZMA_REP_LEN_CODER (LZMA_LEN_CODER + LZMA_NUM_LEN_PROBS)
#define LZMA_LITERAL (LZMA_REP_LEN_CODER + LZMA_NUM_LEN_PROBS)

/* LZMA_REQUIRED_IN_BUFFER_SIZE = number of required input bytes for worst case:
   longest match with longest distance.
   kLzmaInBufferSize must be larger than LZMA_REQUIRED_IN_BUFFER_SIZE
   23 bits = 2 (match select) + 10 (len) + 6 (distance) + 4 (align) + 1 (RC_NORMALIZE)
*/
#define LZMA_REQUIRED_IN_BUFFER_SIZE ((23 * (LZMA_NUM_BIT_MODEL_TOTAL_BITS \
                - LZMA_NUM_MOVE_BITS + 1) + 26 + 9) / 8)


/***************
  Sanity checks
 ***************/

#if LZMA_LITERAL != LZMA_BASE_SIZE
#error BUG: LZMA_LITERAL != LZMA_BASE_SIZE
#endif

#if LZMA_IN_BUFFER_SIZE <= LZMA_REQUIRED_IN_BUFFER_SIZE
#error LZMA_IN_BUFFER_SIZE <= LZMA_REQUIRED_IN_BUFFER_SIZE
#error Fix by increasing LZMA_IN_BUFFER_SIZE.
#endif



/********
  Macros
 ********/

#ifndef MIN
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#endif


/**********
  typedefs
 **********/

typedef struct {
        /* LZMA_REQUIRED_IN_BUFFER_SIZE is added to LZMA_IN_BUFFER_SIZE for
           buffer overflow protection. I'm not 100% if it is really needed
           (I haven't studied the details enough) but allocating a few extra
           bytes shouldn't harm anyone. --Larhzu */
        unsigned char buffer[LZMA_IN_BUFFER_SIZE + LZMA_REQUIRED_IN_BUFFER_SIZE];

        /* Pointer to the current position in buffer[] */
        unsigned char *buffer_position;

        /* In the original version from LZMA SDK buffer_size had
           to be signed. In liblzmadec this should be unsigned. */
        size_t buffer_size;

        /* We don't know the properties of the stream we are going to
           decode in lzmadec_decompressInit. The needed memory
           will be allocated on first call to lzmadec_decode.
           status is used to check if we have parsed the header and
           allocated the memory needed by the LZMA decoder engine. */
        int_fast8_t status;

        uint_fast32_t dictionary_size;
        uint8_t *dictionary;

        uint_fast64_t uncompressed_size;
        uint_fast8_t streamed; /* boolean */

        uint32_t pos_state_mask;
        uint32_t literal_pos_mask;
        uint_fast8_t pb;
        uint_fast8_t lp;
        uint_fast8_t lc;

        CProb *probs;

        uint32_t range;
        uint32_t code;
        uint_fast32_t dictionary_position;
        uint32_t distance_limit;
        uint32_t rep0, rep1, rep2, rep3;
        int state;
        int len;
} lzmadec_state;

