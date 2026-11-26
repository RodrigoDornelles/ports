#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libretro.h"

#include "streetchaves/game.h"
#include "streetchaves/viewer.h"

enum { LUTADORES, CENARIOS, ROSTOS, EFEITOS, ANIMACOES, EXE_IMAGES, SONS, FALAS, MUSICAS, EXE_WAVES, TEXTOS, ESTADO, FONTE, CATEGORIES };

static const struct {
    const char *title, *dir, *ext;
} category[CATEGORIES] = {
    [LUTADORES] = { "Lutadores", "Lutadores", ".bmp" },
    [CENARIOS] = { "Cenarios", "Cenarios", ".bmp" },
    [ROSTOS] = { "Rostos", "Rostos", ".bmp" },
    [EFEITOS] = { "Efeitos", "Efeitos", ".bmp" },
    [ANIMACOES] = { "Animacoes", "Animacoes", ".bmp" },
    [EXE_IMAGES] = { "Imagens do exe", NULL, NULL },
    [SONS] = { "Sons", "Sons", ".wav" },
    [FALAS] = { "Falas", "Falas", ".wav" },
    [MUSICAS] = { "Musicas", "Musicas", ".wav" },
    [EXE_WAVES] = { "Sons do exe", NULL, NULL },
    [TEXTOS] = { "Textos do exe", NULL, NULL },
    [ESTADO] = { "Estado do v1", NULL, NULL },
    [FONTE] = { "Fonte", NULL, NULL },
};

#define MAX_ITEMS 512

typedef struct {
    char *label;
    dfm_item_t item;
} exe_item_t;

static struct {
    const char *files[CATEGORIES][MAX_ITEMS];
    exe_item_t exe_items[2][MAX_ITEMS];
    size_t count[CATEGORIES];
    size_t index[CATEGORIES];
    int cat;
    uint16_t held;

    surface_t *frames, *inverts;
    int nframes, frame, scroll, tick;
    bool mirror, animate, raw;
    uint8_t *buffer;
    wave_t wave;
    bool has_wave;
    char info[640];
} v;

static void collect(void *ctx, const dfm_item_t *item)
{
    (void)ctx;
    if (!item->data.data)
        return;
    bool wave = item->data.size >= 4 && !memcmp(item->data.data, "RIFF", 4);
    int c = wave ? EXE_WAVES : EXE_IMAGES;
    if (v.count[c] == MAX_ITEMS)
        return;
    exe_item_t *e = &v.exe_items[wave][v.count[c]++];
    char label[300];
    snprintf(label, sizeof label, "%s: %s", item->component, item->name);
    e->label = strdup(label);
    e->item = *item;
    e->item.component = e->item.name = NULL;
}

static bool ends_with(const char *s, const char *end)
{
    size_t n = strlen(s), m = strlen(end);
    return n >= m && !strcasecmp(s + n - m, end);
}

static void load(void);

bool viewer_init(const uint8_t *exe, size_t size)
{
    memset(&v, 0, sizeof v);
    blob_t form;
    if (!pe_resource(exe, size, 10, "TFORM1", &form) || !dfm_items(form.data, form.size, collect, NULL))
        streetchaves_log("no form in the exe: its pictures and waves are missing\n");
    for (int c = 0; c < CATEGORIES; c++) {
        if (!category[c].dir)
            continue;
        const char *names[MAX_ITEMS];
        size_t n = vfs_list(category[c].dir, names, MAX_ITEMS);
        for (size_t i = 0; i < n; i++)
            if (ends_with(names[i], category[c].ext))
                v.files[c][v.count[c]++] = names[i];
    }
    v.count[TEXTOS] = 2 * MEMOS;
    v.count[ESTADO] = 1;
    v.count[FONTE] = 1;
    load();
    return true;
}

static void unload(void)
{
    dx_stop_all();
    for (int i = 0; i < v.nframes; i++) {
        surface_free(&v.frames[i]);
        surface_free(&v.inverts[i]);
    }
    free(v.frames);
    free(v.inverts);
    free(v.buffer);
    v.frames = v.inverts = NULL;
    v.buffer = NULL;
    v.nframes = v.frame = v.scroll = 0;
    v.has_wave = v.animate = false;
    v.info[0] = 0;
}

static bool dark(uint32_t color)
{
    return (color & 0xFF) <= 100 && (color >> 8 & 0xFF) <= 100 && (color >> 16 & 0xFF) <= 100;
}

/* carregarlutador: a column dark at the top and bottom rows ends a frame;
 * copiabmp takes up to the column before it and all rows but the last,
 * then the mirrored copy for the other side */
