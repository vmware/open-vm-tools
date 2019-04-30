/*********************************************************
 * Copyright (C) 2011-2019 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the Lesser GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/

/**
 * @file i18n.c
 *
 * Implementation of i18n-related functions.
 *
 * Stolen primarily from bora-vmsoft/apps/vmtoolslib/i18n.c
 *
 * Includes chunks of bora/lib/misc/escape.c and bora/lib/dict/dictll.c
 * converted to use glib.
 */

#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#ifdef _WIN32
#include <windows.h>
#endif

#include "VGAuthBasicDefs.h"
#include "VGAuthUtil.h"
#include "i18n.h"

/* These come from msgid.h. See MsgHasMsgID for explanation. */
#define MSG_MAX_ID      128
/* The X hides MSG_MAGIC so it won't appear in the object file. */
#define MSG_MAGICAL(s)  (strncmp(s, MSG_MAGIC"X", MSG_MAGIC_LEN) == 0)

typedef struct MsgCatalog {
   GHashTable *utf8;
} MsgCatalog;


typedef struct MsgState {
   GHashTable *domains;  /* List of text domains. */
   GMutex lock;          /* Mutex to protect shared state. */
} MsgState;


static MsgState *msgState = NULL;



/*
 ******************************************************************************
 * Walk --                                                               */ /**
 *
 * While 'bufIn' points to a byte in 'sentinel', increment it.
 *
 * @param[in] bufIn     The input buffer.
 * @param[in] sentinel  The set of chars to Walk over.
 *
 * @return The incremented buffer.
 *
 ******************************************************************************
 */

static void const *
Walk(void const * const bufIn,
     int const * const sentinel)
{
   char const *buf;

   buf = (char const *)bufIn;
   ASSERT(buf);

   /* Unsigned does matter --hpreg */
   while (sentinel[(unsigned char)*buf]) {
      buf++;
   }

   return buf;
}


/*
XXX document the escaping/unescaping process: rationale for which chars we escape, and how we escape --hpreg
*/


/*
 * The dictionary line format:
 *
 *    <ws> <name> <ws> = <ws> <value> <ws> <comment>
 * or
 *    <ws> <name> <ws> = <ws> " <quoted-value> " <ws> <comment>
 *
 * where
 *    <name> does not contain any whitespace or = or #
 *    <value> does not contain any double-quote or #
 *    <quoted-value> does not contain any double-quote
 *    <comment> begins with # and ends at the end of the line
 *    <ws> is a sequence spaces and/or tabs
 *    <comment> and <ws> are optional
 */


/*
 ******************************************************************************
 * DictLL_UnmarshalLine --                                               */ /**
 *
 * Reads a dict line from the bufSize-byte buffer buf, which holds one or more
 * new-line delimited lines.  The buffer is not necessarily
 * null-terminated.
 *
 * @param[in]  buf     The input buffer.
 * @param[in]  bufsize The size of the input buffer.
 * @param[out] line    The complete line (must be g_free()d).
 * @param[out] name    The name (must be g_free()d).
 * @param[out] value   The value (must be g_free()d).
 *
 * @return The start of the next line, or NULL if at the end of buffer.
 ******************************************************************************
 */

