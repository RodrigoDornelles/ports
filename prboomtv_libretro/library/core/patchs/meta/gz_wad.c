/**
 * @brief A gzip'd wad loads as the wad it holds: doom1.wad.gz runs as the
 * IWAD, opened as content or named in an m3u playlist, and a gzip'd PWAD
 * or pk3 adds on like the plain file. The shareware licences allow giving
 * the games away only compressed, and a wad gzip'd is a third of its size.
 *
 * W_AddFile inflates the file in memory and parses what comes out; the
 * probes that read a wad by path (the content's classification, its map
 * kind, CheckIWAD's game mode) ask W_GzProbe for the header and directory
 * of the wad inside instead. One gzip member, as gzip -9 writes it.
 */

/**
 * @brief The module: a gzip file inflated in memory, and the header and
 * directory of the wad it holds, kept for the last file probed, as one
 * load probes the same file several times.
 *
 * @patch src/w_pk3.c 42
 */
#include <streams/file_stream.h>

dbool W_IsGzip(const unsigned char *data, int64_t length)
{
  return length >= 18 && data[0] == 0x1f && data[1] == 0x8b && data[2] == 8;
}

unsigned char *W_Gunzip(const unsigned char *gz, int64_t gz_length,
                        int64_t *out_length, const char *name)
{
  const unsigned char *in = gz;
  size_t         in_len   = (size_t)gz_length;
  size_t         wrote    = 0;
  unsigned char *out;
  uint32_t       usize;
  void          *s;
  int            r;

  if (!W_IsGzip(gz, gz_length) || gz_length > (int64_t)0x7fffffff)
    return NULL;
  /* ISIZE, the length mod 2^32: exact for anything a wad can be */
  usize = (uint32_t)gz[gz_length - 4]       | (uint32_t)gz[gz_length - 3] << 8
        | (uint32_t)gz[gz_length - 2] << 16 | (uint32_t)gz[gz_length - 1] << 24;
  if (usize > 0x7fffffff)
    return NULL;
  out = malloc(usize ? usize : 1);
  s   = rinflate_new(31);                              /* gzip wrapper */
  if (!out || !s)
  {
    free(out);
    rinflate_free(s);
    lprintf(LO_WARN, "W_Gunzip: %s: out of memory\n", name);
    return NULL;
  }
  rinflate_set_out(s, out, usize);
  for (;;)
  {
    size_t rd = 0, wr = 0;
    rinflate_set_in(s, in, in_len);
    r       = rinflate_process(s, &rd, &wr);
    in     += rd;
    in_len -= rd;
    wrote  += wr;
    if (r == RDEFLATE_PROCESS_END)
      break;
    if (r != RDEFLATE_PROCESS_NEXT || (rd == 0 && wr == 0))
      break;
  }
  rinflate_free(s);
  if (r != RDEFLATE_PROCESS_END || wrote != usize)
  {
    free(out);
    lprintf(LO_WARN, "W_Gunzip: %s: corrupt or truncated gzip\n", name);
    return NULL;
  }
  *out_length = usize;
  return out;
}

static struct
{
  char       *path;
  int64_t     size;
  wadinfo_t   header;
  filelump_t *dir;      /* NULL when what the gzip holds is not a wad */
} gz_probe;

/* A file at path that is gzip: 1, with the first bytes of what it holds
 * as the header (zeroes if it does not inflate), its length and, when it
 * is a wad, the wad's directory as on disk (malloc'd, the caller frees
 * it). 0 for any other file. */
