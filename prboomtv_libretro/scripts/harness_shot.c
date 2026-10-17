/* headless libretro runner: loads a core + content, runs frames with a
 * scripted joypad, and dumps chosen frames as PPM.
 *
 * usage: shot <core.so> <content> <dir> <frames> [frame:buttons ...] [snap@frame ...]
 *   buttons: comma list of libretro joypad ids held on that frame range
 *            e.g. 300-305:3 holds START (id 3) on frames 300..305
 *   snap@N  writes <dir>/frame_N.ppm
 *
 * Cores that ask for a GL context (SET_HW_RENDER, GLES2 or GL) get one
 * from EGL with no window (Mesa surfaceless, else the default display
 * with a pbuffer) and render into an FBO, read back for the snaps; every
 * frame ends with glFinish, so the fps counts the GPU too; the time spent
 * waiting in it is logged every 300 frames.
 *   SHOT_SYNC=flush   glFlush instead: the GPU works while the core already
 *                     builds the next frame, like a frontend that only
 *                     presents the FBO (no glFinish); the fps is then the
 *                     real throughput
 *   SHOT_NULL_PROC=1  get_proc_address answers NULL, like old Mali EGL
 *   SHOT_EGL=default  skip the surfaceless platform
 *   SHOT_NO_HW=1      refuse the GL context, like a frontend without GL
 * SHOT_INFO=1 shows the core's info log too.
 * SHOT_SLOW_MS=n makes every drawn frame n ms slower (a slow device, for
 * prboom-frameskip=auto); SHOT_SLOW_LOG=1 prints how many were drawn.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "libretro.h"

static const char *g_dir;
static unsigned g_fmt = RETRO_PIXEL_FORMAT_0RGB1555;
static int g_frame;
static struct { int from, to, id; } g_holds[64];
static int g_nholds;
static int g_snaps[64];
static int g_nsnaps;

/* ---- GL through EGL, for SET_HW_RENDER ---- */
typedef void *(*proc_fn)(const char *);
static struct retro_hw_render_callback g_hw;
static int g_hw_on;
static void *g_egl, *g_gl;
static proc_fn g_egl_proc;
static unsigned g_fbo, g_fbo_w, g_fbo_h;
static double g_finish_ms;
static int g_finish_frames;
static void (*p_glGenFramebuffers)(int, unsigned *);
static void (*p_glBindFramebuffer)(unsigned, unsigned);
static void (*p_glGenTextures)(int, unsigned *);
static void (*p_glBindTexture)(unsigned, unsigned);
static void (*p_glTexImage2D)(unsigned, int, int, int, int, int, unsigned, unsigned, const void *);
static void (*p_glTexParameteri)(unsigned, unsigned, int);
static void (*p_glFramebufferTexture2D)(unsigned, unsigned, unsigned, unsigned, int);
static unsigned (*p_glCheckFramebufferStatus)(unsigned);
static void (*p_glReadPixels)(int, int, int, int, unsigned, unsigned, void *);
static void (*p_glFinish)(void);
static void (*p_glFlush)(void);
static void (*p_glPixelStorei)(unsigned, int);

static void *gl_lookup(const char *name)
{
   static const char *const libs[] = { "libGLESv2.so.2", "libGLESv2.so", "libGL.so.1" };
   void *p = g_egl_proc ? g_egl_proc(name) : NULL;
   size_t i;
   if (p) return p;
   p = dlsym(RTLD_DEFAULT, name);
   if (p) return p;
   for (i = 0; !g_gl && i < sizeof(libs) / sizeof(*libs); i++)
      g_gl = dlopen(libs[i], RTLD_LAZY | RTLD_GLOBAL);
   return g_gl ? dlsym(g_gl, name) : NULL;
}

static uintptr_t hw_get_fbo(void) { return g_fbo; }

static retro_proc_address_t hw_get_proc(const char *sym)
{
   void *p = getenv("SHOT_NULL_PROC") ? NULL : gl_lookup(sym);
   return (retro_proc_address_t)p;
}

