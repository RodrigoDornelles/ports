/*
 * glbench_libretro: synthetic GLES2 test for a "CPU decides, GPU paints"
 * Doom renderer. Each frame it builds a Doom-like scene on the CPU, one
 * quad per wall column per layer and one quad per floor/ceiling row,
 * batched per texture, uploads it, and paints it with the indexed shader
 * (8-bit texel -> COLORMAP row by light -> RGB). Texture coordinates go
 * straight from the varyings into texture2D, the path that stays precise
 * on mediump-only GPUs.
 *
 * Settings (environment):
 *   GLBENCH_RES     1920x1080   frame size
 *   GLBENCH_LAYERS  3           wall quads per column
 *   GLBENCH_REPEAT  4           times the scene is painted per frame, to
 *                               measure past vsync (only the last is seen)
 *   GLBENCH_GL      unset       1 asks for desktop GL instead of GLES2 (for
 *                               frontends built without GLES)
 * Every 60 frames it logs: CPU build, upload call, GPU time per scene and
 * the scene rate the GPU alone could reach.
 */
#include <dlfcn.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "libretro.h"

/* ---- the GLES2 subset used, loaded through get_proc_address ---- */
typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int          GLint;
typedef int          GLsizei;
typedef float        GLfloat;
typedef unsigned char GLboolean;
typedef char         GLchar;
typedef long         GLsizeiptr;
typedef unsigned char GLubyte;
typedef unsigned int GLbitfield;

#define GL_TRIANGLES            0x0004
#define GL_UNSIGNED_BYTE        0x1401
#define GL_UNSIGNED_SHORT       0x1403
#define GL_FLOAT                0x1406
#define GL_LUMINANCE            0x1909
#define GL_RGB                  0x1907
#define GL_TEXTURE_2D           0x0DE1
#define GL_TEXTURE0             0x84C0
#define GL_TEXTURE_MIN_FILTER   0x2801
#define GL_TEXTURE_MAG_FILTER   0x2800
#define GL_TEXTURE_WRAP_S       0x2802
#define GL_TEXTURE_WRAP_T       0x2803
#define GL_NEAREST              0x2600
#define GL_REPEAT               0x2901
#define GL_CLAMP_TO_EDGE        0x812F
#define GL_ARRAY_BUFFER         0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STREAM_DRAW          0x88E0
#define GL_STATIC_DRAW          0x88E4
#define GL_FRAGMENT_SHADER      0x8B30
#define GL_VERTEX_SHADER        0x8B31
#define GL_COMPILE_STATUS       0x8B81
#define GL_LINK_STATUS          0x8B82
#define GL_FRAMEBUFFER          0x8D40
#define GL_COLOR_BUFFER_BIT     0x00004000
#define GL_DEPTH_TEST           0x0B71
#define GL_BLEND                0x0BE2
#define GL_CULL_FACE            0x0B44
#define GL_UNPACK_ALIGNMENT     0x0CF5
#define GL_RENDERER             0x1F01
#define GL_VERSION              0x1F02
#define GL_SHADING_LANGUAGE_VERSION 0x8B8C
#define GL_MAX_TEXTURE_SIZE     0x0D33
#define GL_HIGH_FLOAT           0x8DF2
#define GL_MEDIUM_FLOAT         0x8DF1

