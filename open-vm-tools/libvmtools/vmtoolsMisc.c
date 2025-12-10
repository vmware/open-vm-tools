/*********************************************************
 * Copyright (c) 2012-2025 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
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
 * @file vmtoolsMisc.c
 *
 *    Convenience functions for retrieving the information about the
 *    Tools Insallation (Eg: library files location).
 */
#if defined(_WIN32)
#include "windowsu.h"
#else
#include "posix.h"
#endif
#include "vmware/tools/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <glib/gstdio.h>
#ifdef _WIN32
#include <wincrypt.h>
#include "w32Messages.h"
#endif
#include "conf.h"
#include "err.h"
#include "util.h"
#include "dictll.h"
#include "guestApp.h"

#ifndef _WIN32

/**
 * Reads the /etc/vmware-tools/config file and returns the vlaue of
 * 'libdir' property.
 *
 * @return The value of 'libdir' property. NULL if the 'libdir' property is
 * not found in the config file.
 */

gchar *
VMTools_GetLibdir(void)
{
   gchar *confDirPath = NULL;
   gchar *confFilePath = NULL;
   gchar *localPath = NULL;
   FILE *stream = NULL;
   char *libdirValue = NULL;
   char *line = NULL;
   char *name = NULL;
   char *value = NULL;

   confDirPath = GuestApp_GetConfPath();

   if (confDirPath == NULL) {
      Panic("%s: Could not get path to the configuration file.\n", __FUNCTION__);
   }

   confFilePath = g_strdup_printf("%s%sconfig", confDirPath, DIRSEPS);
   localPath = VMTOOLS_GET_FILENAME_LOCAL(confFilePath, NULL);
   if (localPath == NULL) {
      g_warning("Error converting path to local encoding.");
      goto exit;
   }

   stream = g_fopen(localPath, "r");
   if (stream == NULL) {
      g_debug("%s: Failed to open file \"%s\": %s\n",
              __FUNCTION__, confFilePath, Err_ErrString());
      goto exit;
   }

   while (DictLL_ReadLine(stream, &line, &name, &value)
          == DictLL_ReadLineSuccess) {
      if (strcmp(name, "libdir") == 0) {
         libdirValue = Util_SafeStrdup(value);
         break;
      }

      free(line);
      line = NULL;
      free(name);
      name = NULL;
      free(value);
      value = NULL;
   }

exit:

   free(line);
   line = NULL;
   free(name);
   name = NULL;
   free(value);
   value = NULL;

   if (stream != NULL && fclose(stream)) {
      g_warning("%s: Unable to close \"%s\": %s\n",
                __FUNCTION__, confFilePath, Err_ErrString());
   }

   VMTOOLS_RELEASE_FILENAME_LOCAL(localPath);
   g_free(confFilePath);
   g_free(confDirPath);

   return libdirValue;
}
#else  // _WIN32
#define POWERSHELL_GPO_REG_KEY \
        "Software\\Policies\\Microsoft\\Windows\\PowerShell"
#define POWERSHELL_GPO_EXEC_POLICY "ExecutionPolicy"
#define VMW_CERT_ENCODING (X509_ASN_ENCODING | PKCS_7_ASN_ENCODING)


/*
 ******************************************************************************
 * VMTools_IsGPOExecPolicyAllSigned --
 *
 * Check the powershell GPO machine execution policy
 *
 * @return TRUE if powershell GPO MachinePolicy is set to AllSigned.
 *         FALSE in case of error or if MachinePolicy is not AllSigned.
 *
 * Side effects:
 *      None.
 *
 ******************************************************************************
 */

gboolean
VMTools_IsGPOExecPolicyAllSigned(void)
{
   LONG ret;
   HKEY hKey;
   DWORD regSz;
   gchar buf[32];
   /* Set len to one less than sizeof(buf) to null terminate buf */
   DWORD len = sizeof(buf) - 1;

   ret = RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                       POWERSHELL_GPO_REG_KEY,
                       0,
                       KEY_READ,
                       &hKey);
   if (ret != ERROR_SUCCESS) {
      g_debug("%s: Registry open failed.\n", __FUNCTION__);
      return FALSE;
   }

   ret = RegQueryValueExA(hKey,
                          POWERSHELL_GPO_EXEC_POLICY,
                          NULL,
                          &regSz,
                          (LPBYTE)buf,
                          &len);

   RegCloseKey(hKey);

   if (ret != ERROR_SUCCESS || regSz != REG_SZ || len == 0) {
      g_debug("%s: Failed to get powershell GPO policy. ret: %d\n",
              __FUNCTION__,
              ret);
      return FALSE;
   }

   buf[len] = '\0';
   g_debug("%s: Powershell GPO execution policy: \"%s\"\n", __FUNCTION__, buf);

   return (g_strcmp0(buf, "AllSigned") == 0);
}


