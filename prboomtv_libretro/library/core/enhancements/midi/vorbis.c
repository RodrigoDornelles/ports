/**
 * @brief SF3 soundfonts decode with the core's own Vorbis: fluidlite keeps
 * each sample of an .sf3 as an Ogg Vorbis stream and inflated it with
 * libvorbis and libogg, which would make a second Vorbis decoder in the
 * core next to rvorbis (stb_vorbis-derived, already built for oggplayer.c
 * on every platform, the web one included). The samples come out as signed
 * 16-bit mono, the same as libvorbis's ov_read gave them, so the rest of
 * the loader is untouched.
 */

/**
 * @brief The libvorbis headers and the ov_callbacks that read a sample's
 * stream from memory: rvorbis opens it in place.
 *
 * @patch deps/fluidlite/src/fluid_defsfont.c 29-97
 */
#if SF3_SUPPORT
#include <formats/rvorbis.h>
#endif
/* @endpatch */

/**
 * @brief fluid_defsfont_get_sample: an Ogg sample becomes its PCM, read in
 * 4096-sample chunks into a buffer that grows as it fills.
 *
 * @patch deps/fluidlite/src/fluid_defsfont.c 530-553
 */
            short *sampledata_ogg = NULL;
            int sampledata_size = 0;
            int error = 0;
            rvorbis *vf = rvorbis_open_memory(
                (const unsigned char *)sample->data + sample->start,
                (int)(sample->end + 1 - sample->start), &error, NULL);

            if (vf)
            {
                int16_t buffer[4096];
                int numberRead;
                while ((numberRead = rvorbis_get_samples_s16_interleaved(vf, 1, buffer, 4096)) > 0)
                {
                    short *grown = FLUID_REALLOC(sampledata_ogg,
                        sampledata_size + numberRead * sizeof(short));
                    if (!grown)
                        break;
                    sampledata_ogg = grown;
                    memcpy((char *)sampledata_ogg + sampledata_size, buffer,
                           numberRead * sizeof(short));
                    sampledata_size += numberRead * sizeof(short);
                }
                rvorbis_close(vf);
            }
            else
                FLUID_LOG(FLUID_WARN, "Couldn't decode Ogg sample %s (rvorbis error %d)",
                          sample->name, error);
/* @endpatch */
