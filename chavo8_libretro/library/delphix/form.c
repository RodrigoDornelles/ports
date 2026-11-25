#include <stdlib.h>
#include <string.h>

#include "chavo8/delphix.h"
#include "chavo8/viewer.h"

typedef struct {
    surface_t surface;
} picture_t;

typedef struct {
    uint8_t *data;
    wave_t wave;
    bool ok;
} sound_t;

static struct {
    image_t image[FORM_IMAGES];
    struct {
        picture_t *item;
        int count;
    } list[IMAGE_LISTS];
    struct {
        sound_t *item;
        int count;
    } waves[WAVE_LISTS];
    surface_t screen;
    uint32_t input[DX_DEVICES], states[DX_DEVICES];
} form;

static const char *list_name[IMAGE_LISTS] = {
    [LIST_RETRATO] = "DXImageListRetrato", [LIST_EFEITOS] = "DXImageListEfeitos",
    [LIST_CENARIO] = "DXImageListCenario", [LIST_ICONE] = "DXImageListIcone",
    [LIST_LUTADOR] = "DXImageListLutador", [LIST_LUTADOR_INVERT] = "DXImageListLutadorInvert",
    [LIST_ABERTURA] = "DXImageListAbertura", [LIST_FINAL] = "DXImageListFinal",
    [LIST_ANIMACAO] = "DXImageListAnimacao", [LIST_EFEITOS_INVERT] = "DXImageListEfeitosInvert",
};

static const char *waves_name[WAVE_LISTS] = {
    [WAVES_SND] = "DXWaveListSnd", [WAVES_MUSICA] = "DXWaveListMusica", [WAVES_VITORIA] = "DXWaveListVitoria",
};

static void from_dfm(void *ctx, const dfm_item_t *item)
{
    (void)ctx;
    for (int l = 0; l < IMAGE_LISTS; l++) {
        if (strcmp(item->component, list_name[l]))
            continue;
        int i = dx_images_add(l) - 1;
        image_t img;
        if (item->data.data && image_picture(item->data.data, item->data.size, &img)) {
            surface_from_image(&form.list[l].item[i].surface, &img, item->transparent, item->transparent_color);
            image_free(&img);
        }
        return;
    }
    for (int l = 0; l < WAVE_LISTS; l++) {
        if (strcmp(item->component, waves_name[l]))
            continue;
        int i = dx_waves_add(l) - 1;
        sound_t *s = &form.waves[l].item[i];
        s->ok = item->data.data && wave_parse(item->data.data, item->data.size, &s->wave);
        return;
    }
}

bool form_create(const uint8_t *exe, size_t size)
{
    form_destroy();
    surface_new(&form.screen, DX_WIDTH, DX_HEIGHT);
    blob_t dfm;
    return pe_resource(exe, size, 10, "TFORM1", &dfm) && dfm_items(dfm.data, dfm.size, from_dfm, NULL);
}

void form_destroy(void)
{
    surface_free(&form.screen);
    for (int i = 0; i < FORM_IMAGES; i++)
        image_free(&form.image[i]);
    for (int l = 0; l < IMAGE_LISTS; l++) {
        for (int i = 0; i < form.list[l].count; i++)
            surface_free(&form.list[l].item[i].surface);
        free(form.list[l].item);
    }
    for (int l = 0; l < WAVE_LISTS; l++) {
        for (int i = 0; i < form.waves[l].count; i++)
            free(form.waves[l].item[i].data);
        free(form.waves[l].item);
    }
    memset(&form, 0, sizeof form);
}

image_t *form_image(int image)
{
    return &form.image[image];
}

/* Picture.LoadFromFile: the picture is the file's, or stays as it was */
bool form_image_load(int image, const char *name)
{
    size_t size;
    uint8_t *data = vfs_load(name, &size);
    image_t img;
    bool ok = data && image_bmp(data, size, &img);
    free(data);
    if (!ok) {
        chavo8_log("cannot load the picture %s\n", name);
        return false;
    }
    image_free(&form.image[image]);
    form.image[image] = img;
    return true;
}