/*
 ******************************************************************************
 * VMTools_IsCertPresent --
 *
 * Checks if our certificate is present in TrustedPublisher cert store.
 *
 * @return
 *      TRUE if certificate is found.
 *      FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 ******************************************************************************
 */

gboolean
VMTools_IsCertPresent()
{
   HCERTSTORE trustedPublisher;
   static const wchar_t *certStoreW = L"TrustedPublisher";
   static const wchar_t *certSubjectW = L"Broadcom Inc";
   PCCERT_CONTEXT certContext;

   trustedPublisher = CertOpenStore(CERT_STORE_PROV_SYSTEM_W,
                                    0,
                                    (HCRYPTPROV)NULL,
                                    CERT_SYSTEM_STORE_LOCAL_MACHINE |
                                    CERT_STORE_OPEN_EXISTING_FLAG,
                                    certStoreW);

   if (trustedPublisher == NULL) {
      g_debug("%s Failed to open TrustedPublisher store.\n", __FUNCTION__);
      return FALSE;
   }

   certContext = CertFindCertificateInStore(trustedPublisher,
                                            VMW_CERT_ENCODING,
                                            0,
                                            CERT_FIND_SUBJECT_STR,
                                            certSubjectW,
                                            NULL);

   if (certContext == NULL) {
      g_debug("%s: No certificate found.\n", __FUNCTION__);
      CertCloseStore(trustedPublisher, 0);
      return FALSE;
   }

   CertFreeCertificateContext(certContext);
   CertCloseStore(trustedPublisher, 0);

   g_debug("%s: A valid certificate found.\n", __FUNCTION__);
   return TRUE;
}


/*
 ******************************************************************************
 * VMTools_LogWinEvent --
 *
 * Logs a windows event for message.
 *
 * @param[in] msg message to be logged in windows event.
 *
 * @return
 *      None.
 *
 * Side effects:
 *      None.
 *
 ******************************************************************************
 */

void
VMTools_LogWinEvent(const gchar *msg)
{
   HANDLE h;
   wchar_t *msgBufW;

   h = RegisterEventSourceW(NULL, L"VMware Tools");
   if (h == NULL) {
      g_debug("%s: Register windows event failed.\n", __FUNCTION__);
      return;
   }

   msgBufW = (wchar_t *)g_utf8_to_utf16(msg, -1, NULL, NULL, NULL);
   if (msgBufW == NULL) {
      g_warning("%s: Error converting \"%s\".\n", __FUNCTION__, msg);
      goto done;
   }

   ReportEventW(h,
                EVENTLOG_INFORMATION_TYPE,
                0,                        // category
                VMTOOLS_EVENT_LOG_MESSAGE,// Event ID
                NULL,                     // user
                1,                        // numStrings
                0,                        // data size
                &msgBufW,                 // string array
                NULL);                    // any binary data

   g_free(msgBufW);

done:
   DeregisterEventSource(h);
}


/**
 * Gets error message for the last error.
 *
 * @param[in]  error    Error code to be converted to string message.
 *
 * @return The error message, or NULL in case of failure.
 */

static char *
VMToolsGetLastErrorMsg(DWORD error)
{
   char *msg = Win32U_FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
                                    FORMAT_MESSAGE_IGNORE_INSERTS,
                                    NULL,
                                    error,
                                    0,       // Default language
                                    NULL);
   if (msg == NULL) {
      g_warning("Failed to get error message for %d, error=%d.\n",
                error, GetLastError());
      return NULL;
   }

   return msg;
}

#endif // _WIN32


/**
 * Gets an environment variable for the current process.
 *
 * @param[in]  name       Name of the env variable.
 *
 * @return The value of env variable, or NULL in case of error.
 *         The caller is expected to free the returned value.
 */

