/**
 * @file gl.c
 * @brief prboom-render = gl: the software renderer decides, the GPU
 * paints (plan_gpu.txt, render_gl.h).
 *
 * Wall and sky columns: the replay of the wall records (r_drawcmd.c)
 * hands each column here instead of filling its pixels. A column is a one
 * pixel wide quad whose texture coordinate runs linearly down the column,
 * the same frac the column drawers step, so the GPU only interpolates.
 * Textures are 8-bit indexed, uploaded once from the composite (column
 * major, so a texture column is a GL row and the upload needs no
 * transpose); the shader looks the texel up in a table of COLORMAP rows
 * already turned into colours by the current palette.
 *
 * Rules from scripts/glbench.c on the Mali-400 (no highp in the fragment
 * shader): the texture coordinate goes straight from the varying into
 * texture2D, every sum is done on the CPU or in the vertex shader;
 * textures are NEAREST and power of two with REPEAT, so a wall that
 * repeats vertically wraps by itself (a texture whose height is not a
 * power of two is cut where it repeats instead); quads are grouped per
 * texture, walls never overlap so the order inside a group is free.
 *
 * The rest of the frame is still drawn by the CPU into the software
 * frame, uploaded every frame and blended over the columns: where a
 * column was queued the CPU left a key colour, which lets the column show
 * through (see gl_key_pick and shaders/frag_frame.glsl).
 */
#define _GNU_SOURCE
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifndef __EMSCRIPTEN__
#include <dlfcn.h>
#endif

#include "doomdef.h"
#include "doomstat.h"
#include "lprintf.h"
#include "r_data.h"
#include "r_draw.h"
#include "r_drawcmd.h"
#include "r_main.h"
#include "r_patch.h"
#include "r_things.h"
#include "r_plane.h"
#include "v_video.h"
#include "vid_mode.h"
#include "w_wad.h"
#include "prboomtv/render_gl.h"
#include "prboomtv/scale2x.h"
#include "prboomtv/glow.h"
#include "prboomtv/dynlights.h"
#include "prboomtv/shaders.h"
#include "prboomtv/widescreen.h"

/* r_data.c, killough's marker lumps of the extra colormaps */
extern int firstcolormaplump;

dbool dopo_gl = FALSE;

const struct retro_core_option_v2_definition dopo_gl_spectre_option =
{
   "prboom-spectre",
   "Spectre Look (gl only)",
   NULL,
   "How the GPU draws the Spectre and other fuzzy things, as strong as "
   "Spectre Strength says; only with the gl renderer (software keeps the "
   "original fuzz). 'darken' darkens what is behind it, closest to the "
   "original without its shimmer; 'subtract' takes the demon's colours "
   "away from what is behind, a dark negative like the Nightmare Spectre "
   "of the PlayStation and Doom 64; 'multiply' tints what is behind with "
   "the demon's colours, like stained glass; 'translucent' shows the demon "
   "faintly over it, like PlayStation Doom.",
   NULL,
   NULL,
   {
      { "darken",      "Darken" },
      { "subtract",    "Subtract (negative)" },
      { "multiply",    "Multiply (stained glass)" },
      { "translucent", "Translucent" },
      { NULL, NULL },
   },
   "darken"
};

enum
{
   GL_SPECTRE_DARKEN, GL_SPECTRE_SUBTRACT, GL_SPECTRE_MULTIPLY,
   GL_SPECTRE_TRANSLUCENT
};
static int   gl_spectre = GL_SPECTRE_DARKEN;
static float gl_spectre_opacity = 0.3f;

/* each look at the strength it reads best at (prboom-spectre-opacity auto) */
static const float gl_spectre_auto[] = { 0.3f, 0.2f, 0.5f, 0.2f };

const struct retro_core_option_v2_definition dopo_gl_spectre_opacity_option =
{
   "prboom-spectre-opacity",
   "Spectre Strength (gl only)",
   NULL,
   "How strong the Spectre Look is, from barely there to its full effect; "
   "only with the gl renderer. 'auto' takes each look at the strength it "
   "reads best at: darken 30%, subtract 20%, multiply 50%, translucent 20%.",
   NULL,
   NULL,
   {
      { "auto", "Auto" },
      { "10", "10%" }, { "20", "20%" }, { "30", "30%" }, { "40", "40%" },
      { "50", "50%" }, { "60", "60%" }, { "70", "70%" }, { "80", "80%" },
      { "90", "90%" }, { "100", "100%" },
      { NULL, NULL },
   },
   "auto"
};

/* the prboom-scale (scale2x.c) the patch textures were made with */
static int   patchcache_scale = 0;
static int   gl2d_patches_scale = 0;
int R_DrawCmdTexture = 0;
const seg_t *R_DrawCmdSeg = NULL;
int R_SpanTexture = 0;

const struct retro_core_option_v2_definition dopo_gl_option =
{
   "prboom-render",
   "Renderer (Restart Required)",
   NULL,
   "Who paints the 3D view. 'gl' keeps the CPU deciding what is seen and "
   "hands the painting to the GPU through OpenGL ES 2, for high resolutions "
   "on weak CPUs, and does not start without it. 'software' fills every "
   "pixel on the CPU, the classic renderer. 'auto' uses gl when the "
   "frontend gives an OpenGL context, software otherwise.",
   NULL,
   NULL,
   {
      { "auto",     NULL },
      { "gl",       NULL },
      { "software", NULL },
      { NULL, NULL },
   },
   "auto"
};

/* ---- the GLES 2 subset used, loaded at context reset ---- */
typedef unsigned int  GLenum;
typedef unsigned int  GLuint;
typedef int           GLint;
typedef int           GLsizei;
typedef float         GLfloat;
typedef unsigned char GLboolean;
typedef char          GLchar;
typedef ptrdiff_t     GLsizeiptr;
typedef unsigned char GLubyte;
typedef unsigned int  GLbitfield;

#define GL_TRIANGLES              0x0004
#define GL_TRIANGLE_STRIP         0x0005
#define GL_UNSIGNED_BYTE          0x1401
#define GL_SHORT                  0x1402
#define GL_UNSIGNED_SHORT         0x1403
#define GL_FLOAT                  0x1406
#define GL_RGB                    0x1907
#define GL_LUMINANCE              0x1909
#define GL_LUMINANCE_ALPHA        0x190A
#define GL_UNSIGNED_SHORT_5_6_5   0x8363
#define GL_TEXTURE_2D             0x0DE1
#define GL_TEXTURE0               0x84C0
#define GL_TEXTURE_MIN_FILTER     0x2801
#define GL_TEXTURE_MAG_FILTER     0x2800
#define GL_TEXTURE_WRAP_S         0x2802
#define GL_TEXTURE_WRAP_T         0x2803
#define GL_NEAREST                0x2600
#define GL_LINEAR                 0x2601
#define GL_REPEAT                 0x2901
#define GL_CLAMP_TO_EDGE          0x812F
#define GL_ARRAY_BUFFER           0x8892
#define GL_ELEMENT_ARRAY_BUFFER   0x8893
#define GL_STREAM_DRAW            0x88E0
#define GL_STATIC_DRAW            0x88E4
#define GL_FRAGMENT_SHADER        0x8B30
#define GL_VERTEX_SHADER          0x8B31
#define GL_COMPILE_STATUS         0x8B81
#define GL_LINK_STATUS            0x8B82
#define GL_FRAMEBUFFER            0x8D40
#define GL_COLOR_BUFFER_BIT       0x00004000
#define GL_DEPTH_TEST             0x0B71
#define GL_STENCIL_TEST           0x0B90
#define GL_SCISSOR_TEST           0x0C11
#define GL_BLEND                  0x0BE2
#define GL_ZERO                   0
#define GL_ONE                    1
#define GL_SRC_COLOR              0x0300
#define GL_SRC_ALPHA              0x0302
#define GL_ONE_MINUS_SRC_ALPHA    0x0303
#define GL_DST_COLOR              0x0306
#define GL_FUNC_ADD               0x8006
#define GL_FUNC_REVERSE_SUBTRACT  0x800B
#define GL_CULL_FACE              0x0B44
#define GL_DITHER                 0x0BD0
#define GL_UNPACK_ALIGNMENT       0x0CF5
#define GL_RENDERER               0x1F01
#define GL_VERSION                0x1F02
#define GL_MAX_TEXTURE_SIZE       0x0D33

#define GLFN(ret, name, args) static ret (*name) args
GLFN(void, glViewport, (GLint, GLint, GLsizei, GLsizei));
GLFN(void, glClearColor, (GLfloat, GLfloat, GLfloat, GLfloat));
GLFN(void, glClear, (GLbitfield));
GLFN(void, glDisable, (GLenum));
GLFN(void, glEnable, (GLenum));
GLFN(void, glBlendFunc, (GLenum, GLenum));
GLFN(void, glBlendEquation, (GLenum));
GLFN(void, glUniform4f, (GLint, GLfloat, GLfloat, GLfloat, GLfloat));
GLFN(void, glColorMask, (GLboolean, GLboolean, GLboolean, GLboolean));
GLFN(void, glBindFramebuffer, (GLenum, GLuint));
GLFN(void, glGenTextures, (GLsizei, GLuint *));
GLFN(void, glDeleteTextures, (GLsizei, const GLuint *));
GLFN(void, glBindTexture, (GLenum, GLuint));
GLFN(void, glActiveTexture, (GLenum));
GLFN(void, glTexParameteri, (GLenum, GLenum, GLint));
GLFN(void, glTexImage2D, (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *));
GLFN(void, glTexSubImage2D, (GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void *));
GLFN(void, glPixelStorei, (GLenum, GLint));
GLFN(void, glGenBuffers, (GLsizei, GLuint *));
GLFN(void, glDeleteBuffers, (GLsizei, const GLuint *));
GLFN(void, glBindBuffer, (GLenum, GLuint));
GLFN(void, glBufferData, (GLenum, GLsizeiptr, const void *, GLenum));
GLFN(GLuint, glCreateShader, (GLenum));
GLFN(void, glDeleteShader, (GLuint));
GLFN(void, glShaderSource, (GLuint, GLsizei, const GLchar *const *, const GLint *));
GLFN(void, glCompileShader, (GLuint));
GLFN(void, glGetShaderiv, (GLuint, GLenum, GLint *));
GLFN(void, glGetShaderInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *));
GLFN(GLuint, glCreateProgram, (void));
GLFN(void, glDeleteProgram, (GLuint));
GLFN(void, glAttachShader, (GLuint, GLuint));
GLFN(void, glBindAttribLocation, (GLuint, GLuint, const GLchar *));
GLFN(void, glLinkProgram, (GLuint));
GLFN(void, glGetProgramiv, (GLuint, GLenum, GLint *));
GLFN(void, glGetProgramInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *));
GLFN(void, glUseProgram, (GLuint));
GLFN(GLint, glGetUniformLocation, (GLuint, const GLchar *));
GLFN(void, glUniform1i, (GLint, GLint));
GLFN(void, glUniform1f, (GLint, GLfloat));
GLFN(void, glUniform2f, (GLint, GLfloat, GLfloat));
GLFN(void, glUniform3fv, (GLint, GLsizei, const GLfloat *));
GLFN(void, glUniform4fv, (GLint, GLsizei, const GLfloat *));
GLFN(void, glUniform3f, (GLint, GLfloat, GLfloat, GLfloat));
GLFN(void, glEnableVertexAttribArray, (GLuint));
GLFN(void, glDisableVertexAttribArray, (GLuint));
GLFN(void, glVertexAttribPointer, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void *));
GLFN(void, glDrawElements, (GLenum, GLsizei, GLenum, const void *));
GLFN(void, glDrawArrays, (GLenum, GLint, GLsizei));
GLFN(const GLubyte *, glGetString, (GLenum));
GLFN(void, glGetIntegerv, (GLenum, GLint *));
#undef GLFN

/* ---- state ---- */
static struct retro_hw_render_callback hw;
static dbool gl_ready = FALSE;
static GLint max_texture_size = 2048;

/* The key colours (render_gl.h): RGB565 values the current palette does
 * not have, picked with the colour table; the replay and the frame that
 * follows share them. 0 while gl is off, which leaves the fuzz as it was
 * (black fuzzes into black). */
#define GL_KEYS 2
uint16_t dopo_gl_keys[GL_KEYS] = { 0, 0 };

/* 16-bit indices: at most 16383 quads per draw call */
#define GL_MAX_QUADS_PER_DRAW 16383

/* one quad corner, 16 bytes (the upload is slow on the Mali-400): a wall
 * column runs s down the column at a fixed t, a flat span runs both
 * across the row, past the texture's edges (GL_REPEAT) */
typedef struct
{
   int16_t  x, y;   /* frame pixels */
   float    s, t;   /* texture coordinates */
   uint16_t l;      /* colour table row (normalized) */
   uint16_t pad;
} glvert_t;

/* a wall texture (composite, columns along GL rows) or a flat (64x64,
 * as stored); gl_width and gl_height count the patch's texels, a
 * prboom-scale texture holding twice as many each way */
typedef struct
{
   GLuint         id;
   int            gl_width, gl_height;  /* powers of two */
   unsigned       stamp;                /* replay the patch below is from */
   const rpatch_t *patch;
   dbool          bad;                  /* too big for the GPU */
   dbool          alpha;                /* index and coverage, prboom-scale */
   dbool          glow_valid;           /* glow_rgb is known */
   uint32_t       glow_rgb;             /* a sprite's corona colour */
   int            count, first;         /* quads this frame, while sorting */
} gltex_t;

static gltex_t        *texcache = NULL;   /* per texture number */
static int             texcache_n = 0;
static texture_t     **texcache_owner = NULL;
static gltex_t        *flatcache = NULL;  /* per lump number */
static int             flatcache_n = 0;
static lumpinfo_t     *flatcache_owner = NULL;
static unsigned        replay_stamp = 0;

/* quads of the frame and the texture of each: walls, sky and flats never
 * overlap and are drawn grouped per texture; sprites and masked textures
 * are drawn after them in the order the CPU drew them (back to front) */
typedef struct
{
   glvert_t *v;
   gltex_t **tex;
   uint8_t  *fuzz;   /* how it blends: GL_KIND_* */
   uint32_t *rgb;    /* a corona's colour, 0xRRGGBB */
   int       n, cap;
} glquads_t;

static glquads_t       solid_q, masked_q;
static glvert_t       *sorted = NULL;    /* both lists, as uploaded */
static int             sorted_cap = 0;
static gltex_t       **used = NULL;      /* textures with solid quads */
static int             used_cap = 0;
static int             used_n = 0;

/* a view of the frame is being replayed on the GPU (walls, flats, then
 * sprites and masked textures) */
static dbool           view_active = FALSE;
/* something in that view was drawn by the CPU: the masked quads put the
 * key back under themselves, so they still cover it when in front */
static dbool           view_software = FALSE;

/* Floors and ceilings in rectangles: rows of one plane with the same left
 * and right ends drawn as one quad. The floor's texture mapping is
 * projective on the screen, so with each corner's distance as the clip w
 * the GPU interpolates it exactly; the light, a COLORMAP row per distance
 * band (zlight), is looked up per pixel from the interpolated distance in
 * a table texture (rows 0-15: light levels; 16: always row 0, the full
 * bright of a glowing flat or the light amplification visor; 17: always
 * row 32, invulnerability). The shaders are vert_plane and frag_plane. */
typedef struct
{
   int16_t  x, y;   /* frame pixels */
   float    s, t;   /* texture coordinates at the corner */
   float    w;      /* distance at the corner: the clip w */
   float    z;      /* the same over the light table's width */
   uint8_t  lev;    /* light table row */
   uint8_t  base;   /* colour table row of the plane's COLORMAP */
   uint8_t  pad[2];
} glpvert_t;

static glpvert_t      *plane_v = NULL;
static gltex_t       **plane_t = NULL;
static int             plane_n = 0, plane_cap = 0;
static glpvert_t      *plane_sorted = NULL;
static int             plane_sorted_cap = 0;
static gltex_t       **plane_used = NULL;
static int             plane_used_cap = 0;

/* the plane being made into spans (dopo_gl_plane_begin) */
static unsigned        plane_serial = 0;
static fixed_t         plane_height = 0;
static int             plane_level = 0;

/* the run of spans still open: same plane, ends and light, rows yt..yb */
static struct
{
   dbool    on;
   unsigned serial;
   gltex_t *tex;
   fixed_t  height;                   /* its plane's height over the view */
   int      x1, x2, yt, yb, lev, base;
   int32_t  xf_t, yf_t, xs_t, ys_t;   /* the top span */
   int32_t  xf_b, yf_b, xs_b, ys_b;   /* the bottom span */
} plane_run;

static uint8_t         zlight_rows[32 * MAXLIGHTZ];
static dbool           zlight_valid = FALSE;

/* the 2D of this frame goes to the GPU (see "2D" below) */
static dbool                gl2d_on = FALSE;
static int                  gl2d_suspended = 0;
static const unsigned char *gl2d_frame = NULL;  /* screens[0] when it began */
static glquads_t            two_q;              /* fuzz[]: 0 patch, 1 image */

/* the sprite or masked texture whose posts go to the GPU, NULL while the
 * CPU draws them */
static gltex_t        *masked_tex = NULL;
static const rpatch_t *masked_patch = NULL;
static dbool           masked_fuzz = FALSE;
static uint8_t         masked_kind = 0;     /* GL_KIND_* of its quads */

/* how a masked quad blends: a fuzzy thing's look (prboom-spectre), a
 * translucent or additive one (DECORATE RenderStyle, prboom-glow) at
 * alpha 0..32, a corona (prboom-glow 2) */
#define GL_KIND_PLAIN   0
#define GL_KIND_FUZZ    1
#define GL_KIND_ADD     2     /* + alpha: 2..34 */
#define GL_KIND_LERP    35    /* + alpha: 35..67 */
#define GL_KIND_CORONA  255

/* the corona of the additive sprite being drawn, queued after its posts */
static struct
{
   dbool    on;
   int      x0, y0, x1, y1;   /* view pixels, unclipped */
   uint32_t rgb;
} masked_corona;
static gltex_t         corona_tex;          /* a radial ramp, 64x64 */

/* prboom-dynlight: the lights of the frame (dynlights.c), relative to the
 * eye. What they reach is drawn a second time, over itself, by programs
 * that add them: a wall column or a floor rectangle within a light's
 * radius, on the side it faces, goes into a list of its own with where
 * its corners are and its normal; what no light reaches costs nothing
 * more. A sprite gets their sum at its middle. */
static dopo_light_t    gl_dyn[DOPO_LIGHTS_MAX];
static int             gl_dyn_n = 0;
static double          gl_dyn_fx, gl_dyn_fy;   /* the view's direction */

typedef struct
{
   glvert_t v;
   float    p[3];   /* from the eye, map units */
   float    n[3];
} gllvert_t;

typedef struct
{
   glpvert_t v;
   float     p[3];
   float     n[3];
} glplvert_t;

static gllvert_t      *litw_v = NULL;      /* lit wall columns, 4 corners each */
static gltex_t       **litw_t = NULL;
static int             litw_n = 0, litw_cap = 0;
static glplvert_t     *litp_v = NULL;      /* lit floor rectangles */
static gltex_t       **litp_t = NULL;
static int             litp_n = 0, litp_cap = 0;

/* the wall column being made into quads, when a light reaches it */
static struct
{
   dbool  on;
   float  px, py;     /* the column on the wall, from the eye */
   double zk;         /* map units per row */
   float  nx, ny;
} gl_wlit;

