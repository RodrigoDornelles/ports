/**
 * @brief Change Game: switching to another WAD from Settings without
 * leaving the core.
 *
 * The menu lists the WADs (and m3u playlists) next to the running content
 * and asks for a switch; the switch waits for the next retro_run, outside
 * the menu and the game loop, and goes through the same unload and load
 * the frontend uses, which the core already supports in process. If the
 * new content fails to load, the core asks the frontend to shut down. The
 * frontend is not told about the switch: its history and per-game
 * settings keep the first content.
 */

/**
 * @brief Game list, the pending switch and the content in use.
 *
 * @patch libretro/libretro.c 1776
 */
#include "dopo/change_game.h"

#define DOPO_GAMES_MAX  64
#define DOPO_GAME_PATH  512

static char dopo_game_current[DOPO_GAME_PATH];
static char dopo_game_pending[DOPO_GAME_PATH];
static dbool dopo_game_failed;  /* nothing loaded, waiting for shutdown */
static char dopo_games[DOPO_GAMES_MAX][DOPO_GAME_PATH];
static int  dopo_games_count;

/**
 * @brief Whether a file can be offered as a game: an m3u playlist, or a
 * .wad/.iwad/.pwad whose header says IWAD or PWAD.
 */
static dbool dopo_game_candidate(const char *path)
{
   const char *ext = path_get_extension(path);
   char magic[4];
   RFILE *fp;
   dbool ok;

   if (!strcasecmp(ext, "m3u"))
      return TRUE;
   if (strcasecmp(ext, "wad") && strcasecmp(ext, "iwad") && strcasecmp(ext, "pwad"))
      return FALSE;

   fp = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!fp)
      return FALSE;
   ok = filestream_read(fp, magic, sizeof(magic)) == sizeof(magic) &&
        (!memcmp(magic, "IWAD", 4) || !memcmp(magic, "PWAD", 4));
   filestream_close(fp);
   return ok;
}

static int dopo_game_compare(const void *a, const void *b)
{
   return strcasecmp(path_basename((const char *)a), path_basename((const char *)b));
}

/**
 * @brief Lists the games in the running content's directory, sorted by
 * name. Returns how many there are.
 */
int dopo_games_scan(void)
{
   char dir[DOPO_GAME_PATH];
   struct RDIR *rdir;

   dopo_games_count = 0;
   if (!dopo_game_current[0])
      return 0;

   strlcpy(dir, dopo_game_current, sizeof(dir));
   path_basedir(dir);
   rdir = retro_opendir(dir);
   if (!rdir)
      return 0;

   while (dopo_games_count < DOPO_GAMES_MAX && retro_readdir(rdir))
   {
      char *path = dopo_games[dopo_games_count];

      if (retro_dirent_is_dir(rdir, NULL))
         continue;
      fill_pathname_join(path, dir, retro_dirent_get_name(rdir), DOPO_GAME_PATH);
      if (dopo_game_candidate(path))
         dopo_games_count++;
   }
   retro_closedir(rdir);

   qsort(dopo_games, dopo_games_count, sizeof(*dopo_games), dopo_game_compare);
   return dopo_games_count;
}

/**
 * @brief File name of a listed game.
 */
const char *dopo_games_name(int i)
{
   return path_basename(dopo_games[i]);
}

/**
 * @brief Whether a listed game is the one running.
 */
dbool dopo_games_is_current(int i)
{
   return !strcmp(dopo_games[i], dopo_game_current);
}

/**
 * @brief Asks for a switch to a listed game at the next retro_run.
 */
void dopo_games_load(int i)
{
   strlcpy(dopo_game_pending, dopo_games[i], sizeof(dopo_game_pending));
}

static void dopo_upstream_run(void);
static bool dopo_upstream_load_game(const struct retro_game_info *info);

/**
 * @brief Unloads the running content and loads the pending one; if it
 * fails to load, asks the frontend to shut down.
 */
static void dopo_game_switch(void)
{
   struct retro_game_info info = {0};

   info.path = dopo_game_pending;
   retro_unload_game();
   if (!retro_load_game(&info))
   {
      lprintf(LO_ERROR, "Change Game: could not load '%s'\n", dopo_game_pending);
      dopo_game_failed = TRUE;
      environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
   }
}

/**
 * @brief Runs a frame, switching games first when one was asked for; after
 * a failed switch nothing is loaded, so frames are skipped until the
 * frontend shuts down.
 */
void retro_run(void)
{
   if (dopo_game_pending[0])
   {
      dopo_game_switch();
      dopo_game_pending[0] = 0;
   }
   if (!dopo_game_failed)
      dopo_upstream_run();
}

/* @endpatch */

/**
 * @brief Upstream retro_run, renamed.
 *
 * @patch libretro/libretro.c 1776-1776
 */
static void dopo_upstream_run(void)
/* @endpatch */

/**
 * @brief Loads content, remembering its path for Change Game.
 *
 * @patch libretro/libretro.c 2350
 */
bool retro_load_game(const struct retro_game_info *info)
{
   dopo_game_failed = FALSE;
   if (info && info->path)
      strlcpy(dopo_game_current, info->path, sizeof(dopo_game_current));
   return dopo_upstream_load_game(info);
}

/* @endpatch */

/**
 * @brief Upstream retro_load_game, renamed.
 *
 * @patch libretro/libretro.c 2350-2350
 */
static bool dopo_upstream_load_game(const struct retro_game_info *info)
/* @endpatch */
