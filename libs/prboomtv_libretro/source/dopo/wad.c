/**
 * @brief Embeds the Dopo WAD, holding the Odamex big font, next to
 * prboom.wad.
 *
 * The WAD is generated at configure time by scripts/prboomtv_font.cpp.
 */

/**
 * @brief Generated array with the Dopo WAD.
 *
 * @patch src/d_main.c 67
 */
#include "dopo_wad_data.h"
/* @endpatch */

/**
 * @brief Adds every WAD baked into the core, prboom.wad first.
 *
 * Both are pre-sources like the original single prboom.wad entry; the
 * savegame check only signs map lumps, so old saves still load.
 *
 * @patch src/d_main.c 1453-1465
 */
  {
    static const struct
    {
      const char          *name;
      const unsigned char *data;
      unsigned int         length;
    } embedded[] =
    {
      { PACKAGE ".wad", prboom_wad_data, prboom_wad_data_len },
      { "dopo.wad",     dopo_wad_data,   dopo_wad_data_len },
    };
    size_t i;

    for (i = 0; i < sizeof(embedded) / sizeof(*embedded); i++)
    {
      /* name is a label only (never opened); malloc'd so W_ReleaseAllWads
       * frees it uniformly with every other entry. */
      char *embed_name = malloc(strlen(embedded[i].name) + 1);
      wadfiles = realloc(wadfiles, sizeof(*wadfiles) * (numwadfiles + 1));
      memset(&wadfiles[numwadfiles], 0, sizeof(wadfiles[numwadfiles]));
      strcpy(embed_name, embedded[i].name);
      wadfiles[numwadfiles].name = embed_name;
      wadfiles[numwadfiles].src  = source_pre;
      wadfiles[numwadfiles].embedded_data   = embedded[i].data;
      wadfiles[numwadfiles].embedded_length = (int)embedded[i].length;
      numwadfiles++;
    }
  }
/* @endpatch */