static const char *
DictLL_UnmarshalLine(const char *buf,
                     size_t bufSize,
                     char **line,
                     char **name,
                     char **value)
{
   /* Space and tab --hpreg */
   static int const ws_in[] = {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   };
   /* Everything but NUL, space, tab and pound --hpreg */
   static int const wsp_out[] = {
      0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   };
   /* Everything but NUL, space, tab, pound and equal --hpreg */
   static int const wspe_out[] = {
      0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   };
   /* Everything but NUL and double quote --hpreg */
   static int const q_out[] = {
      0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   };
   char const *nBegin;
   char const *nEnd;
   char const *vBegin;
   char const *vEnd;
   char const *tmp;
   char *myLine;
   char *myName;
   char *myValue;
   const char *lineEnd;
   const char *nextLine;

   ASSERT(buf);
   ASSERT(line);
   ASSERT(name);
   ASSERT(value);

   /* Check for end of buffer. */
   if (bufSize == 0) {
      *line = NULL;
      *name = NULL;
      *value = NULL;
      return NULL;
   }

   /* Find end of this line, beginning of next. */
   lineEnd = memchr(buf, '\n', bufSize);
   if (lineEnd != NULL) {
      nextLine = lineEnd + 1;
   } else {
      nextLine = lineEnd = buf + bufSize;
   }

   /* Make local copy of line. */
   myLine = g_strndup(buf, lineEnd - buf);

   /* Check if the line is well-formed --hpreg */
   nBegin = Walk(myLine, ws_in);
   nEnd = Walk(nBegin, wspe_out);
   tmp = Walk(nEnd, ws_in);
   if (nBegin == nEnd || *tmp != '=') {
      goto weird;
   }
   tmp++;
   tmp = Walk(tmp, ws_in);
   if (*tmp == '"') {
      tmp++;
      vBegin = tmp;
      vEnd = Walk(vBegin, q_out);
      tmp = vEnd;
      if (*tmp != '"') {
         goto weird;
      }
      tmp++;
   } else {
      vBegin = tmp;
      vEnd = Walk(vBegin, wsp_out);
      tmp = vEnd;
   }
   tmp = Walk(tmp, ws_in);
   if (*tmp != '\0' && *tmp != '#') {
      goto weird;
   }

   /* The line is well-formed. Extract the name and value --hpreg */

   myName = g_strndup(nBegin, nEnd - nBegin);
   myValue = g_strndup(vBegin, vEnd - vBegin);
   ASSERT(myValue);

   *line = myLine;
   *name = myName;
   *value = myValue;

   return nextLine;

weird:
   /* The line is not well-formed. Let the upper layers handle it --hpreg */

   *line = myLine;
   *name = NULL;
   *value = NULL;

   return nextLine;
}


/*
 ******************************************************************************
 * MsgHasMsgID --                                                        */ /**
 *
 * Check that a string has a message ID. The full "MSG_MAGIC(...)" prefix is
 * required, not just MSG_MAGIC.
 *
 * Copied from msgid.h.
 *
 * @param[in] s    String to check.
 *
 * @return TRUE if the string has a message id.
 *
 ******************************************************************************
 */

static gboolean
MsgHasMsgID(const gchar *s)
{
   return MSG_MAGICAL(s) &&
          *(s += MSG_MAGIC_LEN) == '(' &&
          strchr(s + 1, ')') != NULL;
}


/*
 ******************************************************************************
 * MsgCatalogFree --                                                     */ /**
 *
 * Frees memory allocated for a MsgCatalog structure.
 *
 * @param[in] catalog    The catalog to free.
 *
 ******************************************************************************
 */

static void
MsgCatalogFree(gpointer c)
{
   MsgCatalog *catalog = (MsgCatalog *) c;
   ASSERT(catalog);
   if (catalog->utf8 != NULL) {
      g_hash_table_unref(catalog->utf8);
   }
   g_free(catalog);
}


/*
 ******************************************************************************
 * MsgInitState --                                                       */ /**
 *
 * Initializes the global message state.
 *
 * @param[in] unused   Unused.
 *
 * @return NULL.
 *
 ******************************************************************************
 */

static gpointer
MsgInitState(gpointer unused)
{
   ASSERT(msgState == NULL);
   msgState = g_new0(MsgState, 1);
   g_mutex_init(&msgState->lock);
   return NULL;
}


/*
 ******************************************************************************
 * MsgGetState --                                                        */ /**
 *
 * Get the internal msg state (lazily initialized if needed).
 *
 * @return The message state object.
 *
 ******************************************************************************
 */

static MsgState *
MsgGetState(void)
{
   static GOnce msgStateInit = G_ONCE_INIT;
   g_once(&msgStateInit, MsgInitState, NULL);
   return msgState;
}