static uint32_t        masked_light = 0;   /* the sprite's light, 0xRRGGBB */

/* The view of the software frame stays filled with the key between
 * frames: only the rows something drew on (the HUD, a menu, what the GPU
 * left to the CPU) are sent and laid over the GPU, and filled back with
 * the key after. The whole view is filled again only when it moves, the
 * key changes, or a frame went by without it. */
static dbool           view_keyed = FALSE;
static uint16_t       *keyed_topleft = NULL;
static int             keyed_x0, keyed_y0, keyed_w, keyed_h;
static uint16_t        keyed_key;
static dbool           frame_has_view = FALSE;  /* since the last present */
/* a rectangle of the frame: [x0, x1) x [y0, y1) */
typedef struct
{
   int16_t x0, y0, x1, y1;
} glrect_t;

/* A few dozen per frame (status bar widgets, HUD lines); merged when they
 * touch, and all merged into one when the list fills up. */
#define GL_RECTS 128
typedef struct
{
   glrect_t r[GL_RECTS];
   int      n;
} glrects_t;

static glrects_t       dirty_rects;             /* drawn on this frame */
static int             scan_phase = 0;
static glrects_t       draw_rects;              /* what the frame covers */
static glrects_t       send_rects;              /* what is uploaded this frame */
static glrects_t       pending_rects[2];        /* per frame texture */
static uint16_t       *sent_copy = NULL;        /* rows outside the view, as sent */
static dbool           was_keyed = FALSE;
static int             frame_rows_cap = 0;
static uint16_t       *key_row = NULL;          /* a row of the key */
static int             key_row_w = 0;
static uint16_t        key_row_key = 0;
static GLfloat        *band_verts = NULL;
static int             band_verts_cap = 0;

static gltex_t        *patchcache = NULL;  /* sprite patches, per lump */
static int             patchcache_n = 0;
static lumpinfo_t     *patchcache_owner = NULL;

/* the replay in progress: where the view sits in the frame */
static int             view_x0, view_y0;
static uint16_t       *view_topleft;

/* colour table: COLORMAP rows through the palette, plus one row of the
 * palette alone (the drawers without light) */
#define GL_CMAP_ROWS 34
static GLuint          cmap_tex = 0;
static int             cmap_rows = 0, cmap_gl_rows = 0;
static const uint16_t *cmap_palette = NULL;
static const lighttable_t **cmap_colormaps = NULL;
static int             cmap_count = 0;
static uint16_t        cmap_palette_row[256];
static int             cmap_lumprows[64];

/* programs and buffers */
static GLuint          wall_program = 0, frame_program = 0, plane_program = 0;
static GLuint          fuzz_program = 0, sprite_program = 0, corona_program = 0;
static GLuint          wlit_program = 0, plit_program = 0, slit_program = 0;
static GLuint          litw_vbo = 0, litp_vbo = 0;
typedef struct
{
   GLint scale, offset, tex, cmap, zlight, rows, lpos, lcol, lcount, full, light;
} gllit_u_t;
static gllit_u_t       wlit_u, plit_u, slit_u;
static GLint           corona_u_scale, corona_u_offset, corona_u_tex, corona_u_color;
static GLint           sprite_u_scale, sprite_u_offset, sprite_u_tex, sprite_u_cmap;
static GLuint          two_program = 0, copy_program = 0;
static GLint           two_u_scale, two_u_offset, two_u_tex, two_u_trans, two_u_pal;
static GLint           copy_u_scale, copy_u_offset, copy_u_tex;
static GLuint          two_vbo = 0, pal_tex = 0, trans_tex = 0, ramp_tex = 0;
static GLint           fuzz_u_scale, fuzz_u_offset, fuzz_u_tex, fuzz_u_cmap;
static GLint           fuzz_u_mul, fuzz_u_add;
static GLint           wall_u_scale, wall_u_offset, wall_u_tex, wall_u_cmap;
static GLint           plane_u_scale, plane_u_offset, plane_u_tex, plane_u_cmap;
static GLint           plane_u_zlight, plane_u_rows;
static GLuint          plane_vbo = 0, zlight_tex = 0;
static GLint           frame_u_frame, frame_u_keys;
static GLuint          vbo = 0, ibo = 0, frame_vbo = 0;
static GLuint          frame_tex[2] = { 0, 0 };
static int             frame_tex_w[2], frame_tex_h[2];
static int             frame_tex_next = 0;
static uint16_t       *frame_copy = NULL;
static size_t          frame_copy_size = 0;

/* statistics, logged every GL_STATS_FRAMES presented frames */
#define GL_STATS_FRAMES 300
static double          stat_walls_ms, stat_setup_ms, stat_fill_ms, stat_present_ms;
static double          stat_frame_ms, stat_last_present;
static long            stat_quads, stat_columns, stat_software, stat_draws;
static long            stat_spans, stat_spans_software;
static long            stat_masked, stat_masked_software, stat_posts;
static long            stat_rows, stat_pixels, stat_full_fills, stat_planes;
static long            stat_2d_quads, stat_mirror_pixels;
static double          stat_scan_ms;
static int             stat_frames;

static double gl_now_ms(void)
{
   struct timespec t;
   clock_gettime(CLOCK_MONOTONIC, &t);
   return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

static int gl_pot(int n)
{
   int p = 1;
   while (p < n)
      p <<= 1;
   return p;
}

/* ---- loading the functions ----
 * Old EGL (before 1.5, common with Mali drivers) only hands out extension
 * functions through eglGetProcAddress, so the core GLES2 ones may come
 * back NULL from the frontend: then they come from the GL library already
 * loaded in the process, or from loading it. */
#ifndef __EMSCRIPTEN__
static void *gl_lib = NULL;
#endif

static void *gl_proc(const char *name)
{
   void *p = hw.get_proc_address ? (void *)hw.get_proc_address(name) : NULL;
#ifndef __EMSCRIPTEN__
   static const char *const libs[] =
      { "libGLESv2.so.2", "libGLESv2.so", "libGL.so.1", "libGL.so" };
   size_t i;
   if (p)
      return p;
   p = dlsym(RTLD_DEFAULT, name);
   if (p)
      return p;
   for (i = 0; !gl_lib && i < sizeof(libs) / sizeof(*libs); i++)
      gl_lib = dlopen(libs[i], RTLD_LAZY | RTLD_GLOBAL);
   p = gl_lib ? dlsym(gl_lib, name) : NULL;
#endif
   return p;
}

static dbool gl_load_functions(void)
{
   dbool ok = TRUE;
#define LOAD(name) \
   if (!(*(void **)&name = gl_proc(#name))) \
   { lprintf(LO_ERROR, "prboom-render gl: missing %s\n", #name); ok = FALSE; }
   LOAD(glViewport) LOAD(glClearColor) LOAD(glClear) LOAD(glDisable)
   LOAD(glEnable) LOAD(glBlendFunc) LOAD(glBlendEquation) LOAD(glUniform4f)
   LOAD(glColorMask) LOAD(glBindFramebuffer) LOAD(glGenTextures)
   LOAD(glDeleteTextures) LOAD(glBindTexture) LOAD(glActiveTexture)
   LOAD(glTexParameteri) LOAD(glTexImage2D) LOAD(glTexSubImage2D)
   LOAD(glPixelStorei) LOAD(glGenBuffers) LOAD(glDeleteBuffers)
   LOAD(glBindBuffer) LOAD(glBufferData) LOAD(glCreateShader)
   LOAD(glDeleteShader) LOAD(glShaderSource) LOAD(glCompileShader)
   LOAD(glGetShaderiv) LOAD(glGetShaderInfoLog) LOAD(glCreateProgram)
   LOAD(glDeleteProgram) LOAD(glAttachShader) LOAD(glBindAttribLocation)
   LOAD(glLinkProgram) LOAD(glGetProgramiv) LOAD(glGetProgramInfoLog)
   LOAD(glUseProgram) LOAD(glGetUniformLocation) LOAD(glUniform1i)
   LOAD(glUniform1f) LOAD(glUniform2f) LOAD(glUniform3fv) LOAD(glUniform4fv)
   LOAD(glUniform3f) LOAD(glEnableVertexAttribArray)
   LOAD(glDisableVertexAttribArray) LOAD(glVertexAttribPointer)
   LOAD(glDrawElements) LOAD(glDrawArrays) LOAD(glGetString)
   LOAD(glGetIntegerv)
#undef LOAD
   return ok;
}

/* ---- shaders ---- */

/* the shaders are in shaders/, embedded into prboomtv/shaders.h by
 * dopo/scripts/xxd.cpp at build time */
#define GL_SHADER(name) ((const char *)(name))

static GLuint gl_compile(GLenum type, const char *src)
{
   GLuint s = glCreateShader(type);
   GLint ok = 0;
   glShaderSource(s, 1, &src, NULL);
   glCompileShader(s);
   glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
   if (!ok)
   {
      char log[1024];
      glGetShaderInfoLog(s, sizeof(log), NULL, log);
      lprintf(LO_ERROR, "prboom-render gl: shader: %s\n", log);
   }
   return s;
}

static GLuint gl_program(const char *vs, const char *fs,
                         const char *const *attribs, int nattribs)
{
   GLuint p = glCreateProgram();
   GLuint v = gl_compile(GL_VERTEX_SHADER, vs);
   GLuint f = gl_compile(GL_FRAGMENT_SHADER, fs);
   GLint ok = 0;
   int i;

   glAttachShader(p, v);
   glAttachShader(p, f);
   for (i = 0; i < nattribs; i++)
      glBindAttribLocation(p, i, attribs[i]);
   glLinkProgram(p);
   glDeleteShader(v);
   glDeleteShader(f);
   glGetProgramiv(p, GL_LINK_STATUS, &ok);
   if (!ok)
   {
      char log[1024];
      glGetProgramInfoLog(p, sizeof(log), NULL, log);
      lprintf(LO_ERROR, "prboom-render gl: link: %s\n", log);
      glDeleteProgram(p);
      return 0;
   }
   return p;
}

static void gl_texture_params(GLint wrap)
{
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
}

static void gl2d_forget(void);
static void gl2d_context(void);
static void gl2d_destroy(void);

/* ---- context ---- */

/* every GL name died with the context; nothing is deleted */
static void gl_forget_objects(void)
{
   int i;
   for (i = 0; i < texcache_n; i++)
      texcache[i].id = 0;
   for (i = 0; i < flatcache_n; i++)
      flatcache[i].id = 0;
   for (i = 0; i < patchcache_n; i++)
      patchcache[i].id = 0;
   cmap_tex = 0;
   cmap_palette = NULL;
   wall_program = frame_program = plane_program = fuzz_program = sprite_program = 0;
   corona_program = 0;
   corona_tex.id = 0;
   wlit_program = plit_program = slit_program = 0;
   litw_vbo = litp_vbo = 0;
   two_program = copy_program = 0;
   two_vbo = pal_tex = trans_tex = ramp_tex = 0;
   gl2d_forget();
   plane_vbo = zlight_tex = 0;
   zlight_valid = FALSE;
   vbo = ibo = frame_vbo = 0;
   frame_tex[0] = frame_tex[1] = 0;
}

/* the uniform locations of a lit program (-1 where it has none) */
static void gl_lit_uniforms(gllit_u_t *u, GLuint prog)
{
   if (!prog)
      return;
   u->scale  = glGetUniformLocation(prog, "u_scale");
   u->offset = glGetUniformLocation(prog, "u_offset");
   u->tex    = glGetUniformLocation(prog, "u_tex");
   u->cmap   = glGetUniformLocation(prog, "u_cmap");
   u->zlight = glGetUniformLocation(prog, "u_zlight");
   u->rows   = glGetUniformLocation(prog, "u_rows");
   u->lpos   = glGetUniformLocation(prog, "u_lpos");
   u->lcol   = glGetUniformLocation(prog, "u_lcol");
   u->lcount = glGetUniformLocation(prog, "u_lcount");
   u->full   = glGetUniformLocation(prog, "u_full");
   u->light  = glGetUniformLocation(prog, "u_light");
}

static void gl_context_reset(void)
{
   static const char *const wall_attribs[] = { "a_pos", "a_st", "a_l" };
   static const char *const frame_attribs[] = { "a_pos" };
   uint16_t *idx;
   int i;

   gl_ready = FALSE;
   gl_forget_objects();
   if (!gl_load_functions())
   {
      lprintf(LO_ERROR, "prboom-render gl: the frontend's GL lacks functions\n");
      return;
   }
   glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
   lprintf(LO_INFO, "prboom-render gl: %s | %s | max texture %d\n",
           (const char *)glGetString(GL_RENDERER),
           (const char *)glGetString(GL_VERSION), max_texture_size);

   wall_program = gl_program(GL_SHADER(vert_wall), GL_SHADER(frag_wall), wall_attribs, 3);
   frame_program = gl_program(GL_SHADER(vert_frame), GL_SHADER(frag_frame), frame_attribs, 1);
   if (!wall_program || !frame_program)
      return;
   wall_u_scale  = glGetUniformLocation(wall_program, "u_scale");
   wall_u_offset = glGetUniformLocation(wall_program, "u_offset");
   wall_u_tex    = glGetUniformLocation(wall_program, "u_tex");
   wall_u_cmap   = glGetUniformLocation(wall_program, "u_cmap");
   frame_u_frame = glGetUniformLocation(frame_program, "u_frame");
   frame_u_keys  = glGetUniformLocation(frame_program, "u_keys");
   {
      static const char *const plane_attribs[] = { "a_pos", "a_st", "a_w", "a_z", "a_lb" };
      plane_program = gl_program(GL_SHADER(vert_plane), GL_SHADER(frag_plane), plane_attribs, 5);
      fuzz_program = gl_program(GL_SHADER(vert_wall), GL_SHADER(frag_fuzz), wall_attribs, 3);
      sprite_program = gl_program(GL_SHADER(vert_wall), GL_SHADER(frag_sprite), wall_attribs, 3);
      corona_program = gl_program(GL_SHADER(vert_wall), GL_SHADER(frag_corona), wall_attribs, 3);
      {
         static const char *const wlit_attribs[] = { "a_pos", "a_st", "a_l", "a_world", "a_normal" };
         static const char *const plit_attribs[] = { "a_pos", "a_st", "a_w", "a_z", "a_lb",
                                                     "a_world", "a_normal" };
         wlit_program = gl_program(GL_SHADER(vert_wall_lit), GL_SHADER(frag_wall_lit), wlit_attribs, 5);
         plit_program = gl_program(GL_SHADER(vert_plane_lit), GL_SHADER(frag_plane_lit), plit_attribs, 7);
         slit_program = gl_program(GL_SHADER(vert_wall), GL_SHADER(frag_sprite_lit), wall_attribs, 3);
         gl_lit_uniforms(&wlit_u, wlit_program);
         gl_lit_uniforms(&plit_u, plit_program);
         gl_lit_uniforms(&slit_u, slit_program);
         glGenBuffers(1, &litw_vbo);
         glGenBuffers(1, &litp_vbo);
      }
      two_program  = gl_program(GL_SHADER(vert_wall), GL_SHADER(frag_2d), wall_attribs, 3);
      copy_program = gl_program(GL_SHADER(vert_wall), GL_SHADER(frag_copy), wall_attribs, 3);
      if (!two_program || !copy_program)
         return;
      two_u_scale   = glGetUniformLocation(two_program, "u_scale");
      two_u_offset  = glGetUniformLocation(two_program, "u_offset");
      two_u_tex     = glGetUniformLocation(two_program, "u_tex");
      two_u_trans   = glGetUniformLocation(two_program, "u_trans");
      two_u_pal     = glGetUniformLocation(two_program, "u_pal");
      copy_u_scale  = glGetUniformLocation(copy_program, "u_scale");
      copy_u_offset = glGetUniformLocation(copy_program, "u_offset");
      copy_u_tex    = glGetUniformLocation(copy_program, "u_tex");
      glGenBuffers(1, &two_vbo);
      gl2d_context();
      if (!plane_program || !fuzz_program || !sprite_program || !corona_program)
         return;
      corona_u_scale  = glGetUniformLocation(corona_program, "u_scale");
      corona_u_offset = glGetUniformLocation(corona_program, "u_offset");
      corona_u_tex    = glGetUniformLocation(corona_program, "u_tex");
      corona_u_color  = glGetUniformLocation(corona_program, "u_color");
      {
         /* the corona's ramp: 1 in the middle to 0 at the rim, falling
          * off as the square, so it has no edge */
         uint8_t ramp[64 * 64];
         int x, y;
         for (y = 0; y < 64; y++)
            for (x = 0; x < 64; x++)
            {
               const double dx = (x + 0.5) / 32.0 - 1.0, dy = (y + 0.5) / 32.0 - 1.0;
               double v = 1.0 - sqrt(dx * dx + dy * dy);
               v = v > 0 ? v * v : 0;
               ramp[y * 64 + x] = (uint8_t)(v * 255.0 + 0.5);
            }
         glGenTextures(1, &corona_tex.id);
         glBindTexture(GL_TEXTURE_2D, corona_tex.id);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
         glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
         glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 64, 64, 0,
                      GL_LUMINANCE, GL_UNSIGNED_BYTE, ramp);
         corona_tex.gl_width = corona_tex.gl_height = 64;
      }
      sprite_u_scale  = glGetUniformLocation(sprite_program, "u_scale");
      sprite_u_offset = glGetUniformLocation(sprite_program, "u_offset");
      sprite_u_tex    = glGetUniformLocation(sprite_program, "u_tex");
      sprite_u_cmap   = glGetUniformLocation(sprite_program, "u_cmap");
      fuzz_u_scale  = glGetUniformLocation(fuzz_program, "u_scale");
      fuzz_u_offset = glGetUniformLocation(fuzz_program, "u_offset");
      fuzz_u_tex    = glGetUniformLocation(fuzz_program, "u_tex");
      fuzz_u_cmap   = glGetUniformLocation(fuzz_program, "u_cmap");
      fuzz_u_mul    = glGetUniformLocation(fuzz_program, "u_mul");
      fuzz_u_add    = glGetUniformLocation(fuzz_program, "u_add");
      plane_u_scale  = glGetUniformLocation(plane_program, "u_scale");
      plane_u_offset = glGetUniformLocation(plane_program, "u_offset");
      plane_u_tex    = glGetUniformLocation(plane_program, "u_tex");
      plane_u_cmap   = glGetUniformLocation(plane_program, "u_cmap");
      plane_u_zlight = glGetUniformLocation(plane_program, "u_zlight");
      plane_u_rows   = glGetUniformLocation(plane_program, "u_rows");
      glGenBuffers(1, &plane_vbo);
      glGenTextures(1, &zlight_tex);
   }

   glGenBuffers(1, &vbo);
   glGenBuffers(1, &ibo);
   glGenBuffers(1, &frame_vbo);
   idx = (uint16_t *)malloc(sizeof(uint16_t) * 6 * GL_MAX_QUADS_PER_DRAW);
   if (!idx)
      return;
   for (i = 0; i < GL_MAX_QUADS_PER_DRAW; i++)
   {
      idx[i * 6 + 0] = (uint16_t)(i * 4 + 0);
      idx[i * 6 + 1] = (uint16_t)(i * 4 + 1);
      idx[i * 6 + 2] = (uint16_t)(i * 4 + 2);
      idx[i * 6 + 3] = (uint16_t)(i * 4 + 0);
      idx[i * 6 + 4] = (uint16_t)(i * 4 + 2);
      idx[i * 6 + 5] = (uint16_t)(i * 4 + 3);
   }
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
   glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                sizeof(uint16_t) * 6 * GL_MAX_QUADS_PER_DRAW, idx, GL_STATIC_DRAW);
   free(idx);

   glGenTextures(1, &cmap_tex);
   glGenTextures(2, frame_tex);
   frame_tex_w[0] = frame_tex_w[1] = 0;
   frame_tex_h[0] = frame_tex_h[1] = 0;

   gl_ready = TRUE;
}

static void gl_context_destroy(void)
{
   int i;
   if (gl_ready)
   {
      for (i = 0; i < texcache_n; i++)
         if (texcache[i].id)
            glDeleteTextures(1, &texcache[i].id);
      for (i = 0; i < flatcache_n; i++)
         if (flatcache[i].id)
            glDeleteTextures(1, &flatcache[i].id);
      for (i = 0; i < patchcache_n; i++)
         if (patchcache[i].id)
            glDeleteTextures(1, &patchcache[i].id);
      glDeleteTextures(1, &cmap_tex);
      glDeleteTextures(2, frame_tex);
      glDeleteBuffers(1, &vbo);
      glDeleteBuffers(1, &ibo);
      glDeleteBuffers(1, &frame_vbo);
      glDeleteProgram(wall_program);
      glDeleteProgram(frame_program);
      glDeleteProgram(plane_program);
      glDeleteProgram(fuzz_program);
      glDeleteProgram(sprite_program);
      glDeleteProgram(corona_program);
      glDeleteProgram(wlit_program);
      glDeleteProgram(plit_program);
      glDeleteProgram(slit_program);
      glDeleteBuffers(1, &litw_vbo);
      glDeleteBuffers(1, &litp_vbo);
      glDeleteTextures(1, &corona_tex.id);
      glDeleteProgram(two_program);
      glDeleteProgram(copy_program);
      glDeleteBuffers(1, &two_vbo);
      gl2d_destroy();
      glDeleteBuffers(1, &plane_vbo);
      glDeleteTextures(1, &zlight_tex);
   }
   gl_forget_objects();
   gl_ready = FALSE;
}

dbool dopo_gl_load(retro_environment_t environ_cb)
{
   struct retro_variable var = { "prboom-render", NULL };
   dbool required;

   dopo_gl = FALSE;
   if (!environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) || !var.value)
      var.value = "auto";
   required = !strcmp(var.value, "gl");
#ifdef __EMSCRIPTEN__
   /* gl has not been tried in the web frontend yet: auto stays software */
   if (!required)
      var.value = "software";
#endif
   if (!required && strcmp(var.value, "auto"))
   {
      lprintf(LO_INFO, "prboom-render software\n");
      return TRUE;
   }

   memset(&hw, 0, sizeof(hw));
   hw.context_reset      = gl_context_reset;
   hw.context_destroy    = gl_context_destroy;
   hw.bottom_left_origin = true;
   hw.depth              = false;
   hw.stencil            = false;

   /* GLES 2; a desktop GL 2 context runs the same GLSL ES 1.00 shaders
    * (frontends built without GLES) */
   hw.context_type  = RETRO_HW_CONTEXT_OPENGLES2;
   hw.version_major = 2;
   hw.version_minor = 0;
   if (!environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw))
   {
      hw.context_type = RETRO_HW_CONTEXT_OPENGL;
      if (!environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw))
      {
         if (!required)
         {
            lprintf(LO_INFO, "prboom-render software (auto: the frontend "
                    "gave no OpenGL context)\n");
            return TRUE;
         }
         lprintf(LO_ERROR, "prboom-render gl: the frontend refused an OpenGL "
                 "ES 2 or OpenGL context; set prboom-render to software\n");
         return FALSE;
      }
   }
   lprintf(LO_INFO, "prboom-render gl: %s context requested\n",
           hw.context_type == RETRO_HW_CONTEXT_OPENGLES2 ? "OpenGL ES 2" : "OpenGL");
   dopo_gl = TRUE;
   return TRUE;
}

/* ---- textures ---- */

static void gl_cache_reset(gltex_t **cache, int *n, int size)
{
   int i;
   for (i = 0; i < *n; i++)
      if ((*cache)[i].id && gl_ready)
         glDeleteTextures(1, &(*cache)[i].id);
   free(*cache);
   *cache = (gltex_t *)calloc(size > 0 ? size : 1, sizeof(**cache));
   *n = *cache ? size : 0;
   solid_q.n = masked_q.n = plane_n = 0;
   litw_n = litp_n = 0;
   plane_run.on = FALSE;
}

/* a new set of wads numbers the textures and the lumps anew */
static void gl_texcache_check(void)
{
   if (texcache_owner != textures || texcache_n != numtextures)
   {
      gl_cache_reset(&texcache, &texcache_n, numtextures);
      texcache_owner = textures;
   }
   if (flatcache_owner != lumpinfo || flatcache_n != numlumps)
   {
      gl_cache_reset(&flatcache, &flatcache_n, numlumps);
      flatcache_owner = lumpinfo;
   }
   if (patchcache_owner != lumpinfo || patchcache_n != numlumps
         || patchcache_scale != dopo_scale)
   {
      gl_cache_reset(&patchcache, &patchcache_n, numlumps);
      patchcache_owner = lumpinfo;
      patchcache_scale = dopo_scale;
   }
}

/* a flat lump as it is: 64x64, row major, u along s and v along t */
static gltex_t *gl_flat(int lump)
{
   gltex_t *t;
   const uint8_t *data;
   uint8_t buf[64 * 64];
   int len;

   if (lump < 0 || lump >= flatcache_n)
      return NULL;
   t = &flatcache[lump];
   if (t->id || t->bad)
      return t->bad ? NULL : t;
   len = W_LumpLength(lump);
   data = (const uint8_t *)W_CacheLumpNum(lump);
   memset(buf, 0, sizeof(buf));
   memcpy(buf, data, len < (int)sizeof(buf) ? (size_t)len : sizeof(buf));
   W_UnlockLumpNum(lump);
   glGenTextures(1, &t->id);
   glBindTexture(GL_TEXTURE_2D, t->id);
   gl_texture_params(GL_REPEAT);
   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 64, 64, 0,
                GL_LUMINANCE, GL_UNSIGNED_BYTE, buf);
   t->gl_width = t->gl_height = 64;
   return t;
}