int W_GzProbe(const char *path, wadinfo_t *header, int64_t *size, filelump_t **dir)
{
  unsigned char magic[3];
  RFILE        *fp;
  int           numlumps;

  if (dir)
    *dir = NULL;
  if (!(fp = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
                             RETRO_VFS_FILE_ACCESS_HINT_NONE)))
    return 0;
  if (filestream_read(fp, magic, sizeof(magic)) != sizeof(magic) ||
      !W_IsGzip(magic, 18))
  {
    filestream_close(fp);
    return 0;
  }
  filestream_close(fp);

  if (!gz_probe.path || strcmp(gz_probe.path, path))
  {
    void          *gz     = NULL;
    int64_t        gz_len = 0, len = 0;
    unsigned char *image  = NULL;

    free(gz_probe.path);
    free(gz_probe.dir);
    memset(&gz_probe, 0, sizeof(gz_probe));
    gz_probe.path = strdup(path);
    if (filestream_read_file(path, &gz, &gz_len) > 0 && gz)
      image = W_Gunzip(gz, gz_len, &len, path);
    free(gz);
    if (image && len >= (int64_t)sizeof(wadinfo_t))
    {
      memcpy(&gz_probe.header, image, sizeof(wadinfo_t));
      gz_probe.size = len;
      numlumps      = LONG(gz_probe.header.numlumps);
      if ((!strncmp(gz_probe.header.identification, "IWAD", 4) ||
           !strncmp(gz_probe.header.identification, "PWAD", 4)) &&
          numlumps >= 0 && LONG(gz_probe.header.infotableofs) >= (int)sizeof(wadinfo_t) &&
          (int64_t)LONG(gz_probe.header.infotableofs)
            + (int64_t)numlumps * (int64_t)sizeof(filelump_t) <= len &&
          (gz_probe.dir = malloc(numlumps * sizeof(filelump_t) + 1)))
        memcpy(gz_probe.dir, image + LONG(gz_probe.header.infotableofs),
               numlumps * sizeof(filelump_t));
    }
    free(image);
  }

  if (header)
    *header = gz_probe.header;
  if (size)
    *size = gz_probe.size;
  if (dir && gz_probe.dir)
  {
    size_t n = LONG(gz_probe.header.numlumps) * sizeof(filelump_t);
    if ((*dir = malloc(n + 1)))
      memcpy(*dir, gz_probe.dir, n);
  }
  return 1;
}

/* @endpatch */

/** @patch src/w_wad.c 54 */
dbool W_IsGzip(const unsigned char *data, int64_t length);
unsigned char *W_Gunzip(const unsigned char *gz, int64_t gz_length,
                        int64_t *out_length, const char *name);
/* @endpatch */

/**
 * @brief W_AddFile: a gzip file's data becomes what it holds, before the
 * pk3 check, so a gzip'd pk3 translates like a plain one.
 *
 * @patch src/w_wad.c 259
 */
   if (W_IsGzip(wadfile->data, wadfile->length))
   {
      int64_t newlen = 0;
      unsigned char *image = W_Gunzip(wadfile->data, wadfile->length,
                                      &newlen, wadfile->name);
      if (!image)
      {
         I_Error("W_AddFile: couldn't inflate %s", wadfile->name);
         return;
      }
#ifdef HAVE_MMAP
      if (wadfile->mmapped)
      {
         munmap(wadfile->data, (size_t)wadfile->length);
         wadfile->mmapped = 0;
      }
      else
#endif
         free(wadfile->data);
      wadfile->data   = image;
      wadfile->length = newlen;
      is_archive      = TRUE;
   }

/* @endpatch */

/** @patch src/d_main.c 67 */
int W_GzProbe(const char *path, wadinfo_t *header, int64_t *size, filelump_t **dir);
/* @endpatch */

/**
 * @brief CheckIWAD: a gzip'd IWAD's header and directory from the probe.
 *
 * @patch src/d_main.c 878-880
 */
    filelump_t *gz_dir = NULL;
    int gz = W_GzProbe(iwadname, &header, NULL, &gz_dir);

    if (gz ? gz_dir != NULL
           : rfread(&header, sizeof(header), 1, fp) == 1 &&
             (!strncmp(header.identification, "IWAD", 4) ||
              !strncmp(header.identification, "PWAD", 4)))
/* @endpatch */

/** @patch src/d_main.c 889-892 */
      if (gz_dir)
        fileinfo = gz_dir;
      else
      {
        fileinfo = malloc(length*sizeof(filelump_t));
        if (filestream_seek (fp, header.infotableofs, SEEK_SET) ||
          rfread (fileinfo, sizeof(filelump_t), length, fp) != length)
        I_Error("CheckIWAD: failed to read directory %s",iwadname);
      }