static void slice(const image_t *strip)
{
    int start = 0;
    for (int x = 0; x <= strip->w; x++) {
        if (!dark(canvas_pixel(strip, x, 0)) || !dark(canvas_pixel(strip, x, strip->h - 1)))
            continue;
        int x1 = start, x2 = x - 1, y1 = 0, y2 = strip->h - 1;
        int w = x2 - x1, h = y2 - y1;
        v.frames = realloc(v.frames, (v.nframes + 1) * sizeof *v.frames);
        v.inverts = realloc(v.inverts, (v.nframes + 1) * sizeof *v.inverts);
        surface_t *normal = &v.frames[v.nframes], *invert = &v.inverts[v.nframes];
        v.nframes++;
        start = x + 1;
        memset(normal, 0, sizeof *normal);
        memset(invert, 0, sizeof *invert);
        if (w <= 0 || h <= 0)
            continue;
        image_t frame;
        image_new(&frame, w, h);
        canvas_copy_rect(&frame, 0, 0, w, h, strip, x1, y1, x2, y2);
        surface_from_image(normal, &frame, true, 0xFFFFFF);
        canvas_copy_rect(&frame, 0, 0, w, h, strip, x2 - 1, y1, x1, y2);
        surface_from_image(invert, &frame, true, 0xFFFFFF);
        image_free(&frame);
    }
}

static void single(const image_t *img, bool transparent, uint32_t color)
{
    v.frames = calloc(1, sizeof *v.frames);
    v.inverts = calloc(1, sizeof *v.inverts);
    v.nframes = 1;
    surface_from_image(v.frames, img, transparent, color);
}

static void load(void)
{
    unload();
    int c = v.cat;
    if (!v.count[c])
        return;
    size_t i = v.index[c], size = 0;
    image_t img = { 0 };
    if (c == EXE_IMAGES) {
        const dfm_item_t *e = &v.exe_items[0][i].item;
        if (image_picture(e->data.data, e->data.size, &img))
            single(&img, e->transparent, e->transparent_color);
    } else if (c == EXE_WAVES) {
        const dfm_item_t *e = &v.exe_items[1][i].item;
        v.has_wave = wave_parse(e->data.data, e->data.size, &v.wave);
    } else if (category[c].dir) {
        char path[512];
        snprintf(path, sizeof path, "%s/%s", category[c].dir, v.files[c][i]);
        v.buffer = vfs_load(path, &size);
        if (!v.buffer)
            snprintf(v.info, sizeof v.info, "falhou ao ler %s", path);
        else if (!strcmp(category[c].ext, ".wav"))
            v.has_wave = wave_parse(v.buffer, size, &v.wave);
        else if (image_bmp(v.buffer, size, &img)) {
            if (c == LUTADORES)
                slice(&img);
            else
                single(&img, c != CENARIOS, 0xFFFFFF);
        }
    }
    if (img.rgb)
        snprintf(v.info, sizeof v.info, "%dx%d", img.w, img.h);
    if (v.has_wave)
        snprintf(v.info, sizeof v.info, "%u Hz, %u bits, %s, %.2fs  (A toca, B para)", v.wave.rate, v.wave.bits,
            v.wave.channels == 1 ? "mono" : "stereo", (double)v.wave.frames / v.wave.rate);
    image_free(&img);
}

/* texto: centered on x for 'c' (Round(width / 2), half to even), ending at
 * x for 'e', with a shadow */
static void label(surface_t *s, int x, int y, uint32_t color, char align, int size, const char *text)
{
    int w = canvas_text_width(size, text);
    int off = align == 'c' ? (w >> 1) + (w & (w >> 1) & 1) : align == 'e' ? w : 0;
    canvas_text_out(s, x - off + 1, y + 1, 0x000000, size, text);
    canvas_text_out(s, x - off, y, color, size, text);
}

static void checkers(surface_t *s)
{
    for (int y = 0; y < s->h; y++)
        for (int x = 0; x < s->w; x++)
            s->px[(size_t)y * s->w + x] = ((x >> 3) ^ (y >> 3)) & 1 ? dx_565(0x404040) : dx_565(0x303030);
}

static const char *memo_name[MEMOS] = {
    "MemoNomes", "MemoTexto", "MemoConfig", "MemoFrases", "MemoDialogo", "MemoFinais", "MemoCenarios",
};

/* through game/v1 (streetchaves_v1_line), as the game will read them */
static void draw_texts(surface_t *s)
{
    int idioma = v.index[TEXTOS] < MEMOS ? 1 : 2, memo = (int)(v.index[TEXTOS] % MEMOS);
    size_t count = texts_count(idioma, memo);
    if (v.scroll >= (int)count)
        v.scroll = count ? (int)count - 1 : 0;
    for (int row = 0, y = 24; row < 15 && v.scroll + row < (int)count; row++, y += 15) {
        char text[300];
        snprintf(text, sizeof text, "%3d  %s", v.scroll + row, streetchaves_v1_line(idioma, memo, v.scroll + row));
        label(s, 8, y, 0xFFFFFF, '-', 8, text);
    }
    snprintf(v.info, sizeof v.info, "%s, %zu linhas  (cima/baixo rola)", idioma == 1 ? "portugues" : "espanhol", count);
}

static void draw_state(surface_t *s)
{
    char text[2048];
    snprintf(text, sizeof text, "%s", streetchaves_v1_state());
    int y = 24;
    for (char *line = strtok(text, "\n"); line && y < s->h - 40; line = strtok(NULL, "\n"), y += 13)
        label(s, 6, y, 0xFFFFFF, '-', 7, line);
}

