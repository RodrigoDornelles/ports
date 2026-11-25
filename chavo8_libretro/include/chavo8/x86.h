#pragma once

/* The machine the Delphi code ran on, for the C that scripts/lift.nim makes
 * of game/v1/dump: the registers, the flags (kept as the last operation, read
 * when a jump asks), the x87 as doubles, a stack, and memory at the original
 * addresses, mapped onto the ports' own bytes (ChavesVar's, mainly). */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

enum { X86_EAX, X86_ECX, X86_EDX, X86_EBX, X86_ESP, X86_EBP, X86_ESI, X86_EDI };
enum { FLAG_SUB, FLAG_ADD, FLAG_LOGIC, FLAG_INC, FLAG_DEC, FLAG_SAHF };

typedef struct {
    uint32_t r[8];
    int op, bits;
    uint32_t a, b, res;
    bool carry;
    uint8_t ah;
    double st[8];
    int top;
    bool c0, c2, c3;
    uint32_t fs0;
} x86_t;

extern x86_t x86;

typedef struct {
    uint32_t base, size;
    uint8_t *mem;
} x86_region_t;

#define X86_REGIONS 4
extern x86_region_t x86_regions[X86_REGIONS];

void x86_map(uint32_t base, void *mem, uint32_t size);
void x86_rtl(uint32_t target);
_Noreturn void x86_fail(const char *what, uint32_t at);
extern const char *(*x86_literal)(uint32_t at, uint32_t *len);

static inline uint8_t *x86_at(uint32_t a, uint32_t n)
{
    for (int i = 0; i < X86_REGIONS; i++) {
        const x86_region_t *r = &x86_regions[i];
        if (a - r->base < r->size && a - r->base + n <= r->size)
            return r->mem + (a - r->base);
    }
    x86_fail("no memory", a);
}

static inline uint32_t rd8(uint32_t a) { return *x86_at(a, 1); }
static inline uint32_t rd16(uint32_t a) { uint16_t v; memcpy(&v, x86_at(a, 2), 2); return v; }
static inline uint32_t rd32(uint32_t a) { uint32_t v; memcpy(&v, x86_at(a, 4), 4); return v; }
static inline uint64_t rd64(uint32_t a) { uint64_t v; memcpy(&v, x86_at(a, 8), 8); return v; }
static inline void wr8(uint32_t a, uint32_t v) { *x86_at(a, 1) = (uint8_t)v; }
static inline void wr16(uint32_t a, uint32_t v) { uint16_t w = (uint16_t)v; memcpy(x86_at(a, 2), &w, 2); }
static inline void wr32(uint32_t a, uint32_t v) { memcpy(x86_at(a, 4), &v, 4); }
static inline void wr64(uint32_t a, uint64_t v) { memcpy(x86_at(a, 8), &v, 8); }

static inline void push32(uint32_t v) { x86.r[X86_ESP] -= 4; wr32(x86.r[X86_ESP], v); }
static inline uint32_t pop32(void) { uint32_t v = rd32(x86.r[X86_ESP]); x86.r[X86_ESP] += 4; return v; }

static inline uint32_t x86_mask(int bits) { return bits == 32 ? 0xffffffffu : (1u << bits) - 1; }
static inline bool x86_msb(uint32_t v, int bits) { return (v >> (bits - 1)) & 1; }

static inline bool cf(void)
{
    uint32_t m = x86_mask(x86.bits);
    switch (x86.op) {
    case FLAG_SUB: return (x86.a & m) < (x86.b & m);
    case FLAG_ADD: return (x86.res & m) < (x86.a & m);
    case FLAG_SAHF: return x86.ah & 1;
    case FLAG_INC: case FLAG_DEC: return x86.carry;
    default: return false;
    }
}

/* inc and dec keep CF: it is taken from the operation before */
static inline void set_flags(int op, uint32_t a, uint32_t b, uint32_t res, int bits)
{
    if (op == FLAG_INC || op == FLAG_DEC)
        x86.carry = cf();
    x86.op = op;
    x86.a = a;
    x86.b = b;
    x86.res = res;
    x86.bits = bits;
}

static inline bool zf(void) { return x86.op == FLAG_SAHF ? x86.ah & 0x40 : (x86.res & x86_mask(x86.bits)) == 0; }
static inline bool sf(void) { return x86.op == FLAG_SAHF ? x86.ah & 0x80 : x86_msb(x86.res, x86.bits); }
static inline bool of(void)
{
    uint32_t a = x86.a, b = x86.b, r = x86.res, m = x86_mask(x86.bits);
    switch (x86.op) {
    case FLAG_SUB: return x86_msb((a ^ b) & (a ^ r), x86.bits);
    case FLAG_ADD: return x86_msb(~(a ^ b) & (a ^ r), x86.bits);
    case FLAG_INC: return (r & m) == 1u << (x86.bits - 1);
    case FLAG_DEC: return (r & m) == (1u << (x86.bits - 1)) - 1;
    default: return false;
    }
}
static inline bool pf(void)
{
    if (x86.op == FLAG_SAHF)
        return x86.ah & 4;
    return !__builtin_parity(x86.res & 0xff);
}

static inline void sahf(void)
{
    x86.ah = (uint8_t)(x86.r[X86_EAX] >> 8);
    x86.op = FLAG_SAHF;
}

/* the x87, a stack of doubles */
#define ST(i) x86.st[(x86.top + (i)) & 7]
static inline void fpush(double v) { x86.top = (x86.top + 7) & 7; x86.st[x86.top] = v; }
static inline double fpop(void) { double v = x86.st[x86.top]; x86.top = (x86.top + 1) & 7; return v; }

static inline void fcom(double a, double b)
{
    bool unordered = a != a || b != b;
    x86.c0 = unordered || a < b;
    x86.c3 = unordered || a == b;
    x86.c2 = unordered;
}

static inline uint32_t fnstsw(void)
{
    return (x86.c0 ? 0x100u : 0) | (x86.c2 ? 0x400u : 0) | (x86.c3 ? 0x4000u : 0) | (uint32_t)x86.top << 11;
}

static inline double f32(uint32_t bits) { float f; memcpy(&f, &bits, 4); return f; }
static inline double f64(uint64_t bits) { double d; memcpy(&d, &bits, 8); return d; }
static inline uint32_t bits32(double v) { float f = (float)v; uint32_t b; memcpy(&b, &f, 4); return b; }
static inline uint64_t bits64(double v) { uint64_t b; memcpy(&b, &v, 8); return b; }

/* fistp, as Round: to the nearest, halves to even */
int64_t x86_round(double v);
