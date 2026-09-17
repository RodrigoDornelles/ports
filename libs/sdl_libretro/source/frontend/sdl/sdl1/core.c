#define _GNU_SOURCE
#include <pthread.h>

#include "sdl1.h"

const SDL_version *SDL_Linked_Version(void) {
    static const SDL_version version = {
        SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL,
    };
    return &version;
}

void SDL_Error(SDL_errorcode code) {
    static const char *const k_text[] = {
        [SDL_ENOMEM]      = "out of memory",
        [SDL_EFREAD]      = "error reading from datastream",
        [SDL_EFWRITE]     = "error writing to datastream",
        [SDL_EFSEEK]      = "error seeking in datastream",
        [SDL_UNSUPPORTED] = "that operation is not supported",
    };
    const char *text = (code >= 0 && code < SDL_LASTERROR && k_text[code])
                     ? k_text[code] : "unknown error";
    shim_set_error("%s", text);
}

struct SDL_Thread {
    pthread_t handle;
    int      (*fn)(void *);
    void      *data;
    int        status;
};

static void *thread_trampoline(void *arg) {
    SDL_Thread *thread = arg;
    thread->status = thread->fn(thread->data);
    return NULL;
}

SDL_Thread *SDL_CreateThread(int (*fn)(void *), void *data) {
    if (!fn) {
        shim_set_error("passed a NULL thread function");
        return NULL;
    }
    SDL_Thread *thread = calloc(1, sizeof(*thread));
    if (!thread) {
        shim_set_error("out of memory");
        return NULL;
    }
    thread->fn   = fn;
    thread->data = data;

    int rc = pthread_create(&thread->handle, NULL, thread_trampoline, thread);
    if (rc != 0) {
        shim_set_error("pthread_create failed: %s", strerror(rc));
        free(thread);
        return NULL;
    }
    return thread;
}

void SDL_WaitThread(SDL_Thread *thread, int *status) {
    if (!thread) return;
    pthread_join(thread->handle, NULL);
    if (status) *status = thread->status;
    free(thread);
}

void SDL_KillThread(SDL_Thread *thread) {
    if (!thread) return;
    pthread_detach(thread->handle);
    free(thread);
}

Uint32 SDL_ThreadID(void) {
    return (Uint32)(uintptr_t)pthread_self();
}

Uint32 SDL_GetThreadID(SDL_Thread *thread) {
    return (Uint32)(uintptr_t)(thread ? thread->handle : pthread_self());
}

SDL_bool SDL_HasMMXExt(void)   { return SDL_FALSE; }
SDL_bool SDL_Has3DNow(void)    { return SDL_FALSE; }
SDL_bool SDL_Has3DNowExt(void) { return SDL_FALSE; }