static gchar *
VMToolsEnvGetVar(const char *name)      // IN
{
   gchar *value;

#if defined(_WIN32)
   DWORD valueSize;
   /*
    * Win32U_GetEnvironmentVariable requires buffer to be accurate size.
    * So, we need to get the value size first.
    *
    * Windows bug: GetEnvironmentVariable() does not clear stale
    * error when the return value is 0 because of env variable
    * holding empty string value (just NUL-char). So, we need to
    * clear it before we call the Win32 API.
    */
   SetLastError(ERROR_SUCCESS);
   valueSize = Win32U_GetEnvironmentVariable(name, NULL, 0);
   if (valueSize == 0) {
      goto error;
   }

   value = g_malloc(valueSize);
   SetLastError(ERROR_SUCCESS);
   if (Win32U_GetEnvironmentVariable(name, value, valueSize) == 0) {
      g_free(value);
      goto error;
   }

   return value;

error:
{
   DWORD error = GetLastError();
   if (error == ERROR_SUCCESS) {
      g_message("Env variable %s is empty.\n", name);
   } else if (error == ERROR_ENVVAR_NOT_FOUND) {
      g_message("Env variable %s not found.\n", name);
   } else {
      char *errorMsg = VMToolsGetLastErrorMsg(error);
      if (errorMsg != NULL) {
         g_warning("Failed to get env variable size %s, error=%s.\n",
                   name, errorMsg);
         free(errorMsg);
      } else {
         g_warning("Failed to get env variable size %s, error=%d.\n",
                   name, error);
      }
   }
   return NULL;
}
#else
   value = Posix_Getenv(name);
   return value == NULL ? value : g_strdup(value);
#endif
}


/**
 * Sets an environment variable for the current process.
 *
 * @param[in]  name       Name of the env variable.
 * @param[in]  value      Value for the env variable.
 *
 * @return gboolean, TRUE on success or FALSE in case of error.
 */

static gboolean
VMToolsEnvSetVar(const char *name,      // IN
                 const char *value)     // IN
{
#if defined(_WIN32)
   if (!Win32U_SetEnvironmentVariable(name, value)) {
      char *errorMsg;
      DWORD error = GetLastError();

      errorMsg = VMToolsGetLastErrorMsg(error);
      if (errorMsg != NULL) {
         g_warning("Failed to set env variable %s=%s, error=%s.\n",
                   name, value, errorMsg);
         free(errorMsg);
      } else {
         g_warning("Failed to set env variable %s=%s, error=%d.\n",
                   name, value, error);
      }
      return FALSE;
   }
#else
   if (Posix_Setenv(name, value, TRUE) != 0) {
      g_warning("Failed to set env variable %s=%s, error=%s.\n",
                name, value, strerror(errno));
      return FALSE;
   }
#endif
   return TRUE;
}

/**
 * Unsets an environment variable for the current process.
 *
 * @param[in]  name       Name of the env variable.
 *
 * @return gboolean, TRUE on success or FALSE in case of error.
 */

static gboolean
VMToolsEnvUnsetVar(const char *name)    // IN
{
#if defined(_WIN32)
   if (!Win32U_SetEnvironmentVariable(name, NULL)) {
      char *errorMsg;
      DWORD error = GetLastError();

      errorMsg = VMToolsGetLastErrorMsg(error);
      if (errorMsg != NULL) {
         g_warning("Failed to unset env variable %s, error=%s.\n",
                   name, errorMsg);
         free(errorMsg);
      } else {
         g_warning("Failed to unset env variable %s, error=%d.\n",
                   name, error);
      }
      return FALSE;
   }
#else
   if (Posix_Unsetenv(name) != 0) {
      g_warning("Failed to unset env variable %s, error=%s.\n",
                name, strerror(errno));
      return FALSE;
   }
#endif
   return TRUE;
}


/**
 * Setup environment variables for the current process from
 * a given config group.
 *
 * @param[in]  appName    app name.
 * @param[in]  config     config dictionary.
 * @param[in]  globalVars global variables.
 * @param[in]  group      Configuration group to be read.
 * @param[in]  doUnset    Whether to unset the environment vars.
 */