/*
 ******************************************************************************
 * MsgGetCatalog --                                                      */ /**
 *
 * Retrives the message catalog for a specific domain. This function is not
 * thread-safe, so make sure calls to it are properly protected.
 *
 * @param[in] domain   The domain name (NULL for default).
 *
 * @return The catalog. If a specific catalog is not found, the default one
 *         is returned (which may be NULL).
 *
 ******************************************************************************
 */

static MsgCatalog *
MsgGetCatalog(const char *domain)
{
   MsgState *state = MsgGetState();
   MsgCatalog *catalog = NULL;

   ASSERT(domain != NULL);

   if (state->domains != NULL) {
      catalog = g_hash_table_lookup(state->domains, domain);
   }

   return catalog;
}


/*
 ******************************************************************************
 * MsgGetUserLanguage --                                                 */ /**
 *
 * Returns a string describing the user's default language using the
 * "language[_territory]" format (ISO 639-1 and ISO 3166-1, respectively) as
 * described in the setlocale(3) man page.
 *
 * @return Language code (caller should free).
 *
 ******************************************************************************
 */

static gchar *
MsgGetUserLanguage(void)
{
   gchar *lang;

#if defined(_WIN32)
   /*
    * Windows implementation. Derive the ISO names from the user's current
    * locale.
    */
   wchar_t ctryName[10]; /* MSDN says: max is nine characters + terminator. */
   wchar_t langName[10]; /* MSDN says: max is nine characters + terminator. */

   if (GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_SISO3166CTRYNAME,
                      ctryName, (sizeof(ctryName)/sizeof(langName[0]))) == 0 ||
       GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_SISO639LANGNAME,
                      langName, (sizeof(langName)/sizeof(langName[0]))) == 0) {
      g_warning("Couldn't retrieve user locale data, error = %u.", GetLastError());
      lang = g_strdup("C");
   } else {
      lang = g_strdup_printf("%S_%S", langName, ctryName);
   }
#else
   /*
    * POSIX implementation: just use setlocale() to query the data. Ignore any
    * codeset information.
    */
   char *tmp;
   /*
    * This is useful for testing, and also seems to be used by some
    * distros (NeoKylin) rather than the setlocale() APIs.
    * See PR 1672149
    */
   char *envLocale = getenv("LANG");
   if (envLocale != NULL) {
      lang = g_strdup(envLocale);
      g_debug("%s: Using LANG override of '%s'\n", __FUNCTION__, lang);
      return lang;
   }
   tmp = setlocale(LC_MESSAGES, NULL);
   if (tmp == NULL) {
      lang = g_strdup("C");
   } else {
      char *dot;

      lang = g_strdup(tmp);
      dot = strchr(lang, '.');
      if (dot != NULL) {
         *dot = '\0';
      }
   }
#endif

   return lang;
}


/*
 ******************************************************************************
 * MsgSetCatalog --                                                      */ /**
 *
 * Set the message catalog for a given domain. A NULL catalog clears the
 * current one. This function is not thread-safe, so make sure calls to it
 * are properly protected.
 *
 * @param[in] domain    The text domain being bound.
 * @param[in] catalog   The new message catalog.
 *
 ******************************************************************************
 */

static void
MsgSetCatalog(const char *domain,
              MsgCatalog *catalog)
{
   MsgState *state = MsgGetState();

   ASSERT(domain);

   if (state->domains == NULL) {
      state->domains = g_hash_table_new_full(g_str_hash,
                                             g_str_equal,
                                             g_free,
                                             MsgCatalogFree);
   }

   g_hash_table_insert(state->domains, g_strdup(domain), catalog);
}


/*
 ******************************************************************************
 * MsgLoadCatalog --                                                     */ /**
 *
 * Loads the message catalog at the given path into a new hash table.
 * The catalog entries are a simple <key> = <value>.  Line continutatiom
 * is not supported.
 *
 * @param[in] path    Path containing the message catalog (encoding should be
 *                    UTF-8).
 *
 * @return A new message catalog on success, NULL otherwise.
 *
 ******************************************************************************
 */

