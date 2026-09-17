#define _GNU_SOURCE
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <SDL.h>

#include "shim.h"
#include "window.h"

#ifndef SDL_INIT_EVENTS
#define SDL_INIT_EVENTS 0
#endif

static Uint32 s_inited;

int SDL_InitSubSystem(Uint32 flags) {
    shim_ticks_ms();
    if (!s_inited) {
        shim_events_init();
        shim_ipc_connect();
    }
    s_inited |= flags | SDL_INIT_EVENTS;
    return 0;
}

int SDL_Init(Uint32 flags) {
    return SDL_InitSubSystem(flags);
}

void SDL_QuitSubSystem(Uint32 flags) {
    s_inited &= ~flags;
}

Uint32 SDL_WasInit(Uint32 flags) {
    return flags ? (s_inited & flags) : s_inited;
}

void SDL_Quit(void) {
    shim_audio_quit();
    shim_video_quit();
    shim_ipc_close();
    shim_events_quit();
    s_inited = 0;
}

static void set_error(const char *fmt, va_list ap) {
    char text[512];
    vsnprintf(text, sizeof(text), fmt, ap);
    shim_set_error("%s", text);
}

#if SDL_MAJOR_VERSION >= 2
const char *SDL_GetError(void) {
    return shim_error_text();
}

int SDL_SetError(SDL_PRINTF_FORMAT_STRING const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    set_error(fmt, ap);
    va_end(ap);
    return -1;
}
#else
char *SDL_GetError(void) {
    return (char *)shim_error_text();
}

void SDL_SetError(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    set_error(fmt, ap);
    va_end(ap);
}
#endif

void SDL_ClearError(void) {
    shim_clear_error();
}

Uint32 SDL_GetTicks(void) {
    return (Uint32)shim_ticks_ms();
}

void SDL_Delay(Uint32 ms) {
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
    while (nanosleep(&ts, &ts) != 0) {}
}

void *SDL_malloc(size_t size) {
    return malloc(size);
}

void *SDL_calloc(size_t nmemb, size_t size) {
    return calloc(nmemb, size);
}

void *SDL_realloc(void *mem, size_t size) {
    return realloc(mem, size);
}

void SDL_free(void *mem) {
    free(mem);
}

#if defined(__x86_64__) || defined(__i386__)
#define CPU_HAS(feature) (__builtin_cpu_supports(feature) ? SDL_TRUE : SDL_FALSE)
#else
#define CPU_HAS(feature) SDL_FALSE
#endif

SDL_bool SDL_HasSSE(void)     { return CPU_HAS("sse"); }
SDL_bool SDL_HasSSE2(void)    { return CPU_HAS("sse2"); }
SDL_bool SDL_HasMMX(void)     { return CPU_HAS("mmx"); }
SDL_bool SDL_HasAltiVec(void) { return SDL_FALSE; }
SDL_bool SDL_HasRDTSC(void)   { return SDL_FALSE; }