/* Picture.Bitmap.Width and Height: what was there stays at the top left */
void form_image_resize(int image, int w, int h)
{
    image_t *old = &form.image[image], img;
    if (w < 0)
        w = 0;
    if (h < 0)
        h = 0;
    image_new(&img, w, h);
    for (int y = 0; y < h && y < old->h; y++)
        memcpy(img.rgb + 3 * (size_t)y * w, old->rgb + 3 * (size_t)y * old->w, 3 * (size_t)(w < old->w ? w : old->w));
    image_free(old);
    *old = img;
}

int dx_images_add(int list)
{
    int n = form.list[list].count + 1;
    form.list[list].item = realloc(form.list[list].item, n * sizeof(picture_t));
    memset(&form.list[list].item[n - 1], 0, sizeof(picture_t));
    return form.list[list].count = n;
}

int dx_images_count(int list)
{
    return form.list[list].count;
}

/* Items[index].Picture := image's picture, Transparent, TransparentColor,
 * then Restore, which makes its 16-bit surface */
void dx_images_assign(int list, int index, int image, bool transparent, uint32_t color)
{
    if (index < 0 || index >= form.list[list].count)
        return;
    surface_t *s = &form.list[list].item[index].surface;
    surface_free(s);
    surface_from_image(s, &form.image[image], transparent, color);
}

const surface_t *dx_images_get(int list, int index)
{
    if (index < 0 || index >= form.list[list].count || !form.list[list].item[index].surface.px)
        return NULL;
    return &form.list[list].item[index].surface;
}

int dx_waves_add(int list)
{
    int n = form.waves[list].count + 1;
    form.waves[list].item = realloc(form.waves[list].item, n * sizeof(sound_t));
    memset(&form.waves[list].item[n - 1], 0, sizeof(sound_t));
    return form.waves[list].count = n;
}

int dx_waves_count(int list)
{
    return form.waves[list].count;
}

/* Items[index].Wave.LoadFromFile */
bool dx_waves_load(int list, int index, const char *name)
{
    if (index < 0 || index >= form.waves[list].count)
        return false;
    sound_t *s = &form.waves[list].item[index];
    size_t size;
    uint8_t *data = vfs_load(name, &size);
    wave_t wave;
    if (!data || !wave_parse(data, size, &wave)) {
        free(data);
        chavo8_log("cannot load the wave %s\n", name);
        return false;
    }
    free(s->data);
    s->data = data;
    s->wave = wave;
    return s->ok = true;
}

const wave_t *dx_waves_get(int list, int index)
{
    if (index < 0 || index >= form.waves[list].count || !form.waves[list].item[index].ok)
        return NULL;
    return &form.waves[list].item[index].wave;
}

surface_t *dx_screen(void)
{
    return &form.screen;
}

uint32_t form_image_pixel(int image, int x, int y)
{
    return canvas_pixel(&form.image[image], x, y);
}

void dx_images_draw(int list, int index, int x, int y)
{
    const surface_t *s = dx_images_get(list, index);
    if (s)
        dx_draw(&form.screen, x, y, s);
}

void dx_images_draw_alpha(int list, int index, int l, int t, int r, int b, int alpha)
{
    const surface_t *s = dx_images_get(list, index);
    if (s)
        dx_draw_alpha(&form.screen, l, t, r, b, s, alpha);
}

/* Items[index].Play: each call plays the wave once more, over the others */
void dx_waves_play(int list, int index)
{
    const wave_t *w = dx_waves_get(list, index);
    if (w)
        dx_play(w, false);
}

void dx_input_set(int device, uint32_t states)
{
    form.input[device] = states;
}

void dx_input_update(void)
{
    memcpy(form.states, form.input, sizeof form.states);
}

uint32_t dx_input_states(int device)
{
    return device < DX_DEVICES ? form.states[device] : form.states[DX_KEYBOARD] | form.states[DX_JOYSTICK];
}

/* Items[index].Picture.LoadFromFile, Transparent, TransparentColor, Restore */
bool dx_images_load(int list, int index, const char *name, bool transparent, uint32_t color)
{
    if (index < 0 || index >= form.list[list].count)
        return false;
    size_t size;
    uint8_t *data = vfs_load(name, &size);
    image_t img;
    bool ok = data && image_bmp(data, size, &img);
    free(data);
    if (!ok) {
        chavo8_log("cannot load the picture %s\n", name);
        return false;
    }
    surface_t *s = &form.list[list].item[index].surface;
    surface_free(s);
    surface_from_image(s, &img, transparent, color);
    image_free(&img);
    return true;
}
