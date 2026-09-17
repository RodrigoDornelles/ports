#define _GNU_SOURCE
#include "sdl1.h"

#define RW_TYPE_UNKNOWN 0
#define RW_TYPE_STDIO   2
#define RW_TYPE_MEM     4

static int stdio_seek(SDL_RWops *context, int offset, int whence) {
    FILE *fp = context->hidden.stdio.fp;
    if (fseek(fp, offset, whence) != 0) {
        shim_set_error("fseek failed");
        return -1;
    }
    return (int)ftell(fp);
}

static int stdio_read(SDL_RWops *context, void *ptr, int size, int maxnum) {
    if (size <= 0 || maxnum <= 0) return 0;
    size_t n = fread(ptr, (size_t)size, (size_t)maxnum, context->hidden.stdio.fp);
    if (!n && ferror(context->hidden.stdio.fp)) {
        shim_set_error("fread failed");
        return -1;
    }
    return (int)n;
}

static int stdio_write(SDL_RWops *context, const void *ptr, int size, int num) {
    if (size <= 0 || num <= 0) return 0;
    size_t n = fwrite(ptr, (size_t)size, (size_t)num, context->hidden.stdio.fp);
    if (n != (size_t)num) {
        shim_set_error("fwrite failed");
        return -1;
    }
    return (int)n;
}

static int stdio_close(SDL_RWops *context) {
    int rc = 0;
    if (context->hidden.stdio.autoclose && context->hidden.stdio.fp) {
        rc = fclose(context->hidden.stdio.fp) == 0 ? 0 : -1;
    }
    SDL_FreeRW(context);
    return rc;
}

static int mem_seek(SDL_RWops *context, int offset, int whence) {
    Uint8 *base = context->hidden.mem.base;
    Uint8 *stop = context->hidden.mem.stop;
    Uint8 *at;

    switch (whence) {
        case RW_SEEK_SET: at = base + offset; break;
        case RW_SEEK_CUR: at = context->hidden.mem.here + offset; break;
        case RW_SEEK_END: at = stop + offset; break;
        default:
            shim_set_error("unknown seek origin");
            return -1;
    }
    if (at < base) at = base;
    if (at > stop) at = stop;
    context->hidden.mem.here = at;
    return (int)(at - base);
}

static int mem_read(SDL_RWops *context, void *ptr, int size, int maxnum) {
    if (size <= 0 || maxnum <= 0) return 0;
    size_t left  = (size_t)(context->hidden.mem.stop - context->hidden.mem.here);
    size_t total = (size_t)size * (size_t)maxnum;
    if (total > left) total = (left / (size_t)size) * (size_t)size;
    memcpy(ptr, context->hidden.mem.here, total);
    context->hidden.mem.here += total;
    return (int)(total / (size_t)size);
}

static int mem_write(SDL_RWops *context, const void *ptr, int size, int num) {
    if (size <= 0 || num <= 0) return 0;
    size_t left  = (size_t)(context->hidden.mem.stop - context->hidden.mem.here);
    size_t total = (size_t)size * (size_t)num;
    if (total > left) total = (left / (size_t)size) * (size_t)size;
    memcpy(context->hidden.mem.here, ptr, total);
    context->hidden.mem.here += total;
    return (int)(total / (size_t)size);
}

static int mem_write_ro(SDL_RWops *context, const void *ptr, int size, int num) {
    (void)context; (void)ptr; (void)size; (void)num;
    shim_set_error("the memory buffer is read only");
    return -1;
}

static int mem_close(SDL_RWops *context) {
    SDL_FreeRW(context);
    return 0;
}

SDL_RWops *SDL_AllocRW(void) {
    SDL_RWops *area = calloc(1, sizeof(*area));
    if (!area) shim_set_error("out of memory");
    return area;
}

void SDL_FreeRW(SDL_RWops *area) {
    free(area);
}