#define GLFN(ret, name, args) static ret (*name) args
GLFN(void, glViewport, (GLint, GLint, GLsizei, GLsizei));
GLFN(void, glClearColor, (GLfloat, GLfloat, GLfloat, GLfloat));
GLFN(void, glClear, (GLbitfield));
GLFN(void, glDisable, (GLenum));
GLFN(void, glBindFramebuffer, (GLenum, GLuint));
GLFN(void, glGenTextures, (GLsizei, GLuint *));
GLFN(void, glBindTexture, (GLenum, GLuint));
GLFN(void, glActiveTexture, (GLenum));
GLFN(void, glTexParameteri, (GLenum, GLenum, GLint));
GLFN(void, glTexImage2D, (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *));
GLFN(void, glPixelStorei, (GLenum, GLint));
GLFN(void, glGenBuffers, (GLsizei, GLuint *));
GLFN(void, glBindBuffer, (GLenum, GLuint));
GLFN(void, glBufferData, (GLenum, GLsizeiptr, const void *, GLenum));
GLFN(GLuint, glCreateShader, (GLenum));
GLFN(void, glShaderSource, (GLuint, GLsizei, const GLchar *const *, const GLint *));
GLFN(void, glCompileShader, (GLuint));
GLFN(void, glGetShaderiv, (GLuint, GLenum, GLint *));
GLFN(void, glGetShaderInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *));
GLFN(GLuint, glCreateProgram, (void));
GLFN(void, glAttachShader, (GLuint, GLuint));
GLFN(void, glBindAttribLocation, (GLuint, GLuint, const GLchar *));
GLFN(void, glLinkProgram, (GLuint));
GLFN(void, glGetProgramiv, (GLuint, GLenum, GLint *));
GLFN(void, glGetProgramInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *));
GLFN(void, glUseProgram, (GLuint));
GLFN(GLint, glGetUniformLocation, (GLuint, const GLchar *));
GLFN(void, glUniform1i, (GLint, GLint));
GLFN(void, glUniform2f, (GLint, GLfloat, GLfloat));
GLFN(void, glEnableVertexAttribArray, (GLuint));
GLFN(void, glVertexAttribPointer, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void *));
GLFN(void, glDrawElements, (GLenum, GLsizei, GLenum, const void *));
GLFN(void, glFinish, (void));
GLFN(const GLubyte *, glGetString, (GLenum));
GLFN(void, glGetIntegerv, (GLenum, GLint *));
GLFN(void, glGetShaderPrecisionFormat, (GLenum, GLenum, GLint *, GLint *));

/* ---- libretro plumbing ---- */
static retro_environment_t       environ_cb;
static retro_video_refresh_t     video_cb;
static retro_log_printf_t        log_cb;
static struct retro_hw_render_callback hw;
static int  width = 1920, height = 1080, layers = 3, repeat = 4;
static bool gl_ready;

static void logf_(const char *fmt, ...)
{
   char buf[1024];
   va_list ap;
   va_start(ap, fmt);
   vsnprintf(buf, sizeof(buf), fmt, ap);
   va_end(ap);
   if (log_cb) log_cb(RETRO_LOG_INFO, "glbench: %s\n", buf);
   else fprintf(stderr, "glbench: %s\n", buf);
}

static double now_ms(void)
{
   struct timespec t;
   clock_gettime(CLOCK_MONOTONIC, &t);
   return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

/* ---- scene ---- */
#define WALL_TEXTURES 32   /* 128x128, power of two: GL_REPEAT wraps tall walls */
#define FLATS         8    /* 64x64 */
#define TEXTURES      (WALL_TEXTURES + FLATS)
#define MAX_QUADS_PER_DRAW 16383 /* 16-bit indices */

typedef struct { float x, y, u, v, light; } vertex_t;

static GLuint textures[TEXTURES], colormap_tex, program, vbo, ibo;
static GLint  u_res, u_atlas, u_colormap;
static vertex_t *verts;            /* bucketed per texture */
static int  bucket_start[TEXTURES + 1], bucket_count[TEXTURES];
static int  max_quads;
static unsigned frame;

static const char *vs_src =
   "#version 100\n"
   "attribute vec2 a_pos;\n"
   "attribute vec2 a_uv;\n"
   "attribute float a_light;\n"
   "uniform vec2 u_res;\n"
   "varying vec2 v_uv;\n"
   "varying float v_light;\n"
   "void main() {\n"
   "  v_uv = a_uv;\n"
   "  v_light = a_light;\n"
   "  gl_Position = vec4(a_pos / u_res * 2.0 - 1.0, 0.0, 1.0);\n"
   "}\n";

/* no math on v_uv: it goes straight into texture2D */
static const char *fs_src =
   "#version 100\n"
   "precision mediump float;\n"
   "uniform sampler2D u_atlas;\n"
   "uniform sampler2D u_colormap;\n"
   "varying vec2 v_uv;\n"
   "varying float v_light;\n"
   "void main() {\n"
   "  float index = texture2D(u_atlas, v_uv).r;\n"
   "  gl_FragColor = texture2D(u_colormap, vec2(index * (255.0 / 256.0) + (0.5 / 256.0), v_light));\n"
   "}\n";

static GLuint compile(GLenum type, const char *src)
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
      logf_("shader error: %s", log);
   }
   return s;
}

