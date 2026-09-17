#define _GNU_SOURCE
#include <errno.h>
#include <iconv.h>

#include "sdl1.h"

/* glibc covers most of SDL_stdinc.h through the HAVE_* macros, but these have
   no libc equivalent, so a real libSDL-1.2 exports them and games may link
   against them */

void *SDL_revcpy(void *dst, const void *src, size_t len) {
    return memmove(dst, src, len);
}

char *SDL_strrev(char *string) {
    size_t len = strlen(string);
    for (size_t i = 0; i < len / 2; i++) {
        char swap = string[i];
        string[i] = string[len - 1 - i];
        string[len - 1 - i] = swap;
    }
    return string;
}

char *SDL_strupr(char *string) {
    for (char *at = string; *at; at++) {
        if (*at >= 'a' && *at <= 'z') *at = (char)(*at - 32);
    }
    return string;
}

char *SDL_strlwr(char *string) {
    for (char *at = string; *at; at++) {
        if (*at >= 'A' && *at <= 'Z') *at = (char)(*at + 32);
    }
    return string;
}

char *SDL_ulltoa(Uint64 value, char *string, int radix) {
    static const char k_digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    if (radix < 2 || radix > 36) {
        string[0] = '\0';
        return string;
    }
    char *at = string;
    do {
        *at++ = k_digits[value % (unsigned)radix];
        value /= (unsigned)radix;
    } while (value);
    *at = '\0';
    return SDL_strrev(string);
}

char *SDL_lltoa(Sint64 value, char *string, int radix) {
    if (value < 0 && radix == 10) {
        string[0] = '-';
        SDL_ulltoa((Uint64)(-value), string + 1, radix);
        return string;
    }
    return SDL_ulltoa((Uint64)value, string, radix);
}

char *SDL_ultoa(unsigned long value, char *string, int radix) {
    return SDL_ulltoa((Uint64)value, string, radix);
}

char *SDL_ltoa(long value, char *string, int radix) {
    return SDL_lltoa((Sint64)value, string, radix);
}

SDL_iconv_t SDL_iconv_open(const char *tocode, const char *fromcode) {
    return (SDL_iconv_t)iconv_open(tocode ? tocode : "", fromcode ? fromcode : "");
}

int SDL_iconv_close(SDL_iconv_t cd) {
    return iconv_close((iconv_t)cd);
}

size_t SDL_iconv(SDL_iconv_t cd, const char **inbuf, size_t *inbytesleft,
                 char **outbuf, size_t *outbytesleft) {
    return iconv((iconv_t)cd, (char **)(uintptr_t)inbuf, inbytesleft, outbuf, outbytesleft);
}

char *SDL_iconv_string(const char *tocode, const char *fromcode,
                       const char *inbuf, size_t inbytesleft) {
    if (!inbuf) return NULL;

    iconv_t cd = iconv_open(tocode ? tocode : "UTF-8", fromcode ? fromcode : "UTF-8");
    if (cd == (iconv_t)-1) return NULL;

    size_t cap = inbytesleft * 4 + 4;
    char  *out = malloc(cap);
    if (!out) {
        iconv_close(cd);
        return NULL;
    }

    char       *dst  = out;
    size_t      left = cap - 4;
    const char *src  = inbuf;

    while (inbytesleft) {
        if (iconv(cd, (char **)(uintptr_t)&src, &inbytesleft, &dst, &left) != (size_t)-1) break;
        if (errno == EILSEQ || errno == EINVAL) {
            /* skip the offending byte, matching what SDL does */
            src++;
            inbytesleft--;
            continue;
        }
        free(out);
        iconv_close(cd);
        return NULL;
    }
    memset(dst, 0, 4);
    iconv_close(cd);
    return out;
}
