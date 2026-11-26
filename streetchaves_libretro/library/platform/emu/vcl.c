#include <math.h>
#include <stdio.h>

#include "streetchaves/delphix.h"
#include "streetchaves/x86.h"

/* The form and its components as the lifted code reaches them: objects
 * whose fields hold the addresses of others, read the way Delphi read them
 * ([self+320h] the image list, [list+28h] its items...), down to handles
 * the routines below take back to platform/delphix/form.c.
 *   50000000  TForm1; its published fields 2D0h..340h, a component each
 *   50000400  the components, 400h apiece, in the fields' order
 *   50008000  the lists' collections, 100h apiece: images 0..9, waves 10..12
 *   50009000  the memos' Lines, 100h apiece
 *   5000A000  DXDraw1's surface, then its canvas, font, pen and brush
 *   50010000  the keyboard, 50014000 the joystick: their states at +2038h
 *   5F000000  the classes the code calls virtually, its methods
 *   58000000  the lists' items, 4000h a list, 40h an item; an item's
 *             Picture (+20h) is itself+30h, its own bitmap */

#define OBJECTS 0x50000000u
#define FORM OBJECTS
#define COMPONENT(f) (OBJECTS + 0x400u + ((f) - 0x2d0u) / 4u * 0x400u)
#define COLLECTION(l) (OBJECTS + 0x8000u + (l) * 0x100u)
#define LINES(m) (OBJECTS + 0x9000u + (m) * 0x100u)
#define SURFACE (OBJECTS + 0xa000u)
#define CANVAS (OBJECTS + 0xa100u)
#define FONT (OBJECTS + 0xa200u)
#define PEN (OBJECTS + 0xa300u)
#define BRUSH (OBJECTS + 0xa400u)
#define KEYBOARD (OBJECTS + 0x10000u)
#define JOYSTICK (OBJECTS + 0x14000u)
#define CLASSES 0x5f000000u
#define ITEMS 0x58000000u
#define ITEM(l, i) (ITEMS + (l) * 0x4000u + (i) * 0x40u)
#define WAVE_LIST(w) (IMAGE_LISTS + (w))
#define LISTS (IMAGE_LISTS + WAVE_LISTS)

/* the classes' methods: addresses no code has, to know them by */
enum { STRINGS_GET = 0x7f000001, STRINGS_COUNT, GRAPHIC_EMPTY };
#define VMT_STRINGS CLASSES
#define VMT_GRAPHIC (CLASSES + 0x100u)

static uint8_t objects[0x18000], classes[0x200], items[LISTS * 0x4000];
static struct {
    uint32_t pen, brush;
    bool hollow;
    int font_size, language;
} canvas = { 0, 0xffffff, false, 8, 1 };

static const struct {
    uint32_t field;
    int list;
} image_fields[] = {
    { 0x304, LIST_RETRATO }, { 0x308, LIST_EFEITOS }, { 0x310, LIST_CENARIO }, { 0x314, LIST_ICONE },
    { 0x320, LIST_LUTADOR }, { 0x324, LIST_LUTADOR_INVERT }, { 0x328, LIST_ABERTURA }, { 0x330, LIST_FINAL },
    { 0x338, LIST_ANIMACAO }, { 0x33c, LIST_EFEITOS_INVERT },
    { 0x30c, WAVE_LIST(WAVES_SND) }, { 0x318, WAVE_LIST(WAVES_MUSICA) }, { 0x31c, WAVE_LIST(WAVES_VITORIA) },
};

static const struct {
    uint32_t field;
    int memo;
} memo_fields[] = {
    { 0x2f0, MEMO_NOMES }, { 0x2f4, MEMO_TEXTO }, { 0x2f8, MEMO_CONFIG }, { 0x2fc, MEMO_FRASES },
    { 0x300, MEMO_DIALOGO }, { 0x32c, MEMO_FINAIS }, { 0x334, MEMO_CENARIOS },
};

static void put(uint32_t at, uint32_t v)
{
    wr32(at, v);
}

