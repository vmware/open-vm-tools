/*********************************************************
 * Copyright (C) 2010-2020 VMware, Inc. All rights reserved.
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
 * Implementation of the i18n-related functions of the Tools library.
 */

#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <glib.h>
#include <glib/gprintf.h>

#include "vmware.h"
#include "dictll.h"
#include "escape.h"
#include "file.h"
#include "guestApp.h"
#include "hashTable.h"
#include "str.h"
#include "unicode.h"
#include "vmtoolsInt.h"
#include "vmware/tools/i18n.h"
#include "vmware/tools/utils.h"

/* These come from msgid.h. See MsgHasMsgID for explanation. */
#define MSG_MAX_ID      128
/* The X hides MSG_MAGIC so it won't appear in the object file. */
#define MSG_MAGICAL(s)  (strncmp(s, MSG_MAGIC"X", MSG_MAGIC_LEN) == 0)

typedef struct MsgCatalog {
   HashTable  *utf8;
#if defined(_WIN32)
   HashTable  *utf16;
#endif
} MsgCatalog;


typedef struct MsgState {
   HashTable     *domains; /* List of text domains. */
   GMutex         lock;    /* Mutex to protect shared state. */
} MsgState;


static MsgState *gMsgState = NULL;


/*
 ******************************************************************************
 * MsgCatalogFree --                                                    */ /**
 *
 * Frees memory allocated for a MsgCatalog structure.
 *
 * @param[in] catalog    The catalog to free.
 *
 ******************************************************************************
 */

static void
MsgCatalogFree(MsgCatalog *catalog)
{
   ASSERT(catalog);
#if defined(_WIN32)
   if (catalog->utf16 != NULL) {
      HashTable_Free(catalog->utf16);
   }
#endif
   if (catalog->utf8 != NULL) {
      HashTable_Free(catalog->utf8);
   }
   g_free(catalog);
}


/*
 ******************************************************************************
 * MsgHasMsgID --                                                       */ /**
 *
 * Check that a string has a message ID. The full "MSG_MAGIC(...)" prefix is
 * required, not just MSG_MAGIC.
 *
 * XXX: copied from msgid.h. We can't include that file since we redefine
 * the MSGID macro in our public header, to avoid exposing internal headers.
 *
 * @param[in] s    String to check.
 *
 * @return TRUE if the string has a message id.
 *
 ******************************************************************************
 */

#ifdef VMX86_DEBUG
static INLINE gboolean
MsgHasMsgID(const gchar *s)
{
   return MSG_MAGICAL(s) &&
          *(s += MSG_MAGIC_LEN) == '(' &&
          strchr(s + 1, ')') != NULL;
}
#endif

/*
 ******************************************************************************
 * MsgInitState --                                                      */ /**
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
   ASSERT(gMsgState == NULL);
   gMsgState = g_new0(MsgState, 1);
   g_mutex_init(&gMsgState->lock);
   return NULL;
}


/*
 ******************************************************************************
 * MsgGetState --                                                       */ /**
 *
 * Get the internal msg state (lazily initialized if needed).
 *
 * @return The message state object.
 *
 ******************************************************************************
 */

static INLINE MsgState *
MsgGetState(void)
{
   static GOnce msgStateInit = G_ONCE_INIT;
   g_once(&msgStateInit, MsgInitState, NULL);
   return gMsgState;
}