/* the composite, column major: column x is GL row x, its texels along s */
static void gl_texture_upload(gltex_t *t, const rpatch_t *p)
{
   const int rows = gl_pot(p->height), cols = gl_pot(p->width);
   uint8_t *buf;
   int x;

   if (rows > max_texture_size || cols > max_texture_size)
   {
      t->bad = TRUE;
      return;
   }
   buf = (uint8_t *)calloc((size_t)rows * cols, 1);
   if (!buf)
   {
      t->bad = TRUE;
      return;
   }
   for (x = 0; x < p->width; x++)
      memcpy(buf + (size_t)x * rows, p->pixels + (size_t)x * p->height, p->height);
   glGenTextures(1, &t->id);
   glBindTexture(GL_TEXTURE_2D, t->id);
   gl_texture_params(GL_REPEAT);
   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, rows, cols, 0,
                GL_LUMINANCE, GL_UNSIGNED_BYTE, buf);
   free(buf);
   t->gl_width  = rows;
   t->gl_height = cols;
}

/* A patch with its coverage, index and alpha, column x along GL row x;
 * with prboom-scale bigger through Scale2x (scale2x.c). */
static void gl_alpha_upload(gltex_t *t, const rpatch_t *p, int scale)
{
   const int rows = gl_pot(p->height), cols = gl_pot(p->width);
   uint16_t *img;
   uint8_t *buf;
   int k, x, y;

   t->bad = TRUE;
   t->alpha = TRUE;
   img = dopo_scale_image(p, scale, &k);
   if (!img)
      return;
   if (rows * k > max_texture_size || cols * k > max_texture_size
         || (buf = (uint8_t *)calloc((size_t)rows * cols * k * k, 2)) == NULL)
   {
      free(img);
      return;
   }
   for (x = 0; x < p->width * k; x++)
      for (y = 0; y < p->height * k; y++)
      {
         const uint16_t c = img[(size_t)x * p->height * k + y];
         if (c != DOPO_SCALE_CLEAR)
         {
            uint8_t *d = buf + ((size_t)x * rows * k + y) * 2;
            d[0] = (uint8_t)c;
            d[1] = 255;
         }
      }
   free(img);
   glGenTextures(1, &t->id);
   glBindTexture(GL_TEXTURE_2D, t->id);
   gl_texture_params(GL_CLAMP_TO_EDGE);
   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, rows * k, cols * k, 0,
                GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, buf);
   free(buf);
   t->gl_width  = rows;
   t->gl_height = cols;
   t->bad = FALSE;
}

/* the texture of a record, its composite locked until the replay ends
 * (the replay unlocks what it adopted) */
static gltex_t *gl_texture(int texnum)
{
   gltex_t *t;

   if (texnum < 0 || texnum >= texcache_n)
      return NULL;
   t = &texcache[texnum];
   if (t->stamp != replay_stamp)
   {
      t->patch = R_CacheTextureCompositePatchNum(texnum);
      R_DrawCmdAdoptTextureLock(texnum);
      t->stamp = replay_stamp;
      if (!t->id && !t->bad)
         gl_texture_upload(t, t->patch);
   }
   return t->bad ? NULL : t;
}

/* ---- colour table ---- */

static int gl_colormap_lumprows(int m)
{
   int lump, len;
   if (m == 0 || colormaps[m] == colormaps[0])
      lump = W_GetNumForName("COLORMAP");
   else
      lump = m + firstcolormaplump;
   len = W_LumpLength(lump) / 256;
   return len < GL_CMAP_ROWS ? len : GL_CMAP_ROWS;
}

/* the brightest colours the palette does not have (Doom's has magenta
 * and white, but not every value near them) */
static dbool gl_in_palette(const uint16_t *palette, unsigned c)
{
   int i;
   for (i = 0; i < 256; i++)
      if (palette[i] == c)
         return TRUE;
   return FALSE;
}

/* The first key has both bytes equal, so the view fills with memset (the
 * fastest store the C library has; a 16-bit loop managed ~380 MB/s on the
 * ARM box); the fuzz key is any other free colour. */
static void gl_key_pick(const uint16_t *palette)
{
   int k;

   dopo_gl_keys[0] = 0;
   for (k = 0xff; k > 0; k--)
      if (!gl_in_palette(palette, k * 0x0101u))
      {
         dopo_gl_keys[0] = (uint16_t)(k * 0x0101u);
         break;
      }
   for (k = 0xffff; k > 0; k--)
      if (!gl_in_palette(palette, k))
      {
         if (!dopo_gl_keys[0])
            dopo_gl_keys[0] = (uint16_t)k;
         else if (k != dopo_gl_keys[0])
         {
            dopo_gl_keys[1] = (uint16_t)k;
            break;
         }
      }
}

/* rebuilt when the palette (damage, pickups, gamma) or the colormaps
 * change; every entry is the drawers' V_Palette16[colormap[i]*64 + 63] */
static void gl_colormap_update(void)
{
   uint16_t row[256];
   uint16_t *buf;
   int count, m, r, i;

   for (i = 0; i < 256; i++)
      row[i] = V_Palette16[i * VID_NUMCOLORWEIGHTS + (VID_NUMCOLORWEIGHTS - 1)];
   count = numcolormaps < (int)(sizeof(cmap_lumprows) / sizeof(*cmap_lumprows))
         ? numcolormaps : (int)(sizeof(cmap_lumprows) / sizeof(*cmap_lumprows));
   /* keyed on the colours, not on V_Palette16: a palette blend flips it
    * between two buffers that may hold the same colours */
   if (cmap_palette && cmap_colormaps == colormaps
         && cmap_count == count && !memcmp(row, cmap_palette_row, sizeof(row)))
      return;

   cmap_rows = count * GL_CMAP_ROWS + 1;
   cmap_gl_rows = gl_pot(cmap_rows);
   if (cmap_gl_rows > max_texture_size)
   {
      count = (max_texture_size - 1) / GL_CMAP_ROWS;
      cmap_rows = count * GL_CMAP_ROWS + 1;
      cmap_gl_rows = gl_pot(cmap_rows);
   }
   buf = (uint16_t *)calloc((size_t)256 * cmap_gl_rows, sizeof(uint16_t));
   if (!buf)
      return;
   for (m = 0; m < count; m++)
   {
      const lighttable_t *cm = colormaps[m];
      cmap_lumprows[m] = gl_colormap_lumprows(m);
      for (r = 0; r < cmap_lumprows[m]; r++)
         for (i = 0; i < 256; i++)
            buf[(m * GL_CMAP_ROWS + r) * 256 + i] = row[cm[r * 256 + i]];
   }
   memcpy(buf + (size_t)(cmap_rows - 1) * 256, row, sizeof(row));
   gl_key_pick(row);

   glBindTexture(GL_TEXTURE_2D, cmap_tex);
   gl_texture_params(GL_CLAMP_TO_EDGE);
   glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, cmap_gl_rows, 0,
                GL_RGB, GL_UNSIGNED_SHORT_5_6_5, buf);
   free(buf);

   memcpy(cmap_palette_row, row, sizeof(row));
   cmap_palette = V_Palette16;
   cmap_colormaps = colormaps;
   cmap_count = count;
}

/* colour table row of a colormap pointer, -1 when it is not one of ours */
static int gl_colormap_row(const lighttable_t *cm)
{
   int m;
   for (m = 0; m < cmap_count; m++)
      if (cm >= colormaps[m] && cm < colormaps[m] + cmap_lumprows[m] * 256)
      {
         const ptrdiff_t off = cm - colormaps[m];
         if (off & 255)
            return -1;
         return m * GL_CMAP_ROWS + (int)(off >> 8);
      }
   return -1;
}

static void gl_zlight_update(void);

/* ---- rows drawn on ---- */

/* In gl the frame is the same buffer every frame, so the status bar can
 * update only its changed widgets, as upstream does (the full redraw each
 * frame is for direct render, which gets a new buffer each frame); full
 * after a frame that took the full path, a palette change (the bar is in
 * the palette's colours), a menu or the automap now or the frame before,
 * and once a second: upstream's partial erase can leave the edge of a
 * widget that changed size (an arms digit when a weapon is picked up). */
dbool dopo_gl_statusbar_refresh(void)
{
   /* the palette's colours, not V_Palette16: a fading blend flips it
    * between two buffers every step */
   static uint16_t last_palette[256];
   static dbool last_overlay = TRUE;
   static int frames = 0;
   const dbool overlay = menuactive || (automapmode & am_active);
   uint16_t palette[256];
   dbool refresh;
   int i;

   /* with the 2D on the GPU nothing stays from a frame to the next */
   if (!dopo_gl || gl2d_on)
      return TRUE;
   for (i = 0; i < 256; i++)
      palette[i] = V_Palette16[i * VID_NUMCOLORWEIGHTS + (VID_NUMCOLORWEIGHTS - 1)];
   refresh = !was_keyed || memcmp(palette, last_palette, sizeof(palette))
           || overlay || last_overlay || ++frames >= TICRATE;
   if (refresh)
      frames = 0;
   memcpy(last_palette, palette, sizeof(palette));
   last_overlay = overlay;
   return refresh;
}

static dbool gl_rect_touch(const glrect_t *a, const glrect_t *b)
{
   return a->x0 <= b->x1 && b->x0 <= a->x1 && a->y0 <= b->y1 && b->y0 <= a->y1;
}

static void gl_rect_union(glrect_t *a, const glrect_t *b)
{
   if (b->x0 < a->x0) a->x0 = b->x0;
   if (b->y0 < a->y0) a->y0 = b->y0;
   if (b->x1 > a->x1) a->x1 = b->x1;
   if (b->y1 > a->y1) a->y1 = b->y1;
}

static void gl_rects_add(glrects_t *l, int x0, int y0, int x1, int y1)
{
   glrect_t r;
   int i;

   if (x0 >= x1 || y0 >= y1)
      return;
   r.x0 = (int16_t)x0; r.y0 = (int16_t)y0;
   r.x1 = (int16_t)x1; r.y1 = (int16_t)y1;
   /* the last few are where a neighbour usually is */
   for (i = l->n - 1; i >= 0 && i >= l->n - 4; i--)
      if (gl_rect_touch(&l->r[i], &r))
      {
         gl_rect_union(&l->r[i], &r);
         return;
      }
   if (l->n == GL_RECTS)
   {
      for (i = 1; i < l->n; i++)
         gl_rect_union(&l->r[0], &l->r[i]);
      l->n = 1;
      gl_rect_union(&l->r[0], &r);
      return;
   }
   l->r[l->n++] = r;
}

void dopo_gl_dirty_rect(int x0, int y0, int x1, int y1)
{
   if (!dopo_gl)
      return;
   if (x0 < 0)
      x0 = 0;
   if (x1 > SCREENWIDTH)
      x1 = SCREENWIDTH;
   if (y0 < 0)
      y0 = 0;
   if (y1 > SCREENHEIGHT)
      y1 = SCREENHEIGHT;
   gl_rects_add(&dirty_rects, x0, y0, x1, y1);
}

void dopo_gl_dirty(int y0, int y1)
{
   dopo_gl_dirty_rect(0, y0, SCREENWIDTH, y1 + 1);
}

/* what the GPU leaves to the CPU marks its pixels (view coordinates,
 * x1 past the end, y1 included) */
static dbool gl_software(int x0, int x1, int y0, int y1)
{
   view_software = TRUE;
   dopo_gl_dirty_rect(view_x0 + x0, view_y0 + y0, view_x0 + x1, view_y0 + y1 + 1);
   return FALSE;
}

/* ---- walls ---- */

dbool dopo_gl_walls_begin(void)
{
   const uint16_t *base;
   ptrdiff_t off;
   double t0, t1;

   if (!dopo_gl || !gl_ready || VID_TRUECOLOR || !screens[0].data)
      return FALSE;
   /* only a view drawn straight into the frame */
   base = (const uint16_t *)screens[0].data;
   off  = drawvars.short_topleft - base;
   if (off < 0 || off >= (ptrdiff_t)SCREENWIDTH * SCREENHEIGHT)
      return FALSE;

   t0 = gl_now_ms();
   stat_walls_ms -= t0;
   gl_texcache_check();
   gl_colormap_update();
   gl_zlight_update();
   t1 = gl_now_ms();
   stat_setup_ms += t1 - t0;
   view_topleft = drawvars.short_topleft;
   view_x0 = (int)(off % SCREENWIDTH);
   view_y0 = (int)(off / SCREENWIDTH);
   replay_stamp++;
   view_active = TRUE;
   gl_dyn_n = dopo_dynlight_collect(gl_dyn, DOPO_LIGHTS_MAX);
   gl_dyn_fx = cos(viewangle * (2 * M_PI / 4294967296.0));
   gl_dyn_fy = sin(viewangle * (2 * M_PI / 4294967296.0));

   /* The key over the view (see view_keyed), before anything else is
    * drawn on it: the GPU paints the whole 3D view, and what the CPU still
    * draws (the HUD, what the GPU cannot reproduce) covers it, so the key
    * stays exactly where the GPU shows (where nothing paints, the GPU's
    * black shows instead of a stale frame). Filling it column by column
    * instead touched a new cache line per pixel: 15 ms a frame at 1080p
    * on the ARM. */
   frame_has_view = TRUE;
   /* with the 2D on the GPU the whole frame is already keyed
    * (dopo_gl_frame_begin) */
   if (!gl2d_on && (!view_keyed || keyed_key != dopo_gl_keys[0]
         || keyed_topleft != view_topleft
         || keyed_w != viewwidth || keyed_h != viewheight))
   {
      const uint16_t key = dopo_gl_keys[0];
      int y, x;
      view_keyed    = TRUE;
      keyed_key     = key;
      keyed_topleft = view_topleft;
      keyed_x0      = view_x0;
      keyed_y0      = view_y0;
      keyed_w       = viewwidth;
      keyed_h       = viewheight;
      stat_full_fills++;
      if ((key >> 8) == (key & 0xff) && viewwidth == SURFACE_SHORT_PITCH)
         memset(view_topleft, key & 0xff, (size_t)viewheight * viewwidth * 2);
      else if ((key >> 8) == (key & 0xff))
         for (y = 0; y < viewheight; y++)
            memset(view_topleft + (ptrdiff_t)y * SURFACE_SHORT_PITCH, key & 0xff,
                   (size_t)viewwidth * 2);
      else
         for (y = 0; y < viewheight; y++)
         {
            uint16_t *row = view_topleft + (ptrdiff_t)y * SURFACE_SHORT_PITCH;
            for (x = 0; x < viewwidth; x++)
               row[x] = key;
         }
   }
   stat_fill_ms += gl_now_ms() - t1;
   return TRUE;
}

