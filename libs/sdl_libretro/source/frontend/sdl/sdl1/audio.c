#define _GNU_SOURCE
#include <pthread.h>
#include <time.h>

#include "sdl1.h"

#define AUDIO_DEFAULT_RATE    48000
#define AUDIO_DEFAULT_SAMPLES 1024

static struct {
    SDL_AudioSpec   spec;
    bool            opened;
    bool            running;
    bool            paused;
    pthread_t       thread;
    pthread_mutex_t lock;
    Uint8          *buffer;
    size_t          buffer_bytes;
    int16_t         frames[DOPO_IPC_AUDIO_MAX_FRAMES * DOPO_IPC_AUDIO_MAX_CHANNELS];
} a = { .lock = PTHREAD_MUTEX_INITIALIZER, .paused = true };

static Uint8 silence_of(Uint16 format) {
    return (format & 0x8000) ? 0x00 : ((format & 0xFF) == 8 ? 0x80 : 0x00);
}

static int16_t sample_of(const Uint8 *at, Uint16 format) {
    const bool wide     = (format & 0xFF) == 16;
    const bool is_signed = (format & 0x8000) != 0;
    const bool big_endian = (format & 0x1000) != 0;

    if (!wide) {
        int value = is_signed ? (int)(Sint8)at[0] : (int)at[0] - 128;
        return (int16_t)(value * 256);
    }

    uint16_t raw = big_endian ? (uint16_t)((at[0] << 8) | at[1])
                              : (uint16_t)((at[1] << 8) | at[0]);
    return is_signed ? (int16_t)raw : (int16_t)((int)raw - 32768);
}

/* the core expects interleaved signed 16 bit stereo */
static size_t to_stereo_s16(const Uint8 *src, size_t frames, int16_t *dst) {
    const int step     = ((a.spec.format & 0xFF) == 16) ? 2 : 1;
    const int channels = a.spec.channels > 1 ? 2 : 1;

    for (size_t i = 0; i < frames; i++) {
        int16_t left  = sample_of(src + (size_t)(i * channels) * (size_t)step, a.spec.format);
        int16_t right = channels == 2
                      ? sample_of(src + (size_t)(i * channels + 1) * (size_t)step, a.spec.format)
                      : left;
        dst[i * 2]     = left;
        dst[i * 2 + 1] = right;
    }
    return frames;
}

static void audio_send(size_t frames) {
    size_t sent = 0;
    while (sent < frames) {
        size_t chunk = frames - sent;
        if (chunk > DOPO_IPC_AUDIO_MAX_FRAMES) chunk = DOPO_IPC_AUDIO_MAX_FRAMES;
        shim_ipc_send_blob(DOPO_IPC_PKT_AUDIO, DOPO_IPC_AUDIO_MAX_CHANNELS,
                           (uint16_t)chunk, 0,
                           a.frames + sent * 2, chunk * 2 * sizeof(int16_t));
        sent += chunk;
    }
}

static void *audio_thread(void *arg) {
    (void)arg;
    struct timespec next;
    clock_gettime(CLOCK_MONOTONIC, &next);

    while (a.running) {
        const unsigned samples = a.spec.samples ? a.spec.samples : AUDIO_DEFAULT_SAMPLES;
        const unsigned rate    = a.spec.freq ? (unsigned)a.spec.freq : AUDIO_DEFAULT_RATE;

        pthread_mutex_lock(&a.lock);
        if (a.paused || !a.spec.callback) {
            memset(a.buffer, silence_of(a.spec.format), a.buffer_bytes);
        } else {
            a.spec.callback(a.spec.userdata, a.buffer, (int)a.buffer_bytes);
            audio_send(to_stereo_s16(a.buffer, samples, a.frames));
        }
        pthread_mutex_unlock(&a.lock);

        next.tv_nsec += (long)((1000000000.0 * samples) / rate);
        while (next.tv_nsec >= 1000000000L) {
            next.tv_nsec -= 1000000000L;
            next.tv_sec  += 1;
        }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
    }
    return NULL;
}

