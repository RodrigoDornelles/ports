#include <string.h>

#include "streetchaves/platform.h"

static uint16_t u16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t u32(const uint8_t *p) { return u16(p) | (uint32_t)u16(p + 2) << 16; }

bool wave_parse(const uint8_t *data, size_t size, wave_t *out)
{
    if (size < 12 || memcmp(data, "RIFF", 4) || memcmp(data + 8, "WAVE", 4))
        return false;
    memset(out, 0, sizeof *out);
    const uint8_t *p = data + 12, *end = data + size;
    while (end - p >= 8) {
        uint32_t len = u32(p + 4);
        const uint8_t *body = p + 8;
        if (len > (size_t)(end - body))
            len = (uint32_t)(end - body);
        if (!memcmp(p, "fmt ", 4) && len >= 16) {
            if (u16(body) != 1)
                return false;
            out->channels = (uint8_t)u16(body + 2);
            out->rate = u32(body + 4);
            out->bits = (uint8_t)u16(body + 14);
        } else if (!memcmp(p, "data", 4)) {
            out->pcm = body;
            out->frames = len;
        }
        p = body + len + (len & 1);
    }
    if (!out->pcm || !out->rate || !out->channels || (out->bits != 8 && out->bits != 16))
        return false;
    out->frames /= out->channels * (out->bits / 8u);
    return true;
}