static MsgCatalog *
MsgLoadCatalog(const char *path)
{
   gchar *localPath;
   GError *err = NULL;
   GIOChannel *stream;
   gboolean error = FALSE;
   MsgCatalog *catalog = NULL;
   GHashTable *dict;

   localPath = GET_FILENAME_LOCAL(path, NULL);
   ASSERT(localPath != NULL);

   stream = g_io_channel_new_file(localPath, "r", &err);
   g_debug("%s: loading message catalog '%s'\n", __FUNCTION__, localPath);
   RELEASE_FILENAME_LOCAL(localPath);

   if (err != NULL) {
      g_debug("Unable to open '%s': %s\n", path, err->message);
      g_clear_error(&err);
      return NULL;
   }

   dict = g_hash_table_new_full(g_str_hash,
                                g_str_equal,
                                g_free,
                                g_free);
   for (;;) {
      char *name = NULL;
      char *value = NULL;
      gchar *line;
      gsize len;
      gsize term;
      char *unused = NULL;

      /* Read the next key / value pair. */

      g_io_channel_read_line(stream, &line, &len, &term, &err);

      if (err != NULL) {
         g_warning("Unable to read a line from '%s': %s\n",
                   path, err->message);
         g_clear_error(&err);
         error = TRUE;
         g_free(line);
         break;
      }

      if (line == NULL) {
         /* This signifies EOF. */
         break;
      }

      /*
       * Fix the line break to always be Unix-style, to make lib/dict
       * happy.
       */
      if (line[term] == '\r') {
         line[term] = '\n';
         if (len > term) {
            line[term + 1] = '\0';
         }
      }

      /*
       * Try to parse the string using the dictionary library.
       */
      if (DictLL_UnmarshalLine(line, len, &unused, &name, &value) == NULL) {
         g_warning("Couldn't parse line from catalog: %s", line);
         error = TRUE;
      }
      g_free(unused);
      g_free(line);

      if (error) {
         /*
          * If the local DictLL_UnmarshalLine() returns NULL, name and value
          * will remain NULL pointers.  No malloc'ed memory to free here.
          */
         break;
      }

      if (name != NULL) {
         gchar *val;
         ASSERT(value);

         if (!g_utf8_validate(name, -1, NULL) ||
             !g_utf8_validate(value, -1, NULL)) {
            g_warning("Invalid UTF-8 string in message catalog (key = %s)\n", name);
            error = TRUE;
            g_free(name);
            g_free(value);
            break;
         }

         // remove any escaped chars
         val = g_strcompress(value);
         g_free(value);

         // the hashtable takes ownership of the memory for 'name' and 'val'
         g_hash_table_insert(dict, name, val);
      }
   }

   g_io_channel_unref(stream);

   if (error) {
      g_hash_table_unref(dict);
      dict = NULL;
   } else {
      catalog = g_new0(MsgCatalog, 1);
      catalog->utf8 = dict;
   }

   return catalog;
}


/*
 ******************************************************************************
 * I18n_BindTextDomain --                                                */ /**
 *
 * Loads the message catalog for a text domain. Each text domain contains a
 * different set of messages loaded from a different catalog.
 *
 * If a catalog has already been bound to the given name, it is replaced with
 * the newly loaded data.
 *
 * @param[in] domain   Name of the text domain being loaded.
 * @param[in] lang     Language code for the text domain.
 * @param[in] catdir   Root directory of catalog files (NULL = default).
 *
 ******************************************************************************
 */

