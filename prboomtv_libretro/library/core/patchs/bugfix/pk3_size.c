/**
 * @brief A big pk3 loads: the directory of the wad synthesized from an
 * archive is checked against that wad's size, not the zip's on disk. The
 * synthesized wad holds the members uncompressed, so past a point it is
 * larger than the archive, and its directory (written last) lies beyond
 * the zip's size: the check refused the whole pk3 (Combined Arms: 30 MB
 * of lumps from a 28 MB archive) while small ones, like lights.pk3,
 * passed.
 *
 * @patch src/w_wad.c 353-353
 */
      int64_t wadlen = is_archive ? wadfile->length
                                  : filestream_get_size(wadfile->handle);
/* @endpatch */