static int egl_start(int desktop, unsigned w, unsigned h)
{
   void *(*eglGetDisplay)(void *);
   void *(*eglGetPlatformDisplayEXT)(unsigned, void *, const int *);
   unsigned (*eglInitialize)(void *, int *, int *);
   unsigned (*eglBindAPI)(unsigned);
   unsigned (*eglChooseConfig)(void *, const int *, void **, int, int *);
   void *(*eglCreateContext)(void *, void *, void *, const int *);
   void *(*eglCreatePbufferSurface)(void *, void *, const int *);
   unsigned (*eglMakeCurrent)(void *, void *, void *, void *);
   const int cfg_attr[] = { 0x3033, 0x0001, 0x3040, desktop ? 0x0008 : 0x0004,
                            0x3024, 8, 0x3023, 8, 0x3022, 8, 0x3038 };
   const int es_attr[] = { 0x3098, 2, 0x3038 };
   const int pb_attr[] = { 0x3057, 16, 0x3056, 16, 0x3038 };
   void *dpy = NULL, *cfg = NULL, *ctx, *surf;
   int major, minor, n = 0;
   unsigned tex;

   g_egl = dlopen("libEGL.so.1", RTLD_NOW | RTLD_GLOBAL);
   if (!g_egl) g_egl = dlopen("libEGL.so", RTLD_NOW | RTLD_GLOBAL);
   if (!g_egl) { fprintf(stderr, "no libEGL\n"); return 0; }
#define E(name) *(void **)&name = dlsym(g_egl, #name); if (!name) { fprintf(stderr, "missing %s\n", #name); return 0; }
   E(eglGetDisplay) E(eglInitialize) E(eglBindAPI) E(eglChooseConfig)
   E(eglCreateContext) E(eglCreatePbufferSurface) E(eglMakeCurrent)
#undef E
   *(void **)&g_egl_proc = dlsym(g_egl, "eglGetProcAddress");
   *(void **)&eglGetPlatformDisplayEXT = g_egl_proc ? g_egl_proc("eglGetPlatformDisplayEXT") : NULL;
   if (eglGetPlatformDisplayEXT && !(getenv("SHOT_EGL") && !strcmp(getenv("SHOT_EGL"), "default")))
   {
      dpy = eglGetPlatformDisplayEXT(0x31DD, NULL, NULL); /* surfaceless */
      if (dpy && !eglInitialize(dpy, &major, &minor)) dpy = NULL;
   }
   if (!dpy)
   {
      dpy = eglGetDisplay(NULL);
      if (!dpy || !eglInitialize(dpy, &major, &minor)) { fprintf(stderr, "eglInitialize failed\n"); return 0; }
   }
   eglBindAPI(desktop ? 0x30A2 : 0x30A0);
   if (!eglChooseConfig(dpy, cfg_attr, &cfg, 1, &n) || n < 1) { fprintf(stderr, "no EGL config\n"); return 0; }
   ctx = eglCreateContext(dpy, cfg, NULL, desktop ? NULL : es_attr);
   surf = eglCreatePbufferSurface(dpy, cfg, pb_attr);
   if (!ctx || !eglMakeCurrent(dpy, surf, surf, ctx)) { fprintf(stderr, "eglMakeCurrent failed\n"); return 0; }

#define G(name) *(void **)&p_##name = gl_lookup(#name); if (!p_##name) { fprintf(stderr, "missing %s\n", #name); return 0; }
   G(glGenFramebuffers) G(glBindFramebuffer) G(glGenTextures) G(glBindTexture)
   G(glTexImage2D) G(glTexParameteri) G(glFramebufferTexture2D)
   G(glCheckFramebufferStatus) G(glReadPixels) G(glFinish) G(glFlush) G(glPixelStorei)
#undef G
   p_glGenTextures(1, &tex);
   p_glBindTexture(0x0DE1, tex);
   p_glTexParameteri(0x0DE1, 0x2801, 0x2600);
   p_glTexParameteri(0x0DE1, 0x2800, 0x2600);
   p_glTexImage2D(0x0DE1, 0, 0x1908, w, h, 0, 0x1908, 0x1401, NULL); /* RGBA8 */
   p_glGenFramebuffers(1, &g_fbo);
   p_glBindFramebuffer(0x8D40, g_fbo);
   p_glFramebufferTexture2D(0x8D40, 0x8CE0, 0x0DE1, tex, 0);
   if (p_glCheckFramebufferStatus(0x8D40) != 0x8CD5) { fprintf(stderr, "FBO incomplete\n"); return 0; }
   g_fbo_w = w; g_fbo_h = h;
   fprintf(stderr, "EGL %d.%d, %s context, FBO %ux%u\n", major, minor, desktop ? "GL" : "GLES2", w, h);
   return 1;
}

static void log_cb(enum retro_log_level level, const char *fmt, ...)
{
   va_list ap;
   if (level < RETRO_LOG_WARN && !getenv("SHOT_INFO")) return;
   va_start(ap, fmt);
   vfprintf(stderr, fmt, ap);
   va_end(ap);
}

