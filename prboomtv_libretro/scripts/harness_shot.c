/* headless libretro runner: loads a core + content, runs frames with a
 * scripted joypad, and dumps chosen frames as PPM.
 *
 * usage: shot <core.so> <content> <dir> <frames> [frame:buttons ...] [snap@frame ...]
 *   buttons: comma list of libretro joypad ids held on that frame range
 *            e.g. 300-305:3 holds START (id 3) on frames 300..305
 *   snap@N  writes <dir>/frame_N.ppm
 */
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

static void log_cb(enum retro_log_level level, const char *fmt, ...)
{
   va_list ap;
   if (level < RETRO_LOG_WARN) return;
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
   case RETRO_ENVIRONMENT_GET_CAN_DUPE:
      *(bool *)data = true;
      return true;
   default:
      return false;
   }
}

static void video_cb(const void *data, unsigned w, unsigned h, size_t pitch)
{
   char path[1024];
   FILE *f;
   unsigned x, y;
   int i, snap = 0;

   for (i = 0; i < g_nsnaps; i++)
      if (g_snaps[i] == g_frame) snap = 1;
   if (!snap || !data) return;

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
      }
   }
   retro_unload_game();
   retro_deinit();
   return 0;
}