void dopo_gl_walls_end(void)
{
   stat_walls_ms += gl_now_ms();
}

static glvert_t *gl_quad_in(glquads_t *l, gltex_t *tex)
{
   if (l->n == l->cap)
   {
      const int cap = l->cap ? l->cap * 2 : 8192;
      glvert_t *q = (glvert_t *)realloc(l->v, sizeof(glvert_t) * 4 * cap);
      gltex_t **t;
      if (!q)
         return NULL;
      l->v = q;
      t = (gltex_t **)realloc(l->tex, sizeof(*l->tex) * cap);
      if (!t)
         return NULL;
      l->tex = t;
      {
         uint8_t *f = (uint8_t *)realloc(l->fuzz, cap);
         uint32_t *c;
         if (!f)
            return NULL;
         l->fuzz = f;
         c = (uint32_t *)realloc(l->rgb, sizeof(*c) * cap);
         if (!c)
            return NULL;
         l->rgb = c;
      }
      l->cap = cap;
   }
   l->tex[l->n] = tex;
   l->fuzz[l->n] = 0;
   l->rgb[l->n] = 0;
   return &l->v[4 * l->n++];
}

static glvert_t *gl_quad(gltex_t *tex)
{
   return gl_quad_in(&solid_q, tex);
}

/* ---- prboom-dynlight ---- */

static double gl_angle(angle_t a)
{
   return a * (2 * M_PI / 4294967296.0);
}

/* where a pixel column meets something at depth (along the view) from
 * the eye */
static void gl_ray(int x, double depth, double *px, double *py)
{
   const double len = depth / cos(gl_angle(xtoviewangle[x]));
   const double dir = gl_angle((angle_t)(viewangle + xtoviewangle[x]));
   *px = cos(dir) * len;
   *py = sin(dir) * len;
}

/* Whether a light reaches the wall column of a record, and where the
 * column is: at the depth its scale gives (iscale is map units per row),
 * the wall's normal turned to the eye; a light counts on the side the
 * wall faces, within its radius of the column's span. */
static void gl_wall_lit(const draw_column_vars_t *dc, const seg_t *seg)
{
   const double zk = dc->iscale / 65536.0;
   const double depth = projectiony / 65536.0 * zk;
   double px, py, nx, ny, nl, ztop, zbot;
   int i;

   if (dc->iscale <= 0 || depth < 1 || depth > 8192)
      return;
   gl_ray(dc->x, depth, &px, &py);
   nx = (seg->v2->y - seg->v1->y) / 65536.0;
   ny = -(seg->v2->x - seg->v1->x) / 65536.0;
   nl = sqrt(nx * nx + ny * ny);
   if (nl <= 0)
      return;
   nx /= nl;
   ny /= nl;
   if (-px * nx - py * ny < 0)
   {
      nx = -nx;
      ny = -ny;
   }
   ztop = -(dc->yl - 0.5 - centery) * zk;
   zbot = -(dc->yh + 0.5 - centery) * zk;
   for (i = 0; i < gl_dyn_n; i++)
   {
      const dopo_light_t *l = &gl_dyn[i];
      const double dx = l->x - px, dy = l->y - py;
      const double dz = l->z > ztop ? l->z - ztop : l->z < zbot ? zbot - l->z : 0;
      if (dx * nx + dy * ny > 0 && dx * dx + dy * dy + dz * dz < (double)l->radius * l->radius)
      {
         gl_wlit.on = TRUE;
         gl_wlit.px = (float)px;
         gl_wlit.py = (float)py;
         gl_wlit.zk = zk;
         gl_wlit.nx = (float)nx;
         gl_wlit.ny = (float)ny;
         return;
      }
   }
}

/* a wall quad again, for the lit pass: the column's place, the height of
 * its top and bottom edges (row r's centre is r - centery rows down) */
static void gl_column_lit(gltex_t *t, const glvert_t *q, int y0, int y1)
{
   gllvert_t *v;
   int i;
   if (litw_n == litw_cap)
   {
      const int cap = litw_cap ? litw_cap * 2 : 1024;
      gllvert_t *nv = (gllvert_t *)realloc(litw_v, sizeof(*nv) * 4 * cap);
      gltex_t **nt;
      if (!nv)
         return;
      litw_v = nv;
      nt = (gltex_t **)realloc(litw_t, sizeof(*nt) * cap);
      if (!nt)
         return;
      litw_t = nt;
      litw_cap = cap;
   }
   litw_t[litw_n] = t;
   v = &litw_v[4 * litw_n++];
   for (i = 0; i < 4; i++)
   {
      const int edge = i < 2 ? y0 : y1;
      v[i].v = q[i];
      v[i].p[0] = gl_wlit.px;
      v[i].p[1] = gl_wlit.py;
      v[i].p[2] = (float)(-(edge - 0.5 - centery) * gl_wlit.zk);
      v[i].n[0] = gl_wlit.nx;
      v[i].n[1] = gl_wlit.ny;
      v[i].n[2] = 0;
   }
}

/* A floor rectangle again, for the lit pass, if a light reaches it: each
 * corner where its pixel column meets the plane at the corner's distance;
 * a light counts above a floor (below a ceiling), within its radius of
 * the rectangle. w: the distances at the top and bottom edges. */
static void gl_plane_lit(const glpvert_t *q, const double w[2])
{
   const dbool floor_ = plane_run.yt >= centery;
   const double z = (floor_ ? -1 : 1) * plane_run.height / 65536.0;
   double cx[4], cy[4], minx = 1e30, maxx = -1e30, miny = 1e30, maxy = -1e30;
   glplvert_t *v;
   int i, j, k;

   for (i = 0; i < 2; i++)
      for (j = 0; j < 2; j++)
      {
         const int c = i == 0 ? j : 3 - j;
         gl_ray(j ? plane_run.x2 + 1 : plane_run.x1, w[i], &cx[c], &cy[c]);
         if (cx[c] < minx) minx = cx[c];
         if (cx[c] > maxx) maxx = cx[c];
         if (cy[c] < miny) miny = cy[c];
         if (cy[c] > maxy) maxy = cy[c];
      }
   for (k = 0; k < gl_dyn_n; k++)
   {
      const dopo_light_t *l = &gl_dyn[k];
      const double dz = l->z - z;
      const double dx = l->x < minx ? minx - l->x : l->x > maxx ? l->x - maxx : 0;
      const double dy = l->y < miny ? miny - l->y : l->y > maxy ? l->y - maxy : 0;
      if ((floor_ ? dz > 0 : dz < 0)
            && dx * dx + dy * dy + dz * dz < (double)l->radius * l->radius)
         break;
   }
   if (k == gl_dyn_n)
      return;
   if (litp_n == litp_cap)
   {
      const int cap = litp_cap ? litp_cap * 2 : 256;
      glplvert_t *nv = (glplvert_t *)realloc(litp_v, sizeof(*nv) * 4 * cap);
      gltex_t **nt;
      if (!nv)
         return;
      litp_v = nv;
      nt = (gltex_t **)realloc(litp_t, sizeof(*nt) * cap);
      if (!nt)
         return;
      litp_t = nt;
      litp_cap = cap;
   }
   litp_t[litp_n] = plane_run.tex;
   v = &litp_v[4 * litp_n++];
   for (i = 0; i < 4; i++)
   {
      v[i].v = q[i];
      v[i].p[0] = (float)cx[i];
      v[i].p[1] = (float)cy[i];
      v[i].p[2] = (float)z;
      v[i].n[0] = v[i].n[1] = 0;
      v[i].n[2] = floor_ ? 1.0f : -1.0f;
   }
}

/* the sum of the lights at a sprite's middle, 0xRRGGBB (0: none) */
static uint32_t gl_sprite_lit(const vissprite_t *vis)
{
   double px, py, pz, r = 0, g = 0, b = 0;
   int i;
   if (vis->mobjflags & MF_PLAYERSPRITE)
   {
      px = py = 0;
      pz = -16;           /* the weapon, at the eye */
   }
   else
   {
      px = (vis->gx - viewx) / 65536.0;
      py = (vis->gy - viewy) / 65536.0;
      pz = ((vis->gz / 2 + vis->gzt / 2) - viewz) / 65536.0;
   }
   for (i = 0; i < gl_dyn_n; i++)
   {
      const dopo_light_t *l = &gl_dyn[i];
      const double dx = l->x - px, dy = l->y - py, dz = l->z - pz;
      const double att = 1.0 - sqrt(dx * dx + dy * dy + dz * dz) / l->radius;
      if (att > 0)
      {
         r += l->r * att;
         g += l->g * att;
         b += l->b * att;
      }
   }
   if (r + g + b < 0.02)
      return 0;
   r = r > 1 ? 255 : r * 255;
   g = g > 1 ? 255 : g * 255;
   b = b > 1 ? 255 : b * 255;
   return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b | 0x01000000u;
}

/* a lit program's view, textures and lights */
static void gl_lit_set(const gllit_u_t *u, unsigned width, unsigned height, int offset_x)
{
   GLfloat pos[4 * DOPO_LIGHTS_MAX], col[3 * DOPO_LIGHTS_MAX];
   int i;
   for (i = 0; i < gl_dyn_n; i++)
   {
      pos[4 * i + 0] = gl_dyn[i].x;
      pos[4 * i + 1] = gl_dyn[i].y;
      pos[4 * i + 2] = gl_dyn[i].z;
      pos[4 * i + 3] = gl_dyn[i].radius;
      col[3 * i + 0] = gl_dyn[i].r;
      col[3 * i + 1] = gl_dyn[i].g;
      col[3 * i + 2] = gl_dyn[i].b;
   }
   glUniform2f(u->scale, 2.0f / width, 2.0f / height);
   glUniform2f(u->offset, (float)offset_x, 0.0f);
   glUniform1i(u->tex, 0);
   glUniform1i(u->cmap, 1);
   glUniform1i(u->zlight, 2);
   glUniform1f(u->rows, (float)cmap_gl_rows);
   if (gl_dyn_n)
   {
      glUniform4fv(u->lpos, gl_dyn_n, pos);
      glUniform3fv(u->lcol, gl_dyn_n, col);
   }
   glUniform1i(u->lcount, gl_dyn_n);
   /* the palette alone: the texel at full brightness */
   glUniform1f(u->full, (cmap_rows - 1 + 0.5f) / cmap_gl_rows);
}

/* rows y0..y1-1 of column x, the texel row running from v0 at the top
 * edge to v1 at the bottom edge */
static dbool gl_column(gltex_t *t, int x, int y0, int y1,
                       double v0, double v1, float tc, uint16_t lc)
{
   glvert_t *q = gl_quad(t);
   const float s0 = (float)(v0 / t->gl_width), s1 = (float)(v1 / t->gl_width);
   const int16_t xa = (int16_t)(view_x0 + x), xb = (int16_t)(view_x0 + x + 1);
   const int16_t ya = (int16_t)(view_y0 + y0), yb = (int16_t)(view_y0 + y1);

   if (!q)
      return FALSE;
   q[0].x = xa; q[0].y = ya; q[0].s = s0;
   q[1].x = xb; q[1].y = ya; q[1].s = s0;
   q[2].x = xb; q[2].y = yb; q[2].s = s1;
   q[3].x = xa; q[3].y = yb; q[3].s = s1;
   q[0].t = q[1].t = q[2].t = q[3].t = tc;
   q[0].l = q[1].l = q[2].l = q[3].l = lc;
   if (gl_wlit.on)
      gl_column_lit(t, q, y0, y1);
   return TRUE;
}

static int64_t gl_floordiv(int64_t a, int64_t b)
{
   const int64_t q = a / b;
   return (a % b && (a < 0) != (b < 0)) ? q - 1 : q;
}

/* Pixel y of a column samples frac(y) = texturemid + (y - centery) *
 * iscale (16.16), texel frac >> 16 wrapped to the texture height. The
 * pixel's centre is y + 0.5, so the quad's edges carry frac half a step
 * before the first row and half a step after the last, and the GPU
 * interpolates back to frac(y) at every centre. */
dbool dopo_gl_wall(const draw_column_vars_t *dc, R_DrawColumn_f fn, int gltex,
                   const seg_t *seg)
{
   const rpatch_t *p;
   gltex_t *t;
   ptrdiff_t off;
   int col, row;
   float tc;
   uint16_t lc;
   int64_t f0, period, step;

   /* empty, the drawers skip it too */
   if (dc->yl > dc->yh)
      return TRUE;
   stat_columns++;
   if (!gltex || dc->brightmask || dc->tint || dc->drawingmasked)
   {
      stat_software++;
      return gl_software(dc->x, dc->x + 1, dc->yl, dc->yh);
   }
   t = gl_texture(gltex - 1);
   if (!t)
   {
      stat_software++;
      return gl_software(dc->x, dc->x + 1, dc->yl, dc->yh);
   }
   p = t->patch;
   off = dc->source - p->pixels;
   if (off < 0 || off >= (ptrdiff_t)p->width * p->height || off % p->height
         || (dc->texheight && dc->texheight != p->height))
   {
      stat_software++;
      return gl_software(dc->x, dc->x + 1, dc->yl, dc->yh);
   }
   col = (int)(off / p->height);
   row = R_WallColumnKernelClass(fn) == 1 ? cmap_rows - 1 : gl_colormap_row(dc->colormap);
   if (row < 0)
   {
      stat_software++;
      return gl_software(dc->x, dc->x + 1, dc->yl, dc->yh);
   }
   tc = (float)((col + 0.5) / t->gl_height);
   lc = (uint16_t)(((row + 0.5) / cmap_gl_rows) * 65535.0 + 0.5);
   gl_wlit.on = FALSE;
   if (gl_dyn_n && seg)
      gl_wall_lit(dc, seg);

   if (dc->texheight && !(dc->texheight & (dc->texheight - 1)))
   {
      /* A power of two height: the drawers' texel is frac modulo the
       * height, and their 32-bit frac wraps modulo a multiple of it, so
       * everything can be taken modulo the height. That keeps the
       * coordinate small and is still exact for a far wall whose step
       * overflowed into a negative iscale. One quad, GL_REPEAT wraps. */
      /* in 32 bits like the drawers, the modulo a mask (no 64-bit division,
       * a library call on ARMv7) */
      const uint32_t mask = ((uint32_t)dc->texheight << 16) - 1;
      step = (uint32_t)dc->iscale & mask;
      f0 = ((uint32_t)dc->texturemid
            + (uint32_t)(dc->yl - centery) * (uint32_t)dc->iscale) & mask;
      if (!gl_column(t, dc->x, dc->yl, dc->yh + 1,
                     (f0 - step * 0.5) / 65536.0,
                     (f0 + (dc->yh - dc->yl) * step + step * 0.5) / 65536.0, tc, lc))
      {
         stat_software++;
         return gl_software(dc->x, dc->x + 1, dc->yl, dc->yh);
      }
   }
   else if (dc->iscale <= 0)
   {
      /* a step past 32 bits only stays exact modulo a power of two */
      stat_software++;
      return gl_software(dc->x, dc->x + 1, dc->yl, dc->yh);
   }
   else if (!dc->texheight)
   {
      /* no wrap (a texture that fits the wall): one quad */
      step = dc->iscale;
      f0 = (int64_t)dc->texturemid + (int64_t)(dc->yl - centery) * step;
      if (!gl_column(t, dc->x, dc->yl, dc->yh + 1,
                     (f0 - step * 0.5) / 65536.0,
                     (f0 + (dc->yh - dc->yl) * step + step * 0.5) / 65536.0, tc, lc))
      {
         stat_software++;
         return gl_software(dc->x, dc->x + 1, dc->yl, dc->yh);
      }
   }
   else
   {
      /* another height: one quad per repeat, each moved back into the
       * texture (the drawers wrap frac modulo the height) */
      int ya = dc->yl;
      step = dc->iscale;
      f0 = (int64_t)dc->texturemid + (int64_t)(dc->yl - centery) * step;
      period = (int64_t)dc->texheight << 16;
      while (ya <= dc->yh)
      {
         const int64_t fa = f0 + (int64_t)(ya - dc->yl) * step;
         const int64_t k  = gl_floordiv(fa, period);
         /* first row past this repeat */
         int64_t yb = ya + ((k + 1) * period - fa + step - 1) / step;
         if (yb > dc->yh + 1)
            yb = dc->yh + 1;
         if (!gl_column(t, dc->x, ya, (int)yb,
                        (fa - k * period - step * 0.5) / 65536.0,
                        (fa + (yb - 1 - ya) * step - k * period + step * 0.5) / 65536.0,
                        tc, lc))
         {
            stat_software++;
            return gl_software(dc->x, dc->x + 1, dc->yl, dc->yh);
         }
         ya = (int)yb;
      }
   }
   return TRUE;
}

/* ---- flats ---- */

/* Pixel x of a span samples (xfrac, yfrac) + (x - x1) * (xstep, ystep)
 * (16.16), texel u = xfrac >> 16 & 63, v = yfrac >> 16 & 63: linear
 * along the row, so a one pixel tall quad whose edges carry the
 * coordinates half a step outside the first and last pixels. Both are
 * taken modulo 64 in 32 bits like the drawer, which keeps them small.
 * Only for the spans the light table cannot light (an odd colormap). */
static dbool gl_span_quad(const draw_span_vars_t *ds, gltex_t *t, int row)
{
   glvert_t *q = gl_quad(t);
   const int n = ds->x2 - ds->x1 + 1;
   const double u0 = (double)((uint32_t)ds->xfrac & 0x3fffff);
   const double v0 = (double)((uint32_t)ds->yfrac & 0x3fffff);
   const double du = ds->xstep, dv = ds->ystep;
   const int16_t xa = (int16_t)(view_x0 + ds->x1), xb = (int16_t)(view_x0 + ds->x2 + 1);
   const int16_t ya = (int16_t)(view_y0 + ds->y), yb = (int16_t)(view_y0 + ds->y + 1);

   if (!q)
      return FALSE;
   q[0].x = xa; q[0].y = ya;
   q[1].x = xb; q[1].y = ya;
   q[2].x = xb; q[2].y = yb;
   q[3].x = xa; q[3].y = yb;
   q[0].s = q[3].s = (float)((u0 - du * 0.5) / (65536.0 * 64));
   q[1].s = q[2].s = (float)((u0 + du * (n - 0.5)) / (65536.0 * 64));
   q[0].t = q[3].t = (float)((v0 - dv * 0.5) / (65536.0 * 64));
   q[1].t = q[2].t = (float)((v0 + dv * (n - 0.5)) / (65536.0 * 64));
   q[0].l = q[1].l = q[2].l = q[3].l =
      (uint16_t)(((row + 0.5) / cmap_gl_rows) * 65535.0 + 0.5);
   return TRUE;
}

