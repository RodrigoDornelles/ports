#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "streetchaves/platform.h"
#include "streetchaves/viewer.h"

/* ChavesTexto clears the seven memos (7x call [edx+0CCh]), then tests its
 * language (cmp byte [ebp-1],1; jne Spanish) and adds every line with
 *     <memo>; mov eax,[eax+208h]; <line>; mov ecx,[eax]; call [ecx+34h]
 * the memo being one of its parameters, the line a literal (mov edx,lit),
 * '' (xor edx,edx), or, once, a local built before the memo is loaded:
 *     push lit; lea edx,[ebp+a]; mov eax,n; call IntToStr
 *     push [ebp+a]; push lit; lea eax,[ebp+b]; mov edx,3; call LStrCatN
 * The memos' 380 pixels never wrap a line, so Lines[i] is the i-th added. */

static const struct {
    uint8_t len, bytes[5];
} memo_load[MEMOS] = {
    [MEMO_NOMES] = { 5, { 0x8B, 0x45, 0xF8, 0x8B, 0x00 } },
    [MEMO_TEXTO] = { 5, { 0x8B, 0x45, 0xF4, 0x8B, 0x00 } },
    [MEMO_CONFIG] = { 5, { 0x8B, 0x45, 0x18, 0x8B, 0x00 } },
    [MEMO_FRASES] = { 2, { 0x8B, 0x07 } },
    [MEMO_DIALOGO] = { 2, { 0x8B, 0x06 } },
    [MEMO_FINAIS] = { 2, { 0x8B, 0x03 } },
    [MEMO_CENARIOS] = { 5, { 0x8B, 0x45, 0x08, 0x8B, 0x00 } },
};

static const uint8_t lines_of[] = { 0x8B, 0x80, 0x08, 0x02, 0x00, 0x00 };
static const uint8_t add[] = { 0x8B, 0x08, 0xFF, 0x51, 0x34 };

typedef struct {
    char **line;
    size_t count, cap;
} lines_t;

static lines_t memos[2][MEMOS];

static const uint8_t *exe_data;
static size_t exe_size;

static uint32_t u32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

static bool is(const uint8_t *p, const uint8_t *bytes, size_t n)
{
    return !memcmp(p, bytes, n);
}

/* a Delphi string literal: its length sits just before it */
static char *literal(uint32_t va)
{
    const uint8_t *len = pe_va(exe_data, exe_size, va - 4, 4);
    const uint8_t *s = len ? pe_va(exe_data, exe_size, va, u32(len)) : NULL;
    return s ? strndup((const char *)s, u32(len)) : strdup("");
}

static int memo_before(const uint8_t *p, const uint8_t *start)
{
    for (int m = 0; m < MEMOS; m++)
        if (p - memo_load[m].len >= start && is(p - memo_load[m].len, memo_load[m].bytes, memo_load[m].len))
            return m;
    return -1;
}

static char *concat(const uint8_t *p, const uint8_t *start, uint8_t local)
{
    for (const uint8_t *q = p - 10; q - 26 >= start; q--) {
        if (q[0] != 0x8D || q[1] != 0x45 || q[2] != local || q[3] != 0xBA || u32(q + 4) != 3 || q[8] != 0xE8)
            continue;
        const uint8_t *c = q - 26;
        if (c[0] != 0x68 || c[5] != 0x8D || c[6] != 0x55 || c[8] != 0xB8 || c[13] != 0xE8
            || c[18] != 0xFF || c[19] != 0x75 || c[20] != c[7] || c[21] != 0x68)
            break;
        char *a = literal(u32(c + 1)), *b = literal(u32(c + 22)), *s;
        size_t n = strlen(a) + strlen(b) + 16;
        s = malloc(n);
        snprintf(s, n, "%s%d%s", a, (int32_t)u32(c + 9), b);
        free(a);
        free(b);
        return s;
    }
    streetchaves_log("texts: a computed line not understood\n");
    return strdup("");
}

