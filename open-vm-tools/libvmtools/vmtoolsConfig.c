/*********************************************************
 * Copyright (C) 2008-2016 VMware, Inc. All rights reserved.
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
 * @file vmtoolsConfig.c
 *
 *    Convenience functions for loading tools configuration files, and
 *    automatically migrating from old-style tools configuration files.
 */

#include "vmware/tools/utils.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib/gstdio.h>

#include "vm_assert.h"
#include "dictll.h"
#include "conf.h"
#include "err.h"
#include "guestApp.h"
#include "str.h"
#include "strutil.h"
#include "util.h"

/** Data types supported for translation. */
typedef enum {
   CFG_BOOLEAN,
   CFG_INTEGER,
   CFG_STRING,
   CFG_CALLBACK
} ConfigType;

/** Holds information about how to upgrade an old config entry. */
typedef struct ConfigEntry {
   const gchar         *key;
   const gchar         *destGroup;
   const gchar         *destKey;
   const ConfigType     type;
   const gpointer       data;
} ConfigEntry;

typedef void (*CfgCallback)(GKeyFile *cfg, const ConfigEntry *, const char *);


/**
 * Loads the legacy configuration file in the VMware dictionary format.
 *
 * @return A dictionary with the config data, NULL on error.
 */

static GHashTable *
VMToolsConfigLoadLegacy(void)
{
   gchar *path;
   gchar *localPath;
   char *confPath = GuestApp_GetConfPath();
   gboolean success = FALSE;
   FILE *stream = NULL;
   GHashTable *dict = NULL;

   if (confPath == NULL) {
      Panic("Could not get path to Tools configuration file.\n");
   }

   /* Load the data from the old config file. */
   path = g_strdup_printf("%s%c%s", confPath, DIRSEPC, CONF_FILE);
   localPath = VMTOOLS_GET_FILENAME_LOCAL(path, NULL);
   if (localPath == NULL) {
      g_warning("Error converting path to local encoding.");
      goto exit;
   }

   stream = g_fopen(localPath, "r");
   if (stream == NULL) {
      goto exit;
   }

   dict = g_hash_table_new_full(g_str_hash, g_str_equal, free, free);

   for (;;) {
      char *name;
      char *value;
      char *line;
      int status;

      status = DictLL_ReadLine(stream, &line, &name, &value);
      if (status == 0) {
         g_warning("Unable to read a line from \"%s\": %s\n", path,
                   Err_ErrString());
         goto exit;
      } else if (status == 1) {
         break;
      } else if (status != 2) {
         NOT_IMPLEMENTED();
      }

      if (name && value) {
         g_hash_table_insert(dict, name, value);
      } else {
         free(name);
         free(value);
      }

      free(line);
   }

   success = TRUE;

exit:
   if (stream != NULL && fclose(stream)) {
      g_warning("Unable to close \"%s\": %s\n", path, Err_ErrString());
      success = FALSE;
   }

   VMTOOLS_RELEASE_FILENAME_LOCAL(localPath);
   g_free(path);
   free(confPath);

   if (!success) {
      if (dict != NULL) {
         g_hash_table_destroy(dict);
         dict = NULL;
      }
   }

   return dict;
}


/**
 * Upgrade the logging configuration.
 *
 * @param[in] cfg       Config dictionary.
 * @param[in] entry     The ConfigEntry instance.
 * @param[in] value     Config value.
 */

