/**
 * @brief Faster wall runs whose columns are lit differently.
 *
 * R_DrawWallColumnRun draws up to 64 x-adjacent wall columns row by row.
 * When every column shares one colormap it reads a single composed table
 * (colormap and palette in one) per pixel, with the texture step
 * vectorized; when the light changes across the run (a wall seen at an
 * angle) it fell back to two dependent lookups per pixel,
 * V_Palette16[colormap[texel] * 64 + 63], and no vector step. At 1280x720
 * that mixed path was about two thirds of the wall cost.
 *
 * Here each column gets its own composed table from a cache per wall
 * slice (one render thread draws one slice at a time), so the mixed path
 * reads one table per pixel and shares the vector step. The tables hold
 * the same expression, so every pixel is unchanged.
 */

/**
 * @brief The slice a wall scratch belongs to, which keys its table cache.
 *
 * @patch src/r_drawcmd.c 178
 */
int R_WallScratchSlot(const wallscratch_t *ws)
{
  return ws ? (int)(ws - wall_scratch) : -1;
}
/* @endpatch */

/**
 * @brief Composed tables per wall slice, keyed on the colormap (consecutive
 * light levels are 256 bytes apart, so they never share an entry) and on
 * the palette.
 *
 * @patch src/r_draw.c 186
 */
#define WALL_LUT_SLICES  8
#define WALL_LUT_ENTRIES 32

int R_WallScratchSlot(const wallscratch_t *ws);

typedef struct
{
  const lighttable_t *cm[WALL_LUT_ENTRIES];
  const uint16_t     *pal[WALL_LUT_ENTRIES];
  uint16_t            lut[WALL_LUT_ENTRIES][256];
} wall_luts_t;

static wall_luts_t wall_luts[WALL_LUT_SLICES];

static wall_luts_t *R_WallLuts(const wallscratch_t *ws)
{
  const int slot = R_WallScratchSlot(ws);
  return slot >= 0 && slot < WALL_LUT_SLICES ? &wall_luts[slot] : NULL;
}

static const uint16_t *R_WallLaneLUT(wall_luts_t *luts, const lighttable_t *cm)
{
  const unsigned entry = (unsigned)((uintptr_t)cm >> 8) & (WALL_LUT_ENTRIES - 1);

  if (luts->cm[entry] != cm || luts->pal[entry] != V_Palette16)
  {
    int i;
    for (i = 0; i < 256; i++)
      luts->lut[entry][i] = V_Palette16[ cm[i]*64 + (64-1) ];
    luts->cm[entry]  = cm;
    luts->pal[entry] = V_Palette16;
  }
  return luts->lut[entry];
}

/* @endpatch */

/**
 * @brief A new session may load its colormap and palette at the old
 * addresses, so the keys are dropped with the other composed tables.
 *
 * @patch src/r_draw.c 191
 */
   memset(wall_luts, 0, sizeof(wall_luts));
/* @endpatch */

/**
 * @brief Each column's own table, when the run mixes colormaps.
 *
 * @patch src/r_draw.c 6229
 */
  const uint16_t *lanelut_mixed[WALL_RUN_MAX];
  wall_luts_t    *luts = (!lut && !lane_mode) ? R_WallLuts(ws) : NULL;

  if (luts)
    for (j = 0; j < n; j++)
      lanelut_mixed[j] = R_WallLaneLUT(luts, cmap[j]);
/* @endpatch */

/**
 * @brief A run with no row covering every column.
 *
 * @patch src/r_draw.c 6275-6277
 */
    else if (luts)
      for (y = ymin; y <= ymax; y++)
        WALL_RUN_RAGGED_ROW(lanelut_mixed[j][texel])
    else
      for (y = ymin; y <= ymax; y++)
        WALL_RUN_RAGGED_ROW(V_Palette16[ cmap[j][texel] * 64 + 63 ])
/* @endpatch */

/**
 * @brief Mixed colormaps: one table per column, and the dense band steps
 * the texture four columns at a time like the single-table path.
 *
 * @patch src/r_draw.c 6339
 */
  else if (luts)
  {
    for (y = ymin; y < dtop; y++)
      WALL_RUN_RAGGED_ROW(lanelut_mixed[j][texel])
    for (y = dtop; y <= dbot; y++)
    {
      uint16_t *row = drawvars.short_topleft
                    + y * SURFACE_SHORT_PITCH + x0;
      int j4 = n & ~3;
      for (j = 0; j < j4; j += 4)
      {
        int32_t  idx4[4];
        uint16_t out4[4];
#if defined(WALL_RUN_SSE2)
        __m128i f = _mm_loadu_si128((const __m128i *)&frac[j]);
        __m128i m = _mm_loadu_si128((const __m128i *)&mask[j]);
        __m128i s = _mm_loadu_si128((const __m128i *)&step[j]);
        _mm_storeu_si128((__m128i *)idx4,
                         _mm_srai_epi32(_mm_and_si128(f, m), 16));
        _mm_storeu_si128((__m128i *)&frac[j], _mm_add_epi32(f, s));
#elif defined(WALL_RUN_NEON)
        uint32x4_t f = vld1q_u32((const uint32_t *)&frac[j]);
        uint32x4_t m = vld1q_u32(&mask[j]);
        uint32x4_t s = vld1q_u32((const uint32_t *)&step[j]);
        vst1q_s32(idx4,
                  vshrq_n_s32(vreinterpretq_s32_u32(vandq_u32(f, m)), 16));
        vst1q_u32((uint32_t *)&frac[j], vaddq_u32(f, s));
#else
        int k;
        for (k = 0; k < 4; k++)
        {
          idx4[k] = (int)(frac[j + k] & mask[j + k]) >> 16;
          frac[j + k] += step[j + k];
        }
#endif
        out4[0] = lanelut_mixed[j + 0][src[j + 0][idx4[0]]];
        out4[1] = lanelut_mixed[j + 1][src[j + 1][idx4[1]]];
        out4[2] = lanelut_mixed[j + 2][src[j + 2][idx4[2]]];
        out4[3] = lanelut_mixed[j + 3][src[j + 3][idx4[3]]];
        memcpy(&row[j], out4, 4 * sizeof(uint16_t));
      }
      for (; j < n; j++)
      {
        row[j] = lanelut_mixed[j][ src[j][(int)(frac[j] & mask[j]) >> 16] ];
        frac[j] += step[j];
      }
    }
    for (y = dbot + 1; y <= ymax; y++)
      WALL_RUN_RAGGED_ROW(lanelut_mixed[j][texel])
  }
/* @endpatch */