static void push(int language, int memo, char *line)
{
    lines_t *m = &memos[language][memo];
    if (m->count == m->cap) {
        m->cap = m->cap ? m->cap * 2 : 64;
        m->line = realloc(m->line, m->cap * sizeof *m->line);
    }
    m->line[m->count++] = line;
}

static void scan(int language, const uint8_t *start, const uint8_t *end)
{
    for (const uint8_t *p = start; p + sizeof add <= end; p++) {
        if (!is(p, add, sizeof add))
            continue;
        char *line = NULL;
        const uint8_t *q = NULL;
        if (p - start >= 11 && p[-5] == 0xBA && is(p - 11, lines_of, sizeof lines_of)) {
            line = literal(u32(p - 4));
            q = p - 11;
        } else if (p - start >= 8 && p[-2] == 0x33 && p[-1] == 0xD2 && is(p - 8, lines_of, sizeof lines_of)) {
            line = strdup("");
            q = p - 8;
        } else if (p - start >= 6 && is(p - 6, lines_of, sizeof lines_of)) {
            q = p - 6;
        } else {
            continue;
        }
        int memo = memo_before(q, start);
        if (memo < 0) {
            free(line);
            continue;
        }
        if (!line) {
            const uint8_t *local = q - memo_load[memo].len - 3;
            if (local < start || local[0] != 0x8B || local[1] != 0x55)
                continue;
            line = concat(local, start, local[2]);
        }
        push(language, memo, line);
    }
}

bool texts_load(const uint8_t *exe, size_t size)
{
    texts_free();
    exe_data = exe;
    exe_size = size;
    blob_t code;
    uint32_t base;
    if (!pe_code(exe, size, &code, &base))
        return false;
    const uint8_t *c = code.data, *end = code.data + code.size;
    for (const uint8_t *p = c + 7 * 6; p + 10 <= end; p++) {
        static const uint8_t split[] = { 0x80, 0x7D, 0xFF, 0x01, 0x0F, 0x85 };
        if (!is(p, split, sizeof split))
            continue;
        int clears = 0;
        for (const uint8_t *q = p - 96 > c ? p - 96 : c; q < p; q++)
            clears += q[0] == 0xFF && q[1] == 0x92 && u32(q + 2) == 0xCC;
        if (clears != 7)
            continue;
        const uint8_t *pt = p + 10, *es = pt + (int32_t)u32(p + 6);
        if (es <= pt || es >= end)
            continue;
        const uint8_t *es_end = es;
        while (es_end + 3 <= end && !(es_end[0] == 0x64 && es_end[1] == 0x89 && es_end[2] == 0x10))
            es_end++;
        scan(0, pt, es);
        scan(1, es, es_end);
        return memos[0][MEMO_NOMES].count && memos[1][MEMO_NOMES].count;
    }
    return false;
}

void texts_free(void)
{
    for (int l = 0; l < 2; l++)
        for (int m = 0; m < MEMOS; m++) {
            for (size_t i = 0; i < memos[l][m].count; i++)
                free(memos[l][m].line[i]);
            free(memos[l][m].line);
        }
    memset(memos, 0, sizeof memos);
}

size_t texts_count(int language, int memo)
{
    return memo >= 0 && memo < MEMOS ? memos[language != 1][memo].count : 0;
}

/* TMemo.Lines[i] past the end reads as '' */
const char *texts_line(int language, int memo, size_t index)
{
    return index < texts_count(language, memo) ? memos[language != 1][memo].line[index] : "";
}

/* a string literal of the exe's code, by its address */
size_t exe_literal(uint32_t va, const char **text)
{
    const uint8_t *len = exe_data ? pe_va(exe_data, exe_size, va - 4, 4) : NULL;
    const uint8_t *s = len ? pe_va(exe_data, exe_size, va, u32(len)) : NULL;
    *text = s ? (const char *)s : "";
    return s ? u32(len) : 0;
}