static void gl_plane_flush(void);

void dopo_gl_plane_begin(fixed_t height, int level)
{
   gl_plane_flush();
   plane_serial++;
   plane_height = height;
   plane_level  = level;
}

/* a plane's distance at view row y, in map units, as R_MapPlane has it */
static double gl_plane_distance(fixed_t height, int y)
{
   return FixedMul(height, yslope[y]) / 65536.0;
}

/* The light table row a span's colormap comes from: the plane's light
 * level when it is the distance band R_MapPlane picked, else a constant
 * row; -1 when the table cannot give it. */
static int gl_plane_lev(const draw_span_vars_t *ds)
{
   const ptrdiff_t rel = ds->colormap - fullcolormap;
   if (!fixedcolormap && zlight)
   {
      unsigned index = (unsigned)(FixedMul(plane_height, yslope[ds->y]) >> LIGHTZSHIFT);
      if (index >= MAXLIGHTZ)
         index = MAXLIGHTZ - 1;
      if (ds->colormap == zlight[plane_level][index])
         return plane_level;
   }
   if (rel == 0)
      return 16;
   if (rel == 32 * 256)
      return 17;
   return -1;
}

static glpvert_t *gl_plane_quad(gltex_t *tex)
{
   if (plane_n == plane_cap)
   {
      const int cap = plane_cap ? plane_cap * 2 : 1024;
      glpvert_t *v = (glpvert_t *)realloc(plane_v, sizeof(glpvert_t) * 4 * cap);
      gltex_t **t;
      if (!v)
         return NULL;
      plane_v = v;
      t = (gltex_t **)realloc(plane_t, sizeof(*plane_t) * cap);
      if (!t)
         return NULL;
      plane_t = t;
      plane_cap = cap;
   }
   plane_t[plane_n] = tex;
   return &plane_v[4 * plane_n++];
}

static double gl_lerp(double a, double b, double f)
{
   return a + (b - a) * f;
}

/* The open run as one quad. Each row's span gives u, v at its first
 * pixel's centre and their steps; the corners sit half a pixel outside.
 * Down the run, 1/distance and u/distance are linear on the screen, so
 * they are extended from the first and last rows' centres to the top and
 * bottom edges. Positions stay where they are: clip w only drives the
 * interpolation. */
static void gl_plane_flush(void)
{
   glpvert_t *q;
   const int n = plane_run.x2 - plane_run.x1 + 1;
   const double ut = plane_run.xf_t / 65536.0, vt = plane_run.yf_t / 65536.0;
   /* the bottom span relative to the top, through any 32-bit wrap */
   const double ub = ut + (int32_t)((uint32_t)plane_run.xf_b - (uint32_t)plane_run.xf_t) / 65536.0;
   const double vb = vt + (int32_t)((uint32_t)plane_run.yf_b - (uint32_t)plane_run.yf_t) / 65536.0;
   const double dut = plane_run.xs_t / 65536.0, dvt = plane_run.ys_t / 65536.0;
   const double dub = plane_run.xs_b / 65536.0, dvb = plane_run.ys_b / 65536.0;
   /* u, v at the left and right edges of the top and bottom rows */
   double u[2][2], v[2][2], w[2], qt, qb, ft, fb, base_u, base_v;
   int i, j;

   if (!plane_run.on)
      return;
   plane_run.on = FALSE;
   q = gl_plane_quad(plane_run.tex);
   if (!q)
      return;
   u[0][0] = ut - 0.5 * dut; u[0][1] = ut + (n - 0.5) * dut;
   v[0][0] = vt - 0.5 * dvt; v[0][1] = vt + (n - 0.5) * dvt;
   u[1][0] = ub - 0.5 * dub; u[1][1] = ub + (n - 0.5) * dub;
   v[1][0] = vb - 0.5 * dvb; v[1][1] = vb + (n - 0.5) * dvb;
   qt = 1.0 / gl_plane_distance(plane_run.height, plane_run.yt);
   qb = 1.0 / gl_plane_distance(plane_run.height, plane_run.yb);
   if (plane_run.yb == plane_run.yt)
      w[0] = w[1] = 1.0 / qt;
   else
   {
      const int rows = plane_run.yb - plane_run.yt;
      double qe[2];
      ft = -0.5 / rows;           /* top edge, from the top row's centre */
      fb = 1.0 + 0.5 / rows;      /* bottom edge */
      qe[0] = gl_lerp(qt, qb, ft);
      qe[1] = gl_lerp(qt, qb, fb);
      for (j = 0; j < 2; j++)
      {
         const double ue0 = gl_lerp(u[0][j] * qt, u[1][j] * qb, ft) / qe[0];
         const double ue1 = gl_lerp(u[0][j] * qt, u[1][j] * qb, fb) / qe[1];
         const double ve0 = gl_lerp(v[0][j] * qt, v[1][j] * qb, ft) / qe[0];
         const double ve1 = gl_lerp(v[0][j] * qt, v[1][j] * qb, fb) / qe[1];
         u[0][j] = ue0; u[1][j] = ue1;
         v[0][j] = ve0; v[1][j] = ve1;
      }
      w[0] = 1.0 / qe[0];
      w[1] = 1.0 / qe[1];
   }
   /* GL_REPEAT: whole tiles off, to keep the floats small */
   base_u = floor(u[0][0] / 64) * 64;
   base_v = floor(v[0][0] / 64) * 64;
   for (i = 0; i < 2; i++)
      for (j = 0; j < 2; j++)
      {
         /* corners 0 1 2 3: top left, top right, bottom right, bottom left */
         glpvert_t *c = &q[i == 0 ? j : 3 - j];
         c->x = (int16_t)(view_x0 + (j ? plane_run.x2 + 1 : plane_run.x1));
         c->y = (int16_t)(view_y0 + (i ? plane_run.yb + 1 : plane_run.yt));
         c->s = (float)((u[i][j] - base_u) / 64);
         c->t = (float)((v[i][j] - base_v) / 64);
         c->w = (float)w[i];
         /* light index = distance >> 4 over a 128 wide table */
         c->z = (float)(w[i] / (16.0 * MAXLIGHTZ));
         c->lev = (uint8_t)plane_run.lev;
         c->base = (uint8_t)plane_run.base;
         c->pad[0] = c->pad[1] = 0;
      }
   if (gl_dyn_n)
      gl_plane_lit(q, w);
}

dbool dopo_gl_span(const draw_span_vars_t *ds, int lump)
{
   gltex_t *t;
   int row, lev, base;

   if (!view_active || drawvars.short_topleft != view_topleft)
      return FALSE;
   if (ds->x1 > ds->x2)
      return TRUE;
   stat_spans++;
   if (!lump || ds->brightmask || r_span_translucent
         || (t = gl_flat(lump - 1)) == NULL
         || (row = gl_colormap_row(ds->colormap)) < 0)
   {
      stat_spans_software++;
      return gl_software(ds->x1, ds->x2 + 1, ds->y, ds->y);
   }
   lev  = gl_plane_lev(ds);
   base = gl_colormap_row(fullcolormap);
   if (lev < 0 || base < 0 || base > 255)
   {
      if (!gl_span_quad(ds, t, row))
      {
         stat_spans_software++;
         return gl_software(ds->x1, ds->x2 + 1, ds->y, ds->y);
      }
      return TRUE;
   }

   /* the next row of the open run? (not near the horizon, where the
    * distance grows too fast to extend half a row) */
   if (plane_run.on && plane_run.serial == plane_serial && plane_run.tex == t
         && plane_run.lev == lev && plane_run.base == base
         && plane_run.x1 == ds->x1 && plane_run.x2 == ds->x2
         && ds->y == plane_run.yb + 1 && abs(ds->y - centery) > 2
         && abs(plane_run.yt - centery) > 2)
   {
      plane_run.yb   = ds->y;
      plane_run.xf_b = ds->xfrac;
      plane_run.yf_b = ds->yfrac;
      plane_run.xs_b = ds->xstep;
      plane_run.ys_b = ds->ystep;
      return TRUE;
   }
   gl_plane_flush();
   plane_run.on     = TRUE;
   plane_run.serial = plane_serial;
   plane_run.tex    = t;
   plane_run.height = plane_height;
   plane_run.lev    = lev;
   plane_run.base   = base;
   plane_run.x1     = ds->x1;
   plane_run.x2     = ds->x2;
   plane_run.yt     = plane_run.yb = ds->y;
   plane_run.xf_t   = plane_run.xf_b = ds->xfrac;
   plane_run.yf_t   = plane_run.yf_b = ds->yfrac;
   plane_run.xs_t   = plane_run.xs_b = ds->xstep;
   plane_run.ys_t   = plane_run.ys_b = ds->ystep;
   return TRUE;
}

/* the light table, from the frame's zlight against its COLORMAP */
static void gl_zlight_update(void)
{
   uint8_t rows[32 * MAXLIGHTZ];
   int l, i;

   memset(rows, 0, sizeof(rows));
   if (zlight && fullcolormap)
      for (l = 0; l < LIGHTLEVELS; l++)
         for (i = 0; i < MAXLIGHTZ; i++)
            rows[l * MAXLIGHTZ + i] = (uint8_t)((zlight[l][i] - fullcolormap) >> 8);
   memset(rows + 17 * MAXLIGHTZ, 32, MAXLIGHTZ);
   if (zlight_valid && !memcmp(rows, zlight_rows, sizeof(rows)))
      return;
   memcpy(zlight_rows, rows, sizeof(rows));
   glBindTexture(GL_TEXTURE_2D, zlight_tex);
   gl_texture_params(GL_CLAMP_TO_EDGE);
   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, MAXLIGHTZ, 32, 0,
                GL_LUMINANCE, GL_UNSIGNED_BYTE, rows);
   zlight_valid = TRUE;
}

static void gl_draw_lit_planes(unsigned width, unsigned height, int offset_x);

static void gl_draw_planes(unsigned width, unsigned height, int offset_x)
{
   int i, first = 0, used_n2 = 0;

   gl_plane_flush();
   if (!plane_n)
      return;
   if (plane_sorted_cap < plane_n)
   {
      glpvert_t *v = (glpvert_t *)realloc(plane_sorted, sizeof(glpvert_t) * 4 * plane_n);
      if (!v)
         return;
      plane_sorted = v;
      plane_sorted_cap = plane_n;
   }
   if (plane_used_cap < plane_n)
   {
      gltex_t **u = (gltex_t **)realloc(plane_used, sizeof(*plane_used) * plane_n);
      if (!u)
         return;
      plane_used = u;
      plane_used_cap = plane_n;
   }
   /* grouped per texture, like the solid quads */
   for (i = 0; i < plane_n; i++)
      if (!plane_t[i]->count++)
         plane_used[used_n2++] = plane_t[i];
   for (i = 0; i < used_n2; i++)
   {
      plane_used[i]->first = first;
      first += plane_used[i]->count;
      plane_used[i]->count = 0;
   }
   for (i = 0; i < plane_n; i++)
   {
      gltex_t *t = plane_t[i];
      memcpy(&plane_sorted[4 * (t->first + t->count++)], &plane_v[4 * i], sizeof(glpvert_t) * 4);
   }

   glUseProgram(plane_program);
   glUniform2f(plane_u_scale, 2.0f / width, 2.0f / height);
   glUniform2f(plane_u_offset, (float)offset_x, 0.0f);
   glUniform1i(plane_u_tex, 0);
   glUniform1i(plane_u_cmap, 1);
   glUniform1i(plane_u_zlight, 2);
   glUniform1f(plane_u_rows, (float)cmap_gl_rows);
   glActiveTexture(GL_TEXTURE0 + 1);
   glBindTexture(GL_TEXTURE_2D, cmap_tex);
   glActiveTexture(GL_TEXTURE0 + 2);
   glBindTexture(GL_TEXTURE_2D, zlight_tex);
   glActiveTexture(GL_TEXTURE0);

   glBindBuffer(GL_ARRAY_BUFFER, plane_vbo);
   glBufferData(GL_ARRAY_BUFFER, sizeof(glpvert_t) * 4 * plane_n, plane_sorted, GL_STREAM_DRAW);
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
   for (i = 0; i < 5; i++)
      glEnableVertexAttribArray(i);
   for (i = 0; i < used_n2; i++)
   {
      gltex_t *t = plane_used[i];
      int start = t->first, count = t->count;
      glBindTexture(GL_TEXTURE_2D, t->id);
      while (count > 0)
      {
         const int chunk = count < GL_MAX_QUADS_PER_DRAW ? count : GL_MAX_QUADS_PER_DRAW;
         const char *b = (const char *)(intptr_t)(start * 4 * sizeof(glpvert_t));
         glVertexAttribPointer(0, 2, GL_SHORT, 0, sizeof(glpvert_t), b);
         glVertexAttribPointer(1, 2, GL_FLOAT, 0, sizeof(glpvert_t), b + 4);
         glVertexAttribPointer(2, 1, GL_FLOAT, 0, sizeof(glpvert_t), b + 12);
         glVertexAttribPointer(3, 1, GL_FLOAT, 0, sizeof(glpvert_t), b + 16);
         glVertexAttribPointer(4, 2, GL_UNSIGNED_BYTE, 0, sizeof(glpvert_t), b + 20);
         glDrawElements(GL_TRIANGLES, chunk * 6, GL_UNSIGNED_SHORT, 0);
         stat_draws++;
         start += chunk;
         count -= chunk;
      }
      t->count = 0;
   }
   for (i = 1; i < 5; i++)
      glDisableVertexAttribArray(i);
   stat_planes += plane_n;
   plane_n = 0;
   if (litp_n)
      gl_draw_lit_planes(width, height, offset_x);
}

/* ---- sprites and masked textures ---- */

/* the patch of a sprite lump, laid out like a composite (column x is GL
 * row x); its transparent texels are never drawn, only its posts, or with
 * prboom-scale its columns over a texture with coverage */
static gltex_t *gl_patch(int lump, const rpatch_t *p)
{
   gltex_t *t;
   if (lump < 0 || lump >= patchcache_n)
      return NULL;
   t = &patchcache[lump];
   if (!t->id && !t->bad)
   {
      if (dopo_scale)
         gl_alpha_upload(t, p, dopo_scale);
      else
         gl_texture_upload(t, p);
   }
   return t->bad ? NULL : t;
}

/* a sprite's colour for its corona: its texels through the game's palette,
 * the brighter ones weighing more, brought up to full brightness */
static uint32_t gl_corona_colour(gltex_t *t, const rpatch_t *p)
{
   const uint8_t *pal;
   double r = 0, g = 0, b = 0, w = 0, top;
   int x, i, y;

   if (t->glow_valid)
      return t->glow_rgb;
   t->glow_valid = TRUE;
   t->glow_rgb = 0xffc080;
   pal = (const uint8_t *)W_CacheLumpName("PLAYPAL");
   if (!pal)
      return t->glow_rgb;
   for (x = 0; x < p->width; x++)
      for (i = 0; i < p->columns[x].numPosts; i++)
      {
         const rpost_t *post = &p->columns[x].posts[i];
         for (y = post->topdelta; y < post->topdelta + post->length && y < p->height; y++)
         {
            const uint8_t *c = pal + 3 * p->columns[x].pixels[y];
            const double l = (c[0] * 0.3 + c[1] * 0.59 + c[2] * 0.11) / 255.0;
            const double k = l * l * l;
            r += c[0] * k;
            g += c[1] * k;
            b += c[2] * k;
            w += k;
         }
      }
   W_UnlockLumpName("PLAYPAL");
   if (w > 0)
   {
      r /= w;
      g /= w;
      b /= w;
      top = r > g ? (r > b ? r : b) : (g > b ? g : b);
      if (top > 0)
      {
         r = r * 255 / top;
         g = g * 255 / top;
         b = b * 255 / top;
         t->glow_rgb = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
      }
   }
   return t->glow_rgb;
}

dbool dopo_gl_masked_begin(const rpatch_t *patch, int lump, int texnum,
                           R_DrawColumn_f colfunc, dbool plain, dbool fuzz,
                           const vissprite_t *vis, int x1, int x2)
{
   const int cls = R_WallColumnKernelClass(colfunc);
   const int style = vis && vis->colormap ? vis->translucent : 0;
   int alpha = vis ? vis->alpha : 0;
   const dbool emissive = vis && vis->emissive;
   gltex_t *t = NULL;

   masked_tex = NULL;
   masked_fuzz = FALSE;
   masked_kind = GL_KIND_PLAIN;
   masked_corona.on = FALSE;
   masked_light = 0;
   if (!view_active || drawvars.short_topleft != view_topleft)
      return FALSE;
   stat_masked++;
   if (plain && patch && (cls == 1 || cls == 2 || fuzz))
   {
      if (lump >= 0)
         t = gl_patch(lump, patch);
      else if (texnum >= 0 && texnum < texcache_n)
      {
         t = &texcache[texnum];
         if (!t->id && !t->bad)
            gl_texture_upload(t, patch);
         if (t->bad)
            t = NULL;
      }
   }
   if (!t)
   {
      /* the CPU draws this one; a sprite's rows from its projection (its
       * direct paths skip R_DrawMaskedColumn), a masked texture's rows as
       * its posts go by (dopo_gl_masked_post) */
      stat_masked_software++;
      view_software = TRUE;
      if (lump >= 0 && patch)
      {
         const int64_t top = sprtopscreen, bottom = top + (int64_t)spryscale * patch->height;
         gl_software(x1, x2 + 1, (int)(top >> FRACBITS), (int)(bottom >> FRACBITS));
      }
      return FALSE;
   }
   masked_tex   = t;
   masked_patch = patch;
   masked_fuzz  = fuzz;
   if (alpha < 0)
      alpha = 0;
   if (alpha > 32)
      alpha = 32;
   masked_kind = fuzz ? GL_KIND_FUZZ
               : style == 2 ? (uint8_t)(GL_KIND_ADD + alpha)
               : style ? (uint8_t)(GL_KIND_LERP + alpha) : GL_KIND_PLAIN;
   /* full bright frames shine on their own, as in GZDoom */
   if (gl_dyn_n && vis && masked_kind == GL_KIND_PLAIN && !emissive)
      masked_light = gl_sprite_lit(vis);

   /* a glowing sprite's corona, if its middle shows: a halo a little
    * bigger than the sprite, centred on it */
   if (dopo_glow == 2 && style == 2 && emissive && lump >= 0 && corona_tex.id)
   {
      const int64_t top = sprtopscreen, bottom = top + (int64_t)spryscale * patch->height;
      const int cx = (x1 + x2) / 2;
      const int cy = (int)(((top + bottom) / 2) >> FRACBITS);
      const int h = (int)((bottom - top) >> FRACBITS), w = x2 - x1 + 1;
      const int r = (h > w ? h : w) * 9 / 10 + 1;
      if (cx >= 0 && cx < viewwidth && cy > mceilingclip[cx] && cy < mfloorclip[cx])
      {
         masked_corona.on  = TRUE;
         masked_corona.x0  = cx - r;
         masked_corona.x1  = cx + r + 1;
         masked_corona.y0  = cy - r;
         masked_corona.y1  = cy + r + 1;
         masked_corona.rgb = gl_corona_colour(t, patch);
      }
   }
   return TRUE;
}

