/*********************************************************
 * Copyright (C) 2011-2016,2020 VMware, Inc. All rights reserved.
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
 * @file prefs.c --
 *
 *    Code to support preferences controlling the service and library.
 */

#include "VGAuthLog.h"
#include "VGAuthUtil.h"
#include "prefs.h"
#ifdef _WIN32
#include "winUtil.h"
#endif


/*
 ******************************************************************************
 * Pref_Init --                                                          */ /**
 *
 * Intializes the preferences.
 *
 * @param[in] configFilename  The name of the config file.
 *
 * @return A new PrefHandle.
 *
 ******************************************************************************
 */

PrefHandle
Pref_Init(const gchar *configFilename)
{
   GError *gErr = NULL;
   GKeyFile *keyFile;
   char *fileName = NULL;
   PrefHandle ph;

   /*
    * For Windows, check the registry for the value, and fallback to arg.
    */
#ifdef _WIN32
   fileName = WinUtil_ReadPrefsString(VGAUTH_REGISTRY_KEY,
                                      VGAUTH_REGISTRY_PREFFILE);
#endif

   if (!fileName) {
      fileName = g_strdup(configFilename);
   }
   g_message("%s: Using '%s' as preferences filepath\n",
             __FUNCTION__, fileName);

   keyFile = g_key_file_new();
   if (!g_key_file_load_from_file(keyFile,
                                  fileName,
                                  G_KEY_FILE_NONE,
                                  &gErr)) {
      g_debug("%s: g_key_file_load_from_file(%s) failed: %s\n", __FUNCTION__,
              fileName, gErr->message);
      g_error_free(gErr);
      gErr = NULL;
   }

   ph = g_malloc(sizeof(struct _PrefHandle));
   ph->fileName = fileName;
   ph->keyFile = keyFile;

   return ph;
}


/*
 ******************************************************************************
 * Pref_Shutdown --                                                      */ /**
 *
 * Closes a PrefHandle.
 *
 * @param[in] ph  The PrefHandle to close.
 *
 ******************************************************************************
 */

void
Pref_Shutdown(PrefHandle ph)
{
   g_key_file_free(ph->keyFile);
   g_free(ph->fileName);
   g_free(ph);
}


/*
 ******************************************************************************
 * Pref_GetString --                                                     */ /**
 *
 * Returns a string from the pref file.
 *
 * @param[in] ph             The handle to the preferences.
 * @param[in] prefName       The name of the pref.
 * @param[in] groupName      The config group to check.
 * @param[in] defaultValue   The default value of the pref.
 *
 * @return The value in the config file if set, otherwise defaultValue.
 *            Must be gfree'd by caller.
 *
 ******************************************************************************
 */

gchar *
Pref_GetString(PrefHandle ph,
               const gchar *prefName,
               const gchar *groupName,
               const gchar *defaultVal)
{
   gchar *retVal;
   GError *gErr = NULL;
   GKeyFile *keyFile = ph->keyFile;

   ASSERT(keyFile);
   retVal = g_key_file_get_string(keyFile,
                                  groupName,
                                  prefName,
                                  &gErr);
   if ((NULL == retVal) && (NULL != gErr)) {
      g_debug("%s: Pref_GetString(%s) failed: %s\n", __FUNCTION__,
              prefName, gErr->message);
      g_error_free(gErr);
      retVal = g_strdup(defaultVal);
   } else {
      /* Remove any trailing whitespace. */
      g_strchomp(retVal);
   }

   return retVal;
}


/*
 ******************************************************************************
 * Pref_GetInt --                                                        */ /**
 *
 * Returns an int from the pref file.
 *
 * @param[in] ph             The handle to the preferences.
 * @param[in] prefName   The name of the pref.
 * @param[in] groupName      The config group to check.
 * @param[in] defaultValue   The default value of the pref.
 *
 * @return The value in the config file if set, otherwise defaultValue.
 *
 ******************************************************************************
 */