SDL_RWops *SDL_RWFromFP(FILE *fp, int autoclose) {
    if (!fp) {
        shim_set_error("passed a NULL file pointer");
        return NULL;
    }
    SDL_RWops *rw = SDL_AllocRW();
    if (!rw) return NULL;

    rw->seek  = stdio_seek;
    rw->read  = stdio_read;
    rw->write = stdio_write;
    rw->close = stdio_close;
    rw->type  = RW_TYPE_STDIO;
    rw->hidden.stdio.fp        = fp;
    rw->hidden.stdio.autoclose = autoclose;
    return rw;
}

SDL_RWops *SDL_RWFromFile(const char *file, const char *mode) {
    if (!file || !mode) {
        shim_set_error("passed a NULL file or mode");
        return NULL;
    }
    FILE *fp = fopen(file, mode);
    if (!fp) {
        shim_set_error("could not open %s", file);
        return NULL;
    }
    SDL_RWops *rw = SDL_RWFromFP(fp, 1);
    if (!rw) fclose(fp);
    return rw;
}

SDL_RWops *SDL_RWFromMem(void *mem, int size) {
    if (!mem || size < 0) {
        shim_set_error("passed a NULL or negative sized buffer");
        return NULL;
    }
    SDL_RWops *rw = SDL_AllocRW();
    if (!rw) return NULL;

    rw->seek  = mem_seek;
    rw->read  = mem_read;
    rw->write = mem_write;
    rw->close = mem_close;
    rw->type  = RW_TYPE_MEM;
    rw->hidden.mem.base = mem;
    rw->hidden.mem.here = mem;
    rw->hidden.mem.stop = (Uint8 *)mem + size;
    return rw;
}

SDL_RWops *SDL_RWFromConstMem(const void *mem, int size) {
    SDL_RWops *rw = SDL_RWFromMem((void *)(uintptr_t)mem, size);
    if (rw) rw->write = mem_write_ro;
    return rw;
}

static Uint16 swap16(Uint16 v) {
    return (Uint16)((v >> 8) | (v << 8));
}

static Uint32 swap32(Uint32 v) {
    return (v >> 24) | ((v >> 8) & 0x0000FF00u) | ((v << 8) & 0x00FF0000u) | (v << 24);
}

static Uint64 swap64(Uint64 v) {
    return ((Uint64)swap32((Uint32)v) << 32) | swap32((Uint32)(v >> 32));
}

#define READ_AS(name, type, fixup) \
    type name(SDL_RWops *src) { \
        type value = 0; \
        if (src) SDL_RWread(src, &value, sizeof(value), 1); \
        return fixup(value); \
    }

#define WRITE_AS(name, type, fixup) \
    int name(SDL_RWops *dst, type value) { \
        type stored = fixup(value); \
        return (dst && SDL_RWwrite(dst, &stored, sizeof(stored), 1) == 1) ? 1 : 0; \
    }

#define KEEP(v) (v)

#if SDL_BYTEORDER == SDL_LIL_ENDIAN
#define BE16  swap16
#define BE32  swap32
#define BE64  swap64
#define LE16  KEEP
#define LE32  KEEP
#define LE64  KEEP
#else
#define BE16  KEEP
#define BE32  KEEP
#define BE64  KEEP
#define LE16  swap16
#define LE32  swap32
#define LE64  swap64
#endif

READ_AS(SDL_ReadLE16, Uint16, LE16)
READ_AS(SDL_ReadBE16, Uint16, BE16)
READ_AS(SDL_ReadLE32, Uint32, LE32)
READ_AS(SDL_ReadBE32, Uint32, BE32)
READ_AS(SDL_ReadLE64, Uint64, LE64)
READ_AS(SDL_ReadBE64, Uint64, BE64)

WRITE_AS(SDL_WriteLE16, Uint16, LE16)
WRITE_AS(SDL_WriteBE16, Uint16, BE16)
WRITE_AS(SDL_WriteLE32, Uint32, LE32)
WRITE_AS(SDL_WriteBE32, Uint32, BE32)
WRITE_AS(SDL_WriteLE64, Uint64, LE64)
WRITE_AS(SDL_WriteBE64, Uint64, BE64)