void vcl_create(void)
{
    x86_map(OBJECTS, objects, sizeof objects);
    x86_map(CLASSES, classes, sizeof classes);
    x86_map(ITEMS, items, sizeof items);
    for (uint32_t f = 0x2d0; f <= 0x340; f += 4)
        put(FORM + f, COMPONENT(f));
    for (unsigned i = 0; i < sizeof image_fields / sizeof *image_fields; i++)
        put(COMPONENT(image_fields[i].field) + 0x28, COLLECTION(image_fields[i].list));
    for (unsigned i = 0; i < sizeof memo_fields / sizeof *memo_fields; i++) {
        put(COMPONENT(memo_fields[i].field) + 0x208, LINES(memo_fields[i].memo));
        put(LINES(memo_fields[i].memo), VMT_STRINGS);
    }
    put(COMPONENT(0x2d0) + 0x270, SURFACE);
    put(COMPONENT(0x2d8) + 0x34, JOYSTICK);
    put(COMPONENT(0x2d8) + 0x38, KEYBOARD);
    put(CANVAS + 0xc, FONT);
    put(CANVAS + 0x10, PEN);
    put(CANVAS + 0x14, BRUSH);
    put(VMT_STRINGS + 0xc, STRINGS_GET);
    put(VMT_STRINGS + 0x14, STRINGS_COUNT);
    put(VMT_GRAPHIC + 0x1c, GRAPHIC_EMPTY);
    for (uint32_t l = 0; l < LISTS; l++)
        for (uint32_t i = 0; i < 256; i++) {
            put(ITEM(l, i) + 0x20, ITEM(l, i) + 0x30);
            put(ITEM(l, i) + 0x30, VMT_GRAPHIC);
        }
}

/* the states DXInput1.Update read: each device's at +2038h, both at +45h */
void vcl_input(void)
{
    uint32_t k = dx_input_states(DX_KEYBOARD), j = dx_input_states(DX_JOYSTICK), b = k | j;
    put(KEYBOARD + 0x2038, k);
    put(JOYSTICK + 0x2038, j);
    for (int i = 0; i < 4; i++)
        wr8(COMPONENT(0x2d8) + 0x45 + (uint32_t)i, b >> (8 * i));
}

void vcl_language(int language)
{
    canvas.language = language;
}

static int list_of(uint32_t collection)
{
    return (int)((collection - COLLECTION(0)) / 0x100u);
}

static void item_of(uint32_t item, int *list, int *index)
{
    *list = (int)((item - ITEMS) / 0x4000u);
    *index = (int)((item - ITEMS) % 0x4000u / 0x40u);
}

static uint32_t arg(int i) { return rd32(x86.r[X86_ESP] + 4u * (uint32_t)i); }
static void drop(uint32_t bytes) { x86.r[X86_ESP] += bytes; }

static void rect_at(uint32_t at, int *l, int *t, int *r, int *b)
{
    *l = (int32_t)rd32(at);
    *t = (int32_t)rd32(at + 4);
    *r = (int32_t)rd32(at + 8);
    *b = (int32_t)rd32(at + 12);
}

