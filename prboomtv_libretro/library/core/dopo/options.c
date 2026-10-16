/**
 * @file options.c
 * @brief Core options from the environment, ahead of the frontend's.
 */
#include <ctype.h>
#include <stdlib.h>

#include "prboomtv/options.h"

static retro_environment_t dopo_options_frontend;

/* prboom-resolution -> PRBOOM_RESOLUTION */
static const char *dopo_options_getenv(const char *key)
{
   char name[128];
   size_t i;

   for (i = 0; key[i] && i < sizeof(name) - 1; i++)
      name[i] = isalnum((unsigned char)key[i]) ? (char)toupper((unsigned char)key[i]) : '_';
   name[i] = '\0';
   return key[i] ? NULL : getenv(name);
}

static bool RETRO_CALLCONV dopo_options_answer(unsigned cmd, void *data)
{
   if (cmd == RETRO_ENVIRONMENT_GET_VARIABLE && data)
   {
      struct retro_variable *var = (struct retro_variable *)data;
      const char *value = var->key ? dopo_options_getenv(var->key) : NULL;

      if (value && *value)
      {
         var->value = value;
         return true;
      }
   }
   return dopo_options_frontend(cmd, data);
}

retro_environment_t dopo_options_environment(retro_environment_t cb)
{
   dopo_options_frontend = cb;
   return dopo_options_answer;
}
