#include <math.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>

#include "chavo8/viewer.h"
#include "chavo8/x86.h"

#define STACK_TOP 0x00110000u
#define HEAP 0x40000000u

static uint8_t stack[0x10000], heap[0x100000];
static uint32_t heap_used;

x86_t x86 = { .r = { [X86_ESP] = STACK_TOP, [X86_EBP] = STACK_TOP } };
x86_region_t x86_regions[X86_REGIONS] = {
    { STACK_TOP - sizeof stack, sizeof stack, stack },
    { HEAP, sizeof heap, heap },
};
const char *(*x86_literal)(uint32_t at, uint32_t *len);

/* pascal.nim's, so the code lifted and the code ported draw from one */
extern int32_t RandSeed;

static struct {
    uint32_t target;
    x86_hook_fn fn;
} hooks[32];

void x86_map(uint32_t base, void *mem, uint32_t size)
{
    for (int i = 0; i < X86_REGIONS; i++)
        if (!x86_regions[i].mem || x86_regions[i].base == base) {
            x86_regions[i] = (x86_region_t){ base, size, mem };
            return;
        }
    x86_fail("no room to map", base);
}

void x86_hook(uint32_t target, x86_hook_fn fn)
{
    for (int i = 0; i < 32; i++)
        if (!hooks[i].fn || hooks[i].target == target) {
            hooks[i].target = target;
            hooks[i].fn = fn;
            return;
        }
    x86_fail("no room for a hook", target);
}

static jmp_buf *catcher;

/* a failure in the code lifted (a routine not done, memory not mapped) ends
 * the call that ran it, which returns false, instead of the frontend */
bool x86_call(void (*fn)(void))
{
    jmp_buf here, *outer = catcher;
    catcher = &here;
    if (setjmp(here)) {
        catcher = outer;
        x86.top = 0;
        return false;
    }
    fn();
    catcher = outer;
    return true;
}

_Noreturn void x86_fail(const char *what, uint32_t at)
{
    char text[128];
    snprintf(text, sizeof text, "x86: %s at %08x", what, at);
    chavo8_log("%s\n", text);
    if (!catcher)
        abort();
    chavo8_notify(text);
    longjmp(*catcher, 1);
}

int64_t x86_round(double v)
{
    return (int64_t)nearbyint(v);
}

void x86_heap_reset(void)
{
    heap_used = 0;
}

/* as Delphi lays an AnsiString out: count, length, the characters, a 0 */
uint32_t x86_string_new(const char *text, uint32_t len)
{
    if (!len)
        return 0;
    uint32_t need = (8 + len + 1 + 3) & ~3u;
    if (heap_used + need > sizeof heap)
        x86_fail("the strings' heap is full", heap_used);
    uint8_t *p = heap + heap_used;
    uint32_t one = 1;
    memcpy(p, &one, 4);
    memcpy(p + 4, &len, 4);
    memcpy(p + 8, text, len);
    p[8 + len] = 0;
    heap_used += need;
    return HEAP + (uint32_t)(p + 8 - heap);
}

const char *x86_string(uint32_t at, uint32_t *len)
{
    if (!at) {
        *len = 0;
        return "";
    }
    if (at - HEAP < sizeof heap) {
        memcpy(len, heap + (at - HEAP) - 4, 4);
        return (const char *)heap + (at - HEAP);
    }
    return x86_literal(at, len);
}

static uint32_t arg(int i)
{
    return rd32(x86.r[X86_ESP] + 4u * (uint32_t)i);
}

static void drop(uint32_t bytes)
{
    x86.r[X86_ESP] += bytes;
}

static void concat(uint32_t dest, const uint32_t *parts, int n)
{
    char buf[1024];
    uint32_t len = 0;
    for (int i = 0; i < n; i++) {
        uint32_t l;
        const char *s = x86_string(parts[i], &l);
        if (len + l > sizeof buf)
            l = sizeof buf - len;
        memcpy(buf + len, s, l);
        len += l;
    }
    wr32(dest, x86_string_new(buf, len));
}

static void set_string(uint32_t dest, const char *text)
{
    wr32(dest, x86_string_new(text, (uint32_t)strlen(text)));
}

/* the RTL's routines the game's code calls, as Delphi 7's System and
 * SysUtils did them */
