/**
 * @brief GZDoom's brightmaps.pk3 loads as a wad without breaking the
 * sprites: its brightmaps/doom/BOSSE4.png and the like are PNGs outside
 * the renderable folders, which went to the global namespace under the
 * name of the sprite frame they light and replaced it. Quarantined like
 * filter/ and shaders/, the archive is present but invisible to lump
 * lookups; the brightmaps themselves are read straight from it
 * (enhancements/gpu/hd/brightmaps.c). An m3u playlist can then list it,
 * which the web player needs to fetch it next to the game.
 *
 * @patch src/w_pk3.c 554-554
 */
      !strncasecmp(path, "filter/", 7) ||
      !strncasecmp(path, "brightmaps/", 11))
/* @endpatch */
