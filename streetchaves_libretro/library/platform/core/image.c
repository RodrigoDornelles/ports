#include <stdlib.h>
#include <string.h>

#include "stb_image.h"

#include "streetchaves/platform.h"

static uint32_t u32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

bool image_new(image_t *img, int w, int h)
{
    img->w = w;
    img->h = h;
    img->rgb = calloc((size_t)(w > 0 ? w : 1) * (h > 0 ? h : 1), 3);
    return img->rgb != NULL;
}

void image_free(image_t *img)
{
    free(img->rgb);
    memset(img, 0, sizeof *img);
}

bool image_bmp(const uint8_t *data, size_t size, image_t *out)
{
    int n;
    out->rgb = stbi_load_from_memory(data, (int)size, &out->w, &out->h, &n, 3);
    return out->rgb != NULL;
}

/* TPicture data in a DFM: the graphic's class name, then its own data;
 * TBitmap writes a size and a .bmp, DelphiX's TDIB a .bmp without its file
 * header, given back here so it loads alike */
bool image_picture(const uint8_t *data, size_t size, image_t *out)
{
    if (!size || (size_t)data[0] + 1 > size)
        return false;
    size_t name = data[0];
    const uint8_t *body = data + 1 + name;
    size_t len = size - 1 - name;
    if (name == 7 && !memcmp(data + 1, "TBitmap", 7) && len >= 4)
        return image_bmp(body + 4, len - 4, out);
    if (name != 4 || memcmp(data + 1, "TDIB", 4) || len < 40)
        return false;
    uint32_t header = u32(body), bits = body[14] | body[15] << 8, used = u32(body + 32);
    uint32_t colors = used ? used : bits <= 8 ? 1u << bits : 0;
    uint32_t offset = 14 + header + 4 * colors, file = (uint32_t)len + 14;
    uint8_t *bmp = malloc(file);
    memcpy(bmp, "BM", 2);
    memcpy(bmp + 2, &file, 4);
    memset(bmp + 6, 0, 4);
    memcpy(bmp + 10, &offset, 4);
    memcpy(bmp + 14, body, len);
    bool ok = image_bmp(bmp, file, out);
    free(bmp);
    return ok;
}