int
Pref_GetInt(PrefHandle ph,
            const gchar *prefName,
            const gchar *groupName,
            int defaultVal)
{
   int retVal;
   GKeyFile *keyFile = ph->keyFile;
   GError *gErr = NULL;

   ASSERT(keyFile);
   retVal = g_key_file_get_integer(keyFile,
                                   groupName,
                                   prefName,
                                   &gErr);
   if ((0 == retVal) && (NULL != gErr)) {
      g_debug("%s: Pref_GetInt(%s) failed: %s\n", __FUNCTION__,
              prefName, gErr->message);
      g_error_free(gErr);
      retVal = defaultVal;
   }

   return retVal;
}


/*
 ******************************************************************************
 * Pref_GetBool --                                                       */ /**
 *
 * Returns a bool from the pref file.
 *
 * @param[in] ph             The handle to the preferences.
 * @param[in] prefName   The name of the pref.
 * @param[in] groupName      The config group to check.
 * @param[in] defaultValue   The default value of the pref.
 *
 * @return The value in the config file if set, otherwise defaultValue.
 *
 ******************************************************************************
 */

gboolean
Pref_GetBool(PrefHandle ph,
             const gchar *prefName,
             const gchar *groupName,
             gboolean defaultVal)
{
   gboolean retVal;
   GKeyFile *keyFile = ph->keyFile;
   GError *gErr = NULL;

   ASSERT(keyFile);
   retVal = g_key_file_get_boolean(keyFile,
                                   groupName,
                                   prefName,
                                   &gErr);
   if (!retVal && (NULL != gErr)) {
      g_debug("%s: Pref_GetBool(%s) failed: %s\n", __FUNCTION__,
                prefName, gErr->message);
      g_error_free(gErr);
      retVal = defaultVal;
   }

   return retVal;
}


/*
 ******************************************************************************
 * Pref_LogAllEntries --                                                 */ /**
 *
 * Logs the full contents of the prefs.  Useful for debugging.
 *
 * @param[in] ph  The PrefHandle to dunp.
 *
 ******************************************************************************
 */

void
Pref_LogAllEntries(const PrefHandle ph)
{
   gchar **groupNames = NULL;
   gsize numGroups;
   int i;
   gchar **keyNames = NULL;
   gchar *value = NULL;
   int j;
   gsize numKeys;
   GError *gErr = NULL;
   GKeyFile *keyFile = ph->keyFile;

   groupNames = g_key_file_get_groups(keyFile, &numGroups);
   g_message("%s: %d preference groups in file '%s'\n",
             __FUNCTION__, (int) numGroups, ph->fileName);
   for (i = 0; i < (int) numGroups; i++) {
      g_message("Group '%s'\n", groupNames[i]);
      keyNames = g_key_file_get_keys(keyFile,
                                     groupNames[i],
                                     &numKeys,
                                     &gErr);
      if (NULL != gErr) {
         g_warning("%s: g_key_file_get_keys(%s) failed: %s\n",
                   __FUNCTION__, groupNames[i], gErr->message);
         g_error_free(gErr);
         gErr = NULL;
         continue;
      }
      for (j = 0; j < (int) numKeys; j++) {
         value = g_key_file_get_value(keyFile, groupNames[i],
                                      keyNames[j],
                                      &gErr);
         if ((NULL == value) && (NULL != gErr)) {
            g_warning("%s: g_key_file_get_value(%s:%s) failed: %s\n",
                      __FUNCTION__, groupNames[i], keyNames[j], gErr->message);
            g_error_free(gErr);
            gErr = NULL;
            continue;
         }
         g_message("\t %s=%s\n", keyNames[j], value);
         g_free(value);
      }
      g_strfreev(keyNames);
   }
   g_message("%s: End of preferences\n", __FUNCTION__);
   g_strfreev(groupNames);
}
