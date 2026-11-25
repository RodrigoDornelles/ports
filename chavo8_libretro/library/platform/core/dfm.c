#include <stdio.h>
#include <string.h>

#include "chavo8/platform.h"

/* Delphi's binary form (TPF0): components with their properties and
 * children; collection items are lists of properties */
enum {
    VA_NULL, VA_LIST, VA_INT8, VA_INT16, VA_INT32, VA_EXTENDED, VA_STRING, VA_IDENT,
    VA_FALSE, VA_TRUE, VA_BINARY, VA_SET, VA_LSTRING, VA_NIL, VA_COLLECTION, VA_SINGLE,
    VA_CURRENCY, VA_DATE, VA_WSTRING, VA_INT64, VA_UTF8STRING, VA_DOUBLE,
};

typedef struct {
    const uint8_t *p, *end;
    bool bad;
    dfm_item_fn fn;
    void *ctx;
    char component[256];
} reader_t;

static uint8_t byte(reader_t *r)
{
    if (r->p >= r->end) {
        r->bad = true;
        return 0;
    }
    return *r->p++;
}

static uint8_t peek(const reader_t *r)
{
    return r->p < r->end ? *r->p : VA_NULL;
}

static const uint8_t *take(reader_t *r, size_t n)
{
    if ((size_t)(r->end - r->p) < n) {
        r->bad = true;
        r->p = r->end;
        return NULL;
    }
    const uint8_t *at = r->p;
    r->p += n;
    return at;
}

static uint32_t u32(reader_t *r)
{
    const uint8_t *p = take(r, 4);
    return p ? p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24 : 0;
}

static void str(reader_t *r, char *out, size_t max)
{
    size_t n = byte(r);
    const uint8_t *s = take(r, n);
    snprintf(out, max, "%.*s", s ? (int)n : 0, s ? (const char *)s : "");
}

typedef struct {
    uint8_t type;
    int64_t i;
    char s[256];
    blob_t blob;
} value_t;

static void collection(reader_t *r);

static void value(reader_t *r, value_t *v)
{
    v->type = byte(r);
    v->i = 0;
    v->s[0] = 0;
    switch (v->type) {
    case VA_LIST:
        while (!r->bad && peek(r) != VA_NULL) {
            value_t skip;
            value(r, &skip);
        }
        byte(r);
        break;
    case VA_INT8: v->i = (int8_t)byte(r); break;
    case VA_INT16: { const uint8_t *p = take(r, 2); v->i = p ? (int16_t)(p[0] | p[1] << 8) : 0; break; }
    case VA_INT32: v->i = (int32_t)u32(r); break;
    case VA_EXTENDED: take(r, 10); break;
    case VA_SINGLE: take(r, 4); break;
    case VA_CURRENCY: case VA_DATE: case VA_INT64: case VA_DOUBLE: take(r, 8); break;
    case VA_STRING: case VA_IDENT: str(r, v->s, sizeof v->s); break;
    case VA_TRUE: v->i = 1; break;
    case VA_LSTRING: case VA_UTF8STRING: take(r, u32(r)); break;
    case VA_WSTRING: take(r, 2 * (size_t)u32(r)); break;
    case VA_BINARY:
        v->blob.size = u32(r);
        v->blob.data = take(r, v->blob.size);
        break;
    case VA_SET:
        for (char s[256]; !r->bad && (str(r, s, sizeof s), s[0]);)
            ;
        break;
    case VA_COLLECTION: collection(r); break;
    case VA_NULL: case VA_FALSE: case VA_NIL: break;
    default: r->bad = true;
    }
}

static uint32_t color(const value_t *v)
{
    if (v->type == VA_IDENT)
        return !strcmp(v->s, "clWhite") ? 0xFFFFFF : 0;
    return (uint32_t)v->i;
}

static void collection(reader_t *r)
{
    while (!r->bad && peek(r) != VA_NULL) {
        if (peek(r) == VA_INT8 || peek(r) == VA_INT16 || peek(r) == VA_INT32) {
            value_t order;
            value(r, &order);
        }
        if (byte(r) != VA_LIST) {
            r->bad = true;
            return;
        }
        dfm_item_t item = { .component = r->component };
        char name[256] = "";
        while (!r->bad && peek(r) != VA_NULL) {
            char prop[256];
            value_t v;
            str(r, prop, sizeof prop);
            value(r, &v);
            if (!strcmp(prop, "Name"))
                snprintf(name, sizeof name, "%s", v.s);
            else if (!strcmp(prop, "Picture.Data") || !strcmp(prop, "Wave.WAVE"))
                item.data = v.blob;
            else if (!strcmp(prop, "Transparent"))
                item.transparent = v.i;
            else if (!strcmp(prop, "TransparentColor"))
                item.transparent_color = color(&v);
            else if (!strcmp(prop, "Looped"))
                item.looped = v.i;
        }
        byte(r);
        item.name = name;
        r->fn(r->ctx, &item);
    }
    byte(r);
}

static void component(reader_t *r)
{
    if ((peek(r) & 0xF0) == 0xF0 && byte(r) & 2) {
        value_t position;
        value(r, &position);
    }
    char cls[256], name[256];
    str(r, cls, sizeof cls);
    str(r, name, sizeof name);
    while (!r->bad && peek(r) != VA_NULL) {
        char prop[256];
        value_t v;
        str(r, prop, sizeof prop);
        snprintf(r->component, sizeof r->component, "%s", name);
        value(r, &v);
    }
    byte(r);
    while (!r->bad && peek(r) != VA_NULL)
        component(r);
    byte(r);
}

bool dfm_items(const uint8_t *dfm, size_t size, dfm_item_fn fn, void *ctx)
{
    if (size < 4 || memcmp(dfm, "TPF0", 4))
        return false;
    reader_t r = { .p = dfm + 4, .end = dfm + size, .fn = fn, .ctx = ctx };
    component(&r);
    return !r.bad;
}
