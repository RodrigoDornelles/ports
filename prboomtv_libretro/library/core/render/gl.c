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
 * through (see gl_key_pick and frame_fs).
 */
#define _GNU_SOURCE
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
#include "v_video.h"
#include "vid_mode.h"
#include "w_wad.h"
#include "prboomtv/render_gl.h"

/* r_data.c, killough's marker lumps of the extra colormaps */
extern int firstcolormaplump;

dbool dopo_gl = FALSE;
int R_DrawCmdTexture = 0;
int R_SpanTexture = 0;

const struct retro_core_option_v2_definition dopo_gl_option =
{
   "prboom-render",
   "Renderer (Restart Required)",
   NULL,
   "Who paints the 3D view. 'software' fills every pixel on the CPU, the "
   "classic renderer. 'gl' keeps the CPU deciding what is seen and hands "
   "the painting to the GPU through OpenGL ES 2, for high resolutions on "
   "weak CPUs; still incomplete (walls and sky so far).",
   NULL,
   NULL,
   {
      { "software", NULL },
      { "gl",       NULL },
      { NULL, NULL },
   },
   "software"
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
#define GL_UNSIGNED_SHORT_5_6_5   0x8363
#define GL_TEXTURE_2D             0x0DE1
#define GL_TEXTURE0               0x84C0
#define GL_TEXTURE_MIN_FILTER     0x2801
#define GL_TEXTURE_MAG_FILTER     0x2800
#define GL_TEXTURE_WRAP_S         0x2802
#define GL_TEXTURE_WRAP_T         0x2803
#define GL_NEAREST                0x2600
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
#define GL_ONE                    1
#define GL_SRC_ALPHA              0x0302
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
GLFN(void, glUniform2f, (GLint, GLfloat, GLfloat));
GLFN(void, glUniform3fv, (GLint, GLsizei, const GLfloat *));
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
 * as stored) */
typedef struct
{
   GLuint         id;
   int            gl_width, gl_height;  /* powers of two */
   unsigned       stamp;                /* replay the patch below is from */
   const rpatch_t *patch;
   dbool          bad;                  /* too big for the GPU */
   int            count, first;         /* quads this frame, while sorting */
} gltex_t;

static gltex_t        *texcache = NULL;   /* per texture number */
static int             texcache_n = 0;
static texture_t     **texcache_owner = NULL;
static gltex_t        *flatcache = NULL;  /* per lump number */
static int             flatcache_n = 0;
static lumpinfo_t     *flatcache_owner = NULL;
static unsigned        replay_stamp = 0;

/* quads of the frame, in emission order, and the texture of each */
static glvert_t       *quads = NULL;
static gltex_t       **quad_tex = NULL;
static int             quad_n = 0, quad_cap = 0;
static glvert_t       *sorted = NULL;
static int             sorted_cap = 0;
static gltex_t       **used = NULL;      /* textures with quads this frame */
static int             used_n = 0;

/* a view of the frame is being replayed on the GPU (walls, then flats) */
static dbool           view_active = FALSE;

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
static GLuint          wall_program = 0, frame_program = 0;
static GLint           wall_u_scale, wall_u_offset, wall_u_tex, wall_u_cmap;
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
   LOAD(glEnable) LOAD(glBlendFunc)
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
   LOAD(glUniform2f) LOAD(glUniform3fv) LOAD(glEnableVertexAttribArray)
   LOAD(glDisableVertexAttribArray) LOAD(glVertexAttribPointer)
   LOAD(glDrawElements) LOAD(glDrawArrays) LOAD(glGetString)
   LOAD(glGetIntegerv)
#undef LOAD
   return ok;
}

/* ---- shaders ---- */

/* a column: texture coordinate and colour table row straight from the
 * attributes; positions in frame pixels, row 0 at the top */
static const char *wall_vs =
   "#version 100\n"
   "attribute vec2 a_pos;\n"
   "attribute vec2 a_st;\n"
   "attribute float a_l;\n"
   "uniform vec2 u_scale;\n"
   "uniform vec2 u_offset;\n"
   "varying vec2 v_uv;\n"
   "varying float v_light;\n"
   "void main() {\n"
   "  vec2 p = (a_pos + u_offset) * u_scale;\n"
   "  v_uv = a_st;\n"
   "  v_light = a_l;\n"
   "  gl_Position = vec4(p.x - 1.0, 1.0 - p.y, 0.0, 1.0);\n"
   "}\n";

/* texel (palette index) -> colour table at the column's light row; no sum
 * on v_uv before the first texture2D */
