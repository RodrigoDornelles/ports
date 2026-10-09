/**
 * @brief The core linked into RetroArch's web player, which brings its
 * own libretro-common, older than the core's: two functions the core
 * calls changed since. The core calls them by another name there
 * (cmake/prboomtv.cmake) and these adapt RetroArch's.
 */
#ifdef __EMSCRIPTEN__
#include <stddef.h>
#include <stdint.h>
#include <boolean.h>
#include <formats/image.h>

#undef path_get_size
#undef rpng_process_image

/* RetroArch's, as libretro-common has them in its v1.22.2 */
struct rpng;
int32_t path_get_size(const char *path);
int rpng_process_image(struct rpng *rpng, void **data, size_t size,
                       unsigned *width, unsigned *height);

int64_t prboomtv_path_get_size(const char *path)
{
  return path_get_size(path);
}

/**
 * @brief The image as ARGB (RetroArch's), or with supports_rgba as ABGR,
 * the order the core asks for.
 */
int prboomtv_rpng_process_image(struct rpng *rpng, void **data, size_t size,
                                unsigned *width, unsigned *height, bool supports_rgba)
{
  const int ret = rpng_process_image(rpng, data, size, width, height);

  if (ret == IMAGE_PROCESS_END && supports_rgba && *data)
  {
    uint32_t *pixel = (uint32_t *)*data;
    const size_t count = (size_t)*width * *height;
    size_t i;

    for (i = 0; i < count; i++)
      pixel[i] = (pixel[i] & 0xFF00FF00u) |
                 ((pixel[i] >> 16) & 0xFFu) | ((pixel[i] & 0xFFu) << 16);
  }
  return ret;
}
#endif