/* 8-bit patterns that show any texel jitter when stretched: thin stripes,
 * a diagonal and a 1-texel checker border */
static void make_texture(uint8_t *p, int size, int seed)
{
   int x, y;
   for (y = 0; y < size; y++)
      for (x = 0; x < size; x++)
      {
         int v = (seed * 37 + (x ^ y) * 3) & 0xff;
         if ((y & 7) == 0) v = 250;
         if (((x + y) & 31) == 0) v = 10;
         if (x == 0 || y == 0) v = ((x + y) & 1) ? 255 : 0;
         p[y * size + x] = (uint8_t)v;
      }
}

/* Old EGL (before 1.5, common with Mali drivers) only hands out extension
 * functions through eglGetProcAddress, so the core GLES2 ones may come back
 * NULL from the frontend: then they come straight from the GLES library the
 * frontend already has loaded, or from loading it */
static void *gl_lib;
static int   gl_from_frontend, gl_from_process, gl_from_library;

static void *gl_proc(const char *name)
{
   void *p = hw.get_proc_address ? (void *)hw.get_proc_address(name) : NULL;
   if (p) { gl_from_frontend++; return p; }
   p = dlsym(RTLD_DEFAULT, name);
   if (p) { gl_from_process++; return p; }
   if (!gl_lib) gl_lib = dlopen("libGLESv2.so.2", RTLD_LAZY | RTLD_GLOBAL);
   if (!gl_lib) gl_lib = dlopen("libGLESv2.so", RTLD_LAZY | RTLD_GLOBAL);
   p = gl_lib ? dlsym(gl_lib, name) : NULL;
   if (p) gl_from_library++;
   return p;
}

static void load_gl(void)
{
#define LOAD(name) name = gl_proc(#name); if (!name) logf_("missing %s", #name);
   LOAD(glViewport) LOAD(glClearColor) LOAD(glClear) LOAD(glDisable) LOAD(glBindFramebuffer)
   LOAD(glGenTextures) LOAD(glBindTexture) LOAD(glActiveTexture) LOAD(glTexParameteri)
   LOAD(glTexImage2D) LOAD(glPixelStorei) LOAD(glGenBuffers) LOAD(glBindBuffer) LOAD(glBufferData)
   LOAD(glCreateShader) LOAD(glShaderSource) LOAD(glCompileShader) LOAD(glGetShaderiv)
   LOAD(glGetShaderInfoLog) LOAD(glCreateProgram) LOAD(glAttachShader) LOAD(glBindAttribLocation)
   LOAD(glLinkProgram) LOAD(glGetProgramiv) LOAD(glGetProgramInfoLog) LOAD(glUseProgram)
   LOAD(glGetUniformLocation) LOAD(glUniform1i) LOAD(glUniform2f) LOAD(glEnableVertexAttribArray)
   LOAD(glVertexAttribPointer) LOAD(glDrawElements) LOAD(glFinish) LOAD(glGetString)
   LOAD(glGetIntegerv) LOAD(glGetShaderPrecisionFormat)
#undef LOAD
   logf_("GL functions: %d from the frontend, %d from the process, %d from libGLESv2",
         gl_from_frontend, gl_from_process, gl_from_library);
}