static const char *wall_fs =
   "#version 100\n"
   "precision mediump float;\n"
   "uniform sampler2D u_tex;\n"
   "uniform sampler2D u_cmap;\n"
   "varying vec2 v_uv;\n"
   "varying float v_light;\n"
   "void main() {\n"
   "  float index = texture2D(u_tex, v_uv).r;\n"
   "  gl_FragColor = texture2D(u_cmap, vec2(index * (255.0 / 256.0) + (0.5 / 256.0), v_light));\n"
   "}\n";

static const char *frame_vs =
   "#version 100\n"
   "attribute vec2 a_pos;\n"
   "varying vec2 v_uv;\n"
   "void main() {\n"
   "  v_uv = vec2(a_pos.x, 1.0 - a_pos.y);\n"
   "  gl_Position = vec4(a_pos * 2.0 - 1.0, 0.0, 1.0);\n"
   "}\n";

/* the software frame, blended ONE, SRC_ALPHA over the columns: a pixel
 * of the frame covers them (alpha 0), the key lets them show (alpha 1),
 * the fuzz key darkens them like the fuzz (15/16); a key is within half a
 * 565 step */
static const char *frame_fs =
   "#version 100\n"
   "precision mediump float;\n"
   "uniform sampler2D u_frame;\n"
   "uniform vec3 u_keys[2];\n"
   "varying vec2 v_uv;\n"
   "float near(vec3 c, vec3 k) {\n"
   "  vec3 d = abs(c - k);\n"
   "  return max(max(d.r, d.g), d.b) < 0.004 ? 1.0 : 0.0;\n"
   "}\n"
   "void main() {\n"
   "  vec3 c = texture2D(u_frame, v_uv).rgb;\n"
   "  float keep = near(c, u_keys[0]) + near(c, u_keys[1]) * 0.9375;\n"
   "  gl_FragColor = keep > 0.0 ? vec4(0.0, 0.0, 0.0, keep) : vec4(c, 0.0);\n"
   "}\n";

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

/* ---- context ---- */

/* every GL name died with the context; nothing is deleted */
static void gl_forget_objects(void)
{
   int i;
   for (i = 0; i < texcache_n; i++)
      texcache[i].id = 0;
   for (i = 0; i < flatcache_n; i++)
      flatcache[i].id = 0;
   cmap_tex = 0;
   cmap_palette = NULL;
   wall_program = frame_program = 0;
   vbo = ibo = frame_vbo = 0;
   frame_tex[0] = frame_tex[1] = 0;
}

static void gl_context_reset(void)
{
   static const char *const wall_attribs[] = { "a_pos", "a_st", "a_l" };
   static const char *const frame_attribs[] = { "a_pos" };
   static const float corners[] = { 0, 0, 1, 0, 0, 1, 1, 1 };
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

   wall_program = gl_program(wall_vs, wall_fs, wall_attribs, 3);
   frame_program = gl_program(frame_vs, frame_fs, frame_attribs, 1);
   if (!wall_program || !frame_program)
      return;
   wall_u_scale  = glGetUniformLocation(wall_program, "u_scale");
   wall_u_offset = glGetUniformLocation(wall_program, "u_offset");
   wall_u_tex    = glGetUniformLocation(wall_program, "u_tex");
   wall_u_cmap   = glGetUniformLocation(wall_program, "u_cmap");
   frame_u_frame = glGetUniformLocation(frame_program, "u_frame");
   frame_u_keys  = glGetUniformLocation(frame_program, "u_keys");

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
   glBindBuffer(GL_ARRAY_BUFFER, frame_vbo);
   glBufferData(GL_ARRAY_BUFFER, sizeof(corners), corners, GL_STATIC_DRAW);

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
      glDeleteTextures(1, &cmap_tex);
      glDeleteTextures(2, frame_tex);
      glDeleteBuffers(1, &vbo);
      glDeleteBuffers(1, &ibo);
      glDeleteBuffers(1, &frame_vbo);
      glDeleteProgram(wall_program);
      glDeleteProgram(frame_program);
   }
   gl_forget_objects();
   gl_ready = FALSE;
}