void dopo_gl_masked_end(void)
{
   /* the corona over its sprite, cut to the view */
   if (masked_corona.on)
   {
      const int x0 = masked_corona.x0 < 0 ? 0 : masked_corona.x0;
      const int y0 = masked_corona.y0 < 0 ? 0 : masked_corona.y0;
      const int x1 = masked_corona.x1 > viewwidth ? viewwidth : masked_corona.x1;
      const int y1 = masked_corona.y1 > viewheight ? viewheight : masked_corona.y1;
      const double w = masked_corona.x1 - masked_corona.x0;
      const double h = masked_corona.y1 - masked_corona.y0;
      glvert_t *q;
      if (x0 < x1 && y0 < y1 && (q = gl_quad_in(&masked_q, &corona_tex)) != NULL)
      {
         const float s0 = (float)((x0 - masked_corona.x0) / w), s1 = (float)((x1 - masked_corona.x0) / w);
         const float t0 = (float)((y0 - masked_corona.y0) / h), t1 = (float)((y1 - masked_corona.y0) / h);
         q[0].x = q[3].x = (int16_t)(view_x0 + x0);
         q[1].x = q[2].x = (int16_t)(view_x0 + x1);
         q[0].y = q[1].y = (int16_t)(view_y0 + y0);
         q[2].y = q[3].y = (int16_t)(view_y0 + y1);
         q[0].s = q[3].s = s0;
         q[1].s = q[2].s = s1;
         q[0].t = q[1].t = t0;
         q[2].t = q[3].t = t1;
         q[0].l = q[1].l = q[2].l = q[3].l = 0;
         masked_q.fuzz[masked_q.n - 1] = GL_KIND_CORONA;
         masked_q.rgb[masked_q.n - 1] = masked_corona.rgb;
      }
   }
   masked_corona.on = FALSE;
   masked_tex = NULL;
   masked_fuzz = FALSE;
   masked_kind = GL_KIND_PLAIN;
   masked_light = 0;
}

/* the colour table row of a masked column */
static int gl_masked_row(const draw_column_vars_t *dc, R_DrawColumn_f fn)
{
   /* a fuzzy thing has no colormap: its own colours, full bright */
   return masked_fuzz ? gl_colormap_row(fullcolormap)
        : R_WallColumnKernelClass(fn) == 1 ? cmap_rows - 1 : gl_colormap_row(dc->colormap);
}

/* A sprite column over a texture with coverage (prboom-scale): one quad
 * from two texels above its first post to two below its last (each
 * Scale2x pass can reach a little past the posts), clipped like the posts; the texture's
 * coverage draws the outline. Pixel y samples frac(y) = texturemid +
 * (y - centery) * iscale from the patch's top, column texu (16.16) picks
 * one of the two texture columns of each patch column. */
void dopo_gl_masked_column(const rpatch_t *patch, const draw_column_vars_t *dc,
                           const rcolumn_t *column, R_DrawColumn_f fn)
{
   gltex_t *t = masked_tex;
   const int64_t step = dc->iscale;
   int64_t top, bottom, u;
   int t0, t1, yl, yh, row;
   double f0;
   glvert_t *q;
   float s0, s1, tc;

   if (!t || !t->alpha || !column->numPosts || step <= 0)
      return;
   t0 = column->posts[0].topdelta - 2;
   t1 = column->posts[column->numPosts - 1].topdelta
      + column->posts[column->numPosts - 1].length + 2;
   if (t0 < 0)
      t0 = 0;
   if (t1 > patch->height)
      t1 = patch->height;
   top    = (int64_t)sprtopscreen + (int64_t)spryscale * t0;
   bottom = (int64_t)sprtopscreen + (int64_t)spryscale * t1;
   yl = (int)((top + FRACUNIT - 1) >> FRACBITS);
   yh = (int)((bottom - 1) >> FRACBITS);
   if (yh >= mfloorclip[dc->x])
      yh = mfloorclip[dc->x] - 1;
   if (yl <= mceilingclip[dc->x])
      yl = mceilingclip[dc->x] + 1;
   if (yl < 0)
      yl = 0;
   if (yh >= viewheight)
      yh = viewheight - 1;
   row = gl_masked_row(dc, fn);
   if (yl > yh || row < 0 || (q = gl_quad_in(&masked_q, t)) == NULL)
      return;
   u = dc->texu;
   if (u < 0)
      u = 0;
   if (u > ((int64_t)patch->width << FRACBITS) - 1)
      u = ((int64_t)patch->width << FRACBITS) - 1;
   f0 = (double)dc->texturemid + (double)(yl - centery) * step;
   s0 = (float)((f0 - step * 0.5) / 65536.0 / t->gl_width);
   s1 = (float)((f0 + (yh - yl + 0.5) * step) / 65536.0 / t->gl_width);
   tc = (float)(u / 65536.0 / t->gl_height);
   q[0].x = q[3].x = (int16_t)(view_x0 + dc->x);
   q[1].x = q[2].x = (int16_t)(view_x0 + dc->x + 1);
   q[0].y = q[1].y = (int16_t)(view_y0 + yl);
   q[2].y = q[3].y = (int16_t)(view_y0 + yh + 1);
   q[0].s = q[1].s = s0;
   q[2].s = q[3].s = s1;
   q[0].t = q[1].t = q[2].t = q[3].t = tc;
   q[0].l = q[1].l = q[2].l = q[3].l =
      (uint16_t)(((row + 0.5) / cmap_gl_rows) * 65535.0 + 0.5);
   masked_q.fuzz[masked_q.n - 1] = masked_kind;
   masked_q.rgb[masked_q.n - 1] = masked_light;
}

/* A post, clipped by R_DrawMaskedColumn: pixel y samples frac(y) =
 * texturemid + (y - centery) * iscale from the post's first texel, like a
 * wall column; the post sits topdelta texels down its patch column. */
dbool dopo_gl_masked_post(const draw_column_vars_t *dc, R_DrawColumn_f fn)
{
   const rpatch_t *p = masked_patch;
   gltex_t *t = masked_tex;
   glvert_t *q;
   ptrdiff_t off;
   int col, row, y;
   double base, f0;
   const int64_t step = dc->iscale;
   float s0, s1, tc;
   uint16_t lc;

   if (!t)
   {
      if (view_active && drawvars.short_topleft == view_topleft)
         gl_software(dc->x, dc->x + 1, dc->yl, dc->yh);
      return FALSE;
   }
   if (t->alpha)
   {
      /* its column is already queued (dopo_gl_masked_column) */
      if (view_software)
      {
         uint16_t *dest = view_topleft + (ptrdiff_t)dc->yl * SURFACE_SHORT_PITCH + dc->x;
         for (y = dc->yl; y <= dc->yh; y++, dest += SURFACE_SHORT_PITCH)
            *dest = dopo_gl_keys[0];
      }
      stat_posts++;
      return TRUE;
   }
   off = dc->source - p->pixels;
   row = gl_masked_row(dc, fn);
   if (off < 0 || off >= (ptrdiff_t)p->width * p->height || step <= 0 || row < 0
         || (q = gl_quad_in(&masked_q, t)) == NULL)
      return gl_software(dc->x, dc->x + 1, dc->yl, dc->yh);
   col  = (int)(off / p->height);
   base = (double)(off % p->height);
   f0   = (double)dc->texturemid + (double)(dc->yl - centery) * step;
   s0   = (float)((base + (f0 - step * 0.5) / 65536.0) / t->gl_width);
   s1   = (float)((base + (f0 + (dc->yh - dc->yl + 0.5) * step) / 65536.0) / t->gl_width);
   tc   = (float)((col + 0.5) / t->gl_height);
   lc   = (uint16_t)(((row + 0.5) / cmap_gl_rows) * 65535.0 + 0.5);
   q[0].x = q[3].x = (int16_t)(view_x0 + dc->x);
   q[1].x = q[2].x = (int16_t)(view_x0 + dc->x + 1);
   q[0].y = q[1].y = (int16_t)(view_y0 + dc->yl);
   q[2].y = q[3].y = (int16_t)(view_y0 + dc->yh + 1);
   q[0].s = q[1].s = s0;
   q[2].s = q[3].s = s1;
   q[0].t = q[1].t = q[2].t = q[3].t = tc;
   q[0].l = q[1].l = q[2].l = q[3].l = lc;
   masked_q.fuzz[masked_q.n - 1] = masked_kind;
   masked_q.rgb[masked_q.n - 1] = masked_light;

   /* over something the CPU drew, the post has to win: key under it */
   if (view_software)
   {
      uint16_t *dest = view_topleft + (ptrdiff_t)dc->yl * SURFACE_SHORT_PITCH + dc->x;
      for (y = dc->yl; y <= dc->yh; y++, dest += SURFACE_SHORT_PITCH)
         *dest = dopo_gl_keys[0];
   }
   stat_posts++;
   return TRUE;
}

/* ---- 2D ----
 *
 * The status bar, the HUD, menus, text and full-screen pages, drawn by the
 * GPU in the order the CPU asks for them, over the 3D view: a patch is one
 * quad (its transparent texels discarded), a filled rectangle a quad of
 * one colour, and an image the CPU made (a cached page, a background, the
 * status bar's back screen) a copy of a texture that mirrors it, sent
 * again only when the CPU changes it. Nothing is left in the software
 * frame, which then stays all key; what the CPU still draws there (the
 * automap, ACS screens) shows through it as before. Not with the 4:3 game
 * in its 16:9 frame (the frame is another buffer), where the CPU draws the
 * 2D as before. */

static uint16_t             pal_row[256];
static dbool                pal_valid = FALSE;

/* colour translations: rows of trans_tex, row 0 the identity */
#define GL2D_TRANS 64
static const uint8_t       *trans_keys[GL2D_TRANS];
static int                  trans_n = 0;

/* patches with their coverage, by their rpatch_t (patches and composites
 * live in arrays that do not move while a set of wads is loaded) */
#define GL2D_PATCHES 4096
static struct
{
   const rpatch_t *key;
   gltex_t         tex;
} gl2d_patches[GL2D_PATCHES];
static lumpinfo_t          *gl2d_patches_owner = NULL;

/* textures mirroring CPU images */
#define GL2D_MIRRORS 16
static struct
{
   const void *key;
   GLuint      id;
   int         w, h;
   dbool       dirty;
   unsigned    used;
} gl2d_mirrors[GL2D_MIRRORS];
static unsigned             gl2d_clock = 0;

static void gl_draw_range(GLuint tex, int first, int count);

static void gl2d_forget(void)
{
   int i;
   for (i = 0; i < GL2D_PATCHES; i++)
      gl2d_patches[i].tex.id = 0;
   for (i = 0; i < GL2D_MIRRORS; i++)
   {
      gl2d_mirrors[i].id = 0;
      gl2d_mirrors[i].key = NULL;
   }
   trans_n = 0;
   pal_valid = FALSE;
}

static void gl2d_destroy(void)
{
   int i;
   for (i = 0; i < GL2D_PATCHES; i++)
      if (gl2d_patches[i].tex.id)
         glDeleteTextures(1, &gl2d_patches[i].tex.id);
   for (i = 0; i < GL2D_MIRRORS; i++)
      if (gl2d_mirrors[i].id)
         glDeleteTextures(1, &gl2d_mirrors[i].id);
   glDeleteTextures(1, &pal_tex);
   glDeleteTextures(1, &trans_tex);
   glDeleteTextures(1, &ramp_tex);
}

/* the palette, the translation table and the ramp a filled rectangle
 * samples its one colour from */
static void gl2d_context(void)
{
   uint8_t ramp[256 * 2];
   int i;

   gl2d_forget();
   glGenTextures(1, &pal_tex);
   glGenTextures(1, &trans_tex);
   glGenTextures(1, &ramp_tex);
   for (i = 0; i < 256; i++)
   {
      ramp[i * 2] = (uint8_t)i;
      ramp[i * 2 + 1] = 255;
   }
   glBindTexture(GL_TEXTURE_2D, ramp_tex);
   gl_texture_params(GL_CLAMP_TO_EDGE);
   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, 256, 1, 0,
                GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, ramp);
   glBindTexture(GL_TEXTURE_2D, trans_tex);
   gl_texture_params(GL_CLAMP_TO_EDGE);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 256, GL2D_TRANS, 0,
                GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
   {
      uint8_t identity[256];
      for (i = 0; i < 256; i++)
         identity[i] = (uint8_t)i;
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1, GL_LUMINANCE, GL_UNSIGNED_BYTE, identity);
   }
   trans_n = 1;
   trans_keys[0] = NULL;
}

void dopo_gl_frame_begin(void)
{
   gl2d_on = FALSE;
   gl2d_frame = screens[0].data;
   two_q.n = 0;
   if (!dopo_gl || !gl_ready || VID_TRUECOLOR || !dopo_widescreen || !screens[0].data)
      return;
   gl2d_on = TRUE;
   gl_texcache_check();
   gl_colormap_update();
   if (gl2d_patches_owner != lumpinfo || gl2d_patches_scale != dopo_scale)
   {
      int i;
      for (i = 0; i < GL2D_PATCHES; i++)
      {
         if (gl2d_patches[i].tex.id)
            glDeleteTextures(1, &gl2d_patches[i].tex.id);
         gl2d_patches[i].key = NULL;
         memset(&gl2d_patches[i].tex, 0, sizeof(gltex_t));
      }
      gl2d_patches_owner = lumpinfo;
      gl2d_patches_scale = dopo_scale;
   }

   /* the whole frame keyed: everything the CPU no longer draws */
   if (!view_keyed || keyed_key != dopo_gl_keys[0]
         || keyed_topleft != (uint16_t *)screens[0].data || keyed_x0 || keyed_y0
         || keyed_w != SCREENWIDTH || keyed_h != SCREENHEIGHT)
   {
      const uint16_t key = dopo_gl_keys[0];
      uint16_t *p = (uint16_t *)screens[0].data;
      size_t i;
      const size_t n = (size_t)SCREENWIDTH * SCREENHEIGHT;
      view_keyed    = TRUE;
      keyed_key     = key;
      keyed_topleft = p;
      keyed_x0      = keyed_y0 = 0;
      keyed_w       = SCREENWIDTH;
      keyed_h       = SCREENHEIGHT;
      stat_full_fills++;
      if ((key >> 8) == (key & 0xff))
         memset(p, key & 0xff, n * 2);
      else
         for (i = 0; i < n; i++)
            p[i] = key;
   }
   frame_has_view = TRUE;
}

void dopo_gl_2d_suspend(dbool suspend)
{
   gl2d_suspended += suspend ? 1 : -1;
}

/* a draw on screen 0, the frame being built, that the GPU takes */
static dbool gl2d_target(int scrn)
{
   return gl2d_on && !gl2d_suspended && scrn == 0 && screens[0].data == gl2d_frame;
}

/* the row of a colour translation, 0 when none or no room */
static int gl2d_trans(const uint8_t *trans)
{
   int i;
   if (!trans)
      return 0;
   for (i = 1; i < trans_n; i++)
      if (trans_keys[i] == trans)
         return i;
   if (trans_n == GL2D_TRANS)
      return -1;
   glBindTexture(GL_TEXTURE_2D, trans_tex);
   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
   glTexSubImage2D(GL_TEXTURE_2D, 0, 0, trans_n, 256, 1, GL_LUMINANCE, GL_UNSIGNED_BYTE, trans);
   trans_keys[trans_n] = trans;
   return trans_n++;
}

/* a patch with its coverage (gl_alpha_upload), by its rpatch_t */
static gltex_t *gl2d_patch(const rpatch_t *p)
{
   unsigned h = (unsigned)(((uintptr_t)p >> 4) * 2654435761u) % GL2D_PATCHES;
   int probes;
   gltex_t *t;

   for (probes = 0; probes < GL2D_PATCHES; probes++, h = (h + 1) % GL2D_PATCHES)
      if (gl2d_patches[h].key == p || !gl2d_patches[h].key)
         break;
   if (probes == GL2D_PATCHES)
      return NULL;
   t = &gl2d_patches[h].tex;
   if (gl2d_patches[h].key == p)
      return t->bad ? NULL : t;
   gl2d_patches[h].key = p;
   memset(t, 0, sizeof(*t));
   gl_alpha_upload(t, p, dopo_scale);
   return t->bad ? NULL : t;
}

/* corners top left, top right, bottom right, bottom left */
static dbool gl2d_quad(gltex_t *tex, uint8_t kind, int x0, int y0, int x1, int y1,
                       const float st[4][2], int trans_row)
{
   glvert_t *q;
   int i;
   if (x0 >= x1 || y0 >= y1 || (q = gl_quad_in(&two_q, tex)) == NULL)
      return FALSE;
   q[0].x = q[3].x = (int16_t)x0;
   q[1].x = q[2].x = (int16_t)x1;
   q[0].y = q[1].y = (int16_t)y0;
   q[2].y = q[3].y = (int16_t)y1;
   for (i = 0; i < 4; i++)
   {
      q[i].s = st[i][0];
      q[i].t = st[i][1];
      q[i].l = (uint16_t)(((trans_row + 0.5) / GL2D_TRANS) * 65535.0 + 0.5);
   }
   two_q.fuzz[two_q.n - 1] = kind;
   return TRUE;
}

/* V_DrawMemPatch: pixel x of the patch's screen rectangle samples column
 * (x - left) * dxi (16.16, mirrored when flipped), row y samples row
 * (y - top) * dyi; the corners carry them half a pixel outside, so the GPU
 * gets them back at each pixel's centre */
dbool dopo_gl_2d_patch(const rpatch_t *patch, int left, int top, int right, int bottom,
                       int dxi, int dyi, dbool flip, const uint8_t *trans)
{
   gltex_t *t;
   double u0, u1, v0, v1;
   float st[4][2];
   int row;

   if (!gl2d_target(0) || !patch || patch->width <= 0 || patch->height <= 0)
      return FALSE;
   row = gl2d_trans(trans);
   if (row < 0 || (t = gl2d_patch(patch)) == NULL)
      return FALSE;
   u0 = -0.5 * dxi / 65536.0;
   u1 = (right - left - 0.5) * dxi / 65536.0;
   v0 = -0.5 * dyi / 65536.0;
   v1 = (bottom - top - 0.5) * dyi / 65536.0;
   if (flip)
   {
      u0 = patch->width - u0;
      u1 = patch->width - u1;
   }
   /* s runs down a column (GL width = rows), t across the columns */
   st[0][0] = st[1][0] = (float)(v0 / t->gl_width);
   st[2][0] = st[3][0] = (float)(v1 / t->gl_width);
   st[0][1] = st[3][1] = (float)(u0 / t->gl_height);
   st[1][1] = st[2][1] = (float)(u1 / t->gl_height);
   gl2d_quad(t, 0, left, top, right, bottom, (const float (*)[2])st, row);
   return TRUE;
}

/* V_FillRect: one colour, from the ramp */
dbool dopo_gl_2d_fill(int x, int y, int width, int height, int colour)
{
   static gltex_t ramp;
   float st[4][2];
   int i;

   if (!gl2d_target(0))
      return FALSE;
   ramp.id = ramp_tex;
   for (i = 0; i < 4; i++)
   {
      st[i][0] = (colour + 0.5f) / 256.0f;
      st[i][1] = 0.5f;
   }
   gl2d_quad(&ramp, 0, x, y, x + width, y + height, (const float (*)[2])st, 0);
   return TRUE;
}

