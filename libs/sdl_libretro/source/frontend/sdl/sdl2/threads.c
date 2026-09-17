#define _GNU_SOURCE
#include <pthread.h>

#include "sdl2.h"

struct SDL_Thread {
    pthread_t          handle;
    SDL_ThreadFunction fn;
    void              *data;
    int                status;
    bool               detached;
    char               name[64];
};

static void *thread_trampoline(void *arg) {
    SDL_Thread *thread = arg;
    if (thread->name[0]) pthread_setname_np(pthread_self(), thread->name);
    thread->status = thread->fn(thread->data);
    if (thread->detached) free(thread);
    return NULL;
}

SDL_Thread *SDL_CreateThreadWithStackSize(SDL_ThreadFunction fn, const char *name,
                                          const size_t stacksize, void *data) {
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
    if (name) snprintf(thread->name, sizeof(thread->name), "%s", name);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    if (stacksize > 0) pthread_attr_setstacksize(&attr, stacksize);
    int rc = pthread_create(&thread->handle, &attr, thread_trampoline, thread);
    pthread_attr_destroy(&attr);

    if (rc != 0) {
        shim_set_error("pthread_create failed: %s", strerror(rc));
        free(thread);
        return NULL;
    }
    return thread;
}

SDL_Thread *SDL_CreateThread(SDL_ThreadFunction fn, const char *name, void *data) {
    return SDL_CreateThreadWithStackSize(fn, name, 0, data);
}

void SDL_WaitThread(SDL_Thread *thread, int *status) {
    if (!thread) return;
    pthread_join(thread->handle, NULL);
    if (status) *status = thread->status;
    free(thread);
}

void SDL_DetachThread(SDL_Thread *thread) {
    if (!thread) return;
    thread->detached = true;
    pthread_detach(thread->handle);
}

const char *SDL_GetThreadName(SDL_Thread *thread) {
    return (thread && thread->name[0]) ? thread->name : NULL;
}

SDL_threadID SDL_GetThreadID(SDL_Thread *thread) {
    return (SDL_threadID)(thread ? thread->handle : pthread_self());
}

SDL_threadID SDL_ThreadID(void) {
    return (SDL_threadID)pthread_self();
}

int SDL_SetThreadPriority(SDL_ThreadPriority priority) {
    (void)priority;
    return 0;
}