dbool dopo_gl_load(retro_environment_t environ_cb)
{
   struct retro_variable var = { "prboom-render", NULL };

   dopo_gl = FALSE;
   if (!environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) || !var.value
         || strcmp(var.value, "gl"))
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
   quad_n = 0;
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
   t1 = gl_now_ms();
   stat_setup_ms += t1 - t0;
   view_topleft = drawvars.short_topleft;
   view_x0 = (int)(off % SCREENWIDTH);
   view_y0 = (int)(off / SCREENWIDTH);
   replay_stamp++;
   view_active = TRUE;

   /* The key over the whole view, row by row, before anything else is
    * drawn on it: the walls and the sky are the first thing replayed, then
    * the flats; what the CPU still draws (sprites, masked textures, what
    * the GPU cannot reproduce) covers it after, so the key stays exactly
    * where the GPU paints (and where nothing does, the GPU's black shows
    * instead of a stale frame). Filling it column by column instead
    * touched a new cache line per pixel: 15 ms a frame at 1080p on the
    * ARM. */
   {
      const uint16_t key = dopo_gl_keys[0];
      int y, x;
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

static glvert_t *gl_quad(gltex_t *tex)
{
   if (quad_n == quad_cap)
   {
      const int cap = quad_cap ? quad_cap * 2 : 8192;
      glvert_t *q = (glvert_t *)realloc(quads, sizeof(glvert_t) * 4 * cap);
      gltex_t **t;
      if (!q)
         return NULL;
      quads = q;
      t = (gltex_t **)realloc(quad_tex, sizeof(*quad_tex) * cap);
      if (!t)
         return NULL;
      quad_tex = t;
      quad_cap = cap;
   }
   quad_tex[quad_n] = tex;
   return &quads[4 * quad_n++];
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
dbool dopo_gl_wall(const draw_column_vars_t *dc, R_DrawColumn_f fn, int gltex)
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
      return FALSE;
   }
   t = gl_texture(gltex - 1);
   if (!t)
   {
      stat_software++;
      return FALSE;
   }
   p = t->patch;
   off = dc->source - p->pixels;
   if (off < 0 || off >= (ptrdiff_t)p->width * p->height || off % p->height
         || (dc->texheight && dc->texheight != p->height))
   {
      stat_software++;
      return FALSE;
   }
   col = (int)(off / p->height);
   row = R_WallColumnKernelClass(fn) == 1 ? cmap_rows - 1 : gl_colormap_row(dc->colormap);
   if (row < 0)
   {
      stat_software++;
      return FALSE;
   }
   tc = (float)((col + 0.5) / t->gl_height);
   lc = (uint16_t)(((row + 0.5) / cmap_gl_rows) * 65535.0 + 0.5);

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
         return FALSE;
      }
   }
   else if (dc->iscale <= 0)
   {
      /* a step past 32 bits only stays exact modulo a power of two */
      stat_software++;
      return FALSE;
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
         return FALSE;
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
            return FALSE;
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
 * taken modulo 64 in 32 bits like the drawer, which keeps them small. */
dbool dopo_gl_span(const draw_span_vars_t *ds, int lump)
{
   gltex_t *t;
   glvert_t *q;
   int row, n;
   uint16_t lc;
   double u0, v0, du, dv;
   int16_t xa, xb, ya, yb;

   if (!view_active || drawvars.short_topleft != view_topleft)
      return FALSE;
   if (ds->x1 > ds->x2)
      return TRUE;
   stat_spans++;
   if (!lump || ds->brightmask || r_span_translucent
         || (t = gl_flat(lump - 1)) == NULL
         || (row = gl_colormap_row(ds->colormap)) < 0
         || (q = gl_quad(t)) == NULL)
   {
      stat_spans_software++;
      return FALSE;
   }
   lc = (uint16_t)(((row + 0.5) / cmap_gl_rows) * 65535.0 + 0.5);
   n  = ds->x2 - ds->x1 + 1;
   u0 = (double)((uint32_t)ds->xfrac & 0x3fffff);
   v0 = (double)((uint32_t)ds->yfrac & 0x3fffff);
   du = ds->xstep;
   dv = ds->ystep;
   xa = (int16_t)(view_x0 + ds->x1);
   xb = (int16_t)(view_x0 + ds->x2 + 1);
   ya = (int16_t)(view_y0 + ds->y);
   yb = (int16_t)(view_y0 + ds->y + 1);
   q[0].x = xa; q[0].y = ya;
   q[1].x = xb; q[1].y = ya;
   q[2].x = xb; q[2].y = yb;
   q[3].x = xa; q[3].y = yb;
   q[0].s = q[3].s = (float)((u0 - du * 0.5) / (65536.0 * 64));
   q[1].s = q[2].s = (float)((u0 + du * (n - 0.5)) / (65536.0 * 64));
   q[0].t = q[3].t = (float)((v0 - dv * 0.5) / (65536.0 * 64));
   q[1].t = q[2].t = (float)((v0 + dv * (n - 0.5)) / (65536.0 * 64));
   q[0].l = q[1].l = q[2].l = q[3].l = lc;
   return TRUE;
}

/* ---- presenting ---- */

/* quads grouped per texture, in the order the textures first showed up */
static int gl_sort_quads(void)
{
   int i;

   if (sorted_cap < quad_n)
   {
      glvert_t *s = (glvert_t *)realloc(sorted, sizeof(glvert_t) * 4 * quad_cap);
      gltex_t **u = (gltex_t **)realloc(used, sizeof(*used) * quad_cap);
      if (!s || !u)
      {
         if (s) sorted = s;
         if (u) used = u;
         return 0;
      }
      sorted = s;
      used = u;
      sorted_cap = quad_cap;
   }
   used_n = 0;
   for (i = 0; i < quad_n; i++)
   {
      gltex_t *t = quad_tex[i];
      if (!t->count++)
         used[used_n++] = t;
   }
   {
      int first = 0;
      for (i = 0; i < used_n; i++)
      {
         gltex_t *t = used[i];
         t->first = first;
         first += t->count;
         t->count = 0;
      }
   }
   for (i = 0; i < quad_n; i++)
   {
      gltex_t *t = quad_tex[i];
      memcpy(&sorted[4 * (t->first + t->count++)], &quads[4 * i], sizeof(glvert_t) * 4);
   }
   return 1;
}

static void gl_draw_walls(unsigned width, unsigned height, int offset_x)
{
   int i;

   if (!quad_n || !gl_sort_quads())
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
   glBufferData(GL_ARRAY_BUFFER, sizeof(glvert_t) * 4 * quad_n, sorted, GL_STREAM_DRAW);
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
   glEnableVertexAttribArray(0);
   glEnableVertexAttribArray(1);
   glEnableVertexAttribArray(2);

   for (i = 0; i < used_n; i++)
   {
      gltex_t *t = used[i];
      int first = t->first, count = t->count;
      glBindTexture(GL_TEXTURE_2D, t->id);
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
      t->count = 0;
   }
   glDisableVertexAttribArray(1);
   glDisableVertexAttribArray(2);
   stat_quads += quad_n;
}

/* the software frame over the columns; two textures in turn, so the
 * upload does not wait for the previous frame's draw */
static void gl_draw_frame(const void *frame, unsigned width, unsigned height, size_t pitch)
{
   const int k = frame_tex_next;
   const void *pixels = frame;

   frame_tex_next ^= 1;
   if (pitch != width * sizeof(uint16_t))
   {
      unsigned y;
      const size_t size = (size_t)width * height * sizeof(uint16_t);
      if (frame_copy_size < size)
      {
         uint16_t *c = (uint16_t *)realloc(frame_copy, size);
         if (!c)
            return;
         frame_copy = c;
         frame_copy_size = size;
      }
      for (y = 0; y < height; y++)
         memcpy(frame_copy + (size_t)y * width, (const uint8_t *)frame + y * pitch,
                width * sizeof(uint16_t));
      pixels = frame_copy;
   }

   glBindTexture(GL_TEXTURE_2D, frame_tex[k]);
   glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
   if (frame_tex_w[k] != (int)width || frame_tex_h[k] != (int)height)
   {
      gl_texture_params(GL_CLAMP_TO_EDGE);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0,
                   GL_RGB, GL_UNSIGNED_SHORT_5_6_5, pixels);
      frame_tex_w[k] = width;
      frame_tex_h[k] = height;
   }
   else
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                      GL_RGB, GL_UNSIGNED_SHORT_5_6_5, pixels);

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
   glEnableVertexAttribArray(0);
   glVertexAttribPointer(0, 2, GL_FLOAT, 0, 0, 0);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glDisable(GL_BLEND);
   glColorMask(1, 1, 1, 1);
}