int SDL_OpenAudio(SDL_AudioSpec *desired, SDL_AudioSpec *obtained) {
    if (!desired || !desired->callback) {
        shim_set_error("passed a NULL audio spec or callback");
        return -1;
    }
    if (a.opened) {
        shim_set_error("audio device is already open");
        return -1;
    }

    a.spec = *desired;
    if (a.spec.freq <= 0)     a.spec.freq     = AUDIO_DEFAULT_RATE;
    if (!a.spec.samples)      a.spec.samples  = AUDIO_DEFAULT_SAMPLES;
    if (a.spec.samples > DOPO_IPC_AUDIO_MAX_FRAMES) a.spec.samples = DOPO_IPC_AUDIO_MAX_FRAMES;
    if (a.spec.channels < 1)  a.spec.channels = 2;
    if (a.spec.channels > DOPO_IPC_AUDIO_MAX_CHANNELS) a.spec.channels = DOPO_IPC_AUDIO_MAX_CHANNELS;

    switch (a.spec.format) {
        case AUDIO_U8:
        case AUDIO_S8:
        case AUDIO_U16LSB:
        case AUDIO_S16LSB:
        case AUDIO_U16MSB:
        case AUDIO_S16MSB:
            break;
        default:
            a.spec.format = AUDIO_S16SYS;
            break;
    }

    a.spec.silence = silence_of(a.spec.format);
    a.spec.size    = (Uint32)a.spec.samples * a.spec.channels * (((a.spec.format & 0xFF) == 16) ? 2 : 1);

    a.buffer_bytes = a.spec.size;
    a.buffer       = calloc(1, a.buffer_bytes);
    if (!a.buffer) {
        shim_set_error("out of memory");
        return -1;
    }

    shim_ipc_send(DOPO_IPC_PKT_AUDIO_CFG, 0, DOPO_IPC_AUDIO_MAX_CHANNELS, (uint32_t)a.spec.freq);

    a.opened  = true;
    a.paused  = true;
    a.running = true;
    if (pthread_create(&a.thread, NULL, audio_thread, NULL) != 0) {
        shim_set_error("could not start the audio thread");
        free(a.buffer);
        a.buffer  = NULL;
        a.opened  = false;
        a.running = false;
        return -1;
    }

    if (obtained) *obtained = a.spec;
    fprintf(stderr, SHIM_TAG " audio %d Hz, %u channels, %u samples\n",
            a.spec.freq, a.spec.channels, a.spec.samples);
    return 0;
}

void SDL_CloseAudio(void) {
    shim_audio_quit();
}

void shim_audio_quit(void) {
    if (!a.opened) return;
    a.running = false;
    pthread_join(a.thread, NULL);
    shim_ipc_send(DOPO_IPC_PKT_AUDIO_STOP, 0, 0, 0);

    free(a.buffer);
    a.buffer       = NULL;
    a.buffer_bytes = 0;
    a.opened       = false;
    a.paused       = true;
    memset(&a.spec, 0, sizeof(a.spec));
}

void SDL_PauseAudio(int pause_on) {
    pthread_mutex_lock(&a.lock);
    a.paused = pause_on != 0;
    pthread_mutex_unlock(&a.lock);
}

SDL_audiostatus SDL_GetAudioStatus(void) {
    if (!a.opened) return SDL_AUDIO_STOPPED;
    return a.paused ? SDL_AUDIO_PAUSED : SDL_AUDIO_PLAYING;
}

void SDL_LockAudio(void) {
    pthread_mutex_lock(&a.lock);
}

void SDL_UnlockAudio(void) {
    pthread_mutex_unlock(&a.lock);
}

int SDL_AudioInit(const char *driver_name) {
    (void)driver_name;
    return 0;
}

void SDL_AudioQuit(void) {
    shim_audio_quit();
}

char *SDL_AudioDriverName(char *namebuf, int maxlen) {
    if (!namebuf || maxlen <= 0) return NULL;
    snprintf(namebuf, (size_t)maxlen, "%s", DOPO_DRIVER);
    return namebuf;
}

