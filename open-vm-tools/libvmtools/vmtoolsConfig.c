/*********************************************************
 * Copyright (C) 2008-2019 VMware, Inc. All rights reserved.
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
#include "conf.h"
#include "err.h"
#include "guestApp.h"


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
   gchar *defaultPath = NULL;
   gchar *localPath = NULL;
   /* GStatBuf was added in 2.26. */
   GStatBuf confStat;
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
   if (err == NULL || err->code == G_KEY_FILE_ERROR_GROUP_NOT_FOUND) {
      goto exit;
   }

   g_warning("Cannot load config file: %s", err->message);

   g_key_file_free(cfg);
   cfg = NULL;

exit:
   g_clear_error(&err);
   if (cfg != NULL) {
      if (*config != NULL) {
         g_key_file_free(*config);
      }
      *config = cfg;
      if (mtime != NULL) {
         *mtime = confStat.st_mtime;
      }
   }
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
 * @param[in]  key      Key to look for in the section.
 * @param[in]  defValue Default value if the key is not found or error.
 *
 * @return value of the key if value was read successfully, else defValue.
 */

gboolean
VMTools_ConfigGetBoolean(GKeyFile *config,
                         const gchar *section,
                         const gchar *key,
                         const gboolean defValue)
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
      g_debug("%s: Returning default value for '[%s] %s'=%s "
              "(Not found err=%d).\n",
              __FUNCTION__, section, key, defValue ? "TRUE" : "FALSE",
              err->code);
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
 * @param[in]  key      Key to look for in the section.
 * @param[in]  defValue Default value if the key is not found or error.
 *
 * @return value of the key if value was read successfully, else defValue.
 */

gint
VMTools_ConfigGetInteger(GKeyFile *config,
                         const gchar *section,
                         const gchar *key,
                         const gint defValue)
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
 * @param[in]  key      Key to look for in the section.
 * @param[in]  defValue Default value if the key is not found or error.
 *
 * @return value of the key if value was read successfully, else a copy
 * of defValue unless defValue is NULL, in which case it's NULL.
 * The returned string should be freed with g_free() when no longer needed.
 */

gchar *
VMTools_ConfigGetString(GKeyFile *config,
                        const gchar *section,
                        const gchar *key,
                        const gchar *defValue)
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
      value = g_strdup(defValue);
      g_clear_error(&err);
   }
   return value;
}