static void context_reset(void)
{
   GLint ok = 0, range[2] = {0, 0}, precision = 0, max_tex = 0;
   uint8_t *pix;
   uint16_t *idx;
   int i;

   load_gl();
   logf_("GL_RENDERER: %s", (const char *)glGetString(GL_RENDERER));
   logf_("GL_VERSION: %s | GLSL: %s", (const char *)glGetString(GL_VERSION),
         (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION));
   glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_tex);
   logf_("GL_MAX_TEXTURE_SIZE: %d", max_tex);
   if (glGetShaderPrecisionFormat)
   {
      glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_HIGH_FLOAT, range, &precision);
      logf_("fragment highp: precision %d bits%s", precision, precision ? "" : " (not supported)");
      glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_MEDIUM_FLOAT, range, &precision);
      logf_("fragment mediump: precision %d bits", precision);
   }

   program = glCreateProgram();
   glAttachShader(program, compile(GL_VERTEX_SHADER, vs_src));
   glAttachShader(program, compile(GL_FRAGMENT_SHADER, fs_src));
   glBindAttribLocation(program, 0, "a_pos");
   glBindAttribLocation(program, 1, "a_uv");
   glBindAttribLocation(program, 2, "a_light");
   glLinkProgram(program);
   glGetProgramiv(program, GL_LINK_STATUS, &ok);
   if (!ok)
   {
      char log[1024];
      glGetProgramInfoLog(program, sizeof(log), NULL, log);
      logf_("link error: %s", log);
   }
   u_res = glGetUniformLocation(program, "u_res");
   u_atlas = glGetUniformLocation(program, "u_atlas");
   u_colormap = glGetUniformLocation(program, "u_colormap");

   /* indexed textures */
   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
   glGenTextures(TEXTURES, textures);
   pix = (uint8_t *)malloc(128 * 128);
   for (i = 0; i < TEXTURES; i++)
   {
      const int size = i < WALL_TEXTURES ? 128 : 64;
      make_texture(pix, size, i);
      glBindTexture(GL_TEXTURE_2D, textures[i]);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, size, size, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, pix);
   }
   free(pix);

   /* COLORMAP: 256 colours x 32 light levels, darker going down */
   {
      uint8_t *cm = (uint8_t *)malloc(256 * 32 * 3);
      int c, l;
      for (l = 0; l < 32; l++)
         for (c = 0; c < 256; c++)
         {
            const float k = 1.0f - l / 34.0f;
            cm[(l * 256 + c) * 3 + 0] = (uint8_t)(((c * 7) & 0xff) * k);
            cm[(l * 256 + c) * 3 + 1] = (uint8_t)(c * k);
            cm[(l * 256 + c) * 3 + 2] = (uint8_t)(((255 - c) * 3 & 0xff) * k);
         }
      glGenTextures(1, &colormap_tex);
      glBindTexture(GL_TEXTURE_2D, colormap_tex);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, 32, 0, GL_RGB, GL_UNSIGNED_BYTE, cm);
      free(cm);
   }

   /* quads: walls (columns x layers) + floor/ceiling rows */
   max_quads = width * layers + height;
   free(verts);
   verts = (vertex_t *)malloc(sizeof(vertex_t) * 4 * max_quads);
   glGenBuffers(1, &vbo);
   glGenBuffers(1, &ibo);
   idx = (uint16_t *)malloc(sizeof(uint16_t) * 6 * MAX_QUADS_PER_DRAW);
   for (i = 0; i < MAX_QUADS_PER_DRAW; i++)
   {
      idx[i * 6 + 0] = (uint16_t)(i * 4 + 0);
      idx[i * 6 + 1] = (uint16_t)(i * 4 + 1);
      idx[i * 6 + 2] = (uint16_t)(i * 4 + 2);
      idx[i * 6 + 3] = (uint16_t)(i * 4 + 0);
      idx[i * 6 + 4] = (uint16_t)(i * 4 + 2);
      idx[i * 6 + 5] = (uint16_t)(i * 4 + 3);
   }
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
   glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint16_t) * 6 * MAX_QUADS_PER_DRAW, idx, GL_STATIC_DRAW);
   free(idx);

   logf_("%dx%d, %d wall layers, %d quads/scene, scene painted %d times per frame",
         width, height, layers, max_quads, repeat);
   gl_ready = true;
}

static void context_destroy(void) { gl_ready = false; }

static void quad(vertex_t *q, float x0, float y0, float x1, float y1,
                 float u0, float v0, float u1, float v1, float light, int column)
{
   /* a column runs its v down the quad, a row runs its u across */
   q[0] = (vertex_t){ x0, y0, u0, v0, light };
   q[1] = (vertex_t){ x1, y0, column ? u0 : u1, column ? v0 : v1, light };
   q[2] = (vertex_t){ x1, y1, column ? u0 : u1, column ? v1 : v1, light };
   q[3] = (vertex_t){ x0, y1, u0, column ? v1 : v0, light };
}

/* Doom-like frame: ceiling and floor rows (plane per band, u/v affine along
 * the row), then for each column a wall split in `layers` pieces, each a
 * 1-pixel wide quad whose v stretches its texture over the piece; light
 * from the distance. Quads are bucketed per texture (solid walls and
 * planes never overlap, so the order inside a bucket does not matter). */