void SDL_MixAudio(Uint8 *dst, const Uint8 *src, Uint32 len, int volume) {
    if (!dst || !src || !len || volume <= 0) return;
    if (volume > SDL_MIX_MAXVOLUME) volume = SDL_MIX_MAXVOLUME;

    if ((a.spec.format & 0xFF) == 16) {
        for (Uint32 i = 0; i + 1 < len; i += 2) {
            int16_t in, out;
            memcpy(&in, src + i, sizeof(in));
            memcpy(&out, dst + i, sizeof(out));
            int mixed = out + (in * volume) / SDL_MIX_MAXVOLUME;
            if (mixed >  32767) mixed =  32767;
            if (mixed < -32768) mixed = -32768;
            out = (int16_t)mixed;
            memcpy(dst + i, &out, sizeof(out));
        }
        return;
    }

    const bool is_signed = (a.spec.format & 0x8000) != 0;
    for (Uint32 i = 0; i < len; i++) {
        int in    = is_signed ? (Sint8)src[i] : (int)src[i] - 128;
        int out   = is_signed ? (Sint8)dst[i] : (int)dst[i] - 128;
        int mixed = out + (in * volume) / SDL_MIX_MAXVOLUME;
        if (mixed >  127) mixed =  127;
        if (mixed < -128) mixed = -128;
        dst[i] = is_signed ? (Uint8)(Sint8)mixed : (Uint8)(mixed + 128);
    }
}

SDL_AudioSpec *SDL_LoadWAV_RW(SDL_RWops *src, int freesrc, SDL_AudioSpec *spec,
                              Uint8 **audio_buf, Uint32 *audio_len) {
    SDL_AudioSpec *result = NULL;

    if (!src || !spec || !audio_buf || !audio_len) {
        shim_set_error("passed a NULL wave argument");
        goto done;
    }
    *audio_buf = NULL;
    *audio_len = 0;

    char tag[4];
    if (SDL_RWread(src, tag, 1, 4) != 4 || memcmp(tag, "RIFF", 4) != 0) {
        shim_set_error("not a RIFF file");
        goto done;
    }
    SDL_ReadLE32(src);
    if (SDL_RWread(src, tag, 1, 4) != 4 || memcmp(tag, "WAVE", 4) != 0) {
        shim_set_error("not a wave file");
        goto done;
    }

    Uint16 channels = 0, bits = 0, encoding = 0;
    Uint32 rate = 0;
    bool   have_format = false;

    for (;;) {
        if (SDL_RWread(src, tag, 1, 4) != 4) {
            shim_set_error("wave file has no data chunk");
            goto done;
        }
        Uint32 size = SDL_ReadLE32(src);

        if (memcmp(tag, "fmt ", 4) == 0) {
            encoding = SDL_ReadLE16(src);
            channels = SDL_ReadLE16(src);
            rate     = SDL_ReadLE32(src);
            SDL_ReadLE32(src);                  /* bytes per second */
            SDL_ReadLE16(src);                  /* block align      */
            bits     = SDL_ReadLE16(src);
            if (size > 16) SDL_RWseek(src, (int)size - 16, RW_SEEK_CUR);
            have_format = true;
            continue;
        }
        if (memcmp(tag, "data", 4) == 0) {
            if (!have_format) {
                shim_set_error("wave file has no format chunk");
                goto done;
            }
            if (encoding != 1) {
                shim_set_error("only uncompressed pcm wave files are supported");
                goto done;
            }
            if (bits != 8 && bits != 16) {
                shim_set_error("%u bit wave files are not supported", bits);
                goto done;
            }

            Uint8 *data = malloc(size ? size : 1);
            if (!data) {
                shim_set_error("out of memory");
                goto done;
            }
            if (size && SDL_RWread(src, data, 1, (int)size) != (int)size) {
                free(data);
                shim_set_error("truncated wave data");
                goto done;
            }

            memset(spec, 0, sizeof(*spec));
            spec->freq     = (int)rate;
            spec->channels = (Uint8)channels;
            spec->format   = bits == 8 ? AUDIO_U8 : AUDIO_S16LSB;
            spec->samples  = AUDIO_DEFAULT_SAMPLES;
            spec->silence  = silence_of(spec->format);
            spec->size     = size;

            *audio_buf = data;
            *audio_len = size;
            result     = spec;
            goto done;
        }
        SDL_RWseek(src, (int)((size + 1) & ~1u), RW_SEEK_CUR);
    }

done:
    if (freesrc && src) SDL_RWclose(src);
    return result;
}

void SDL_FreeWAV(Uint8 *audio_buf) {
    free(audio_buf);
}