static void
VMToolsConfigUpgradeLog(GKeyFile *cfg,
                        const ConfigEntry *entry,
                        const char *value)
{
   /*
    * The old configuration used the same file path for both guestd
    * and vmware-user. Instead of baking in the library whether we're
    * running one or the other, separate the two configurations, so that
    * the user can choose a different location for one or another.
    *
    * Also append the PID to the vmusr log path, since the old code did
    * that automatically.
    */
   gchar *userlog;

   g_key_file_set_string(cfg, entry->destGroup,
                         VMTOOLS_GUEST_SERVICE ".handler", "file");
   g_key_file_set_string(cfg, entry->destGroup,
                         VMTOOLS_GUEST_SERVICE ".level", "debug");
   g_key_file_set_string(cfg, entry->destGroup,
                         VMTOOLS_GUEST_SERVICE ".data", value);

   userlog = g_strdup_printf("%s.user.${PID}", value);
   g_key_file_set_string(cfg, entry->destGroup,
                         VMTOOLS_USER_SERVICE ".handler", "file");
   g_key_file_set_string(cfg, entry->destGroup,
                         VMTOOLS_USER_SERVICE ".level", "debug");
   g_key_file_set_string(cfg, entry->destGroup,
                         VMTOOLS_USER_SERVICE ".data", userlog);

   /*
    * Keep the log.file entry since vmware-user is still using the old-style
    * config. This can go away once it's ported over.
    */
   g_key_file_set_string(cfg, entry->destGroup, CONFNAME_LOGFILE, value);

   g_free(userlog);
}


/**
 * Translates a VMware dictionary containing tools configuration data
 * to a GKeyFile dictionary. Only known tools options are translated.
 *
 * @param[in] old    The old configuration data.
 * @param[in] dst    The destination.
 */

static void
VMToolsConfigUpgrade(GHashTable *old,
                     GKeyFile *dst)
{
   const ConfigEntry entries[] = {
      /* Logging options. */
      { CONFNAME_LOGFILE, "logging", NULL, CFG_CALLBACK, VMToolsConfigUpgradeLog },
      { CONFNAME_LOG, "logging", "log", CFG_BOOLEAN, NULL },
      /* Power op scripts. */
      { CONFNAME_POWEROFFSCRIPT, "powerops", CONFNAME_POWEROFFSCRIPT, CFG_STRING, NULL },
      { CONFNAME_POWERONSCRIPT, "powerops", CONFNAME_POWERONSCRIPT, CFG_STRING, NULL },
      { CONFNAME_RESUMESCRIPT, "powerops", CONFNAME_RESUMESCRIPT, CFG_STRING, NULL },
      { CONFNAME_SUSPENDSCRIPT, "powerops", CONFNAME_SUSPENDSCRIPT, CFG_STRING, NULL },
      /* guestd options. */
      { CONFNAME_GUESTINFO_DISABLEQUERYDISKINFO, "guestinfo", CONFNAME_GUESTINFO_DISABLEQUERYDISKINFO, CFG_BOOLEAN, NULL },
      { CONFNAME_DISABLETOOLSVERSION, "vmsvc", CONFNAME_DISABLETOOLSVERSION, CFG_BOOLEAN, NULL },
#if defined(_WIN32)
      { CONFNAME_DISABLEPMTIMERWARNING, "desktopevents", CONFNAME_DISABLEPMTIMERWARNING, CFG_BOOLEAN, NULL },
#endif
      /* Unity options. */
      { "unity.forceEnable", CONFGROUPNAME_UNITY, CONFNAME_UNITY_FORCEENABLE, CFG_BOOLEAN, NULL },
      { "unity.desktop.backgroundColor", CONFGROUPNAME_UNITY, CONFNAME_UNITY_BACKGROUNDCOLOR, CFG_INTEGER, NULL },
      /* Null terminator. */
      { NULL, }
   };
   const ConfigEntry *entry;

   for (entry = entries; entry->key != NULL; entry++) {
      const char *value = g_hash_table_lookup(old, entry->key);

      if (value == NULL) {
         continue;
      }

      switch (entry->type) {
      case CFG_BOOLEAN:
         {
            gboolean val = Str_Strcasecmp(value, "TRUE") == 0;
            g_key_file_set_boolean(dst, entry->destGroup, entry->destKey, val);
            break;
         }

      case CFG_INTEGER:
         {
            gint val;
            if (StrUtil_StrToInt(&val, value)) {
               g_key_file_set_integer(dst, entry->destGroup, entry->destKey, val);
            }
            break;
         }

      case CFG_STRING:
         g_key_file_set_string(dst, entry->destGroup, entry->destKey, value);
         break;

      case CFG_CALLBACK:
         ASSERT(entry->data);
         ((CfgCallback)entry->data)(dst, entry, value);
         break;

      default:
         NOT_REACHED();
      }
   }
}


