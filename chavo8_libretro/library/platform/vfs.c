#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "stb_image.h"

#include "chavo8/platform.h"

typedef struct {
    char *name;
    char *key;
    uint32_t offset, csize, usize;
    uint16_t method;
} entry_t;

static struct {
    char *root;
    FILE *zip;
    char *exe;
    entry_t *e;
    size_t n, cap;
} vfs;

static char *key_of(const char *name)
{
    char *k = strdup(name);
    for (char *c = k; *c; c++)
        *c = *c == '\\' ? '/' : (char)tolower((unsigned char)*c);
    return k;
}

static void add(const char *name, uint32_t offset, uint32_t csize, uint32_t usize, uint16_t method)
{
    if (vfs.n == vfs.cap) {
        vfs.cap = vfs.cap ? vfs.cap * 2 : 512;
        vfs.e = realloc(vfs.e, vfs.cap * sizeof *vfs.e);
    }
    entry_t *e = &vfs.e[vfs.n++];
    e->name = strdup(name);
    for (char *c = e->name; *c; c++)
        if (*c == '\\')
            *c = '/';
    e->key = key_of(name);
    e->offset = offset;
    e->csize = csize;
    e->usize = usize;
    e->method = method;
}

static void walk(const char *rel, int depth)
{
    char path[4096];
    snprintf(path, sizeof path, "%s/%s", vfs.root, rel);
    DIR *d = opendir(path);
    if (!d)
        return;
    struct dirent *de;
    while ((de = readdir(d))) {
        if (de->d_name[0] == '.')
            continue;
        char name[4096];
        struct stat st;
        if (snprintf(name, sizeof name, "%s%s%s", rel, *rel ? "/" : "", de->d_name) >= (int)sizeof name
            || snprintf(path, sizeof path, "%s/%s", vfs.root, name) >= (int)sizeof path
            || stat(path, &st))
            continue;
        if (S_ISDIR(st.st_mode)) {
            if (depth < 2)
                walk(name, depth + 1);
        } else {
            add(name, 0, 0, (uint32_t)st.st_size, 0);
        }
    }
    closedir(d);
}

static uint16_t u16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t u32(const uint8_t *p) { return u16(p) | (uint32_t)u16(p + 2) << 16; }

static bool ends_with(const char *s, const char *end)
{
    size_t n = strlen(s), m = strlen(end);
    return n >= m && !strcmp(s + n - m, end);
}

/* the central directory, then only what sits beside the exe, Chaves.exe
 * first or else any exe, relative to its folder */
static bool zip_index(void)
{
    uint8_t tail[65557];
    fseek(vfs.zip, 0, SEEK_END);
    long size = ftell(vfs.zip);
    long n = size < (long)sizeof tail ? size : (long)sizeof tail;
    fseek(vfs.zip, size - n, SEEK_SET);
    if (fread(tail, 1, (size_t)n, vfs.zip) != (size_t)n)
        return false;
    long eocd = n - 22;
    while (eocd >= 0 && u32(tail + eocd) != 0x06054b50)
        eocd--;
    if (eocd < 0)
        return false;
    uint32_t count = u16(tail + eocd + 10), cd_size = u32(tail + eocd + 12), cd_offset = u32(tail + eocd + 16);
    uint8_t *cd = malloc(cd_size);
    fseek(vfs.zip, cd_offset, SEEK_SET);
    if (fread(cd, 1, cd_size, vfs.zip) != cd_size) {
        free(cd);
        return false;
    }
    const uint8_t *p = cd;
    for (uint32_t i = 0; i < count && p + 46 <= cd + cd_size && u32(p) == 0x02014b50; i++) {
        uint16_t nlen = u16(p + 28), xlen = u16(p + 30), clen = u16(p + 32);
        char name[1024];
        snprintf(name, sizeof name, "%.*s", nlen, (const char *)p + 46);
        if (!ends_with(name, "/"))
            add(name, u32(p + 42), u32(p + 20), u32(p + 24), u16(p + 10));
        p += 46 + nlen + xlen + clen;
    }
    free(cd);
    entry_t *all = vfs.e;
    size_t all_n = vfs.n;
    const entry_t *exe = NULL;
    for (size_t i = 0; i < all_n; i++) {
        const char *base = strrchr(all[i].key, '/');
        base = base ? base + 1 : all[i].key;
        if (!strcmp(base, "chaves.exe") || (!exe && ends_with(base, ".exe")))
            exe = &all[i];
    }
    if (!exe)
        return false;
    const char *slash = strrchr(exe->key, '/');
    size_t prefix = slash ? (size_t)(slash - exe->key) + 1 : 0;
    char *dir = strndup(exe->key, prefix);
    vfs.e = NULL;
    vfs.n = vfs.cap = 0;
    for (size_t i = 0; i < all_n; i++) {
        if (!strncmp(all[i].key, dir, prefix) && all[i].key[prefix]) {
            add(all[i].name + prefix, all[i].offset, all[i].csize, all[i].usize, all[i].method);
            if (&all[i] == exe)
                vfs.exe = strdup(all[i].name + prefix);
        }
        free(all[i].name);
        free(all[i].key);
    }
    free(all);
    free(dir);
    return true;
}

