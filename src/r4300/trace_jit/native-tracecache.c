/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - native-tracecache.c                                     *
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

#if defined(__GNUC__)
#  include <unistd.h>
#  if !defined(__MINGW32__)
#    include <sys/mman.h>
#  endif
#endif

#include "native-config.h"
#include "native-tracecache.h"
#include "mips-interp.h"
#include "mips-jit.h"

#if defined(ARCH_EMIT_TRACE)

TracePage Traces[N64_PAGE_COUNT];

void* CodeCache;

size_t CodeCacheSize;

/* Pointer to the TraceHeader preceding, in memory, the oldest trace compiled
 * for the current game which has not yet been cleared.
 * If there are no traces, this is equivalent to NextHeader.
 */
static struct TraceHeader* FirstHeader;

/* Pointer to the TraceHeader at which the next trace to be compiled will be
 * described and after which, in memory, its code will be written.
 * This pointer only acts as a boundary for the code written before it; it
 * does not refer to a valid TraceHeader.
 */
static struct TraceHeader* NextHeader;

void ResetCodeCache()
{
	FirstHeader = NextHeader = CodeCache;
}

void* AllocExec(size_t Size)
{
#if defined(WIN32)
	return VirtualAlloc(NULL, Size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
#elif defined(__GNUC__)

	#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
		#define MAP_ANONYMOUS MAP_ANON
	#endif

	void* Result;
	if ((Result = mmap(NULL, Size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)) == MAP_FAILED) {
		Result = NULL;
	}
	return Result;
#else
	return malloc(Size);
#endif
}

void SetCodeCache(void* Code, size_t Size)
{
	CodeCache = Code;
	CodeCacheSize = Size;
	ResetCodeCache();
}

void FreeExec(void* Code, size_t Size)
{
#if defined(WIN32)
	VirtualFree(Code, 0, MEM_RELEASE);
#elif defined(__GNUC__)
	munmap(Code, Size);
#else
	free(Code);
#endif
}

size_t GetCodeCacheSize()
{
	return CodeCacheSize - 2 * sizeof(struct TraceHeader);
}

size_t GetCodeBytesUsed()
{
	if (FirstHeader == NextHeader)
		return 0;
	else if (FirstHeader < NextHeader)
		return (uint8_t*) NextHeader - (uint8_t*) FirstHeader;
	else
		/* FirstHeader > NextHeader: FirstHeader goes to the end of the cache,
		 * wraps back to the beginning, and continues up to NextHeader */
		return ((uint8_t*) CodeCache + CodeCacheSize - (uint8_t*) FirstHeader)
		     + ((uint8_t*) NextHeader - (uint8_t*) CodeCache);
}

size_t GetCodeBytesAvailable()
{
	size_t Result;
	/* First calculate how many bytes are available including headers. */
	if (FirstHeader <= NextHeader)
		Result = CodeCacheSize
		     - ((uint8_t*) NextHeader - (uint8_t*) CodeCache);
	else
		/* FirstHeader > NextHeader: between NextHeader and FirstHeader is
		 * usable */
		Result = ((uint8_t*) FirstHeader - (uint8_t*) NextHeader);

	/* Next, calculate how many bytes would be available for the function,
	 * excluding its header, if it were also immediately followed by a wrap
	 * header. */
	if (Result > 2 * sizeof(struct TraceHeader))
		return Result - 2 * sizeof(struct TraceHeader);
	else
		return 0;
}

TraceEntry GetTraceForHeader(struct TraceHeader* Header)
{
	return (TraceEntry) (Header + 1);
}

struct TraceHeader* GetHeaderForTrace(TraceEntry Func)
{
	return ((struct TraceHeader*) Func) - 1;
}

/* Clears the first trace, deletes its mapping if a newer version has not been
 * mapped for the same N64 address, and advances FirstTrace. Returns the
 * number of bytes that have been made available due to the clearing. */
static size_t ClearFirstTrace()
{
	if (FirstHeader != NextHeader) {
		struct TraceHeader* PreviousHeader;
		struct TraceHeader* SecondHeader = FirstHeader->Next;

		if (FirstHeader->UserPC != 0
		 && GetTraceAt(FirstHeader->UserPC) == GetTraceForHeader(FirstHeader)) {
			/* We allow traces to issue direct jumps to native code emitted
			 * for traces in the same page. Since we're clearing this one now,
			 * and other traces in the same page may refer to it, we must
			 * clear the other native traces of this page, otherwise they may
			 * jump to overwritten code. */
			TracePage Page = Traces[FirstHeader->UserPC >> N64_PAGE_SHIFT];
			uint32_t Insn;
			for (Insn = 0; Insn < N64_PAGE_INSNS; Insn++) {
				if (IsNativeTrace(Page[Insn])) {
					struct TraceHeader* Header = GetHeaderForTrace(Page[Insn]);
					Page[Insn] = FORMERLY_CODE;
					Header->UserPC = 0;
				}
			}
		}

		PreviousHeader = FirstHeader;
		FirstHeader = SecondHeader;
		if (PreviousHeader < SecondHeader) {
			return (uint8_t*) SecondHeader - (uint8_t*) PreviousHeader;
		} else {
			return ((uint8_t*) CodeCache + CodeCacheSize - (uint8_t*) PreviousHeader)
			     + ((uint8_t*) SecondHeader - (uint8_t*) CodeCache);
		}
	}
	else return 0;
}

void ClearCodeCache()
{
	while (FirstHeader != NextHeader) {
		ClearFirstTrace();
	}
	ResetCodeCache();
}

void ClearCodeBytes(size_t Bytes)
{
	size_t BytesCleared = 0;
	while (FirstHeader != NextHeader && BytesCleared < Bytes) {
		BytesCleared += ClearFirstTrace();
	}
	if (FirstHeader == NextHeader) {
		ResetCodeCache();
	}
}

TraceEntry GetNextTrace()
{
	return GetTraceForHeader(NextHeader);
}

void SetUpHeader(uint32_t N64Address, void* Next)
{
	NextHeader->UserPC = N64Address;
	NextHeader->Next = Next;
	NextHeader = Next;
}

void SetUpWrapHeader()
{
	NextHeader->UserPC = 0;
	NextHeader->Next = CodeCache;
	NextHeader = CodeCache;
}

TraceEntry GetTraceAt(uint32_t N64Address)
{
	TracePage Page = Traces[N64Address >> N64_PAGE_SHIFT];
	if (Page != NULL) {
		return Page[(N64Address & (N64_PAGE_SIZE - 1)) / N64_INSN_SIZE];
	}
	else return NOT_CODE;
}

TraceEntry GetTraceEnd(TraceEntry Trace)
{
	return GetHeaderForTrace(Trace)->Next;
}

TraceEntry GetFollowingTrace(TraceEntry Trace)
{
	return GetHeaderForTrace(Trace)->Next + 1;
}

bool IsNativeTrace(TraceEntry Trace)
{
	return (uint8_t*) Trace >= (uint8_t*) CodeCache
	    && (uint8_t*) Trace <  (uint8_t*) CodeCache + CodeCacheSize;
}

bool SetTraceAt(uint32_t N64Address, TraceEntry Func)
{
	TracePage Page = Traces[N64Address >> N64_PAGE_SHIFT];
	if (Page != NULL) {
		Page[(N64Address & (N64_PAGE_SIZE - 1)) / N64_INSN_SIZE] = Func;
		return true;
	} else {
		Page = malloc(N64_PAGE_INSNS * sizeof(TraceEntry));
		if (Page != NULL) {
			size_t i;
			Traces[N64Address >> N64_PAGE_SHIFT] = Page;
			for (i = 0; i < N64_PAGE_INSNS; i++) {
				Page[i] = NOT_CODE;
			}
			Page[(N64Address & (N64_PAGE_SIZE - 1)) / N64_INSN_SIZE] = Func;
			return true;
		}
		else return false;
	}
}

void ClearTracePage(uint32_t N64Address)
{
	size_t PageNumber = N64Address >> N64_PAGE_SHIFT;
	if (Traces[PageNumber] != NULL) {
		free(Traces[PageNumber]);
		Traces[PageNumber] = NULL;
	}
}

void FreeTraceCache()
{
	size_t i;
	for (i = 0; i < N64_PAGE_COUNT; i++) {
		if (Traces[i] != NULL) {
			free(Traces[i]);
			Traces[i] = NULL;
		}
	}
	ResetCodeCache();
}

#endif /* ARCH_EMIT_TRACE */