void x86_rtl(uint32_t target)
{
    uint32_t *r = x86.r;
    for (int i = 0; i < 32 && hooks[i].fn; i++)
        if (hooks[i].target == target) {
            hooks[i].fn();
            return;
        }
    switch (target) {
    case 0x402a1c: /* Random(Range) */
        RandSeed = (int32_t)((uint32_t)RandSeed * 0x08088405u + 1);
        r[X86_EAX] = (uint32_t)(((uint64_t)(uint32_t)RandSeed * r[X86_EAX]) >> 32);
        return;
    case 0x402894: /* Round */
    case 0x4028a0: { /* Trunc */
        double v = fpop();
        uint64_t i = (uint64_t)(target == 0x402894 ? x86_round(v) : (int64_t)v);
        r[X86_EAX] = (uint32_t)i;
        r[X86_EDX] = (uint32_t)(i >> 32);
        return;
    }
    case 0x40287c: /* Int */
        ST(0) = trunc(ST(0));
        return;
    case 0x40286c: /* Cos: fcos leaves an operand past 2^63 flagged, which Cos makes 0 */
        ST(0) = fabs(ST(0)) >= 9223372036854775808.0 ? 0.0 : cos(ST(0));
        return;
    case 0x405f44: { /* _llmul */
        uint64_t a = (uint64_t)r[X86_EDX] << 32 | r[X86_EAX];
        uint64_t b = (uint64_t)arg(1) << 32 | arg(0);
        uint64_t p = a * b;
        r[X86_EAX] = (uint32_t)p;
        r[X86_EDX] = (uint32_t)(p >> 32);
        drop(8);
        return;
    }
    case 0x406de4: /* RGB */
        r[X86_EAX] = (r[X86_EAX] & 0xff) | (r[X86_EDX] & 0xff) << 8 | (r[X86_ECX] & 0xff) << 16;
        return;
    case 0x40d760: { /* Rect(Left, Top, Right, Bottom) */
        uint32_t out = arg(0);
        wr32(out, r[X86_EAX]);
        wr32(out + 4, r[X86_EDX]);
        wr32(out + 8, r[X86_ECX]);
        wr32(out + 12, arg(1));
        drop(8);
        return;
    }
    case 0x408190: { /* IntToStr(Integer) */
        char buf[16];
        snprintf(buf, sizeof buf, "%d", (int32_t)r[X86_EAX]);
        set_string(r[X86_EDX], buf);
        return;
    }
    case 0x4081c0: { /* IntToStr(Int64) */
        char buf[24];
        snprintf(buf, sizeof buf, "%lld", (long long)((uint64_t)arg(1) << 32 | arg(0)));
        set_string(r[X86_EAX], buf);
        drop(8);
        return;
    }
    case 0x408ddc: { /* FloatToStr: 15 digits, the separator of a Portuguese Windows */
        char buf[40];
        snprintf(buf, sizeof buf, "%.15g", x86_rd80(r[X86_ESP]));
        for (char *c = buf; *c; c++)
            if (*c == '.')
                *c = ',';
        set_string(r[X86_EAX], buf);
        drop(12);
        return;
    }
    case 0x403d60: { /* LStrCmp: the flags of comparing the strings */
        uint32_t na, nb;
        const char *a = x86_string(r[X86_EAX], &na), *b = x86_string(r[X86_EDX], &nb);
        int c = memcmp(a, b, na < nb ? na : nb);
        int order = c ? c : (na > nb) - (na < nb);
        set_flags(FLAG_SUB, order > 0, order < 0, order != 0, 32);
        return;
    }
    case 0x403e04: /* LStrAddRef: none is ever freed but by the heap's reset */
        return;
    case 0x4039d0: /* LStrClr */
        wr32(r[X86_EAX], 0);
        return;
    case 0x4039f4: /* LStrArrayClr */
        for (uint32_t i = 0; i < r[X86_EDX]; i++)
            wr32(r[X86_EAX] + 4 * i, 0);
        return;
    case 0x403a24: /* LStrAsg */
        wr32(r[X86_EAX], r[X86_EDX]);
        return;
    case 0x403c50: { /* Length */
        uint32_t n;
        x86_string(r[X86_EAX], &n);
        r[X86_EAX] = n;
        return;
    }
    case 0x403c58: { /* LStrCat: dest := dest + s */
        uint32_t parts[2] = { rd32(r[X86_EAX]), r[X86_EDX] };
        concat(r[X86_EAX], parts, 2);
        return;
    }
    case 0x403c9c: { /* LStrCat3: dest := a + b */
        uint32_t parts[2] = { r[X86_EDX], r[X86_ECX] };
        concat(r[X86_EAX], parts, 2);
        return;
    }
    case 0x403d10: { /* LStrCatN: dest := the n pushed, first pushed first */
        int n = (int)r[X86_EDX];
        uint32_t parts[16];
        for (int i = 0; i < n && i < 16; i++)
            parts[i] = arg(n - 1 - i);
        concat(r[X86_EAX], parts, n);
        drop(4u * (uint32_t)n);
        return;
    }
    case 0x403b78: { /* LStrFromChar */
        char c = (char)(r[X86_EDX] & 0xff);
        wr32(r[X86_EAX], x86_string_new(&c, 1));
        return;
    }
    case 0x403bf4: { /* LStrFromString: a short string */
        uint32_t n = rd8(r[X86_EDX]);
        char buf[256];
        for (uint32_t i = 0; i < n; i++)
            buf[i] = (char)rd8(r[X86_EDX] + 1 + i);
        wr32(r[X86_EAX], x86_string_new(buf, n));
        return;
    }
    }
    if (!vcl_call(target))
        x86_fail("a routine not done", target);
}
