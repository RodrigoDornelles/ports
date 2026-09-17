#define _GNU_SOURCE
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#include "shim.h"

/* error text and the clock, shared by every shim regardless of which library
   it pretends to be */

static char     s_error[512];
static uint64_t s_start_ms;

uint64_t shim_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)(ts.tv_nsec / 1000000);
}

uint64_t shim_ticks_ms(void) {
    if (!s_start_ms) s_start_ms = shim_now_ms();
    return shim_now_ms() - s_start_ms;
}

void shim_set_error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s_error, sizeof(s_error), fmt, ap);
    va_end(ap);
    fprintf(stderr, SHIM_TAG " %s\n", s_error);
}

const char *shim_error_text(void) {
    return s_error;
}

void shim_clear_error(void) {
    s_error[0] = '\0';
}