static void build_scene(void)
{
   const float t = frame * 0.02f;
   int x, y, k, n;
   int fill[TEXTURES];

   /* pass 1: count per texture */
   memset(bucket_count, 0, sizeof(bucket_count));
   for (y = 0; y < height; y++)
      bucket_count[WALL_TEXTURES + (y * FLATS / height)]++;
   for (x = 0; x < width; x++)
      for (k = 0; k < layers; k++)
         bucket_count[(x / 64 + k * 7) % WALL_TEXTURES]++;
   n = 0;
   for (k = 0; k < TEXTURES; k++)
   {
      bucket_start[k] = n;
      fill[k] = n;
      n += bucket_count[k];
   }
   bucket_start[TEXTURES] = n;

   /* pass 2: fill */
   for (y = 0; y < height; y++)
   {
      const int flat = WALL_TEXTURES + (y * FLATS / height);
      const float dist = 1.0f + 200.0f / (1.0f + (y < height / 2 ? height / 2 - y : y - height / 2));
      const float u0 = t * 3.0f, v0 = dist * 4.0f + t;
      const float light = ((y * 31 / height) + 0.5f) / 32.0f;
      quad(&verts[fill[flat]++ * 4], 0, (float)y, (float)width, (float)(y + 1),
           u0 / 64.0f, v0 / 64.0f, (u0 + dist * 30.0f) / 64.0f, v0 / 64.0f, light, 0);
   }
   for (x = 0; x < width; x++)
   {
      const float h = height * (0.35f + 0.3f * (0.5f + 0.5f * __builtin_sinf(x * 0.004f + t)));
      const float top = (height - h) / 2;
      const float light = ((float)((x / 8 + frame / 4) % 31) + 0.5f) / 32.0f;
      for (k = 0; k < layers; k++)
      {
         const int tex = (x / 64 + k * 7) % WALL_TEXTURES;
         const float y0 = top + h * k / layers, y1 = top + h * (k + 1) / layers;
         /* the full 128 texels (and more) stretched over the piece */
         const float u = ((x * 2 + frame) % 128 + 0.5f) / 128.0f;
         const float v0 = 0.0f, v1 = 1.5f;
         quad(&verts[fill[tex]++ * 4], (float)x, y0, (float)(x + 1), y1,
              u, v0, u, v1, light, 1);
      }
   }
}

/* the test's floor/ceiling rows span the whole width (unlike Doom's, which
 * stop at the walls), so they go first and the walls over them */
static void draw_scene(void)
{
   int i;
   for (i = 0; i < TEXTURES; i++)
   {
      const int k = (i + WALL_TEXTURES) % TEXTURES;
      int first = bucket_start[k];
      int count = bucket_count[k];
      glBindTexture(GL_TEXTURE_2D, textures[k]);
      while (count > 0)
      {
         const int chunk = count < MAX_QUADS_PER_DRAW ? count : MAX_QUADS_PER_DRAW;
         const char *base = (const char *)(intptr_t)(first * 4 * sizeof(vertex_t));
         glVertexAttribPointer(0, 2, GL_FLOAT, 0, sizeof(vertex_t), base);
         glVertexAttribPointer(1, 2, GL_FLOAT, 0, sizeof(vertex_t), base + 8);
         glVertexAttribPointer(2, 1, GL_FLOAT, 0, sizeof(vertex_t), base + 16);
         glDrawElements(GL_TRIANGLES, chunk * 6, GL_UNSIGNED_SHORT, 0);
         first += chunk;
         count -= chunk;
      }
   }
}

static double sum_build, sum_upload, sum_gpu;