static void
VMToolsInitEnvGroup(const gchar *appName,// IN
                    GKeyFile *config,    // IN
                    gboolean globalVars, // IN
                    const gchar *group,  // IN
                    gboolean doUnset)    // IN
{
   gsize i;
   gsize length;
   GError *err = NULL;
   gchar **keys = g_key_file_get_keys(config, group, &length, &err);
   if (keys == NULL) {
      if (err != NULL) {
         if (err->code != G_KEY_FILE_ERROR_GROUP_NOT_FOUND) {
            g_warning("Failed to get keys for config group %s (err=%d).\n",
                      group, err->code);
         }
         g_clear_error(&err);
      } else {
         g_warning("Failed to get keys for config group %s (err=unknown).\n",
                   group);
      }
      g_info("Skipping environment initialization for %s from %s config.\n",
             appName, group);
      return;
   }

   g_info("Found %"FMTSZ"d environment variable(s) in %s config.\n",
          length, group);

   /*
    * Following 2 formats are supported:
    * 1. <variableName> = <value>
    * 2. <serviceName>.<variableName> = <value>
    *
    * Variables specified in format #1 are applied to all services and
    * variables specified in format #2 are applied to specified service only.
    *
    * When the globalVars is TRUE, only format #1 settings will be applied.
    * All service-specific, format #2, settings will be skipped.
    * When the globalVars is FALSE, only format #2 settings will be applied.
    * All global, format #1, settings will be skipped.
    *
    * Services and applications requiring both formats to be applied must
    * call the VMTools_SetupEnv twice with globalVars TRUE, then FALSE.
    * These are processed separately to allow the global variables to be
    * set very early on in the process start, to fix GLib, when the service
    * name is unknown.
    */
   for (i = 0; i < length; i++) {
      const gchar *name = NULL;
      const gchar *key = keys[i];
      const gchar *delim;

      /*
       * Pick the keys that have service name prefix or no prefix.
       */
      delim = strchr(key, '.');
      if (globalVars) {
         if (delim == NULL) {
            name = key;
         }
      } else if (delim != NULL) {
         if (strncmp(key, appName, delim - key) == 0) {
            name = delim + 1;
         }
      }

      /*
       * Ignore entries with empty env variable names.
       */
      if (name != NULL && *name != '\0') {
         gchar *oldValue = VMToolsEnvGetVar(name);
         if (doUnset) {
            /*
             * We can't avoid duplicate removals, but removing a non-existing
             * environment variable is a no-op anyway.
             */
            if (VMToolsEnvUnsetVar(name)) {
               g_message("Removed env var %s=[%s]\n",
                         name, oldValue == NULL ? "(null)" : oldValue);
            }
         } else {
            gchar *value = VMTools_ConfigGetString(config, group,
                                                   key, NULL);
            if (value != NULL) {
               /*
                * Get rid of trailing space.
                */
               g_strchomp(value);

               /*
                * Avoid updating environment var if it is already set to
                * the same value.
                *
                * Also, g_key_file_get_keys() does not filter out duplicates
                * but, VMTools_ConfigGetString returns only last entry
                * for the key. So, by comparing old value, we avoid setting
                * the environment multiple times when there are duplicates.
                *
                * NOTE: Need to use g_strcmp0 because oldValue can be NULL.
                * As value can't be NULL but oldValue can be NULL, we might
                * still do an unnecessary update in cases like setting a
                * variable to empty/no value twice. However, it does not harm
                * and is not worth avoiding it.
                */
               if (g_strcmp0(oldValue, value) == 0) {
                  g_info("Env var %s already set to [%s], skipping.\n",
                         name, oldValue);
                  g_free(oldValue);
                  g_free(value);
                  continue;
               }
               g_debug("Changing env var %s from [%s] -> [%s]\n",
                       name, oldValue == NULL ? "(null)" : oldValue, value);
               if (VMToolsEnvSetVar(name, value)) {
                  g_message("Updated env var %s from [%s] -> [%s]\n",
                            name, oldValue == NULL ? "(null)" : oldValue,
                            value);
               }
               g_free(value);
            }
         }
         g_free(oldValue);
      }
   }

   g_info("Initialized environment for %s from %s config.\n",
          appName, group);
   g_strfreev(keys);
}


/**
 * Setup environment variables for the current process.
 *
 * @param[in]  appName    app name.
 * @param[in]  config     config dictionary.
 * @param[in]  globalVars global variables.
 */

static void
VMToolsInitEnv(const gchar *appName,
               GKeyFile *config,
               gboolean globalVars)
{
   /*
    * First apply unset environment configuration to start clean.
    */
   VMToolsInitEnvGroup(appName,
                       config,
                       globalVars,
                       CONFGROUPNAME_UNSET_ENVIRONMENT,
                       TRUE);
   VMToolsInitEnvGroup(appName,
                       config,
                       globalVars,
                       CONFGROUPNAME_SET_ENVIRONMENT,
                       FALSE);
}


/**
 * Setup environment variables for the current process.
 *
 * @param[in]  appName    app name.
 * @param[in]  config     config dictionary.
 * @param[in]  globalVars global variables.
 */

void
VMTools_SetupEnv(const gchar *appName,
                 GKeyFile *config,
                 gboolean globalVars)
{
   /* Initialize the environment from config. */
   VMToolsInitEnv(appName, config, globalVars);
}
