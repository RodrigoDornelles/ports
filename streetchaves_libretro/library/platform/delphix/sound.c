#include <string.h>

#include "streetchaves/delphix.h"

#define CHANNELS 32

static struct {
    const wave_t *wave;
    uint64_t pos, step;
    bool loop;
} channel[CHANNELS];

int dx_play(const wave_t *wave, bool loop)
{
    for (int i = 0; i < CHANNELS; i++) {
        if (channel[i].wave)
            continue;
        channel[i].wave = wave;
        channel[i].pos = 0;
        channel[i].step = ((uint64_t)wave->rate << 32) / DX_RATE;
        channel[i].loop = loop;
        return i;
    }
    return -1;
}

void dx_stop(const wave_t *wave)
{
    for (int i = 0; i < CHANNELS; i++)
        if (channel[i].wave == wave)
            channel[i].wave = NULL;
}

void dx_stop_all(void)
{
    memset(channel, 0, sizeof channel);
}

static int32_t sample(const wave_t *w, uint32_t frame, int c)
{
    if (frame >= w->frames)
        return 0;
    size_t i = (size_t)frame * w->channels + (c < w->channels ? c : 0);
    if (w->bits == 8)
        return (w->pcm[i] - 128) * 256;
    return (int16_t)(w->pcm[2 * i] | w->pcm[2 * i + 1] << 8);
}

void dx_mix(int16_t *out, size_t frames)
{
    int32_t mix[2 * 4096];
    while (frames) {
        size_t n = frames < 4096 ? frames : 4096;
        memset(mix, 0, sizeof mix);
        for (int k = 0; k < CHANNELS; k++) {
            const wave_t *w = channel[k].wave;
            for (size_t i = 0; w && i < n; i++) {
                uint32_t f = (uint32_t)(channel[k].pos >> 32);
                if (f >= w->frames) {
                    if (!channel[k].loop || !w->frames) {
                        channel[k].wave = w = NULL;
                        break;
                    }
                    channel[k].pos -= (uint64_t)w->frames << 32;
                    f -= w->frames;
                }
                int32_t t = (int32_t)((channel[k].pos >> 16) & 0xFFFF);
                uint32_t next = f + 1 < w->frames || !channel[k].loop ? f + 1 : 0;
                for (int c = 0; c < 2; c++) {
                    int32_t a = sample(w, f, c), b = sample(w, next, c);
                    mix[2 * i + c] += a + (((b - a) * t) >> 16);
                }
                channel[k].pos += channel[k].step;
            }
        }
        for (size_t i = 0; i < 2 * n; i++)
            out[i] = (int16_t)(mix[i] > 32767 ? 32767 : mix[i] < -32768 ? -32768 : mix[i]);
        out += 2 * n;
        frames -= n;
    }
}