/**
 * Returns the path to the default tools config file.
 *
 * @return String with the default config path (should be freed by caller).
 */

static gchar *
VMToolsGetToolsConfFile(void)
{
   char *confPath = GuestApp_GetConfPath();
   gchar *confFilePath;

   /*
    * XXX: GuestApp_GetConfPath() is racy. If two different people are calling
    * the function and the conf directory doesn't exist, there's the risk that
    * one of them will fail to create the directory and return NULL. So if we
    * get NULL back, retry the call (at which point the directory will
    * hopefully exist), and assert if it's NULL (which now should only happen
    * if we fail to allocate memory).
    */
   if (confPath == NULL) {
      confPath = GuestApp_GetConfPath();
      ASSERT(confPath != NULL);
   }
   confFilePath = g_build_filename(confPath, CONF_FILE, NULL);
   free(confPath);

   return confFilePath;
}


/**
 * Loads the configuration file at the given path.
 *
 * If an old configuration file is detected and the current process has write
 * permission to the file, the configuration data will automatically upgraded to
 * the new configuration format (the old configuration file is saved with a
 * ".old" extension).
 *
 * @param[in]     path     Path to the configuration file, or NULL for default
 *                         Tools config file.
 * @param[in]     flags    Flags for opening the file.
 * @param[in,out] config   Where to store the config dictionary; when reloading
 *                         the file, the old config object will be destroyed.
 * @param[in,out] mtime    Last known modification time of the config file.
 *                         When the function succeeds, will contain the new
 *                         modification time read from the file. If NULL (or 0),
 *                         the config dictionary is always loaded.
 *
 * @return Whether a new config dictionary was loaded.
 */

gboolean
VMTools_LoadConfig(const gchar *path,
                   GKeyFileFlags flags,
                   GKeyFile **config,
                   time_t *mtime)
{
   gchar *backup = NULL;
   gchar *defaultPath = NULL;
   gchar *localPath = NULL;
   /* GStatBuf was added in 2.26. */
#if GLIB_CHECK_VERSION(2, 26, 0)
   GStatBuf confStat;
#else
   struct stat confStat;
#endif
   GHashTable *old = NULL;
   GError *err = NULL;
   GKeyFile *cfg = NULL;
   static gboolean hadConfFile = TRUE;

   g_return_val_if_fail(config != NULL, FALSE);

   if (path == NULL) {
      defaultPath = VMToolsGetToolsConfFile();
   }

   localPath = VMTOOLS_GET_FILENAME_LOCAL((path != NULL) ? path : defaultPath, &err);
   if (err != NULL) {
      g_warning("Error converting to local encoding: %s\n", err->message);
      goto exit;
   }

   if (g_stat(localPath, &confStat) == -1) {
      /*
       * If the file doesn't exist, it's not an error. Just return an
       * empty dictionary in that case. The mtime will be set to 0 if
       * the caller requested it.
       */
      memset(&confStat, 0, sizeof confStat);
      if (errno != ENOENT) {
         g_warning("Failed to stat conf file: %s\n", strerror(errno));
         goto exit;
      } else {
         /*
          * If we used to have a file, create a config.
          * Otherwise we can re-use the empty GKeyFile from before.
          */
         if (hadConfFile) {
            cfg = g_key_file_new();
         }
         hadConfFile = FALSE;
         goto exit;
      }
   }

   hadConfFile = TRUE;

   /* Check if we really need to load the data. */
   if (mtime != NULL && confStat.st_mtime <= *mtime) {
      goto exit;
   }

   /* Need to load the configuration data. */

   cfg = g_key_file_new();

   /* Empty file: just return an empty dictionary. */
   if (confStat.st_size == 0) {
      goto exit;
   }

   g_key_file_load_from_file(cfg, localPath, flags, &err);
   if (err == NULL) {
      goto exit;
   }

   if (err->code != G_KEY_FILE_ERROR_GROUP_NOT_FOUND) {
      g_warning("Cannot load config file: %s", err->message);
      goto error;
   }

   /*
    * Failed to load the config file; try to upgrade if requested. But only do
    * it if the user is using the default conf file path; the legacy API doesn't
    * allow us to provide a custom config file path.
    */
   if (path == NULL) {
      old = VMToolsConfigLoadLegacy();
      if (old == NULL) {
         g_warning("Error loading old tools config data, bailing out.\n");
         goto error;
      }

      VMToolsConfigUpgrade(old, cfg);
      backup = g_strdup_printf("%s.old", localPath);

      if (!g_file_test(backup, G_FILE_TEST_IS_REGULAR)) {
         if (g_rename(localPath, backup) == -1) {
            g_warning("Error creating backup of old config file.\n");
            goto error;
         }
      } else {
         g_warning("Backup config exists, skipping backup.\n");
      }

      g_clear_error(&err);

      if (!VMTools_WriteConfig((path != NULL) ? path : defaultPath, cfg, NULL)) {
         goto error;
      }
   }

   goto exit;

error:
   g_key_file_free(cfg);
   cfg = NULL;

exit:
   g_clear_error(&err);
   if (old != NULL) {
      g_hash_table_destroy(old);
   }
   if (cfg != NULL) {
      if (*config != NULL) {
         g_key_file_free(*config);
      }
      *config = cfg;
      if (mtime != NULL) {
         *mtime = confStat.st_mtime;
      }
   }
   g_free(backup);
   g_free(defaultPath);
   VMTOOLS_RELEASE_FILENAME_LOCAL(localPath);
   return (cfg != NULL);
}


