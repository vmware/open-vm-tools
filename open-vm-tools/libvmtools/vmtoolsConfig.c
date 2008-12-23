/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib/gstdio.h>
#include "vmtools.h"
#include "conf.h"
#include "file.h"
#include "guestApp.h"
#include "str.h"
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
    */
   gchar *userlog;

   g_key_file_set_string(cfg, entry->destGroup,
                         VMTOOLS_GUEST_SERVICE ".handler", "file");
   g_key_file_set_string(cfg, entry->destGroup,
                         VMTOOLS_GUEST_SERVICE ".level", "debug");
   g_key_file_set_string(cfg, entry->destGroup,
                         VMTOOLS_GUEST_SERVICE ".data", value);

   userlog = g_strdup_printf("%s.user", value);
   g_key_file_set_string(cfg, entry->destGroup,
                         VMTOOLS_USER_SERVICE ".handler", "file");
   g_key_file_set_string(cfg, entry->destGroup,
                         VMTOOLS_USER_SERVICE ".level", "debug");
   g_key_file_set_string(cfg, entry->destGroup,
                         VMTOOLS_USER_SERVICE ".data", userlog);

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
VMToolsConfigUpgrade(GuestApp_Dict *old,
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
      { CONFNAME_MAX_WIPERSIZE, "vmsvc", CONFNAME_MAX_WIPERSIZE, CFG_INTEGER, NULL },
      { CONFNAME_DISABLEQUERYDISKINFO, "guestinfo", CONFNAME_DISABLEQUERYDISKINFO, CFG_BOOLEAN, NULL },
      { CONFNAME_DISABLETOOLSVERSION, "vmsvc", CONFNAME_DISABLETOOLSVERSION, CFG_BOOLEAN, NULL },
#if !defined(_WIN32)
      { CONFNAME_HALT, "vmsvc", CONFNAME_HALT, CFG_STRING, NULL },
      { CONFNAME_REBOOT, "vmsvc", CONFNAME_REBOOT, CFG_STRING, NULL },
      /* HGFS options. */
      { CONFNAME_MOUNT_POINT, "hgfs", CONFNAME_MOUNT_POINT, CFG_STRING, NULL },
      /* Tray options. */
      { CONFNAME_SHOW_WIRELESS_ICON, "vmtray", CONFNAME_SHOW_WIRELESS_ICON, CFG_BOOLEAN, NULL },
#endif
      /* Null terminator. */
      { NULL, }
   };
   const ConfigEntry *entry;

   for (entry = entries; entry->key != NULL; entry++) {
      const char *value = GuestApp_GetDictEntry(old, entry->key);
      const char *dfltValue = GuestApp_GetDictEntryDefault(old, entry->key);
      
      if (value == NULL || (dfltValue != NULL && strcmp(value, dfltValue) == 0)) {
         continue;
      }
      switch (entry->type) {
      case CFG_BOOLEAN:
         {
            gboolean val = GuestApp_GetDictEntryBool(old, entry->key);
            g_key_file_set_boolean(dst, entry->destGroup, entry->destKey, val);
            break;
         }

      case CFG_INTEGER:
         {
            gint val;
            if (GuestApp_GetDictEntryInt(old, entry->key, &val)) {
               g_key_file_set_integer(dst, entry->destGroup, entry->destKey, val);
            }
            break;
         }

      case CFG_STRING:
         {
            const char *val = GuestApp_GetDictEntry(old, entry->key);
            if (val != NULL) {
               g_key_file_set_string(dst, entry->destGroup, entry->destKey, val);
            }
            break;
         }

      case CFG_CALLBACK:
         {
            const char *val = GuestApp_GetDictEntry(old, entry->key);
            if (val != NULL) {
               g_assert(entry->data);
               ((CfgCallback)entry->data)(dst, entry, val);
            }
            break;
         }

      default:
         g_assert_not_reached();
      }
   }
}


/**
 * Returns the path to the default tools config file.
 *
 * @return String with the default config path (should be freed by caller).
 */

gchar *
VMTools_GetToolsConfFile(void)
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
      g_assert(confPath != NULL);
   }
   confFilePath = g_strdup_printf("%s%c%s", confPath, DIRSEPC, CONF_FILE);
   free(confPath);

   return confFilePath;
}