void
I18n_BindTextDomain(const char *domain,
                    const char *lang,
                    const char *catdir)
{
   gchar *file;
   gchar *usrlang = NULL;
   MsgState *state = MsgGetState();
   MsgCatalog *catalog;

   ASSERT(domain);

   /*
    * If the caller has asked for the default user language, detect it and
    * translate to our internal language string representation.
    */

   if (lang == NULL || *lang == '\0') {
      usrlang = MsgGetUserLanguage();
      lang = usrlang;
   } else {
      usrlang = g_strdup(lang);
   }

   /*
    * XXX may want to handle a NULL 'catdir', and look in relative
    * to the installed location.
    */

   g_debug("%s: user locale=%s\n", __FUNCTION__, lang);

   file = g_strdup_printf("%s%smessages%s%s%s%s.vmsg",
                          catdir, DIRSEPS, DIRSEPS, lang, DIRSEPS, domain);

   /*
    * If we couldn't find the catalog file for the user's language, see if
    * there's an encoding to chop off first, eg zh_CN.UTF-8
    */
   if (!g_file_test(file, G_FILE_TEST_IS_REGULAR)) {
      const char *sep = strrchr(lang, '.');
      if (sep != NULL) {
         usrlang[sep - lang] = '\0';
         g_free(file);
         file = g_strdup_printf("%s%smessages%s%s%s%s.vmsg",
                                catdir, DIRSEPS, DIRSEPS, usrlang, DIRSEPS, domain);
      }
   }

   /*
    * If we couldn't find the catalog file for the user's language, see if
    * we can find a more generic language (e.g., for "en_US", also try "en").
    */
   if (!g_file_test(file, G_FILE_TEST_IS_REGULAR)) {
      const char *sep = strrchr(lang, '_');
      if (sep != NULL) {
         usrlang[sep - lang] = '\0';
         g_free(file);
         file = g_strdup_printf("%s%smessages%s%s%s%s.vmsg",
                                catdir, DIRSEPS, DIRSEPS, usrlang, DIRSEPS, domain);
      }
   }

   catalog = MsgLoadCatalog(file);

   if (catalog == NULL) {
      if (strncmp(lang, "en", 2)) {
         /*
          * Don't warn about english dictionary, which may not exist (it is the
          * default translation).
          */
         g_message("Cannot load message catalog for domain '%s', language '%s', "
                   "catalog dir '%s'.\n", domain, lang, catdir);
      }
   } else {
      g_mutex_lock(&state->lock);
      MsgSetCatalog(domain, catalog);
      g_mutex_unlock(&state->lock);
   }
   g_free(file);
   g_free(usrlang);
}


/*
 ******************************************************************************
 * I18n_GetString --                                                     */ /**
 *
 * Retrieves a localized string in the requested encoding.
 *
 * All messages are retrieved based on the catalog data loaded in UTF-8.
 * Strings in other encodings are lazily converted from the UTF-8 version
 * as they are queried.
 *
 * @param[in] domain       Domain from where to fetch the messages.
 * @param[in] msgid        The message id, including the default English text.
 *
 * @return The string in the desired encoding.
 *
 ******************************************************************************
 */

const char *
I18n_GetString(const char *domain,
               const char *msgid)
{
   const char *idp;
   const char *strp;
   char idBuf[MSG_MAX_ID];
   size_t len;
   GHashTable *source = NULL;
   MsgCatalog *catalog;
   MsgState *state = MsgGetState();

   /* All message strings must be prefixed by the message ID. */
   ASSERT(domain != NULL);
   ASSERT(msgid != NULL);
   ASSERT(MsgHasMsgID(msgid));

   /*
    * Find the beginning of the ID (idp) and the string (strp).
    * The string should have the correct MSG_MAGIC(...)... form.
    */

   idp = msgid + MSG_MAGIC_LEN + 1;
   strp = strchr(idp, ')') + 1;

   len = strp - idp - 1;
   ASSERT(len <= MSG_MAX_ID - 1);
   memcpy(idBuf, idp, len);
   idBuf[len] = '\0';

   g_mutex_lock(&state->lock);

   catalog = MsgGetCatalog(domain);
   if (catalog != NULL) {
      source = catalog->utf8;
   }

   if (source != NULL) {
      const void *retval = NULL;

      retval = g_hash_table_lookup(source, idBuf);
      if (NULL != retval) {
         strp = retval;
      }
   }

   g_mutex_unlock(&state->lock);

   return strp;
}