/*
 ******************************************************************************
 * MsgGetCatalog --                                                     */ /**
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

static INLINE MsgCatalog *
MsgGetCatalog(const char *domain)
{
   MsgState *state = MsgGetState();
   MsgCatalog *catalog = NULL;

   ASSERT(domain != NULL);

   if (state->domains != NULL) {
      MsgCatalog *domaincat;
      if (HashTable_Lookup(state->domains, domain, (void **) &domaincat)) {
         catalog = domaincat;
      }
   }

   return catalog;
}


/*
 ******************************************************************************
 * MsgGetUserLanguage --                                                */ /**
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

   if (GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SISO3166CTRYNAME,
                     ctryName, ARRAYSIZE(ctryName)) == 0 ||
       GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SISO639LANGNAME,
                     langName, ARRAYSIZE(langName)) == 0) {
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
   char *tmp = setlocale(LC_MESSAGES, NULL);
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
 * MsgSetCatalog --                                                     */ /**
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
      state->domains = HashTable_Alloc(8, HASH_STRING_KEY | HASH_FLAG_COPYKEY,
                                       (HashTableFreeEntryFn) MsgCatalogFree);
      ASSERT_MEM_ALLOC(state->domains);
   }

   HashTable_ReplaceOrInsert(state->domains, domain, catalog);
}


/*
 ******************************************************************************
 * MsgGetString --                                                      */ /**
 *
 * Retrieves a localized string in the requested encoding.
 *
 * All messages are retrieved based on the catalog data loaded in UTF-8.
 * Strings in other encodings are lazily converted from the UTF-8 version
 * as they are queried.
 *
 * @param[in] msgid        The message id, including the default English text.
 * @param[in] catalog      Catalog from where to fetch the messages.
 * @param[in] encoding     The desired encoding. Only STRING_ENCODING_UTF8 and
 *                         STRING_ENCODING_UTF16_LE (Win32) are supported by
 *                         this function.
 *
 * @return The string in the desired encoding.
 *
 ******************************************************************************
 */

static const void *
MsgGetString(const char *domain,
             const char *msgid,
             StringEncoding encoding)
{
   const char *idp;
   const char *strp;
   char idBuf[MSG_MAX_ID];
   size_t len;
   HashTable *source = NULL;
   MsgCatalog *catalog;
   MsgState *state = MsgGetState();

   /* All message strings must be prefixed by the message ID. */
   ASSERT(domain != NULL);
   ASSERT(msgid != NULL);
   ASSERT(MsgHasMsgID(msgid));
#if defined(_WIN32)
   ASSERT(encoding == STRING_ENCODING_UTF8 ||
          encoding == STRING_ENCODING_UTF16_LE);
#else
   ASSERT(encoding == STRING_ENCODING_UTF8);
#endif

   /*
    * Find the beginning of the ID (idp) and the string (strp).
    * The string should have the correct MSG_MAGIC(...)... form.
    */

   idp = msgid + MSG_MAGIC_LEN + 1;
   strp = strchr(idp, ')') + 1;

   len = strp - idp - 1;
   ASSERT_NOT_IMPLEMENTED(len <= MSG_MAX_ID - 1);
   memcpy(idBuf, idp, len);
   idBuf[len] = '\0';

   /*
    * This lock is pretty coarse-grained, but a lot of the code below just runs
    * in exceptional situations, so it should be OK.
    */
   g_mutex_lock(&state->lock);

   catalog = MsgGetCatalog(domain);
   if (catalog != NULL) {
      switch (encoding) {
      case STRING_ENCODING_UTF8:
         source = catalog->utf8;
         break;

#if defined(_WIN32)
      case STRING_ENCODING_UTF16_LE:
         source = catalog->utf16;
         break;
#endif

      default:
         NOT_IMPLEMENTED();
      }
   }

#if defined(_WIN32)
   /*
    * Lazily create the local and UTF-16 dictionaries. This may require also
    * registering an empty message catalog for the desired domain.
    */
   if (source == NULL && encoding == STRING_ENCODING_UTF16_LE) {
      catalog = MsgGetCatalog(domain);
      if (catalog == NULL) {
         if (domain == NULL) {
            g_error("Application did not set up a default text domain.");
         }
         catalog = g_new0(MsgCatalog, 1);
         MsgSetCatalog(domain, catalog);
      }

      catalog->utf16 = HashTable_Alloc(8, HASH_STRING_KEY, g_free);
      ASSERT_MEM_ALLOC(catalog->utf16);
      source = catalog->utf16;
   }
#endif

   /*
    * Look up the localized string, converting to requested encoding as needed.
    */

   if (source != NULL) {
      const void *retval = NULL;

      if (HashTable_Lookup(source, idBuf, (void **) &retval)) {
         strp = retval;
#if defined(_WIN32)
      } else if (encoding == STRING_ENCODING_UTF16_LE) {
         gchar *converted;
         Bool success;

         /*
          * Look up the string in UTF-8, convert it and cache it.
          */
         retval = MsgGetString(domain, msgid, STRING_ENCODING_UTF8);
         ASSERT(retval);

         converted = (gchar *) g_utf8_to_utf16(retval, -1, NULL, NULL, NULL);
         ASSERT(converted != NULL);

         success = HashTable_Insert(source, idBuf, converted);
         ASSERT(success);
         strp = converted;
#endif
      }
   }

   g_mutex_unlock(&state->lock);

   return strp;
}