void retro_run(void)
{
   double t0, t1, t2, t3;
   int r;

   if (!gl_ready)
   {
      video_cb(NULL, width, height, 0);
      return;
   }

   t0 = now_ms();
   build_scene();
   t1 = now_ms();

   glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)hw.get_current_framebuffer());
   glViewport(0, 0, width, height);
   glDisable(GL_DEPTH_TEST);
   glDisable(GL_BLEND);
   glDisable(GL_CULL_FACE);
   glUseProgram(program);
   glUniform2f(u_res, (float)width, (float)height);
   glUniform1i(u_atlas, 0);
   glUniform1i(u_colormap, 1);
   glActiveTexture(GL_TEXTURE0 + 1);
   glBindTexture(GL_TEXTURE_2D, colormap_tex);
   glActiveTexture(GL_TEXTURE0);

   glBindBuffer(GL_ARRAY_BUFFER, vbo);
   glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_t) * 4 * bucket_start[TEXTURES], verts, GL_STREAM_DRAW);
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
   glEnableVertexAttribArray(0);
   glEnableVertexAttribArray(1);
   glEnableVertexAttribArray(2);
   t2 = now_ms();

   for (r = 0; r < repeat; r++)
   {
      glClearColor(0, 0, 0, 1);
      glClear(GL_COLOR_BUFFER_BIT);
      draw_scene();
   }
   glFinish();
   t3 = now_ms();

   sum_build += t1 - t0;
   sum_upload += t2 - t1;
   sum_gpu += t3 - t2;
   frame++;
   if (frame % 60 == 0)
   {
      const double gpu_scene = sum_gpu / 60 / repeat;
      logf_("cpu build %.2f ms | upload %.2f ms | gpu %.2f ms/scene (%.0f scenes/s gpu only) | %d quads, %.1f MB vertices",
            sum_build / 60, sum_upload / 60, gpu_scene, gpu_scene > 0 ? 1000.0 / gpu_scene : 0,
            bucket_start[TEXTURES], sizeof(vertex_t) * 4.0 * bucket_start[TEXTURES] / (1024 * 1024));
      sum_build = sum_upload = sum_gpu = 0;
   }

   video_cb(RETRO_HW_FRAME_BUFFER_VALID, width, height, 0);
}

/* ---- the rest of the libretro API ---- */
void retro_set_environment(retro_environment_t cb)
{
   bool no_game = true;
   struct retro_log_callback log;
   environ_cb = cb;
   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_game);
   if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
      log_cb = log.log;
}
void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { (void)cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { (void)cb; }
void retro_set_input_poll(retro_input_poll_t cb) { (void)cb; }
void retro_set_input_state(retro_input_state_t cb) { (void)cb; }
unsigned retro_api_version(void) { return RETRO_API_VERSION; }

void retro_init(void)
{
   const char *res = getenv("GLBENCH_RES");
   const char *l = getenv("GLBENCH_LAYERS");
   const char *r = getenv("GLBENCH_REPEAT");
   if (res) sscanf(res, "%dx%d", &width, &height);
   if (l) layers = atoi(l);
   if (r) repeat = atoi(r);
   if (layers < 1) layers = 1;
   if (repeat < 1) repeat = 1;
}
void retro_deinit(void) { free(verts); verts = NULL; }

void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name = "glbench";
   info->library_version = "1";
   info->need_fullpath = false;
   info->valid_extensions = "";
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   memset(info, 0, sizeof(*info));
   info->timing.fps = 60.0;
   info->timing.sample_rate = 44100.0;
   info->geometry.base_width = width;
   info->geometry.base_height = height;
   info->geometry.max_width = width;
   info->geometry.max_height = height;
   info->geometry.aspect_ratio = (float)width / height;
}

bool retro_load_game(const struct retro_game_info *game)
{
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
   (void)game;
   environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);
   memset(&hw, 0, sizeof(hw));
   hw.context_type = getenv("GLBENCH_GL") ? RETRO_HW_CONTEXT_OPENGL : RETRO_HW_CONTEXT_OPENGLES2;
   hw.context_reset = context_reset;
   hw.context_destroy = context_destroy;
   hw.bottom_left_origin = true;
   if (!environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw))
   {
      logf_("frontend refused the GL context");
      return false;
   }
   return true;
}

void retro_unload_game(void) {}
void retro_set_controller_port_device(unsigned port, unsigned device) { (void)port; (void)device; }
void retro_reset(void) {}
size_t retro_serialize_size(void) { return 0; }
bool retro_serialize(void *data, size_t size) { (void)data; (void)size; return false; }
bool retro_unserialize(const void *data, size_t size) { (void)data; (void)size; return false; }
void retro_cheat_reset(void) {}
void retro_cheat_set(unsigned index, bool enabled, const char *code) { (void)index; (void)enabled; (void)code; }
bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{ (void)type; (void)info; (void)num; return false; }
unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
void *retro_get_memory_data(unsigned id) { (void)id; return NULL; }
size_t retro_get_memory_size(unsigned id) { (void)id; return 0; }
