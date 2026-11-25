#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "chavo8/viewer.h"
#include "chavo8/x86.h"

#define STACK_TOP 0x00110000u

static uint8_t stack[0x10000];

x86_t x86 = { .r = { [X86_ESP] = STACK_TOP, [X86_EBP] = STACK_TOP } };
x86_region_t x86_regions[X86_REGIONS] = { { STACK_TOP - sizeof stack, sizeof stack, stack } };
const char *(*x86_literal)(uint32_t at, uint32_t *len);

/* pascal.nim's, so the code lifted and the code ported draw from one */
extern int32_t RandSeed;

void x86_map(uint32_t base, void *mem, uint32_t size)
{
    for (int i = 0; i < X86_REGIONS; i++)
        if (!x86_regions[i].mem) {
            x86_regions[i] = (x86_region_t){ base, size, mem };
            return;
        }
    x86_fail("no room to map", base);
}

_Noreturn void x86_fail(const char *what, uint32_t at)
{
    chavo8_log("x86: %s at %08x\n", what, at);
    abort();
}

int64_t x86_round(double v)
{
    return (int64_t)nearbyint(v);
}

static bool same(uint32_t a, uint32_t b, int *order)
{
    uint32_t na = 0, nb = 0;
    const char *sa = a ? x86_literal(a, &na) : "", *sb = b ? x86_literal(b, &nb) : "";
    int c = memcmp(sa, sb, na < nb ? na : nb);
    *order = c ? c : (na > nb) - (na < nb);
    return *order == 0;
}

/* the RTL's routines the game's code calls, as Delphi 7's System did them */
void x86_rtl(uint32_t target)
{
    uint32_t *r = x86.r;
    switch (target) {
    case 0x402a1c: /* Random(Range) */
        RandSeed = (int32_t)((uint32_t)RandSeed * 0x08088405u + 1);
        r[X86_EAX] = (uint32_t)(((uint64_t)(uint32_t)RandSeed * r[X86_EAX]) >> 32);
        break;
    case 0x402894: /* Round */
    case 0x4028a0: { /* Trunc */
        double v = fpop();
        uint64_t i = (uint64_t)(target == 0x402894 ? x86_round(v) : (int64_t)v);
        r[X86_EAX] = (uint32_t)i;
        r[X86_EDX] = (uint32_t)(i >> 32);
        break;
    }
    case 0x40287c: /* Int */
        ST(0) = trunc(ST(0));
        break;
    case 0x40286c: /* Cos: fcos leaves an operand past 2^63 flagged, which Cos makes 0 */
        ST(0) = fabs(ST(0)) >= 9223372036854775808.0 ? 0.0 : cos(ST(0));
        break;
    case 0x403d60: { /* LStrCmp: the flags of comparing the strings */
        int order;
        same(r[X86_EAX], r[X86_EDX], &order);
        set_flags(FLAG_SUB, order > 0, order < 0, order != 0, 32);
        break;
    }
    case 0x403e04: /* LStrAddRef: literals are never counted */
        break;
    case 0x4039d0: /* LStrClr */
        wr32(r[X86_EAX], 0);
        break;
    default:
        x86_fail("an RTL routine not done", target);
    }
}
