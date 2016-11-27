#include <stddef.h>

#include "main/device.h"
#include "r4300/r4300_core.h"

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

    DEFINE(struct r4300_core, cp0);
    DEFINE(struct cp0, regs);
    DEFINE(struct cp0, tlb);

    DEFINE(struct tlb, entries);
    DEFINE(struct tlb, LUT_r);
    DEFINE(struct tlb, LUT_w);
}