static int gl2d_mirror(const void *key, int w, int h)
{
   int i, oldest = 0;
   for (i = 0; i < GL2D_MIRRORS; i++)
      if (gl2d_mirrors[i].key == key && gl2d_mirrors[i].w == w && gl2d_mirrors[i].h == h)
      {
         gl2d_mirrors[i].used = ++gl2d_clock;
         return i;
      }
   for (i = 1; i < GL2D_MIRRORS; i++)
      if (gl2d_mirrors[i].used < gl2d_mirrors[oldest].used)
         oldest = i;
   if (!gl2d_mirrors[oldest].id)
      glGenTextures(1, &gl2d_mirrors[oldest].id);
   glBindTexture(GL_TEXTURE_2D, gl2d_mirrors[oldest].id);
   gl_texture_params(GL_CLAMP_TO_EDGE);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL);
   gl2d_mirrors[oldest].key   = key;
   gl2d_mirrors[oldest].w     = w;
   gl2d_mirrors[oldest].h     = h;
   gl2d_mirrors[oldest].dirty = TRUE;
   gl2d_mirrors[oldest].used  = ++gl2d_clock;
   return oldest;
}

void dopo_gl_2d_image_dirty(const void *key)
{
   int i;
   for (i = 0; i < GL2D_MIRRORS; i++)
      if (gl2d_mirrors[i].key == key)
         gl2d_mirrors[i].dirty = TRUE;
}

void dopo_gl_2d_screen_dirty(int scrn)
{
   if (dopo_gl && scrn > 0 && scrn < NUM_SCREENS && screens[scrn].data)
      dopo_gl_2d_image_dirty(screens[scrn].data);
}

/* Part (sx, sy, w, h) of a w_img x h_img RGB565 image the CPU made, to
 * (dx, dy) on screen 0; the image is sent only when it changed. */
dbool dopo_gl_2d_image(const void *image, int w_img, int h_img,
                       int sx, int sy, int w, int h, int dx, int dy)
{
   /* a texture holder per mirror: runs of copies compare holders */
   static gltex_t holders[GL2D_MIRRORS];
   float st[4][2];
   int m;

   if (!gl2d_target(0) || !image || w_img <= 0 || h_img <= 0
         || w_img > max_texture_size || h_img > max_texture_size)
      return FALSE;
   m = gl2d_mirror(image, w_img, h_img);
   if (gl2d_mirrors[m].dirty)
   {
      glBindTexture(GL_TEXTURE_2D, gl2d_mirrors[m].id);
      glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w_img, h_img, GL_RGB,
                      GL_UNSIGNED_SHORT_5_6_5, image);
      gl2d_mirrors[m].dirty = FALSE;
      stat_mirror_pixels += (long)w_img * h_img;
   }
   holders[m].id = gl2d_mirrors[m].id;
   st[0][0] = st[3][0] = (float)sx / w_img;
   st[1][0] = st[2][0] = (float)(sx + w) / w_img;
   st[0][1] = st[1][1] = (float)sy / h_img;
   st[2][1] = st[3][1] = (float)(sy + h) / h_img;
   gl2d_quad(&holders[m], 1, dx, dy, dx + w, dy + h, (const float (*)[2])st, 0);
   return TRUE;
}

dbool dopo_gl_2d_image_full(const void *image)
{
   return dopo_gl_2d_image(image, SCREENWIDTH, SCREENHEIGHT, 0, 0,
                           SCREENWIDTH, SCREENHEIGHT, 0, 0);
}

/* V_CopyRect from a back screen to screen 0 */
dbool dopo_gl_2d_copy(int srcscrn, int sx, int sy, int w, int h, int dx, int dy)
{
   int sh;
   if (srcscrn <= 0 || srcscrn >= NUM_SCREENS || !screens[srcscrn].data)
      return FALSE;
   sh = screens[srcscrn].height > 0 ? screens[srcscrn].height : SCREENHEIGHT;
   if (sx < 0 || sy < 0 || sx + w > SCREENWIDTH || sy + h > sh)
      return FALSE;
   return dopo_gl_2d_image(screens[srcscrn].data, SCREENWIDTH, sh, sx, sy, w, h, dx, dy);
}

static void gl2d_palette(void)
{
   uint16_t row[256];
   int i;
   for (i = 0; i < 256; i++)
      row[i] = V_Palette16[i * VID_NUMCOLORWEIGHTS + (VID_NUMCOLORWEIGHTS - 1)];
   if (pal_valid && !memcmp(row, pal_row, sizeof(row)))
      return;
   memcpy(pal_row, row, sizeof(row));
   glBindTexture(GL_TEXTURE_2D, pal_tex);
   gl_texture_params(GL_CLAMP_TO_EDGE);
   glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, 1, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, row);
   pal_valid = TRUE;
}

/* the 2D in the order it came, one draw per run of the same texture */
static void gl_draw_2d(unsigned width, unsigned height)
{
   int i;

   if (!two_q.n)
      return;
   gl2d_palette();
   glBindBuffer(GL_ARRAY_BUFFER, two_vbo);
   glBufferData(GL_ARRAY_BUFFER, sizeof(glvert_t) * 4 * two_q.n, two_q.v, GL_STREAM_DRAW);
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
   glEnableVertexAttribArray(0);
   glEnableVertexAttribArray(1);
   glEnableVertexAttribArray(2);
   glActiveTexture(GL_TEXTURE0 + 1);
   glBindTexture(GL_TEXTURE_2D, trans_tex);
   glActiveTexture(GL_TEXTURE0 + 2);
   glBindTexture(GL_TEXTURE_2D, pal_tex);
   glActiveTexture(GL_TEXTURE0);
   for (i = 0; i < two_q.n; )
   {
      int j = i + 1;
      while (j < two_q.n && two_q.tex[j] == two_q.tex[i] && two_q.fuzz[j] == two_q.fuzz[i])
         j++;
      if (two_q.fuzz[i])
      {
         glUseProgram(copy_program);
         glUniform2f(copy_u_scale, 2.0f / width, 2.0f / height);
         glUniform2f(copy_u_offset, 0, 0);
         glUniform1i(copy_u_tex, 0);
      }
      else
      {
         glUseProgram(two_program);
         glUniform2f(two_u_scale, 2.0f / width, 2.0f / height);
         glUniform2f(two_u_offset, 0, 0);
         glUniform1i(two_u_tex, 0);
         glUniform1i(two_u_trans, 1);
         glUniform1i(two_u_pal, 2);
      }
      gl_draw_range(two_q.tex[i]->id, i, j - i);
      i = j;
   }
   glDisableVertexAttribArray(1);
   glDisableVertexAttribArray(2);
   stat_2d_quads += two_q.n;
   two_q.n = 0;
}

/* ---- presenting ---- */

/* the solid quads grouped per texture (in the order the textures first
 * showed up), then the masked quads as they came */
static int gl_sort_quads(void)
{
   const int total = solid_q.n + masked_q.n;
   int i, first = 0;

   if (sorted_cap < total)
   {
      glvert_t *v = (glvert_t *)realloc(sorted, sizeof(glvert_t) * 4 * total);
      if (!v)
         return 0;
      sorted = v;
      sorted_cap = total;
   }
   if (used_cap < solid_q.n)
   {
      gltex_t **u = (gltex_t **)realloc(used, sizeof(*used) * solid_q.n);
      if (!u)
         return 0;
      used = u;
      used_cap = solid_q.n;
   }
   used_n = 0;
   for (i = 0; i < solid_q.n; i++)
   {
      gltex_t *t = solid_q.tex[i];
      if (!t->count++)
         used[used_n++] = t;
   }
   for (i = 0; i < used_n; i++)
   {
      gltex_t *t = used[i];
      t->first = first;
      first += t->count;
      t->count = 0;
   }
   for (i = 0; i < solid_q.n; i++)
   {
      gltex_t *t = solid_q.tex[i];
      memcpy(&sorted[4 * (t->first + t->count++)], &solid_q.v[4 * i], sizeof(glvert_t) * 4);
   }
   if (masked_q.n)
      memcpy(&sorted[4 * solid_q.n], masked_q.v, sizeof(glvert_t) * 4 * masked_q.n);
   return 1;
}

static void gl_draw_range(GLuint tex, int first, int count)
{
   glBindTexture(GL_TEXTURE_2D, tex);
   while (count > 0)
   {
      const int chunk = count < GL_MAX_QUADS_PER_DRAW ? count : GL_MAX_QUADS_PER_DRAW;
      const char *base = (const char *)(intptr_t)(first * 4 * sizeof(glvert_t));
      glVertexAttribPointer(0, 2, GL_SHORT, 0, sizeof(glvert_t), base);
      glVertexAttribPointer(1, 2, GL_FLOAT, 0, sizeof(glvert_t), base + 4);
      glVertexAttribPointer(2, 1, GL_UNSIGNED_SHORT, 1, sizeof(glvert_t), base + 12);
      glDrawElements(GL_TRIANGLES, chunk * 6, GL_UNSIGNED_SHORT, 0);
      stat_draws++;
      first += chunk;
      count -= chunk;
   }
}

/* Into and out of a fuzzy thing's look (prboom-spectre). The blends only
 * read what is behind inside the GPU's tile, so they cost nothing more;
 * the frame's alpha stays the clear's. */
static void gl_fuzz_state(unsigned width, unsigned height, int offset_x, dbool on)
{
   if (!on)
   {
      glDisable(GL_BLEND);
      glBlendEquation(GL_FUNC_ADD);
      glColorMask(1, 1, 1, 1);
      glUseProgram(wall_program);
      return;
   }
   glUseProgram(fuzz_program);
   glUniform2f(fuzz_u_scale, 2.0f / width, 2.0f / height);
   glUniform2f(fuzz_u_offset, (float)offset_x, 0.0f);
   glUniform1i(fuzz_u_tex, 0);
   glUniform1i(fuzz_u_cmap, 1);
   glEnable(GL_BLEND);
   glColorMask(1, 1, 1, 0);
   /* every look goes from nothing (strength 0) to its full effect (1) */
   {
      const float k = gl_spectre_opacity;
      switch (gl_spectre)
      {
      case GL_SPECTRE_MULTIPLY:
         /* behind x mix(1, the demon's colours, k) */
         glUniform4f(fuzz_u_mul, k, k, k, 1);
         glUniform4f(fuzz_u_add, 1 - k, 1 - k, 1 - k, 0);
         glBlendEquation(GL_FUNC_ADD);
         glBlendFunc(GL_DST_COLOR, GL_ZERO);
         break;
      case GL_SPECTRE_DARKEN:
         /* behind x (1 - k) */
         glUniform4f(fuzz_u_mul, 0, 0, 0, 0);
         glUniform4f(fuzz_u_add, 1 - k, 1 - k, 1 - k, 1);
         glBlendEquation(GL_FUNC_ADD);
         glBlendFunc(GL_ZERO, GL_SRC_COLOR);
         break;
      case GL_SPECTRE_TRANSLUCENT:
         /* the demon at opacity k */
         glUniform4f(fuzz_u_mul, 1, 1, 1, 0);
         glUniform4f(fuzz_u_add, 0, 0, 0, k);
         glBlendEquation(GL_FUNC_ADD);
         glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
         break;
      case GL_SPECTRE_SUBTRACT:
         /* behind minus the demon's colours x k */
         glUniform4f(fuzz_u_mul, k, k, k, 0);
         glUniform4f(fuzz_u_add, 0, 0, 0, 0);
         glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
         glBlendFunc(GL_ONE, GL_ONE);
         break;
      default:
         /* behind x (1 - k) */
         glUniform4f(fuzz_u_mul, 0, 0, 0, 0);
         glUniform4f(fuzz_u_add, 1 - k, 1 - k, 1 - k, 1);
         glBlendEquation(GL_FUNC_ADD);
         glBlendFunc(GL_ZERO, GL_SRC_COLOR);
         break;
      }
   }
}

/* Into a translucent or additive sprite's blend (GL_KIND_ADD, _LERP, at
 * alpha 0..32), or the fuzz look; out of it with gl_fuzz_state(FALSE).
 * Through the fuzz program: the colour times u_mul plus u_add. */
static void gl_blend_state(unsigned width, unsigned height, int offset_x, int kind)
{
   float a;
   if (kind == GL_KIND_FUZZ)
   {
      gl_fuzz_state(width, height, offset_x, TRUE);
      return;
   }
   glUseProgram(fuzz_program);
   glUniform2f(fuzz_u_scale, 2.0f / width, 2.0f / height);
   glUniform2f(fuzz_u_offset, (float)offset_x, 0.0f);
   glUniform1i(fuzz_u_tex, 0);
   glUniform1i(fuzz_u_cmap, 1);
   glEnable(GL_BLEND);
   glColorMask(1, 1, 1, 0);
   glBlendEquation(GL_FUNC_ADD);
   if (kind < GL_KIND_LERP)
   {
      /* behind + the sprite x alpha */
      a = (kind - GL_KIND_ADD) / 32.0f;
      glUniform4f(fuzz_u_mul, a, a, a, 1);
      glUniform4f(fuzz_u_add, 0, 0, 0, 0);
      glBlendFunc(GL_ONE, GL_ONE);
   }
   else
   {
      /* the sprite at opacity alpha */
      a = (kind - GL_KIND_LERP) / 32.0f;
      glUniform4f(fuzz_u_mul, 1, 1, 1, 0);
      glUniform4f(fuzz_u_add, 0, 0, 0, a);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   }
}

void dopo_gl_spectre_read(retro_environment_t environ_cb)
{
   struct retro_variable var = { "prboom-spectre", NULL };
   struct retro_variable opacity = { "prboom-spectre-opacity", NULL };
   int percent = 0;

   gl_spectre = GL_SPECTRE_DARKEN;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "subtract"))
         gl_spectre = GL_SPECTRE_SUBTRACT;
      else if (!strcmp(var.value, "multiply"))
         gl_spectre = GL_SPECTRE_MULTIPLY;
      else if (!strcmp(var.value, "translucent"))
         gl_spectre = GL_SPECTRE_TRANSLUCENT;
   }
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &opacity) && opacity.value)
      percent = atoi(opacity.value);
   gl_spectre_opacity = percent > 0 && percent <= 100
                      ? percent / 100.0f : gl_spectre_auto[gl_spectre];
}


/* the wall columns the lights reach, over themselves (prboom-dynlight) */
static void gl_draw_lit_walls(unsigned width, unsigned height, int offset_x)
{
   int i;
   glUseProgram(wlit_program);
   gl_lit_set(&wlit_u, width, height, offset_x);
   glBindBuffer(GL_ARRAY_BUFFER, litw_vbo);
   glBufferData(GL_ARRAY_BUFFER, sizeof(gllvert_t) * 4 * litw_n, litw_v, GL_STREAM_DRAW);
   glEnableVertexAttribArray(3);
   glEnableVertexAttribArray(4);
   for (i = 0; i < litw_n; )
   {
      int j = i + 1, start, count;
      while (j < litw_n && litw_t[j] == litw_t[i])
         j++;
      glBindTexture(GL_TEXTURE_2D, litw_t[i]->id);
      for (start = i, count = j - i; count > 0; )
      {
         const int chunk = count < GL_MAX_QUADS_PER_DRAW ? count : GL_MAX_QUADS_PER_DRAW;
         const char *b = (const char *)(intptr_t)(start * 4 * sizeof(gllvert_t));
         glVertexAttribPointer(0, 2, GL_SHORT, 0, sizeof(gllvert_t), b);
         glVertexAttribPointer(1, 2, GL_FLOAT, 0, sizeof(gllvert_t), b + 4);
         glVertexAttribPointer(2, 1, GL_UNSIGNED_SHORT, 1, sizeof(gllvert_t), b + 12);
         glVertexAttribPointer(3, 3, GL_FLOAT, 0, sizeof(gllvert_t), b + 16);
         glVertexAttribPointer(4, 3, GL_FLOAT, 0, sizeof(gllvert_t), b + 28);
         glDrawElements(GL_TRIANGLES, chunk * 6, GL_UNSIGNED_SHORT, 0);
         stat_draws++;
         start += chunk;
         count -= chunk;
      }
      i = j;
   }
   glDisableVertexAttribArray(3);
   glDisableVertexAttribArray(4);
   litw_n = 0;
}

/* the floor rectangles the lights reach, over themselves */
static void gl_draw_lit_planes(unsigned width, unsigned height, int offset_x)
{
   int i;
   glUseProgram(plit_program);
   gl_lit_set(&plit_u, width, height, offset_x);
   glBindBuffer(GL_ARRAY_BUFFER, litp_vbo);
   glBufferData(GL_ARRAY_BUFFER, sizeof(glplvert_t) * 4 * litp_n, litp_v, GL_STREAM_DRAW);
   for (i = 0; i < 7; i++)
      glEnableVertexAttribArray(i);
   for (i = 0; i < litp_n; )
   {
      int j = i + 1, start, count;
      while (j < litp_n && litp_t[j] == litp_t[i])
         j++;
      glBindTexture(GL_TEXTURE_2D, litp_t[i]->id);
      for (start = i, count = j - i; count > 0; )
      {
         const int chunk = count < GL_MAX_QUADS_PER_DRAW ? count : GL_MAX_QUADS_PER_DRAW;
         const char *b = (const char *)(intptr_t)(start * 4 * sizeof(glplvert_t));
         glVertexAttribPointer(0, 2, GL_SHORT, 0, sizeof(glplvert_t), b);
         glVertexAttribPointer(1, 2, GL_FLOAT, 0, sizeof(glplvert_t), b + 4);
         glVertexAttribPointer(2, 1, GL_FLOAT, 0, sizeof(glplvert_t), b + 12);
         glVertexAttribPointer(3, 1, GL_FLOAT, 0, sizeof(glplvert_t), b + 16);
         glVertexAttribPointer(4, 2, GL_UNSIGNED_BYTE, 0, sizeof(glplvert_t), b + 20);
         glVertexAttribPointer(5, 3, GL_FLOAT, 0, sizeof(glplvert_t), b + 24);
         glVertexAttribPointer(6, 3, GL_FLOAT, 0, sizeof(glplvert_t), b + 36);
         glDrawElements(GL_TRIANGLES, chunk * 6, GL_UNSIGNED_SHORT, 0);
         stat_draws++;
         start += chunk;
         count -= chunk;
      }
      i = j;
   }
   for (i = 1; i < 7; i++)
      glDisableVertexAttribArray(i);
   litp_n = 0;
}

