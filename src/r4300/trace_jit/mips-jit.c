/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - mips-jit.c                                              *
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

#include <stdio.h>
#include <inttypes.h>

#include "memory/memory.h"

#include "mips-analysis.h"
#include "mips-interp.h"
#include "mips-jit.h"
#include "mips-simplify.h"
#include "mips-tracebounds.h"
#include "native-config.h"
#include "native-tracecache.h"

struct TJSettings TraceJITSettings;

void* MakeTraceAt(uint32_t address)
{
#if defined(ARCH_EMIT_TRACE)
	uint32_t ops[N64_PAGE_INSNS];
	size_t op_count = N64_PAGE_INSNS - (address & (N64_PAGE_SIZE - 1)) / N64_INSN_SIZE;
	enum TJEmitTraceResult result;
	void* code;
	void* code_end;
	size_t avail;
#ifdef SHOW_TRACE_INSTRUCTIONS
	size_t i;
#endif

	op_count = MakeTrace(ops, fast_mem_access(address), op_count);

#ifdef SHOW_TRACE_INSTRUCTIONS
	printf("BLOCK %08" PRIX32 ":", address);
	for (i = 0; i < op_count; i++) {
		printf(" %08" PRIX32, fast_mem_access(address)[i]);
	}
	printf("\n");
#endif

	do {
		code_end = code = GetNextTrace();
		avail = GetCodeBytesAvailable();
		result = ARCH_EMIT_TRACE(&code_end, avail, address, ops, op_count);

		if (result == TJ_MEMORY_ERROR) {
			size_t new_avail;

			/* Try to double the amount of available space. Clear at least
			 * 4 KiB. */
			ClearCodeBytes(avail > 4096 ? avail : 4096);
			new_avail = GetCodeBytesAvailable();

			if (new_avail == avail) {
				/* We're at the end of the cache, freeing old code at its
				 * start. This gives us no more usable bytes. Set up a wrap
				 * header here and try again. */
				SetUpWrapHeader();
			}
			avail = new_avail;
		}
	} while (result == TJ_MEMORY_ERROR && avail < GetCodeCacheSize());

	if (result != TJ_SUCCESS)
		code = NULL;

	if (!SetTraceAt(address, code)) {
		DebugMessage(M64MSG_WARNING, "There is insufficient memory to manage trace mappings for N64 address %08" PRIX32 ".", address);
		return NULL;
	}

	switch (result) {
	case TJ_SUCCESS:
	{
		uint32_t i;
		DebugMessage(M64MSG_VERBOSE, "compiled %08" PRIX32 "+%4zi at %" PRIXPTR "+%4zi", address, op_count, code, (uint8_t*) code_end - (uint8_t*) code);
		SetUpHeader(address, code_end);
		/* Operations in the trace have been seen as code. */
		for (i = 1; i < op_count; i++) {
			if (GetTraceAt(address + i * N64_INSN_SIZE) == NOT_CODE) {
				SetTraceAt(address + i * N64_INSN_SIZE, FORMERLY_CODE);
			}
		}
		break;
	}
	case TJ_MEMORY_ERROR:
		DebugMessage(M64MSG_ERROR, "Code for N64 address %08" PRIX32 "+%zi instructions exceeds the capacity of the code cache (%zi bytes).", address, op_count, GetCodeCacheSize());
		break;
	case TJ_FAILURE:
		DebugMessage(M64MSG_ERROR, "The Trace JIT failed to create code for N64 address %08" PRIX32 "+%zi instructions.", address, op_count);
		break;
	}

	return code;
#else
	DebugMessage(M64MSG_ERROR, "MakeTraceAt is not supported on this architecture.");
	return NULL;
#endif
}

void* GetOrMakeTraceAt(uint32_t pc)
{
#if defined(ARCH_EMIT_TRACE) && defined(ARCH_JIT_ENTRY)
	void* Trace;

	if ((Trace = GetTraceAt(pc)) == NOT_CODE || Trace == FORMERLY_CODE) {
		Trace = MakeTraceAt(pc);

		if (Trace == NULL) {
			DebugMessage(M64MSG_ERROR, "Emitted code was expecting to be able to jump somewhere, but the above error means it may no longer be able to continue.");
		}
	}

	return Trace;
#else
	DebugMessage(M64MSG_ERROR, "GetOrMakeTraceAt is not supported on this architecture.");
	return NULL;
#endif
}

void TJ_NOT_CODE()
{
#if defined(ARCH_EMIT_TRACE) && !defined(ARCH_JIT_ENTRY)
	void* Trace = MakeTraceAt(TJ_PC.addr);

	if (Trace != NULL) {
		/* Invoke the function right now. Avoids returning to the loop which
		 * would then need to reobtain the function with GetTraceAt. */
		(*(void (*) (void)) Trace) ();
	} else {
		TJFallback();
	}
#else
	DebugMessage(M64MSG_ERROR, "TJ_NOT_CODE is not supported on this architecture.");
#endif
}

/* The address of this function must be different from the one of TJ_NOT_CODE,
 * above. It's used to determine whether data at an N64 memory address was
 * once seen as code. */
void TJ_FORMERLY_CODE()
{
	TJ_NOT_CODE();
}