bool vcl_call(uint32_t target)
{
    uint32_t *r = x86.r;
    int list, index, L, T, R, B;
    switch (target) {
    case 0x461dcc: /* TPictureCollection.Items[i] */
    case 0x449f54: /* TWaveCollection.Items[i] */
        r[X86_EAX] = ITEM((uint32_t)list_of(r[X86_EAX]), r[X86_EDX] & 0xff);
        return true;
    case 0x40efcc: { /* TCollection.Count */
        int l = list_of(r[X86_EAX]);
        r[X86_EAX] = (uint32_t)(l < IMAGE_LISTS ? dx_images_count(l) : dx_waves_count(l - IMAGE_LISTS));
        return true;
    }
    case 0x46161c: /* Item.Draw(Dest, X, Y, PatternIndex) */
        item_of(r[X86_EAX], &list, &index);
        dx_images_draw(list, index, (int32_t)r[X86_ECX], (int32_t)arg(1));
        drop(8);
        return true;
    case 0x461670: /* Item.StretchDraw(Dest, Rect, PatternIndex) */
        item_of(r[X86_EAX], &list, &index);
        rect_at(r[X86_ECX], &L, &T, &R, &B);
        dx_images_stretch(list, index, L, T, R, B);
        drop(4);
        return true;
    case 0x4616c0: /* Item.DrawAlpha(Dest, Rect, PatternIndex, Alpha) */
        item_of(r[X86_EAX], &list, &index);
        rect_at(r[X86_ECX], &L, &T, &R, &B);
        dx_images_draw_alpha(list, index, L, T, R, B, (int32_t)arg(0));
        drop(8);
        return true;
    case 0x461714: { /* Item.DrawRotate(Dest, X, Y, Width, Height, PatternIndex, CenterX, CenterY, Angle) */
        item_of(r[X86_EAX], &list, &index);
        double cx = f64(rd64(r[X86_ESP] + 12)), cy = f64(rd64(r[X86_ESP] + 4));
        dx_images_rotate(list, index, (int32_t)r[X86_ECX], (int32_t)arg(8), (int32_t)arg(7), (int32_t)arg(6),
            cx, cy, (int32_t)arg(0));
        drop(0x24);
        return true;
    }
    case 0x461608: /* Item.Width */
    case 0x4615e0: { /* Item.Height */
        item_of(r[X86_EAX], &list, &index);
        const surface_t *s = dx_images_get(list, index);
        r[X86_EAX] = s ? (uint32_t)(target == 0x461608 ? s->w : s->h) : 0;
        return true;
    }
    case 0x41a9fc: /* Picture.Bitmap: the item's picture stands for it */
        return true;
    case GRAPHIC_EMPTY: {
        item_of(r[X86_EAX] - 0x30, &list, &index);
        r[X86_EAX] = (r[X86_EAX] & ~0xffu) | (dx_images_get(list, index) ? 0u : 1u);
        return true;
    }
    case 0x449cac: /* Item.Play(Wait) */
        item_of(r[X86_EAX], &list, &index);
        dx_waves_play(list - IMAGE_LISTS, index);
        return true;
    case 0x449e68: /* Item.Stop */
        item_of(r[X86_EAX], &list, &index);
        dx_waves_stop(list - IMAGE_LISTS, index);
        return true;
    case STRINGS_GET: { /* Lines[i] */
        int memo = (int)((r[X86_EAX] - LINES(0)) / 0x100u);
        const char *line = texts_line(canvas.language, memo, r[X86_EDX]);
        wr32(r[X86_ECX], x86_string_new(line, (uint32_t)strlen(line)));
        return true;
    }
    case STRINGS_COUNT:
        r[X86_EAX] = (uint32_t)texts_count(canvas.language, (int)((r[X86_EAX] - LINES(0)) / 0x100u));
        return true;
    case 0x44dee8: /* DXInput1.Update */
        dx_input_update();
        vcl_input();
        return true;
    case 0x45cd9c: /* Surface.Canvas */
        r[X86_EAX] = CANVAS;
        return true;
    case 0x45bb18: /* Canvas.Release */
        return true;
    case 0x4181bc: /* Brush.Color */
        canvas.brush = r[X86_EDX];
        return true;
    case 0x4182d8: /* Brush.Style: bsClear is 1 */
        canvas.hollow = (r[X86_EDX] & 0xff) == 1;
        return true;
    case 0x417ed8: /* Pen.Color */
        canvas.pen = r[X86_EDX];
        return true;
    case 0x417c8c: /* Font.Size */
        canvas.font_size = (int)(r[X86_EDX] & 0xff);
        return true;
    case 0x418560: /* Canvas.Ellipse(X1, Y1, X2, Y2) */
        canvas_ellipse(dx_screen(), (int32_t)r[X86_EDX], (int32_t)r[X86_ECX], (int32_t)arg(1), (int32_t)arg(0),
            canvas.pen, canvas.brush, canvas.hollow);
        drop(8);
        return true;
    case 0x41887c: { /* Canvas.TextWidth */
        uint32_t n;
        const char *s = x86_string(r[X86_EDX], &n);
        char buf[512];
        snprintf(buf, sizeof buf, "%.*s", (int)n, s);
        r[X86_EAX] = (uint32_t)canvas_text_width(canvas.font_size, buf);
        return true;
    }
    }
    return false;
}