static void gl_draw_scene(unsigned width, unsigned height, int offset_x)
{
   int i;

   if ((!solid_q.n && !masked_q.n) || !gl_sort_quads())
      return;

   glUseProgram(wall_program);
   glUniform2f(wall_u_scale, 2.0f / width, 2.0f / height);
   glUniform2f(wall_u_offset, (float)offset_x, 0.0f);
   glUniform1i(wall_u_tex, 0);
   glUniform1i(wall_u_cmap, 1);
   glActiveTexture(GL_TEXTURE0 + 1);
   glBindTexture(GL_TEXTURE_2D, cmap_tex);
   glActiveTexture(GL_TEXTURE0);

   glBindBuffer(GL_ARRAY_BUFFER, vbo);
   glBufferData(GL_ARRAY_BUFFER, sizeof(glvert_t) * 4 * (solid_q.n + masked_q.n),
                sorted, GL_STREAM_DRAW);
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
   glEnableVertexAttribArray(0);
   glEnableVertexAttribArray(1);
   glEnableVertexAttribArray(2);

   for (i = 0; i < used_n; i++)
   {
      gltex_t *t = used[i];
      gl_draw_range(t->id, t->first, t->count);
      t->count = 0;
   }
   if (litw_n)
   {
      gl_draw_lit_walls(width, height, offset_x);
      glUseProgram(wall_program);
      glBindBuffer(GL_ARRAY_BUFFER, vbo);
   }
   /* in order, one draw per run of the same texture (a sprite's posts) */
   for (i = 0; i < masked_q.n; )
   {
      int j = i + 1;
      while (j < masked_q.n && masked_q.tex[j] == masked_q.tex[i]
             && masked_q.fuzz[j] == masked_q.fuzz[i] && masked_q.rgb[j] == masked_q.rgb[i])
         j++;
      if (masked_q.fuzz[i] == GL_KIND_CORONA)
      {
         int k;
         glUseProgram(corona_program);
         glUniform2f(corona_u_scale, 2.0f / width, 2.0f / height);
         glUniform2f(corona_u_offset, (float)offset_x, 0.0f);
         glUniform1i(corona_u_tex, 0);
         glEnable(GL_BLEND);
         glColorMask(1, 1, 1, 0);
         glBlendEquation(GL_FUNC_ADD);
         glBlendFunc(GL_ONE, GL_ONE);
         for (k = i; k < j; k++)
         {
            const uint32_t c = masked_q.rgb[k];
            const float a = 0.45f / 255.0f;
            glUniform4f(corona_u_color, ((c >> 16) & 255) * a, ((c >> 8) & 255) * a,
                        (c & 255) * a, 0.0f);
            gl_draw_range(masked_q.tex[k]->id, solid_q.n + k, 1);
         }
         gl_fuzz_state(width, height, offset_x, FALSE);
      }
      else if (masked_q.fuzz[i])
      {
         gl_blend_state(width, height, offset_x, masked_q.fuzz[i]);
         gl_draw_range(masked_q.tex[i]->id, solid_q.n + i, j - i);
         gl_fuzz_state(width, height, offset_x, FALSE);
      }
      else if (masked_q.rgb[i])
      {
         /* near dynamic lights (prboom-dynlight) */
         const uint32_t c = masked_q.rgb[i];
         glUseProgram(slit_program);
         gl_lit_set(&slit_u, width, height, offset_x);
         glUniform3f(slit_u.light, ((c >> 16) & 255) / 255.0f, ((c >> 8) & 255) / 255.0f,
                     (c & 255) / 255.0f);
         gl_draw_range(masked_q.tex[i]->id, solid_q.n + i, j - i);
         glUseProgram(wall_program);
      }
      else if (masked_q.tex[i]->alpha)
      {
         /* whole columns (prboom-scale): the coverage cuts the outline */
         glUseProgram(sprite_program);
         glUniform2f(sprite_u_scale, 2.0f / width, 2.0f / height);
         glUniform2f(sprite_u_offset, (float)offset_x, 0.0f);
         glUniform1i(sprite_u_tex, 0);
         glUniform1i(sprite_u_cmap, 1);
         gl_draw_range(masked_q.tex[i]->id, solid_q.n + i, j - i);
         glUseProgram(wall_program);
      }
      else
         gl_draw_range(masked_q.tex[i]->id, solid_q.n + i, j - i);
      i = j;
   }
   glDisableVertexAttribArray(1);
   glDisableVertexAttribArray(2);
   stat_quads += solid_q.n + masked_q.n;
}

/* the software frame over the columns; two textures in turn, so the
 * upload does not wait for the previous frame's draw */
static void gl_band(GLfloat **v, int *n, unsigned width, unsigned height,
                    int x0, int y0, int x1, int y1)
{
   /* a_pos runs bottom up: row 0 is at 1 */
   const GLfloat top = 1.0f - (GLfloat)y0 / height, bottom = 1.0f - (GLfloat)y1 / height;
   const GLfloat left = (GLfloat)x0 / width, right = (GLfloat)x1 / width;
   GLfloat *q = *v;
   q[0] = left;  q[1]  = bottom; q[2]  = right; q[3]  = bottom; q[4]  = left;  q[5]  = top;
   q[6] = right; q[7]  = bottom; q[8]  = right; q[9]  = top;    q[10] = left;  q[11] = top;
   *v += 12;
   (*n)++;
}

/* The frame's rectangles: send_rects uploaded to this frame's texture,
 * then the rows outside the view (y < view_top or y >= view_bottom, which
 * the texture always holds) and draw_rects laid over the GPU. GLES 2
 * uploads tight rectangles only, so a part of a row is copied out first. */
static void gl_draw_frame(const void *frame, unsigned width, unsigned height,
                          size_t pitch, int view_top, int view_bottom)
{
   const int k = frame_tex_next;
   const uint8_t *pixels = (const uint8_t *)frame;
   GLfloat *v;
   int i, nbands = 0;

   frame_tex_next ^= 1;
   glBindTexture(GL_TEXTURE_2D, frame_tex[k]);
   glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
   if (frame_tex_w[k] != (int)width || frame_tex_h[k] != (int)height)
   {
      gl_texture_params(GL_CLAMP_TO_EDGE);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0,
                   GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL);
      frame_tex_w[k] = width;
      frame_tex_h[k] = height;
   }

   for (i = 0; i < send_rects.n; i++)
   {
      const glrect_t *r = &send_rects.r[i];
      const int w = r->x1 - r->x0, h = r->y1 - r->y0;
      if (r->x0 == 0 && w == (int)width && pitch == width * sizeof(uint16_t))
         glTexSubImage2D(GL_TEXTURE_2D, 0, 0, r->y0, w, h, GL_RGB,
                         GL_UNSIGNED_SHORT_5_6_5, pixels + (size_t)r->y0 * pitch);
      else
      {
         const size_t size = (size_t)w * h * sizeof(uint16_t);
         int row;
         if (frame_copy_size < size)
         {
            uint16_t *c = (uint16_t *)realloc(frame_copy, size);
            if (!c)
               return;
            frame_copy = c;
            frame_copy_size = size;
         }
         for (row = 0; row < h; row++)
            memcpy(frame_copy + (size_t)row * w,
                   pixels + (size_t)(r->y0 + row) * pitch + (size_t)r->x0 * sizeof(uint16_t),
                   w * sizeof(uint16_t));
         glTexSubImage2D(GL_TEXTURE_2D, 0, r->x0, r->y0, w, h, GL_RGB,
                         GL_UNSIGNED_SHORT_5_6_5, frame_copy);
      }
      stat_rows += h;
      stat_pixels += (long)w * h;
   }

   if (band_verts_cap < (GL_RECTS + 2) * 12)
   {
      GLfloat *nv = (GLfloat *)realloc(band_verts, sizeof(GLfloat) * (GL_RECTS + 2) * 12);
      if (!nv)
         return;
      band_verts = nv;
      band_verts_cap = (GL_RECTS + 2) * 12;
   }
   v = band_verts;
   if (view_top > 0)
      gl_band(&v, &nbands, width, height, 0, 0, width, view_top);
   if (view_bottom < (int)height)
      gl_band(&v, &nbands, width, height, 0, view_bottom, width, height);
   for (i = 0; i < draw_rects.n; i++)
      gl_band(&v, &nbands, width, height, draw_rects.r[i].x0, draw_rects.r[i].y0,
              draw_rects.r[i].x1, draw_rects.r[i].y1);
   if (!nbands)
      return;

   glUseProgram(frame_program);
   glUniform1i(frame_u_frame, 0);
   {
      GLfloat keys[GL_KEYS * 3];
      int j;
      for (j = 0; j < GL_KEYS; j++)
      {
         keys[j * 3 + 0] = ((dopo_gl_keys[j] >> 11) & 31) / 31.0f;
         keys[j * 3 + 1] = ((dopo_gl_keys[j] >> 5) & 63) / 63.0f;
         keys[j * 3 + 2] = (dopo_gl_keys[j] & 31) / 31.0f;
      }
      glUniform3fv(frame_u_keys, GL_KEYS, keys);
   }
   /* the frame's alpha only carries the blend; the output keeps the
    * clear's opaque alpha */
   glEnable(GL_BLEND);
   glBlendFunc(GL_ONE, GL_SRC_ALPHA);
   glColorMask(1, 1, 1, 0);
   glBindBuffer(GL_ARRAY_BUFFER, frame_vbo);
   glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 12 * nbands, band_verts, GL_STREAM_DRAW);
   glEnableVertexAttribArray(0);
   glVertexAttribPointer(0, 2, GL_FLOAT, 0, 0, 0);
   glDrawArrays(GL_TRIANGLES, 0, 6 * nbands);
   glDisable(GL_BLEND);
   glColorMask(1, 1, 1, 1);
}

#define GL_SCAN_STRIDE 32

/* Which parts of the frame the GPU needs, to draw (draw_rects) and to
 * send to this frame's texture (send_rects): all of it, unless the view
 * is kept keyed (view_keyed) and spans the frame's width. Then the parts
 * of the view something was drawn on are drawn and sent, and filled back
 * with the key once sent; the rows outside the view (the status bar) are
 * drawn every frame but their parts sent only when they change, to each
 * of the two frame textures in turn (pending_rects). */
/* a part of the rows outside the view changed: kept, and owed to both
 * frame textures */
static void gl_outside_changed(const void *frame, size_t pitch, unsigned width,
                               int x0, int y0, int x1, int y1)
{
   int y;
   if (x0 >= x1 || y0 >= y1)
      return;
   for (y = y0; y < y1; y++)
      memcpy(sent_copy + (size_t)y * width + x0,
             (const uint8_t *)frame + (size_t)y * pitch + (size_t)x0 * sizeof(uint16_t),
             (x1 - x0) * sizeof(uint16_t));
   gl_rects_add(&pending_rects[0], x0, y0, x1, y1);
   gl_rects_add(&pending_rects[1], x0, y0, x1, y1);
}

static void gl_frame_rows(const void *frame, unsigned width, unsigned height, size_t pitch,
                          int *view_top, int *view_bottom)
{
   const dbool keyed = frame_has_view && view_keyed && frame == screens[0].data
      && keyed_x0 == 0 && keyed_w == (int)width && pitch == width * sizeof(uint16_t);
   const int k = frame_tex_next;
   const size_t rowbytes = width * sizeof(uint16_t);
   int i, y, vt, vb;

   draw_rects.n = send_rects.n = 0;
   *view_top = *view_bottom = 0;
   if (keyed && (frame_rows_cap < (int)height || !sent_copy))
   {
      uint16_t *c = (uint16_t *)realloc(sent_copy, rowbytes * height);
      if (!c)
         return;
      sent_copy = c;
      frame_rows_cap = height;
      was_keyed = FALSE;
   }
   if (keyed && (key_row_w != (int)width || key_row_key != keyed_key))
   {
      uint16_t *r = (uint16_t *)realloc(key_row, rowbytes);
      unsigned x;
      if (!r)
         return;
      key_row = r;
      for (x = 0; x < width; x++)
         key_row[x] = keyed_key;
      key_row_w = width;
      key_row_key = keyed_key;
   }
   if (!keyed)
   {
      /* everything, and the other texture is behind on all of it */
      gl_rects_add(&draw_rects, 0, 0, width, height);
      gl_rects_add(&send_rects, 0, 0, width, height);
      pending_rects[k].n = 0;
      pending_rects[k ^ 1].n = 0;
      gl_rects_add(&pending_rects[k ^ 1], 0, 0, width, height);
      /* the view holds whatever was drawn last: fill it whole next time */
      view_keyed = FALSE;
      was_keyed = FALSE;
      return;
   }
   vt = keyed_y0;
   vb = keyed_y0 + keyed_h;
   *view_top = vt;
   *view_bottom = vb;
   if (frame_tex_w[k] != (int)width || frame_tex_h[k] != (int)height)
   {
      pending_rects[k].n = 0;
      gl_rects_add(&pending_rects[k], 0, 0, width, vt);
      gl_rects_add(&pending_rects[k], 0, vb, width, height);
   }
   if (!was_keyed)
   {
      gl_outside_changed(frame, pitch, width, 0, 0, width, vt);
      gl_outside_changed(frame, pitch, width, 0, vb, width, height);
   }

   /* What the 2D drawing and the CPU fallbacks marked: in the view it is
    * sent and drawn this frame, outside it is owed to both textures. */
   for (i = 0; i < dirty_rects.n; i++)
   {
      const glrect_t *r = &dirty_rects.r[i];
      const int y0 = r->y0 > vt ? r->y0 : vt, y1 = r->y1 < vb ? r->y1 : vb;
      if (y0 < y1)
         gl_rects_add(&draw_rects, r->x0, y0, r->x1, y1);
      gl_outside_changed(frame, pitch, width, r->x0, r->y0, r->x1, r->y0 < vt ? (r->y1 < vt ? r->y1 : vt) : r->y0);
      gl_outside_changed(frame, pitch, width, r->x0, r->y0 > vb ? r->y0 : vb, r->x1, r->y1);
   }
   /* Against what draws without marking, one row in GL_SCAN_STRIDE is also
    * checked whole, a different one each frame: in the view against the
    * key, outside it against what was sent (reading it all cost 18 ms a
    * frame at 1080p on the ARM). */
   for (y = scan_phase; y < (int)height; y += GL_SCAN_STRIDE)
   {
      const uint8_t *row = (const uint8_t *)frame + (size_t)y * pitch;
      if (y >= vt && y < vb)
      {
         if (memcmp(row, key_row, rowbytes))
            gl_rects_add(&draw_rects, 0, y, width, y + 1);
      }
      else if (memcmp(row, sent_copy + (size_t)y * width, rowbytes))
         gl_outside_changed(frame, pitch, width, 0, y, width, y + 1);
   }
   scan_phase = (scan_phase + 1) % GL_SCAN_STRIDE;

   /* the view's parts are fresh; what this texture is owed outside it */
   send_rects = draw_rects;
   for (i = 0; i < pending_rects[k].n; i++)
      gl_rects_add(&send_rects, pending_rects[k].r[i].x0, pending_rects[k].r[i].y0,
                   pending_rects[k].r[i].x1, pending_rects[k].r[i].y1);
   pending_rects[k].n = 0;
   was_keyed = TRUE;
}

/* the parts of the view sent this frame go back to the key */
static void gl_frame_rekey(void *frame, size_t pitch)
{
   int i, y;
   if (!view_keyed)
      return;
   for (i = 0; i < draw_rects.n; i++)
   {
      const glrect_t *r = &draw_rects.r[i];
      for (y = r->y0; y < r->y1; y++)
         memcpy((uint8_t *)frame + (size_t)y * pitch + r->x0 * sizeof(uint16_t),
                key_row, (r->x1 - r->x0) * sizeof(uint16_t));
   }
}

static void gl_stats(void)
{
   if (++stat_frames < GL_STATS_FRAMES)
      return;
   /* all per frame, averaged: frame = from one present to the next, the
    * whole retro_run; walls = setup (textures, colour table) + key fill +
    * the columns themselves; present = frame upload and draws (CPU side) */
   lprintf(LO_INFO, "prboom-render gl: %.1f fps, frame %.2f ms | walls %.2f ms "
           "(setup %.2f, key %.2f, columns %.2f) | present %.2f ms "
           "(rows %.2f, %ld rows / %ld kpixels sent; %ld full key fills in the period) | "
           "%ld quads, %ld draws, %ld posts | software: %ld of %ld columns, "
           "%ld of %ld spans, %ld of %ld sprites/masked | %ld plane quads | "
           "2D: %ld quads, %ld kpixels of images sent\n",
           stat_frame_ms > 0 ? 1000.0 * stat_frames / stat_frame_ms : 0.0,
           stat_frame_ms / stat_frames, stat_walls_ms / stat_frames,
           stat_setup_ms / stat_frames, stat_fill_ms / stat_frames,
           (stat_walls_ms - stat_setup_ms - stat_fill_ms) / stat_frames,
           stat_present_ms / stat_frames, stat_scan_ms / stat_frames,
           stat_rows / stat_frames, stat_pixels / stat_frames / 1000,
           stat_full_fills, stat_quads / stat_frames,
           stat_draws / stat_frames, stat_posts / stat_frames,
           stat_software / stat_frames,
           stat_columns / stat_frames, stat_spans_software / stat_frames,
           stat_spans / stat_frames, stat_masked_software / stat_frames,
           stat_masked / stat_frames, stat_planes / stat_frames,
           stat_2d_quads / stat_frames, stat_mirror_pixels / stat_frames / 1000);
   stat_walls_ms = stat_setup_ms = stat_fill_ms = stat_present_ms = 0;
   stat_frame_ms = 0;
   stat_quads = stat_columns = stat_software = stat_draws = 0;
   stat_spans = stat_spans_software = 0;
   stat_masked = stat_masked_software = stat_posts = 0;
   stat_rows = stat_pixels = stat_full_fills = stat_planes = 0;
   stat_2d_quads = stat_mirror_pixels = 0;
   stat_scan_ms = 0;
   stat_frames = 0;
}

/* the software renderer's line, to compare with gl's: the whole frame,
 * from one present to the next */
void dopo_gl_software_frame(void)
{
   static double last, sum;
   static int frames;
   const double now = gl_now_ms();

   if (last > 0)
      sum += now - last;
   last = now;
   if (++frames < GL_STATS_FRAMES)
      return;
   lprintf(LO_INFO, "prboom-render software: %.1f fps, frame %.2f ms\n",
           sum > 0 ? 1000.0 * frames / sum : 0.0, sum / frames);
   sum = 0;
   frames = 0;
}

void dopo_gl_present(retro_video_refresh_t video_cb, const void *frame,
                     unsigned width, unsigned height, size_t pitch)
{
   const double t0 = gl_now_ms();

   if (!gl_ready || !hw.get_current_framebuffer)
   {
      solid_q.n = masked_q.n = plane_n = two_q.n = 0;
      plane_run.on = FALSE;
      view_active = view_software = frame_has_view = view_keyed = FALSE;
      video_cb(NULL, width, height, 0);
      return;
   }

   glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)hw.get_current_framebuffer());
   glViewport(0, 0, width, height);
   glDisable(GL_DEPTH_TEST);
   glDisable(GL_STENCIL_TEST);
   glDisable(GL_SCISSOR_TEST);
   glDisable(GL_BLEND);
   glDisable(GL_CULL_FACE);
   glDisable(GL_DITHER);
   glColorMask(1, 1, 1, 1);
   glClearColor(0, 0, 0, 1);
   glClear(GL_COLOR_BUFFER_BIT);

   view_active = view_software = FALSE;
   gl_draw_planes(width, height, ((int)width - SCREENWIDTH) / 2);
   gl_draw_scene(width, height, ((int)width - SCREENWIDTH) / 2);
   solid_q.n = masked_q.n = 0;
   {
      const double ts = gl_now_ms();
      int view_top, view_bottom;
      gl_frame_rows(frame, width, height, pitch, &view_top, &view_bottom);
      stat_scan_ms += gl_now_ms() - ts;
      gl_draw_frame(frame, width, height, pitch, view_top, view_bottom);
   }
   if (gl2d_on)
      gl_draw_2d(width, height);
   two_q.n = 0;
   gl_frame_rekey((void *)frame, pitch);
   dirty_rects.n = 0;
   frame_has_view = FALSE;

   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
   glUseProgram(0);

   {
      const double t1 = gl_now_ms();
      stat_present_ms += t1 - t0;
      if (stat_last_present > 0)
         stat_frame_ms += t1 - stat_last_present;
      stat_last_present = t1;
   }
   gl_stats();
   video_cb(RETRO_HW_FRAME_BUFFER_VALID, width, height, 0);
}