static int by_key(const void *a, const void *b)
{
    return strcmp(((const entry_t *)a)->key, ((const entry_t *)b)->key);
}

bool vfs_open(const char *content)
{
    vfs_close();
    char *key = key_of(content);
    bool zip = ends_with(key, ".zip");
    free(key);
    if (zip) {
        vfs.zip = fopen(content, "rb");
        if (!vfs.zip || !zip_index()) {
            vfs_close();
            return false;
        }
    } else {
        vfs.root = strdup(content);
        char *slash = strrchr(vfs.root, '/');
        if (!slash) {
            vfs_close();
            return false;
        }
        *slash = 0;
        vfs.exe = strdup(slash + 1);
        walk("", 0);
    }
    qsort(vfs.e, vfs.n, sizeof *vfs.e, by_key);
    return true;
}

void vfs_close(void)
{
    for (size_t i = 0; i < vfs.n; i++) {
        free(vfs.e[i].name);
        free(vfs.e[i].key);
    }
    free(vfs.e);
    free(vfs.root);
    free(vfs.exe);
    if (vfs.zip)
        fclose(vfs.zip);
    memset(&vfs, 0, sizeof vfs);
}

const char *vfs_exe(void)
{
    return vfs.exe;
}

uint8_t *vfs_load(const char *name, size_t *size)
{
    entry_t probe = { .key = key_of(name) };
    entry_t *e = bsearch(&probe, vfs.e, vfs.n, sizeof *vfs.e, by_key);
    free(probe.key);
    if (!e)
        return NULL;
    uint8_t *data = malloc(e->usize ? e->usize : 1);
    bool ok = false;
    if (vfs.zip) {
        uint8_t local[30];
        fseek(vfs.zip, e->offset, SEEK_SET);
        if (fread(local, 1, 30, vfs.zip) == 30 && u32(local) == 0x04034b50) {
            fseek(vfs.zip, u16(local + 26) + u16(local + 28), SEEK_CUR);
            if (e->method == 0) {
                ok = fread(data, 1, e->usize, vfs.zip) == e->usize;
            } else if (e->method == 8) {
                char *packed = malloc(e->csize ? e->csize : 1);
                ok = fread(packed, 1, e->csize, vfs.zip) == e->csize
                    && stbi_zlib_decode_noheader_buffer((char *)data, (int)e->usize, packed, (int)e->csize) == (int)e->usize;
                free(packed);
            }
        }
    } else {
        char path[4096];
        snprintf(path, sizeof path, "%s/%s", vfs.root, e->name);
        FILE *f = fopen(path, "rb");
        if (f) {
            ok = fread(data, 1, e->usize, f) == e->usize;
            fclose(f);
        }
    }
    if (!ok) {
        free(data);
        return NULL;
    }
    *size = e->usize;
    return data;
}

size_t vfs_list(const char *dir, const char **names, size_t max)
{
    char *prefix = key_of(dir);
    size_t len = strlen(prefix), n = 0;
    for (size_t i = 0; i < vfs.n && n < max; i++) {
        const char *k = vfs.e[i].key;
        if (!strncmp(k, prefix, len) && k[len] == '/' && !strchr(k + len + 1, '/'))
            names[n++] = vfs.e[i].name + len + 1;
    }
    free(prefix);
    return n;
}