/*
 ******************************************************************************
 * MsgLoadCatalog --                                                    */ /**
 *
 * Loads the message catalog at the given path into a new hash table.
 *
 * This function supports an "extended" format for the catalog files. Aside
 * from the usual things you can put in a lib/dict-based dictionary, this code
 * supports multi-line messages so that long messages can be broken down.
 *
 * These lines are any lines following a key / value declaration that start with
 * a quote character (ignoring any leading spaces and tabs). So a long message
 * could look like this:
 *
 * @code
 * message.id = "This is the first part of the message. "
 *              "This is the continuation line, still part of the same message."
 * @endcode
 *
 * The complete value for the "message.id" key will be the concatenation of
 * the values in quotes.
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
   HashTable *dict;

   ASSERT(path != NULL);
   localPath = VMTOOLS_GET_FILENAME_LOCAL(path, NULL);
   ASSERT(localPath != NULL);

   stream = g_io_channel_new_file(localPath, "r", &err);
   VMTOOLS_RELEASE_FILENAME_LOCAL(localPath);

   if (err != NULL) {
      g_debug("Unable to open '%s': %s\n", path, err->message);
      g_clear_error(&err);
      return NULL;
   }

   dict = HashTable_Alloc(8, HASH_STRING_KEY | HASH_FLAG_COPYKEY, g_free);
   ASSERT_MEM_ALLOC(dict);

   for (;;) {
      gboolean eof = FALSE;
      char *name = NULL;
      char *value = NULL;
      gchar *line = NULL;

      /* Read the next key / value pair. */
      for (;;) {
         gsize i;
         gsize len;
         gsize term;
         char *unused = NULL;
         gboolean cont = FALSE;
         GIOStatus status;

         status = g_io_channel_read_line(stream, &line, &len, &term, &err);

         if (status == G_IO_STATUS_ERROR || err != NULL) {
            g_warning("Unable to read a line from '%s': %s\n",
                      path, err ? err->message : "G_IO_STATUS_ERROR");
            g_clear_error(&err);
            error = TRUE;
            g_free(line);
            break;
         }

         if (line == NULL) {
            eof = TRUE;
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
          * If currently name is not NULL, then check if this is a continuation
          * line and, if it is, just append the contents to the current value.
          */
         if (term > 0 && name != NULL && line[term - 1] == '"') {
            for (i = 0; i < len; i++) {
               if (line[i] == '"') {
                  /* OK, looks like a continuation line. */
                  char *tmp;
                  char *unescaped;

                  line[term - 1] = '\0';
                  unescaped = Escape_Undo('|', line + i + 1, len - i, NULL);
                  tmp = Str_Asprintf(NULL, "%s%s", value, unescaped);
                  free(unescaped);
                  free(value);
                  value = tmp;
                  cont = TRUE;
                  break;
               } else if (line[i] != ' ' && line[i] != '\t') {
                  break;
               }
            }
         }

         /*
          * If not a continuation line and we have a name, break out of the
          * inner loop to update the dictionary.
          */
         if (!cont && name != NULL) {
            g_free(line);
            break;
         }

         /*
          * Finally, try to parse the string using the dictionary library.
          */
         if (!cont && DictLL_UnmarshalLine(line, len, &unused, &name, &value) == NULL) {
            g_warning("Couldn't parse line from catalog: %s", line);
            error = TRUE;
         }

         g_free(line);
         free(unused);
      }

      if (error) {
         free(name);
         free(value);
         break;
      }

      if (name != NULL) {
         gchar *val;
         ASSERT(value);

         if (!Unicode_IsBufferValid(name, strlen(name) + 1, STRING_ENCODING_UTF8) ||
             !Unicode_IsBufferValid(value, strlen(value) + 1, STRING_ENCODING_UTF8)) {
            g_warning("Invalid UTF-8 string in message catalog (key = %s)\n", name);
            error = TRUE;
            free(name);
            free(value);
            break;
         }

         val = g_strcompress(value);
         free(value);
         HashTable_ReplaceOrInsert(dict, name, val);
         free(name);
      }

      if (eof) {
         break;
      }
   }

   g_io_channel_unref(stream);

   if (error) {
      HashTable_Free(dict);
      dict = NULL;
   } else {
      catalog = g_new0(MsgCatalog, 1);
      catalog->utf8 = dict;
   }

   return catalog;
}


