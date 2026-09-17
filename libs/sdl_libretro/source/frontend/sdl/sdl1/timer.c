#define _GNU_SOURCE
#include <pthread.h>
#include <time.h>

#include "sdl1.h"

struct _SDL_TimerID {
    pthread_t            thread;
    SDL_NewTimerCallback callback;
    void                *param;
    Uint32               interval;
    bool                 running;
};

static SDL_TimerID      s_legacy;
static SDL_TimerCallback s_legacy_callback;

static void *timer_thread(void *arg) {
    SDL_TimerID timer = arg;
    while (timer->running) {
        Uint32 interval = timer->interval;
        struct timespec ts = { interval / 1000, (long)(interval % 1000) * 1000000L };
        while (nanosleep(&ts, &ts) != 0) {}
        if (!timer->running) break;

        Uint32 next = timer->callback(interval, timer->param);
        if (!next) break;
        timer->interval = next;
    }
    timer->running = false;
    return NULL;
}

SDL_TimerID SDL_AddTimer(Uint32 interval, SDL_NewTimerCallback callback, void *param) {
    if (!callback || !interval) {
        shim_set_error("passed a NULL timer callback or zero interval");
        return NULL;
    }
    SDL_TimerID timer = calloc(1, sizeof(*timer));
    if (!timer) {
        shim_set_error("out of memory");
        return NULL;
    }
    timer->callback = callback;
    timer->param    = param;
    timer->interval = interval;
    timer->running  = true;

    if (pthread_create(&timer->thread, NULL, timer_thread, timer) != 0) {
        shim_set_error("could not start the timer thread");
        free(timer);
        return NULL;
    }
    return timer;
}

SDL_bool SDL_RemoveTimer(SDL_TimerID t) {
    if (!t) return SDL_FALSE;
    t->running = false;
    pthread_join(t->thread, NULL);
    free(t);
    return SDL_TRUE;
}

static Uint32 legacy_tick(Uint32 interval, void *param) {
    (void)param;
    return s_legacy_callback ? s_legacy_callback(interval) : 0;
}

int SDL_SetTimer(Uint32 interval, SDL_TimerCallback callback) {
    if (s_legacy) {
        SDL_RemoveTimer(s_legacy);
        s_legacy = NULL;
    }
    s_legacy_callback = callback;
    if (!interval || !callback) return 0;

    s_legacy = SDL_AddTimer(interval, legacy_tick, NULL);
    return s_legacy ? 0 : -1;
}
