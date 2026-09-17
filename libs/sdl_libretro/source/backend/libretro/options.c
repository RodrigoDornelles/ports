#define _GNU_SOURCE
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "core.h"
#include "dopo.h"

#define OPTION_MAX 8

static retro_environment_t s_environ;
static const char         *s_value[OPTION_MAX];

static const struct retro_variable k_variables[] = {
    { "sdl_bin",     "Interpreter binary; " },
    { "sdl_ld",      "Extra library path; " },
    { "sdl_gptk",    "Keyboard mapping file; " },
    { "sdl_shim",    "Shim directory; " },
    { "sdl_format",  "Video output; rgba8888|rgb565|egl" },
    { NULL, NULL },
};

void options_init(retro_environment_t cb) {
    s_environ = cb;
    if (!s_environ) return;
    s_environ(RETRO_ENVIRONMENT_SET_VARIABLES, (void *)k_variables);
    options_refresh();
}

void options_refresh(void) {
    if (!s_environ) return;
    for (size_t i = 0; k_variables[i].key; i++) {
        struct retro_variable var = { k_variables[i].key, NULL };
        s_value[i] = s_environ(RETRO_ENVIRONMENT_GET_VARIABLE, &var) ? var.value : NULL;
    }
}

static const char *option_env(const char *key) {
    char name[64];
    int  n = snprintf(name, sizeof(name), "%s_", DOPO_ENV_PREFIX);
    if (n < 0 || (size_t)n >= sizeof(name)) return NULL;

    for (size_t i = 0; key[i] && (size_t)n < sizeof(name) - 1; i++) {
        name[n++] = (char)toupper((unsigned char)key[i]);
    }
    name[n] = '\0';

    const char *value = getenv(name);
    return (value && value[0]) ? value : NULL;
}

const char *option_get(const char *key) {
    if (!key) return NULL;
    for (size_t i = 0; k_variables[i].key; i++) {
        if (strcmp(k_variables[i].key, key) != 0) continue;

        const char *env = option_env(key);
        if (env) return env;
        return (s_value[i] && s_value[i][0]) ? s_value[i] : NULL;
    }
    return NULL;
}

bool option_is(const char *key, const char *value) {
    const char *got = option_get(key);
    return got && strcmp(got, value) == 0;
}

void path_resolve_rel(const char *given, const char *base_file, char *out, size_t cap) {
    if (!given || !out || cap == 0) return;

    if (given[0] == '/' || !base_file || !base_file[0]) {
        snprintf(out, cap, "%s", given);
        return;
    }

    char base[1024];
    snprintf(base, sizeof(base), "%s", base_file);
    char *slash = strrchr(base, '/');
    if (!slash) {
        snprintf(out, cap, "%s", given);
        return;
    }
    *slash = '\0';

    if (given[0] == '.' && given[1] == '/') given += 2;
    snprintf(out, cap, "%s/%s", base, given);
}

bool path_which(const char *name, char *out, size_t cap) {
    if (!name || !name[0] || !out || cap == 0) return false;

    const char *path = getenv("PATH");
    if (!path || !path[0]) return false;

    char list[4096];
    snprintf(list, sizeof(list), "%s", path);

    for (char *dir = strtok(list, ":"); dir; dir = strtok(NULL, ":")) {
        if (!dir[0]) continue;
        int n = snprintf(out, cap, "%s/%s", dir, name);
        if (n < 0 || (size_t)n >= cap) continue;
        if (access(out, X_OK) == 0) return true;
    }
    out[0] = '\0';
    return false;
}
