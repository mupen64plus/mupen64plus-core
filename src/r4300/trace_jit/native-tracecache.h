/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - native-tracecache.h                                     *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2015 Nebuleon <nebuleon.fumika@gmail.com>               *
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

#ifndef M64P_TRACE_JIT_NATIVE_TRACECACHE_H
#define M64P_TRACE_JIT_NATIVE_TRACECACHE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "native-config.h"

#if defined(ARCH_EMIT_TRACE)

#define N64_PAGE_SIZE    0x1000
#define N64_PAGE_SHIFT       12
#define N64_PAGE_COUNT 0x100000
#define N64_INSN_SIZE         4
#define N64_PAGE_INSNS (N64_PAGE_SIZE / N64_INSN_SIZE)

typedef void (*TraceFunc) (void);
typedef void* TraceEntry;

/* TracePage: An array of N64_PAGE_INSNS pointers to TraceEntry. */
typedef TraceEntry* TracePage;

/* Pointers to traces are stored here.
 *
 * This is an array of pointers, with one pointer for each N64 memory page.
 * Initially, those pointers are all null; however, when the first trace is
 * about to be compiled in a page, that page's pointer is assigned a memory
 * allocation large enough for N64_PAGE_INSNS TraceEntry, which are filled
 * with pointers to NOT_CODE.
 *
 * Therefore, for an N64 address A, the trace that starts there is at:
 *   Traces [A >> N64_PAGE_SHIFT] [(A & (N64_PAGE_SIZE - 1)) / N64_INSN_SIZE]
 * if
 *   Traces [A >> N64_PAGE_SHIFT] != NULL
 * otherwise, the trace is implicitly NOT_CODE.
 */
extern TracePage Traces[N64_PAGE_COUNT];

struct TraceHeader {
	/* Address in N64 memory where the piece of code following this
	 * TraceHeader, in memory, is used, or 0 if it's not really used.
	 *
	 * For example, if there is a piece of code at 0x0064_883C that is being
	 * used as the trace for MIPS III code at N64 address 0x8020_F944, then
	 * the TraceHeader before the piece of code at 0x0064_883C will have
	 * 0x8020_F944 in its UserPC field. This assists in partially clearing the
	 * code cache.
	 */
	uint32_t UserPC;
	/* Pointer to the TraceHeader preceding, in memory, the first trace
	 * compiled after this one, in time.
	 *
	 * A backward jump in the address of this pointer relative to the address
	 * of the TraceHeader indicates a return to the beginning of the code
	 * cache after some of it was cleared.
	 */
	struct TraceHeader* Next;
};

/* Main memory arena containing native code written for traces of MIPS III code.
 * It is allocated as part of platform initialisation, and its size has also
 * been written to CodeCacheSize.
 *
 * The code cache must be readable, writable and executable, and its initial
 * alignment must be enough to store the first member of 'struct TraceHeader',
 * which is a pointer.
 *
 * The memory in a full code cache will look essentially like this:
 *   [struct TraceHeader][native code]  [struct TraceHeader][native code]...
 *
 * If alignment is required before the start of the next TraceHeader, it is
 * added by the architecture emitters after the end of a piece of code.
 */
extern void* CodeCache;

extern size_t CodeCacheSize;

/* Resets the pointers so that the code cache is considered to contain no
 * traces. */
void ResetCodeCache(void);

/* Allocates an executable memory region of the chosen Size. */
void* AllocExec(size_t Size);

/* Sets the code cache used by the Trace JIT to the given region. */
void SetCodeCache(void* Code, size_t Size);

/* Frees an executable memory region that had been allocated by AllocExec. */
void FreeExec(void* Code, size_t Size);

/* Returns the capacity of the code cache, in bytes, excluding two headers
 * (one for the header of the next piece of code, and one for an eventual wrap
 * header). */
size_t GetCodeCacheSize(void);

/* Returns the number of bytes used in the code cache. */
size_t GetCodeBytesUsed(void);

/* Returns the number of bytes usable for the next piece of code in the code
 * cache, excluding two headers (one for the piece of code itself and one for
 * an eventual wrap header). Only contiguous bytes are counted. */
size_t GetCodeBytesAvailable(void);

/* Returns a pointer to the piece of code immediately following the given
 * header in memory. */
TraceEntry GetTraceForHeader(struct TraceHeader* Header);

/* Returns a pointer to the header immediately preceding the given piece of
 * code in memory. */
struct TraceHeader* GetHeaderForTrace(TraceEntry Func);

/* Clears the entire code cache and deletes all trace mappings. */
void ClearCodeCache(void);

/* Clears the oldest parts of the code cache until at least the specified
 * number of bytes has been made available. Deletes corresponding trace
 * mappings. */
void ClearCodeBytes(size_t Bytes);

/* Return the address at which the next trace shall be compiled. */
TraceEntry GetNextTrace(void);

/* Sets up the latest TraceHeader and trace mapping after a trace has been
 * compiled. */
void SetUpHeader(uint32_t N64Address, void* Next);

/* Sets up a dummy header that does not create any trace mapping and whose
 * sole purpose is to declare that the end of the code cache is too small for
 * the code being emitted. Must be PRECEDED by a call to ClearCodeBytes with
 * more bytes for the start of the code cache. */
void SetUpWrapHeader(void);

/* Gets the piece of code implementing the trace of MIPS III instructions at the
 * given N64 memory address. */
TraceEntry GetTraceAt(uint32_t N64Address);

/* Gets the address of the byte after the last one in the given trace, which
 * must be a native trace. */
TraceEntry GetTraceEnd(TraceEntry Trace);

/* Gets the address of the first byte at which the trace following this one is
 * written. This includes any bookkeeping information written between traces. */
TraceEntry GetFollowingTrace(TraceEntry Trace);

/* Determines whether the given piece of code corresponds to emitted native
 * code (true) as opposed to an interpreter piece of code (false). */
bool IsNativeTrace(TraceEntry);

/* Sets the piece of code implementing the trace of MIPS III instructions at the
 * given N64 memory address. Returns false if a new page of trace piece of code
 * pointers was required and there was insufficient memory to allocate it. */
bool SetTraceAt(uint32_t N64Address, TraceEntry Func);

/* Clears the page of trace mappings containing the given N64 memory address.
 * This function is called after writing into N64 memory. */
void ClearTracePage(uint32_t N64Address);

/* Frees the cache of trace mappings. */
void FreeTraceCache(void);

#endif /* ARCH_EMIT_TRACE */

#endif /* !M64P_TRACE_JIT_NATIVE_TRACECACHE_H */