static void draw_font(surface_t *s)
{
    static const int sizes[] = { 7, 8, 9, 10, 11, 12, 15, 20, 25 };
    int y = 24;
    for (size_t i = 0; i < sizeof sizes / sizeof *sizes; i++) {
        char text[128];
        snprintf(text, sizeof text, "%d: %s - Configura\xE7\xF5\x65s, Portugu\xEAs", sizes[i], streetchaves_game_name(1));
        label(s, 8, y, 0x00FFFF, '-', sizes[i], text);
        y += (sizes[i] * 96 + 36) / 72 + 6;
    }
}

void viewer_run(uint16_t buttons, surface_t *screen)
{
    uint16_t press = buttons & ~v.held;
    v.held = buttons;
#define PRESSED(id) (press & (1 << RETRO_DEVICE_ID_JOYPAD_##id))
    if (PRESSED(L) || PRESSED(R)) {
        v.cat = (v.cat + (PRESSED(R) ? 1 : CATEGORIES - 1)) % CATEGORIES;
        load();
    }
    int c = v.cat;
    if ((PRESSED(LEFT) || PRESSED(RIGHT)) && v.count[c]) {
        v.index[c] = (v.index[c] + (PRESSED(RIGHT) ? 1 : v.count[c] - 1)) % v.count[c];
        load();
    }
    if (c == TEXTOS) {
        if (PRESSED(UP))
            v.scroll -= v.scroll > 0;
        if (PRESSED(DOWN))
            v.scroll++;
    } else if (c == CENARIOS && v.nframes) {
        if (buttons & (1 << RETRO_DEVICE_ID_JOYPAD_UP))
            v.scroll -= 4;
        if (buttons & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN))
            v.scroll += 4;
        int max = v.frames[0].w - screen->w;
        v.scroll = v.scroll < 0 ? 0 : v.scroll > max ? (max > 0 ? max : 0) : v.scroll;
    } else if (v.nframes > 1) {
        if (PRESSED(UP))
            v.frame = (v.frame + v.nframes - 1) % v.nframes;
        if (PRESSED(DOWN))
            v.frame = (v.frame + 1) % v.nframes;
    }
    if (PRESSED(A)) {
        if (v.has_wave)
            dx_play(&v.wave, c == MUSICAS);
        else
            v.mirror = !v.mirror;
    }
    if (PRESSED(B))
        dx_stop_all();
    if (PRESSED(X))
        v.raw = !v.raw;
    if (PRESSED(START))
        v.animate = !v.animate;
    if (v.animate && v.nframes > 1 && ++v.tick % 4 == 0)
        v.frame = (v.frame + 1) % v.nframes;

    checkers(screen);
    if (c == TEXTOS) {
        draw_texts(screen);
    } else if (c == ESTADO) {
        draw_state(screen);
    } else if (c == FONTE) {
        draw_font(screen);
    } else if (v.nframes) {
        surface_t *s = (v.mirror && v.inverts[v.frame].px ? v.inverts : v.frames) + v.frame;
        bool transparent = s->transparent;
        s->transparent = transparent && !v.raw;
        if (c == CENARIOS)
            dx_draw(screen, -v.scroll, (screen->h - s->h) / 2, s);
        else
            dx_draw(screen, (screen->w - s->w) / 2, 26 + (screen->h - 52 - s->h) / 2, s);
        s->transparent = transparent;
    }

    char head[512];
    const char *name = !v.count[c] ? "(vazio)"
        : c == EXE_IMAGES || c == EXE_WAVES ? v.exe_items[c == EXE_WAVES][v.index[c]].label
        : c == TEXTOS ? memo_name[v.index[c] % MEMOS]
        : c == ESTADO ? "depois do FormCreate"
        : category[c].dir ? v.files[c][v.index[c]] : "Comic Relief";
    snprintf(head, sizeof head, "%s  %zu/%zu  %s", category[c].title, v.count[c] ? v.index[c] + 1 : 0, v.count[c], name);
    label(screen, 6, 4, 0xFFFFFF, '-', 9, head);
    char foot[sizeof v.info];
    if (c == LUTADORES && v.nframes)
        snprintf(foot, sizeof foot, "frame %d/%d  %dx%d%s  (cima/baixo, A espelha, START anima)", v.frame + 1, v.nframes,
            v.frames[v.frame].w, v.frames[v.frame].h, v.mirror ? "  espelhado" : "");
    else
        snprintf(foot, sizeof foot, "%s", v.info);
    label(screen, 6, screen->h - 36, 0x00FFFF, '-', 9, foot);
    label(screen, 6, screen->h - 18, 0x808080, '-', 8, "L/R pasta  esq/dir item  X sem chave");
}

void viewer_deinit(void)
{
    unload();
    for (int w = 0; w < 2; w++)
        for (size_t i = 0; i < v.count[w ? EXE_WAVES : EXE_IMAGES]; i++)
            free(v.exe_items[w][i].label);
    memset(&v, 0, sizeof v);
}
