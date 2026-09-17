#define _GNU_SOURCE
#include <time.h>
#include <unistd.h>

#include "sdl2.h"

static char    *s_clipboard;
static SDL_bool s_text_input;

/* audio reaches the core through the jack shim, so there is nothing to stop */
void shim_audio_quit(void) {
}

char *SDL_GetErrorMsg(char *errstr, int maxlen) {
    if (errstr && maxlen > 0) snprintf(errstr, (size_t)maxlen, "%s", SDL_GetError());
    return errstr;
}

void SDL_GetVersion(SDL_version *ver) {
    if (!ver) return;
    ver->major = SDL_MAJOR_VERSION;
    ver->minor = SDL_MINOR_VERSION;
    ver->patch = SDL_PATCHLEVEL;
}

const char *SDL_GetRevision(void) {
    return DOPO_SHIM_ID;
}

int SDL_GetRevisionNumber(void) {
    return 0;
}

const char *SDL_GetPlatform(void) {
    return "Linux";
}

SDL_bool SDL_SetHint(const char *name, const char *value) {
    (void)name; (void)value;
    return SDL_TRUE;
}

SDL_bool SDL_SetHintWithPriority(const char *name, const char *value, SDL_HintPriority priority) {
    (void)name; (void)value; (void)priority;
    return SDL_TRUE;
}

const char *SDL_GetHint(const char *name) {
    (void)name;
    return NULL;
}

SDL_bool SDL_GetHintBoolean(const char *name, SDL_bool default_value) {
    (void)name;
    return default_value;
}

Uint64 SDL_GetTicks64(void) {
    return shim_ticks_ms();
}

Uint64 SDL_GetPerformanceCounter(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (Uint64)ts.tv_sec * 1000000000ull + (Uint64)ts.tv_nsec;
}

Uint64 SDL_GetPerformanceFrequency(void) {
    return 1000000000ull;
}

#if defined(__x86_64__) || defined(__i386__)
#define CPU_HAS(feature) (__builtin_cpu_supports(feature) ? SDL_TRUE : SDL_FALSE)
#else
#define CPU_HAS(feature) SDL_FALSE
#endif

SDL_bool SDL_HasSSE3(void)   { return CPU_HAS("sse3"); }
SDL_bool SDL_HasSSE41(void)  { return CPU_HAS("sse4.1"); }
SDL_bool SDL_HasSSE42(void)  { return CPU_HAS("sse4.2"); }
SDL_bool SDL_HasAVX(void)    { return CPU_HAS("avx"); }
SDL_bool SDL_HasAVX2(void)   { return CPU_HAS("avx2"); }
SDL_bool SDL_HasAVX512F(void){ return CPU_HAS("avx512f"); }
SDL_bool SDL_HasNEON(void)   { return SDL_FALSE; }

int SDL_GetCPUCount(void) {
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (int)n : 1;
}

int SDL_GetSystemRAM(void) {
    long pages = sysconf(_SC_PHYS_PAGES);
    long psize = sysconf(_SC_PAGESIZE);
    if (pages <= 0 || psize <= 0) return 0;
    return (int)(((Uint64)pages * (Uint64)psize) / (1024u * 1024u));
}

char *SDL_GetClipboardText(void) {
    return strdup(s_clipboard ? s_clipboard : "");
}

int SDL_SetClipboardText(const char *text) {
    free(s_clipboard);
    s_clipboard = text ? strdup(text) : NULL;
    return 0;
}

SDL_bool SDL_HasClipboardText(void) {
    return (s_clipboard && s_clipboard[0]) ? SDL_TRUE : SDL_FALSE;
}

void SDL_StartTextInput(void) {
    s_text_input = SDL_TRUE;
}

void SDL_StopTextInput(void) {
    s_text_input = SDL_FALSE;
}

SDL_bool SDL_IsTextInputActive(void) {
    return s_text_input;
}

void SDL_SetTextInputRect(const SDL_Rect *rect) {
    (void)rect;
}

SDL_bool SDL_HasScreenKeyboardSupport(void) {
    return SDL_FALSE;
}

SDL_bool SDL_IsScreenKeyboardShown(SDL_Window *window) {
    (void)window;
    return SDL_FALSE;
}

int SDL_WarpMouseGlobal(int x, int y) {
    (void)x; (void)y;
    return 0;
}

void SDL_WarpMouseInWindow(SDL_Window *window, int x, int y) {
    (void)window; (void)x; (void)y;
}

int SDL_SetRelativeMouseMode(SDL_bool enabled) {
    (void)enabled;
    return 0;
}

SDL_bool SDL_GetRelativeMouseMode(void) {
    return SDL_FALSE;
}

Uint32 SDL_GetMouseState(int *x, int *y) {
    if (x) *x = 0;
    if (y) *y = 0;
    return 0;
}

Uint32 SDL_GetGlobalMouseState(int *x, int *y) {
    return SDL_GetMouseState(x, y);
}

int SDL_CaptureMouse(SDL_bool enabled) {
    (void)enabled;
    return 0;
}