/**
 * Saves the given config data to the given path.
 *
 * @param[in]  path     Where to save the data.
 * @param[in]  config   Config data.
 * @param[out] err      Where to store error information (may be NULL).
 *
 * @return Whether saving was successful.
 */

gboolean
VMTools_WriteConfig(const gchar *path,
                    GKeyFile *config,
                    GError **err)
{
   gboolean ret = FALSE;
   gchar *data = NULL;
   gchar *defaultPath = NULL;
   gchar *localPath = NULL;
   FILE *out = NULL;
   GError *lerr = NULL;

   ASSERT(config != NULL);

   if (path == NULL) {
      defaultPath = VMToolsGetToolsConfFile();
   }

   localPath = VMTOOLS_GET_FILENAME_LOCAL((path != NULL) ? path : defaultPath, &lerr);
   if (lerr != NULL) {
      g_warning("Error converting to local encoding: %s\n", lerr->message);
      goto exit;
   }

   data = g_key_file_to_data(config, NULL, &lerr);
   if (lerr != NULL) {
      g_warning("Error serializing conf data: %s\n", lerr->message);
      goto exit;
   }

   out = g_fopen(localPath, "w");

   if (out == NULL) {
      const char *errstr = strerror(errno);
      g_warning("Error opening conf file for writing: %s\n", errstr);
      g_set_error(&lerr, G_FILE_ERROR, G_FILE_ERROR_FAILED, "%s", errstr);
      goto exit;
   }

   if (g_fprintf(out, "%s", data) < 0) {
      const char *errstr = strerror(errno);
      g_warning("Error writing conf file: %s\n", errstr);
      g_set_error(&lerr, G_FILE_ERROR, G_FILE_ERROR_FAILED, "%s", errstr);
      goto exit;
   }

   ret = TRUE;

exit:
   if (out != NULL) {
      fclose(out);
   }
   if (err != NULL && lerr != NULL) {
      *err = lerr;
   } else {
      g_clear_error(&lerr);
   }
   g_free(data);
   g_free(defaultPath);
   VMTOOLS_RELEASE_FILENAME_LOCAL(localPath);
   return ret;
}