static void gl_stats(void)
{
   if (++stat_frames < GL_STATS_FRAMES)
      return;
   /* all per frame, averaged: frame = from one present to the next, the
    * whole retro_run; walls = setup (textures, colour table) + key fill +
    * the columns themselves; present = frame upload and draws (CPU side) */
   lprintf(LO_INFO, "prboom-render gl: %.1f fps, frame %.2f ms | walls %.2f ms "
           "(setup %.2f, key %.2f, columns %.2f) | present %.2f ms | "
           "%ld quads, %ld draws | software: %ld of %ld columns, %ld of %ld spans\n",
           stat_frame_ms > 0 ? 1000.0 * stat_frames / stat_frame_ms : 0.0,
           stat_frame_ms / stat_frames, stat_walls_ms / stat_frames,
           stat_setup_ms / stat_frames, stat_fill_ms / stat_frames,
           (stat_walls_ms - stat_setup_ms - stat_fill_ms) / stat_frames,
           stat_present_ms / stat_frames, stat_quads / stat_frames,
           stat_draws / stat_frames, stat_software / stat_frames,
           stat_columns / stat_frames, stat_spans_software / stat_frames,
           stat_spans / stat_frames);
   stat_walls_ms = stat_setup_ms = stat_fill_ms = stat_present_ms = 0;
   stat_frame_ms = 0;
   stat_quads = stat_columns = stat_software = stat_draws = 0;
   stat_spans = stat_spans_software = 0;
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
      quad_n = 0;
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

   view_active = FALSE;
   gl_draw_walls(width, height, ((int)width - SCREENWIDTH) / 2);
   quad_n = 0;
   gl_draw_frame(frame, width, height, pitch);

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