static bool env_cb(unsigned cmd, void *data)
{
   switch (cmd)
   {
   case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
      g_fmt = *(const enum retro_pixel_format *)data;
      return true;
   case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
   case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
      *(const char **)data = g_dir;
      return true;
   case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
      ((struct retro_log_callback *)data)->log = log_cb;
      return true;
   case RETRO_ENVIRONMENT_GET_VARIABLE:
   {
      struct retro_variable *var = (struct retro_variable *)data;
      const char *all = getenv("SHOT_VARS");
      static char value[256];
      size_t klen = strlen(var->key);
      const char *p = all;
      var->value = NULL;
      while (p && *p)
      {
         const char *end = strchr(p, ',');
         size_t len = end ? (size_t)(end - p) : strlen(p);
         if (len > klen && !strncmp(p, var->key, klen) && p[klen] == '=')
         {
            snprintf(value, sizeof(value), "%.*s", (int)(len - klen - 1), p + klen + 1);
            var->value = value;
            return true;
         }
         p = end ? end + 1 : NULL;
      }
      return false;
   }
   case RETRO_ENVIRONMENT_SET_GEOMETRY:
   case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
      return true;
   case RETRO_ENVIRONMENT_SET_HW_RENDER:
   {
      struct retro_hw_render_callback *cb = (struct retro_hw_render_callback *)data;
      if (getenv("SHOT_NO_HW")
            || (cb->context_type != RETRO_HW_CONTEXT_OPENGLES2 && cb->context_type != RETRO_HW_CONTEXT_OPENGL))
         return false;
      cb->get_current_framebuffer = hw_get_fbo;
      cb->get_proc_address = hw_get_proc;
      g_hw = *cb;
      g_hw_on = 1;
      return true;
   }
   case RETRO_ENVIRONMENT_GET_CAN_DUPE:
      *(bool *)data = true;
      return true;
   default:
      return false;
   }
}

static int g_drawn;

static void video_cb(const void *data, unsigned w, unsigned h, size_t pitch)
{
   char path[1024];
   FILE *f;
   unsigned x, y;
   int i, snap = 0;

   static uint8_t *hw_pixels;
   static size_t hw_size;

   if (data)
   {
      g_drawn++;
      if (getenv("SHOT_SLOW_MS"))
      {
         struct timespec d = { 0, atoi(getenv("SHOT_SLOW_MS")) * 1000000L };
         nanosleep(&d, NULL);
      }
   }

   if (data == RETRO_HW_FRAME_BUFFER_VALID && getenv("SHOT_SYNC") && !strcmp(getenv("SHOT_SYNC"), "flush"))
      p_glFlush();
   else if (data == RETRO_HW_FRAME_BUFFER_VALID)
   {
      /* what the GPU (and the driver behind it) still owes the frame */
      struct timespec a, b;
      clock_gettime(CLOCK_MONOTONIC, &a);
      p_glFinish();
      clock_gettime(CLOCK_MONOTONIC, &b);
      g_finish_ms += (b.tv_sec - a.tv_sec) * 1000.0 + (b.tv_nsec - a.tv_nsec) / 1e6;
      if (++g_finish_frames == 300)
      {
         fprintf(stderr, "glFinish: %.2f ms per frame waiting for the GPU\n", g_finish_ms / 300);
         g_finish_ms = 0;
         g_finish_frames = 0;
      }
   }
   for (i = 0; i < g_nsnaps; i++)
      if (g_snaps[i] == g_frame) snap = 1;
   if (!snap || !data) return;
   if (data == RETRO_HW_FRAME_BUFFER_VALID)
   {
      /* RGBA, bottom row first: turned into the XRGB8888 rows below */
      const size_t size = (size_t)w * h * 4;
      uint8_t *flip;
      if (hw_size < size) { hw_pixels = realloc(hw_pixels, size * 2); hw_size = size; }
      flip = hw_pixels + size;
      p_glBindFramebuffer(0x8D40, g_fbo);
      p_glPixelStorei(0x0D05, 1); /* PACK_ALIGNMENT */
      p_glReadPixels(0, 0, w, h, 0x1908, 0x1401, hw_pixels);
      for (y = 0; y < h; y++)
         for (x = 0; x < w; x++)
         {
            const uint8_t *src = hw_pixels + ((size_t)(h - 1 - y) * w + x) * 4;
            uint32_t *dst = (uint32_t *)(flip + ((size_t)y * w + x) * 4);
            *dst = (uint32_t)src[0] << 16 | (uint32_t)src[1] << 8 | src[2];
         }
      data = flip;
      pitch = (size_t)w * 4;
      g_fmt = RETRO_PIXEL_FORMAT_XRGB8888;
   }

   snprintf(path, sizeof(path), "%s/frame_%d.ppm", g_dir, g_frame);
   f = fopen(path, "wb");
   fprintf(f, "P6\n%u %u\n255\n", w, h);
   for (y = 0; y < h; y++)
      for (x = 0; x < w; x++)
      {
         const uint8_t *row = (const uint8_t *)data + y * pitch;
         uint8_t rgb[3];
         if (g_fmt == RETRO_PIXEL_FORMAT_XRGB8888)
         {
            uint32_t p = ((const uint32_t *)row)[x];
            rgb[0] = p >> 16; rgb[1] = p >> 8; rgb[2] = p;
         }
         else
         {
            uint16_t p = ((const uint16_t *)row)[x];
            rgb[0] = ((p >> 11) & 31) << 3; rgb[1] = ((p >> 5) & 63) << 2; rgb[2] = (p & 31) << 3;
         }
         fwrite(rgb, 1, 3, f);
      }
   fclose(f);
   fprintf(stderr, "snap %s (%ux%u)\n", path, w, h);
}

