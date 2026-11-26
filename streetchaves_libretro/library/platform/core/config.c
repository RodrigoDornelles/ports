#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "streetchaves/platform.h"

#define MAX_KEYS 64

static struct {
    char *path;
    char *key[MAX_KEYS], *value[MAX_KEYS];
    int count;
} config;

static char *trim(char *s)
{
    while (*s == ' ' || *s == '\t')
        s++;
    char *end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n'))
        *--end = 0;
    return s;
}

void config_open(const char *dir)
{
    config_close();
    if (!dir || !*dir)
        return;
    size_t n = strlen(dir) + sizeof "/streetchaves.ini";
    config.path = malloc(n);
    snprintf(config.path, n, "%s/streetchaves.ini", dir);
    FILE *f = fopen(config.path, "r");
    if (!f)
        return;
    char line[512];
    while (fgets(line, sizeof line, f)) {
        char *eq = strchr(line, '=');
        char *k = trim(line);
        if (!eq || *k == ';' || *k == '[')
            continue;
        *eq = 0;
        config_set(trim(k), trim(eq + 1));
    }
    fclose(f);
}

const char *config_get(const char *key)
{
    for (int i = 0; i < config.count; i++)
        if (!strcmp(config.key[i], key))
            return config.value[i];
    return NULL;
}

void config_set(const char *key, const char *value)
{
    for (int i = 0; i < config.count; i++)
        if (!strcmp(config.key[i], key)) {
            free(config.value[i]);
            config.value[i] = strdup(value);
            return;
        }
    if (config.count < MAX_KEYS) {
        config.key[config.count] = strdup(key);
        config.value[config.count++] = strdup(value);
    }
}

void config_clear(void)
{
    for (int i = 0; i < config.count; i++) {
        free(config.key[i]);
        free(config.value[i]);
    }
    config.count = 0;
}

bool config_save(void)
{
    if (!config.path)
        return false;
    FILE *f = fopen(config.path, "w");
    if (!f)
        return false;
    fputs("[streetchaves]\n", f);
    for (int i = 0; i < config.count; i++)
        fprintf(f, "%s=%s\n", config.key[i], config.value[i]);
    return fclose(f) == 0;
}

void config_close(void)
{
    config_clear();
    free(config.path);
    config.path = NULL;
}
