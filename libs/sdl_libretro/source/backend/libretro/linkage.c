#define _GNU_SOURCE
#include <elf.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "core.h"

/*
 * A binary already says which libraries it wants in its DT_NEEDED entries, so
 * the core reads them rather than asking. It decides nothing about how they are
 * loaded, only whether a directory of ours belongs on the search path at all:
 * the fake libX11 must stay out of the way of games that never asked for X.
 */

typedef struct {
    const uint8_t *base;
    size_t         size;
    bool           wide;
} image_t;

static bool image_get(const image_t *img, size_t offset, void *out, size_t bytes) {
    if (offset > img->size || img->size - offset < bytes) return false;
    memcpy(out, img->base + offset, bytes);
    return true;
}

/* program headers, read through whichever ELF class the file uses */
typedef struct {
    uint32_t type;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t filesz;
} phdr_t;

static bool phdr_get(const image_t *img, uint64_t table, uint16_t entsize, uint16_t index,
                     phdr_t *out) {
    size_t at = (size_t)table + (size_t)entsize * index;

    if (img->wide) {
        Elf64_Phdr ph;
        if (entsize < sizeof(ph) || !image_get(img, at, &ph, sizeof(ph))) return false;
        out->type   = ph.p_type;
        out->offset = ph.p_offset;
        out->vaddr  = ph.p_vaddr;
        out->filesz = ph.p_filesz;
        return true;
    }

    Elf32_Phdr ph;
    if (entsize < sizeof(ph) || !image_get(img, at, &ph, sizeof(ph))) return false;
    out->type   = ph.p_type;
    out->offset = ph.p_offset;
    out->vaddr  = ph.p_vaddr;
    out->filesz = ph.p_filesz;
    return true;
}

/* a virtual address only becomes a file offset through the segment holding it */
static bool vaddr_to_offset(const image_t *img, uint64_t table, uint16_t entsize, uint16_t count,
                            uint64_t vaddr, uint64_t *out) {
    for (uint16_t i = 0; i < count; i++) {
        phdr_t ph;
        if (!phdr_get(img, table, entsize, i, &ph)) return false;
        if (ph.type != PT_LOAD) continue;
        if (vaddr < ph.vaddr || vaddr >= ph.vaddr + ph.filesz) continue;
        *out = vaddr - ph.vaddr + ph.offset;
        return true;
    }
    return false;
}

static bool dyn_get(const image_t *img, size_t at, uint64_t *tag, uint64_t *value) {
    if (img->wide) {
        Elf64_Dyn dyn;
        if (!image_get(img, at, &dyn, sizeof(dyn))) return false;
        *tag   = dyn.d_tag;
        *value = dyn.d_un.d_val;
        return true;
    }
    Elf32_Dyn dyn;
    if (!image_get(img, at, &dyn, sizeof(dyn))) return false;
    *tag   = dyn.d_tag;
    *value = dyn.d_un.d_val;
    return true;
}

static bool image_needs(const image_t *img, const char *soname) {
    unsigned char ident[EI_NIDENT];
    if (!image_get(img, 0, ident, sizeof(ident))) return false;
    if (memcmp(ident, ELFMAG, SELFMAG) != 0) return false;

    uint64_t phoff;
    uint16_t phentsize, phnum;

    if (img->wide) {
        Elf64_Ehdr eh;
        if (!image_get(img, 0, &eh, sizeof(eh))) return false;
        phoff     = eh.e_phoff;
        phentsize = eh.e_phentsize;
        phnum     = eh.e_phnum;
    } else {
        Elf32_Ehdr eh;
        if (!image_get(img, 0, &eh, sizeof(eh))) return false;
        phoff     = eh.e_phoff;
        phentsize = eh.e_phentsize;
        phnum     = eh.e_phnum;
    }
    if (!phoff || !phnum) return false;

    phdr_t dynamic = { 0, 0, 0, 0 };
    for (uint16_t i = 0; i < phnum; i++) {
        phdr_t ph;
        if (!phdr_get(img, phoff, phentsize, i, &ph)) return false;
        if (ph.type == PT_DYNAMIC) {
            dynamic = ph;
            break;
        }
    }
    if (dynamic.type != PT_DYNAMIC) return false;

    const size_t step = img->wide ? sizeof(Elf64_Dyn) : sizeof(Elf32_Dyn);

    uint64_t strtab_vaddr = 0;
    for (uint64_t at = 0; at + step <= dynamic.filesz; at += step) {
        uint64_t tag, value;
        if (!dyn_get(img, (size_t)(dynamic.offset + at), &tag, &value)) return false;
        if (tag == DT_NULL) break;
        if (tag == DT_STRTAB) {
            strtab_vaddr = value;
            break;
        }
    }
    if (!strtab_vaddr) return false;

    uint64_t strtab = 0;
    if (!vaddr_to_offset(img, phoff, phentsize, phnum, strtab_vaddr, &strtab)) return false;

    for (uint64_t at = 0; at + step <= dynamic.filesz; at += step) {
        uint64_t tag, value;
        if (!dyn_get(img, (size_t)(dynamic.offset + at), &tag, &value)) return false;
        if (tag == DT_NULL) break;
        if (tag != DT_NEEDED) continue;

        size_t name = (size_t)(strtab + value);
        if (name >= img->size) continue;

        const char *needed = (const char *)img->base + name;
        if (!memchr(needed, '\0', img->size - name)) continue;
        if (strcmp(needed, soname) == 0) return true;
    }
    return false;
}

bool linkage_needs(const char *path, const char *soname) {
    if (!path || !path[0] || !soname) return false;

    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;

    struct stat st;
    if (fstat(fd, &st) != 0 || st.st_size < (off_t)sizeof(Elf32_Ehdr)) {
        close(fd);
        return false;
    }

    void *map = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (map == MAP_FAILED) return false;

    image_t img = { map, (size_t)st.st_size, ((const unsigned char *)map)[EI_CLASS] == ELFCLASS64 };
    bool    needs = image_needs(&img, soname);
    munmap(map, (size_t)st.st_size);
    return needs;
}