/*
 ******************************************************************************
 * VMToolsMsgCleanup --                                                 */ /**
 *
 * Cleanup internal state, freeing up any used memory. After calling this
 * function, it's not safe to call any of the API exposed by this file, so
 * this is only called internally when the library is being unloaded.
 *
 ******************************************************************************
 */

void
VMToolsMsgCleanup(void)
{
   if (gMsgState != NULL) {
      if (gMsgState->domains != NULL) {
         HashTable_Free(gMsgState->domains);
      }
      g_mutex_clear(&gMsgState->lock);
      g_free(gMsgState);
   }
}


/*
 ******************************************************************************
 * VMTools_BindTextDomain --                                            */ /**
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
VMTools_BindTextDomain(const char *domain,
                       const char *lang,
                       const char *catdir)
{
   char *dfltdir = NULL;
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
   }

   g_debug("%s: user locale=%s\n", __FUNCTION__, lang);

   /*
    * Use the default install directory if none is provided.
    */

   if (catdir == NULL) {
#if defined(OPEN_VM_TOOLS)
      dfltdir = Util_SafeStrdup(VMTOOLS_DATA_DIR);
#else
      dfltdir = GuestApp_GetInstallPath();
#endif
      catdir = (dfltdir) ? dfltdir : ".";
   }

   file = g_strdup_printf("%s%smessages%s%s%s%s.vmsg",
                          catdir, DIRSEPS, DIRSEPS, lang, DIRSEPS, domain);

   if (!File_IsFile(file)) {
      /*
       * If we couldn't find the catalog file for the user's language, see if
       * we can find a more generic language (e.g., for "en_US", also try "en").
       */
      char *sep = Str_Strrchr(lang, '_');
      if (sep != NULL) {
         if (usrlang == NULL) {
            usrlang = Util_SafeStrdup(lang);
         }
         usrlang[sep - lang] = '\0';
         g_free(file);
         file = g_strdup_printf("%s%smessages%s%s%s%s.vmsg",
                                catdir, DIRSEPS, DIRSEPS, usrlang, DIRSEPS, domain);
      }
   }

   catalog = MsgLoadCatalog(file);

   if (catalog == NULL) {
      if (Str_Strncmp(lang, "en", 2)) {
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
   free(dfltdir);
   g_free(usrlang);
}


/*
 ******************************************************************************
 * VMTools_GetString --                                                 */ /**
 *
 * Returns a localized version of the requested string in UTF-8.
 *
 * @param[in] domain    Text domain.
 * @param[in] msgid     Message id (including English translation).
 *
 * @return The localized string.
 *
 ******************************************************************************
 */

const char *
VMTools_GetString(const char *domain,
                  const char *msgid)
{
   return MsgGetString(domain, msgid, STRING_ENCODING_UTF8);
}


#if defined(_WIN32)
/*
 ******************************************************************************
 * VMTools_GetUtf16String --                                            */ /**
 *
 * Returns a localized string in UTF-16LE encoding. Win32 only.
 *
 * @param[in] domain    Text domain.
 * @param[in] msgid     Message id (including English translation).
 *
 * @return The localized string.
 *
 ******************************************************************************
 */

const wchar_t *
VMTools_GetUtf16String(const char *domain,
                       const char *msgid)
{
   return MsgGetString(domain, msgid, STRING_ENCODING_UTF16_LE);
}
#endif

