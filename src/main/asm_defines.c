#include <stddef.h>

#include "main/device.h"
#include "memory/memory.h"
#include "r4300/r4300_core.h"
#include "ri/ri_controller.h"

/* magic macros -- don't change these */
#undef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#define SIZEOF(TYPE, MEMBER) (sizeof(((TYPE *)0)->MEMBER))

#define _DEFINE(sym, val) asm volatile("\n-> " #sym " %0 " #val "\n" : : "i" (val))
#define DEFINE(s, m) \
    _DEFINE(offsetof_##s##_##m,offsetof(s,m));

void foo(void)
{
    DEFINE(struct device, r4300);

    DEFINE(struct r4300_core, regs);
    DEFINE(struct r4300_core, hi);
    DEFINE(struct r4300_core, lo);

    DEFINE(struct r4300_core, stop);

#if defined(__x86_64__)
    DEFINE(struct r4300_core, save_rsp);
    DEFINE(struct r4300_core, save_rip);
#endif
    DEFINE(struct r4300_core, return_address);

    DEFINE(struct r4300_core, cp0);
    DEFINE(struct cp0, regs);
    DEFINE(struct cp0, next_interrupt);
    DEFINE(struct cp0, last_addr);
    DEFINE(struct cp0, count_per_op);
    DEFINE(struct cp0, tlb);

    DEFINE(struct tlb, entries);
    DEFINE(struct tlb, LUT_r);
    DEFINE(struct tlb, LUT_w);

    DEFINE(struct r4300_core, cached_interp);
    DEFINE(struct cached_interp, invalid_code);

    DEFINE(struct device, mem);

    DEFINE(struct memory, wbyte);
    DEFINE(struct memory, whword);
    DEFINE(struct memory, wword);
    DEFINE(struct memory, wdword);
    DEFINE(struct memory, address);

    DEFINE(struct device, ri);
    DEFINE(struct ri_controller, rdram);
    DEFINE(struct rdram, dram);
}