static void audio_cb(int16_t l, int16_t r) { (void)l; (void)r; }
static size_t audio_batch_cb(const int16_t *d, size_t n) { (void)d; return n; }
static void poll_cb(void) {}

static int16_t state_cb(unsigned port, unsigned device, unsigned index, unsigned id)
{
   int i;
   if (port || device != RETRO_DEVICE_JOYPAD) return 0;
   if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
   {
      int16_t mask = 0;
      for (i = 0; i < g_nholds; i++)
         if (g_frame >= g_holds[i].from && g_frame <= g_holds[i].to)
            mask |= 1 << g_holds[i].id;
      return mask;
   }
   for (i = 0; i < g_nholds; i++)
      if (g_holds[i].id == (int)id && g_frame >= g_holds[i].from && g_frame <= g_holds[i].to)
         return 1;
   return 0;
}

#define SYM(name) name = (typeof(name))dlsym(core, #name); if (!name) { fprintf(stderr, "missing %s\n", #name); return 1; }

int main(int argc, char **argv)
{
   void (*retro_set_environment)(retro_environment_t);
   void (*retro_set_video_refresh)(retro_video_refresh_t);
   void (*retro_set_audio_sample)(retro_audio_sample_t);
   void (*retro_set_audio_sample_batch)(retro_audio_sample_batch_t);
   void (*retro_set_input_poll)(retro_input_poll_t);
   void (*retro_set_input_state)(retro_input_state_t);
   void (*retro_init)(void);
   bool (*retro_load_game)(const struct retro_game_info *);
   void (*retro_run)(void);
   void (*retro_unload_game)(void);
   void (*retro_deinit)(void);
   struct retro_game_info game = {0};
   void *core;
   int frames, i;

   if (argc < 5) { fprintf(stderr, "usage\n"); return 1; }
   core = dlopen(argv[1], RTLD_NOW);
   if (!core) { fprintf(stderr, "%s\n", dlerror()); return 1; }
   g_dir = argv[3];
   frames = atoi(argv[4]);
   for (i = 5; i < argc; i++)
   {
      int a, b, id;
      if (sscanf(argv[i], "snap@%d", &a) == 1) g_snaps[g_nsnaps++] = a;
      else if (sscanf(argv[i], "%d-%d:%d", &a, &b, &id) == 3)
      { g_holds[g_nholds].from = a; g_holds[g_nholds].to = b; g_holds[g_nholds++].id = id; }
   }

   SYM(retro_set_environment) SYM(retro_set_video_refresh) SYM(retro_set_audio_sample)
   SYM(retro_set_audio_sample_batch) SYM(retro_set_input_poll) SYM(retro_set_input_state)
   SYM(retro_init) SYM(retro_load_game) SYM(retro_run) SYM(retro_unload_game) SYM(retro_deinit)

   retro_set_environment(env_cb);
   retro_set_video_refresh(video_cb);
   retro_set_audio_sample(audio_cb);
   retro_set_audio_sample_batch(audio_batch_cb);
   retro_set_input_poll(poll_cb);
   retro_set_input_state(state_cb);
   retro_init();
   game.path = argv[2];
   if (!retro_load_game(&game)) { fprintf(stderr, "load failed\n"); return 1; }
   if (g_hw_on)
   {
      void (*retro_get_system_av_info)(struct retro_system_av_info *);
      struct retro_system_av_info av;
      SYM(retro_get_system_av_info)
      retro_get_system_av_info(&av);
      if (!egl_start(g_hw.context_type == RETRO_HW_CONTEXT_OPENGL,
                     av.geometry.max_width, av.geometry.max_height))
         return 1;
      g_hw.context_reset();
   }
   {
      /* fps over everything after the first 150 frames (loading, title) */
      const int warm = frames > 300 ? 150 : 0;
      struct timespec t0, t1;
      for (g_frame = 0; g_frame < frames; g_frame++)
      {
         if (g_frame == warm)
            clock_gettime(CLOCK_MONOTONIC, &t0);
         retro_run();
      }
      clock_gettime(CLOCK_MONOTONIC, &t1);
      {
         double secs = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
         fprintf(stderr, "core only: %d frames in %.2fs = %.1f fps\n",
                 frames - warm, secs, (frames - warm) / secs);
         if (getenv("SHOT_SLOW_LOG"))
            fprintf(stderr, "drawn: %d of %d frames\n", g_drawn, frames);
      }
   }
   retro_unload_game();
   retro_deinit();
   return 0;
}
