#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>

#include "core.h"
#include "ipc.h"

#define QUEUE_FRAMES 8192

static retro_audio_sample_batch_t s_batch;
static int16_t                    s_queue[QUEUE_FRAMES * DOPO_IPC_AUDIO_MAX_CHANNELS];
static size_t                     s_queued;
static unsigned                   s_rate;
static unsigned                   s_channels;

void audio_init(retro_audio_sample_batch_t cb) {
    s_batch    = cb;
    s_queued   = 0;
    s_rate     = 0;
    s_channels = 0;
}

void audio_configure(unsigned rate, unsigned channels) {
    if (!rate || !channels) return;
    if (channels > DOPO_IPC_AUDIO_MAX_CHANNELS) channels = DOPO_IPC_AUDIO_MAX_CHANNELS;
    if (rate == s_rate && channels == s_channels) return;

    s_rate     = rate;
    s_channels = channels;
    s_queued   = 0;
}

void audio_push(const int16_t *data, size_t frames) {
    if (!s_batch || !data || !frames) return;
    if (!s_rate) audio_configure(48000, 2);

    if (s_channels == 2) {
        size_t room = QUEUE_FRAMES - s_queued;
        if (frames > room) frames = room;
        memcpy(s_queue + s_queued * 2, data, frames * 2 * sizeof(int16_t));
        s_queued += frames;
    } else {
        for (size_t i = 0; i < frames && s_queued < QUEUE_FRAMES; i++, s_queued++) {
            s_queue[s_queued * 2]     = data[i];
            s_queue[s_queued * 2 + 1] = data[i];
        }
    }

    size_t sent = 0;
    while (sent < s_queued) {
        size_t n = s_batch(s_queue + sent * 2, s_queued - sent);
        if (!n) break;
        sent += n;
    }

    if (sent && sent < s_queued) {
        memmove(s_queue, s_queue + sent * 2, (s_queued - sent) * 2 * sizeof(int16_t));
    }
    s_queued -= sent;
}

void audio_stop(void) {
    s_queued   = 0;
    s_rate     = 0;
    s_channels = 0;
}

void audio_reset(void) {
    audio_stop();
}
