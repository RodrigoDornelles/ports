/**
 * @brief LANGUAGE keeps only English: the engine has no language setting,
 * and every section ([enu default], [fr], [ita]) went into one table, the
 * last lump winning, so zdoom.pk3's language.ita put Chex Quest 3 in
 * Italian. Sections for enu, en, eng or default count, and what comes
 * before any header.
 *
 * @patch src/u_zmapinfo.c 62-62
 */
  u_scanner_t s;
  dbool keep = TRUE;
/* @endpatch */

/** @patch src/u_zmapinfo.c 69-75 */
    /* "[enu default]" section headers: English or the default only */
    if (U_CheckToken(&s, '['))
    {
      keep = FALSE;
      while (U_HasTokensLeft(&s) && !U_CheckToken(&s, ']'))
      {
        U_GetNextToken(&s, TRUE);
        if (s.string && (!strcasecmp(s.string, "enu") || !strcasecmp(s.string, "en") ||
                         !strcasecmp(s.string, "eng") || !strcasecmp(s.string, "default")))
          keep = TRUE;
      }
      continue;
    }
/* @endpatch */

/** @patch src/u_zmapinfo.c 92-100 */
      if (val && keep)
      {
        zlang = realloc(zlang, (zlang_count + 1) * sizeof(*zlang));
        zlang[zlang_count].key = key;
        zlang[zlang_count].value = val;
        zlang_count++;
      }
      else
      {
        free(key);
        free(val);
      }
/* @endpatch */
