#include <ctype.h>
#include <string.h>

#include "streetchaves/platform.h"

typedef struct {
    const uint8_t *exe;
    size_t size;
    const uint8_t *sections;
    unsigned nsections;
    uint32_t rsrc, base, entry;
} pe_t;

static uint16_t u16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t u32(const uint8_t *p) { return u16(p) | (uint32_t)u16(p + 2) << 16; }

static const uint8_t *at(const pe_t *pe, uint32_t rva, size_t len)
{
    for (unsigned i = 0; i < pe->nsections; i++) {
        const uint8_t *s = pe->sections + 40 * i;
        uint32_t va = u32(s + 12), raw_size = u32(s + 16), raw = u32(s + 20);
        if (rva >= va && rva - va + len <= raw_size && raw + (rva - va) + len <= pe->size)
            return pe->exe + raw + (rva - va);
    }
    return NULL;
}

static bool open_pe(pe_t *pe, const uint8_t *exe, size_t size)
{
    if (size < 0x40 || exe[0] != 'M' || exe[1] != 'Z')
        return false;
    uint32_t nt = u32(exe + 0x3C);
    if ((size_t)nt + 24 + 96 + 24 > size || memcmp(exe + nt, "PE\0\0", 4))
        return false;
    const uint8_t *opt = exe + nt + 24;
    pe->exe = exe;
    pe->size = size;
    pe->nsections = u16(exe + nt + 6);
    pe->sections = opt + u16(exe + nt + 20);
    pe->rsrc = u32(opt + 96 + 2 * 8);
    pe->entry = u32(opt + 16);
    pe->base = u32(opt + 28);
    return pe->sections + 40 * pe->nsections <= exe + size;
}

static bool same_name(const pe_t *pe, uint32_t offset, const char *name)
{
    const uint8_t *s = at(pe, pe->rsrc + offset, 2);
    if (!s)
        return false;
    size_t len = u16(s);
    s = at(pe, pe->rsrc + offset + 2, 2 * len);
    if (!s || len != strlen(name))
        return false;
    for (size_t i = 0; i < len; i++)
        if (u16(s + 2 * i) > 127 || toupper(s[2 * i]) != toupper((unsigned char)name[i]))
            return false;
    return true;
}

/* the subdirectory or data entry of the resource directory at offset
 * matching id (name NULL) or name; the first one when both are absent */
static bool find(const pe_t *pe, uint32_t dir, unsigned id, const char *name, bool any, uint32_t *out)
{
    const uint8_t *d = at(pe, pe->rsrc + dir, 16);
    if (!d)
        return false;
    unsigned n = u16(d + 12) + u16(d + 14);
    for (unsigned i = 0; i < n; i++) {
        const uint8_t *e = at(pe, pe->rsrc + dir + 16 + 8 * i, 8);
        if (!e)
            return false;
        uint32_t key = u32(e), value = u32(e + 4) & 0x7FFFFFFF;
        bool named = key & 0x80000000;
        if (any || (name ? named && same_name(pe, key & 0x7FFFFFFF, name) : !named && key == id)) {
            *out = value;
            return true;
        }
    }
    return false;
}

bool pe_resource(const uint8_t *exe, size_t size, unsigned type, const char *name, blob_t *out)
{
    pe_t pe;
    uint32_t types, names, data;
    if (!open_pe(&pe, exe, size)
        || !find(&pe, 0, type, NULL, false, &types)
        || !find(&pe, types, 0, name, false, &names)
        || !find(&pe, names, 0, NULL, true, &data))
        return false;
    const uint8_t *entry = at(&pe, pe.rsrc + data, 16);
    if (!entry)
        return false;
    out->size = u32(entry + 4);
    out->data = at(&pe, u32(entry), out->size);
    return out->data != NULL;
}

const uint8_t *pe_va(const uint8_t *exe, size_t size, uint32_t va, size_t len)
{
    pe_t pe;
    if (!open_pe(&pe, exe, size) || va < pe.base)
        return NULL;
    return at(&pe, va - pe.base, len);
}

bool pe_code(const uint8_t *exe, size_t size, blob_t *code, uint32_t *va)
{
    pe_t pe;
    if (!open_pe(&pe, exe, size))
        return false;
    for (unsigned i = 0; i < pe.nsections; i++) {
        const uint8_t *s = pe.sections + 40 * i;
        uint32_t rva = u32(s + 12), raw_size = u32(s + 16), raw = u32(s + 20);
        if (pe.entry >= rva && pe.entry - rva < raw_size && (size_t)raw + raw_size <= size) {
            code->data = exe + raw;
            code->size = raw_size;
            *va = pe.base + rva;
            return true;
        }
    }
    return false;
}
