/*
 * Minimal SDL 1.2 build configuration for the shim.
 *
 * The upstream tarball only ships SDL_config.h.in, which configure expands.
 * We never build SDL itself, only its public headers, so this covers what
 * SDL_stdinc.h needs to pick the right fixed width types on linux.
 */
#ifndef _SDL_config_h
#define _SDL_config_h

#include "SDL_platform.h"

#define HAVE_STDIO_H     1
#define HAVE_STDLIB_H    1
#define HAVE_STDDEF_H    1
#define HAVE_STDARG_H    1
#define HAVE_STDINT_H    1
#define HAVE_STRING_H    1
#define HAVE_STRINGS_H   1
#define HAVE_INTTYPES_H  1
#define HAVE_SYS_TYPES_H 1
#define HAVE_CTYPE_H     1
#define STDC_HEADERS     1

#define SDL_HAS_64BIT_TYPE 1

#define SDL_LOADSO_DLOPEN  1
#define SDL_THREAD_PTHREAD 1
#define SDL_TIMER_UNIX     1

#endif