/**
 * Loads the configuration file at the given path. If an old configuration
 * file is detected, the caller can request for it to be automatically upgraded
 * to the new configuration format (the old configuration file is saved with a
 * ".old" extension).
 *
 * @param[in] path         Path to the configuration file.
 * @param[in] flags        Flags for opening the file.
 * @param[in] autoUpgrade  Whether to try to upgrade old tools configuration.
 *
 * @return A configuration dictionary, or NULL on error.
 */

GKeyFile *
VMTools_LoadConfig(const gchar *path,
                   GKeyFileFlags flags,
                   gboolean autoUpgrade)
{
   gchar *backup = NULL;
   gchar *localPath;
   GuestApp_Dict *old = NULL;
   GError *err = NULL;
   GKeyFile *cfg;

   cfg = g_key_file_new();

   localPath = VMTOOLS_GET_FILENAME_LOCAL(path, &err);
   if (err != NULL) {
      g_warning("Error converting to local encoding: %s\n", err->message);
      goto exit;
   }

   if (!File_IsFile(path)) {
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

   /* Failed to load the config file; try to upgrade if requested. */
   if (!autoUpgrade) {
      goto error;
   }

   old = Conf_Load();
   if (old == NULL) {
      g_warning("Error loading old tools config data, bailing out.\n");
      goto error;
   }

   VMToolsConfigUpgrade(old, cfg);
   backup = g_strdup_printf("%s.old", path);

   if (!File_IsFile(backup)) {
      if (!File_Rename(path, backup)) {
         g_warning("Error creating backup of old config file.\n");
         goto error;
      }
   } else {
      g_warning("Backup config exists, skipping backup.\n");
   }

   g_clear_error(&err);

   if (!VMTools_WriteConfig(path, cfg, NULL)) {
      goto error;
   }

   goto exit;

error:
   g_key_file_free(cfg);
   cfg = NULL;

exit:
   g_clear_error(&err);
   if (old != NULL) {
      GuestApp_FreeDict(old);
   }
   g_free(backup);
   VMTOOLS_RELEASE_FILENAME_LOCAL(localPath);
   return cfg;
}


/**
 * Reloads the configuration file at the given path if it has changed since the
 * given timestamp. No translation (such as in VMTools_LoadConfig()) will be
 * performed.
 *
 * @param[in]     path     Path to the config file.
 * @param[in]     flags    Flags to use when opening the file.
 * @param[in,out] config   GKeyFile object; when reloading the file, the old
 *                         config object will be destroyed.
 * @param[in,out] mtime    Last known modification time of the config file.
 *                         When the function succeeds, will contain the new
 *                         modification time read from the file.
 *
 * @return Whether the file was reloaded.
 */

gboolean
VMTools_ReloadConfig(const gchar *path,
                     GKeyFileFlags flags,
                     GKeyFile **config,
                     time_t *mtime)
{
   struct stat confStat;
   gboolean ret = FALSE;
   GKeyFile *newConfig = NULL;

   g_assert(config != NULL);
   g_assert(mtime != NULL);

   if (g_stat(path, &confStat) == -1) {
      g_warning("Failed to stat conf file: %s\n", strerror(errno));
      goto exit;
   }

   if (*mtime == 0 || confStat.st_mtime > *mtime) {
      GError *err = NULL;
      gchar *localPath;

      localPath = VMTOOLS_GET_FILENAME_LOCAL(path, &err);
      if (err != NULL) {
         g_warning("Error converting to local encoding: %s\n", err->message);
         goto exit;
      }

      newConfig = g_key_file_new();
      g_key_file_load_from_file(newConfig, localPath, flags, &err);

      if (err != NULL) {
         g_warning("Error loading conf file: %s\n", err->message);
         g_clear_error(&err);
         g_key_file_free(newConfig);
         newConfig = NULL;
      } else {
         ret = TRUE;
      }

      VMTOOLS_RELEASE_FILENAME_LOCAL(localPath);
   }

   if (newConfig != NULL) {
      if (*config != NULL) {
         g_key_file_free(*config);
      }
      *config = newConfig;
      *mtime = confStat.st_mtime;
   }

exit:
   return ret;
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
   gchar *localPath = NULL;
   FILE *out = NULL;
   GError *lerr = NULL;

   g_assert(path != NULL);
   g_assert(config != NULL);

   localPath = VMTOOLS_GET_FILENAME_LOCAL(path, &lerr);
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
   VMTOOLS_RELEASE_FILENAME_LOCAL(localPath);
   return ret;
}