/* @endpatch */

/**
 * @brief IdentifyVersion: tnt.wad.gz is TNT, as tnt.wad, the mission told
 * by the name without .gz.
 *
 * @patch src/d_main.c 1104-1104
 */
        i = strlen(iwad);
        if (i > 3 && !strcasecmp(iwad + i - 3, ".gz"))
          i -= 3;
/* @endpatch */

/** @patch libretro/libretro.c 64 */
int W_GzProbe(const char *path, wadinfo_t *header, int64_t *size, filelump_t **dir);
/* @endpatch */

/** @patch libretro/libretro.c 1029-1029 */
   info->valid_extensions = "wad|iwad|pwad|lmp|m3u|pk3|ipk3|zip|gz";
/* @endpatch */

/**
 * @brief remove_extension: doom1.wad.gz goes by doom1 and its extension is
 * wad, so its .deh and .prboom.cfg are found as for doom1.wad.
 *
 * @patch libretro/libretro.c 1898-1899
 */
  *base = '\0';
  if (!strcasecmp(base + 1, "gz"))
  {
    char *inner = strrchr(buf, '.');
    if (inner)
    {
      *inner = '\0';
      return inner + 1;
    }
  }
  return base + 1;
/* @endpatch */

/**
 * @brief get_wadinfo: the header and length of what a gzip file holds.
 *
 * @patch libretro/libretro.c 1918
 */
   if (W_GzProbe(path, &header, size, NULL))
   {
      if (fp)
         filestream_close(fp);
      return header;
   }

/* @endpatch */

/**
 * @brief wad_contains_playpal: the directory of a gzip'd wad.
 *
 * @patch libretro/libretro.c 1969-1978
 */
   filelump_t *gz_dir = NULL;
   wadinfo_t   gz_header;

   fp = NULL;
   if (W_GzProbe(path, &gz_header, NULL, &gz_dir))
   {
      if (!gz_dir)
         return false;
      numlumps = LONG(gz_header.numlumps);
   }
   else
   {
      fp = filestream_open(path,
            RETRO_VFS_FILE_ACCESS_READ,
            RETRO_VFS_FILE_ACCESS_HINT_NONE);
      if (!fp)
         return false;
      if (filestream_seek(fp, infotableofs, SEEK_SET) != 0)
      {
         filestream_close(fp);
         return false;
      }
   }
/* @endpatch */

/** @patch libretro/libretro.c 1981-1982 */
      if (gz_dir)
         lump = gz_dir[i];
      else if (rfread(&lump, sizeof(lump), 1, fp) != 1)
         break;
/* @endpatch */

/** @patch libretro/libretro.c 1992-1992 */
   if (gz_dir)
      free(gz_dir);
   else
      filestream_close(fp);
/* @endpatch */

/**
 * @brief scan_pwad_map_kind: the directory of a gzip'd wad.
 *
 * @patch libretro/libretro.c 2037-2046
 */
   filelump_t *gz_dir = NULL;
   wadinfo_t   gz_header;

   fp = NULL;
   if (W_GzProbe(path, &gz_header, NULL, &gz_dir))
   {
      if (!gz_dir)
         return PWAD_MAP_NONE;
      numlumps = LONG(gz_header.numlumps);
   }
   else
   {
      fp = filestream_open(path,
            RETRO_VFS_FILE_ACCESS_READ,
            RETRO_VFS_FILE_ACCESS_HINT_NONE);
      if (!fp)
         return PWAD_MAP_NONE;
      if (filestream_seek(fp, infotableofs, SEEK_SET) != 0)
      {
         filestream_close(fp);
         return PWAD_MAP_NONE;
      }
   }
/* @endpatch */

/** @patch libretro/libretro.c 2050-2051 */
      if (gz_dir)
         lump = gz_dir[i];
      else if (rfread(&lump, sizeof(lump), 1, fp) != 1)
         break;
/* @endpatch */

/** @patch libretro/libretro.c 2083-2083 */
   if (gz_dir)
      free(gz_dir);
   else
      filestream_close(fp);
/* @endpatch */