/**
 * Loads boolean value for a key from the specified config section.
 *
 * @param[in]  config   Config file to read the key from.
 * @param[in]  section  Section to look for in the config file.
 * @param[in]  defValue Default value if the key is not found or error.
 *
 * @return value of the key if value was read successfully, else defValue.
 */

gboolean
VMTools_ConfigGetBoolean(GKeyFile *config,
                         const gchar *section,
                         const gchar *key,
                         gboolean defValue)
{
   GError *err = NULL;
   gboolean value;

   if (config == NULL || section == NULL || key == NULL) {
      g_debug("%s: Returning default value for '[%s] %s'=%s.\n",
              __FUNCTION__, section ? section : "(null)",
              key ? key : "(null)", defValue ? "TRUE" : "FALSE");
      return defValue;
   }

   value = g_key_file_get_boolean(config, section, key, &err);
   if (err != NULL) {
      if (err->code != G_KEY_FILE_ERROR_KEY_NOT_FOUND &&
          err->code != G_KEY_FILE_ERROR_GROUP_NOT_FOUND) {
         g_warning("%s: Failed to get value for '[%s] %s': %s (err=%d).\n",
                   __FUNCTION__, section, key, err->message, err->code);
      }
      g_debug("%s: Returning default value for '[%s] %s'=%s.\n",
              __FUNCTION__, section, key, defValue ? "TRUE" : "FALSE");
      value = defValue;
      g_clear_error(&err);
   }
   return value;
}


/**
 * Loads integer value for a key from the specified config section.
 *
 * @param[in]  config   Config file to read the key from.
 * @param[in]  section  Section to look for in the config file.
 * @param[in]  defValue Default value if the key is not found or error.
 *
 * @return value of the key if value was read successfully, else defValue.
 */

gint
VMTools_ConfigGetInteger(GKeyFile *config,
                         const gchar *section,
                         const gchar *key,
                         gint defValue)
{
   GError *err = NULL;
   gint value;

   ASSERT(config);
   ASSERT(key);
   ASSERT(section);

   value = g_key_file_get_integer(config, section, key, &err);
   if (err != NULL) {
      if (err->code != G_KEY_FILE_ERROR_KEY_NOT_FOUND &&
          err->code != G_KEY_FILE_ERROR_GROUP_NOT_FOUND) {
         g_warning("%s: Failed to get value for '[%s] %s': %s (err=%d).\n",
                   __FUNCTION__, section, key, err->message, err->code);
      }
      g_debug("%s: Returning default value for '[%s] %s'=%d.\n",
              __FUNCTION__, section, key, defValue);
      value = defValue;
      g_clear_error(&err);
   }
   return value;
}


/**
 * Loads string value for a key from the specified config section.
 *
 * @param[in]  config   Config file to read the key from.
 * @param[in]  section  Section to look for in the config file.
 * @param[in]  defValue Default value if the key is not found or error.
 *
 * @return value of the key if value was read successfully, else defValue.
 */

gchar *
VMTools_ConfigGetString(GKeyFile *config,
                        const gchar *section,
                        const gchar *key,
                        gchar *defValue)
{
   GError *err = NULL;
   gchar *value;

   ASSERT(config);
   ASSERT(key);
   ASSERT(section);

   value = g_key_file_get_string(config, section, key, &err);
   if (err != NULL) {
      if (err->code != G_KEY_FILE_ERROR_KEY_NOT_FOUND &&
          err->code != G_KEY_FILE_ERROR_GROUP_NOT_FOUND) {
         g_warning("%s: Failed to get value for '[%s] %s': %s (err=%d).\n",
                   __FUNCTION__, section, key, err->message, err->code);
      }
      g_debug("%s: Returning default value for '[%s] %s'=%s.\n",
              __FUNCTION__, section, key, defValue ? defValue : "(null)");
      value = defValue;
      g_clear_error(&err);
   }
   return value;
}
