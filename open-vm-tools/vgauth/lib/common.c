/*********************************************************
 * Copyright (C) 2011-2017 VMware, Inc. All rights reserved.
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


/*
 * @file common.c
 *
 * Common client functionality
 */


#include "VGAuthCommon.h"
#include "VGAuthInt.h"
#include "buildNumber.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef _WIN32
#define  PAM_DIRECTORY  "/etc/pam.d"
#include <errno.h>
#endif

PrefHandle gPrefs = NULL;


/*
 ******************************************************************************
 * VGAuthValidateExtraParamsImpl --                                      */ /**
 *
 * Checks that the number of elements is sane and that all the keys and
 * values are valid.
 *
 * @param[in]  funcName    The name of the calling function.
 * @param[in]  numParams   The number of elements in the params array.
 * @param[in]  params      The params to validate.
 *
 * @retval VGAUTH_E_INVALID_ARGUMENT If one of the extra parameters is invalid
 *                                   or the number of extra parameters is
 *                                   inconsistent with the provided array.
 * @reval VGAUTH_E_OK If the extra parameters validate successfully.
 *
 ******************************************************************************
 */

VGAuthError
VGAuthValidateExtraParamsImpl(const char *funcName,
                              int numParams,
                              const VGAuthExtraParams *params)
{
   int i;

   if ((numParams < 0) || (numParams > 0 && NULL == params)) {
      Warning("%s: invalid number of parameters: %d.\n", funcName, numParams);
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   for (i = 0; i < numParams; i++) {
      if (NULL == params[i].name) {
         Warning("%s: incomplete ExtraParam setting at index %d.\n",
                 funcName, i);
         return VGAUTH_E_INVALID_ARGUMENT;
      }
      if (!g_utf8_validate(params[i].name, -1, NULL) ||
          ((params[i].value != NULL) &&
           !g_utf8_validate(params[i].value, -1, NULL))) {
         Warning("%s: non-UTF-8 parameter at index %d.\n",funcName, i);
         return VGAUTH_E_INVALID_ARGUMENT;
      }
   }

   return VGAUTH_E_OK;
}


/*
 ******************************************************************************
 * VGAuthGetBoolExtraParamImpl --                                        */ /**
 *
 * Get the boolean value of the specified extra param in the params array.
 *
 * @param[in]  funcName    The name of the calling function.
 * @param[in]  numParams   The number of elements in the params array.
 * @param[in]  params      The params array to get param value from.
 * @param[in]  paramName   The param name to get its value.
 * @param[in]  defValue    The param default value if not set in the array.
 * @param[out] paramValue  Returned param value, TRUE or FALSE.
 *
 * @retval VGAUTH_E_INVALID_ARGUMENT If incomplete arguments are passed in,
 *                                   the specified extra parameter is passed
 *                                   in the array multiple times or the
 *                                   parameter value is invalid.
 * @reval VGAUTH_E_OK If no error is encountered.
 *
 ******************************************************************************
 */

VGAuthError
VGAuthGetBoolExtraParamImpl(const char *funcName,
                            int numParams,
                            const VGAuthExtraParams *params,
                            const char *paramName,
                            gboolean defValue,
                            gboolean *paramValue)
{
   gboolean paramSet = FALSE;
   int i;

   if ((numParams < 0) || (numParams > 0 && NULL == params)) {
      Warning("%s: invalid number of parameters: %d.\n", funcName, numParams);
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   if (NULL == paramName || NULL == paramValue) {
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   *paramValue = defValue;

   for (i = 0; i < numParams; i++) {
      if (g_strcmp0(params[i].name, paramName) == 0) {
         // only allow it to be set once
         if (paramSet) {
            Warning("%s: extraParam '%s' passed multiple times.\n",
                    funcName, params[i].name);
            return VGAUTH_E_INVALID_ARGUMENT;
         }
         if (params[i].value) {
            if (g_ascii_strcasecmp(VGAUTH_PARAM_VALUE_TRUE,
                                   params[i].value) == 0) {
               *paramValue = TRUE;
               paramSet = TRUE;
            } else if (g_ascii_strcasecmp(VGAUTH_PARAM_VALUE_FALSE,
                                          params[i].value) == 0) {
               *paramValue = FALSE;
               paramSet = TRUE;
            } else {
               Warning("%s: Unrecognized value '%s' for boolean param %s\n",
                       funcName, params[i].value, params[i].name);
               return VGAUTH_E_INVALID_ARGUMENT;
            }
         } else {
            return VGAUTH_E_INVALID_ARGUMENT;
         }
      }
   }

   return VGAUTH_E_OK;
}


/*
 ******************************************************************************
 * VGAuth_Init --                                                        */ /**
 *
 * @brief Initializes the library, and specifies any configuration information.
 *
 * @a applicationName is the name of the application (argv[0]), and is needed
 * on Posix operating systems to initialize @b pam(3).
 *
 * @remark Can be called by any user.
 *
 * @param[in]  applicationName   The name of the application.
 * @param[in]  numExtraParams    The number of elements in extraParams.
 * @param[in]  extraParams       Any optional, additional paramaters to the
 *                               function. Currently none are supported, so
 *                               this must be NULL.
 * @param[out] ctx               The new VGAuthContext.
 *
 * @retval VGAUTH_E_INVALID_ARGUMENT For a bad argument.
 * @retval VGAUTH_E_OUT_OF_MEMORY For an out-of-memory failure.
 * @return VGAUTH_E_OK on success, VGAuthError on failure.
 *
 ******************************************************************************
 */

VGAuthError
VGAuth_Init(const char *applicationName,
            int numExtraParams,
            const VGAuthExtraParams *extraParams,
            VGAuthContext **ctx)
{
   VGAuthContext *newCtx = NULL;
   VGAuthError err = VGAUTH_E_OK;
   static gboolean firstTime = TRUE;
   int i;

   /*
    * The application name cannot be an empty string.
    */
   if ((NULL == applicationName) || ('\0' == *applicationName) ||
       (NULL == ctx)) {
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   *ctx = NULL;

   /* XXX process any options */

   if (!g_utf8_validate(applicationName, -1, NULL)) {
      Warning("%s: invalid applicationName\n", __FUNCTION__);
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   err = VGAuthValidateExtraParams(numExtraParams, extraParams);
   if (VGAUTH_E_OK != err) {
      return err;
   }

   newCtx = g_malloc0(sizeof(VGAuthContext));
   if (NULL == newCtx) {
      return VGAUTH_E_OUT_OF_MEMORY;
   }

   newCtx->applicationName = g_strdup(applicationName);
   newCtx->isImpersonating = FALSE;
   newCtx->impersonatedUser = NULL;

   /*
    * Only init prefs, i18n and auditing once.
    */
   if (firstTime) {
      gboolean logSuccessAudits;
      gchar *msgCatalog;

      gPrefs = Pref_Init(VGAUTH_PREF_CONFIG_FILENAME);
      logSuccessAudits = Pref_GetBool(gPrefs,
                                      VGAUTH_PREF_AUDIT_SUCCESS,
                                      VGAUTH_PREF_GROUP_NAME_AUDIT,
                                      TRUE);
      msgCatalog = Pref_GetString(gPrefs,
                                  VGAUTH_PREF_LOCALIZATION_DIR,
                                  VGAUTH_PREF_GROUP_NAME_LOCALIZATION,
                                  VGAUTH_PREF_DEFAULT_LOCALIZATION_CATALOG);

      I18n_BindTextDomain(VMW_TEXT_DOMAIN, NULL, msgCatalog);
      g_free(msgCatalog);
      Audit_Init("VGAuth", logSuccessAudits);

      firstTime = FALSE;
   }

   newCtx->numExtraParams = numExtraParams;
   newCtx->extraParams = g_malloc0(sizeof(*newCtx->extraParams) *
                                   numExtraParams);
   for (i = 0; i < numExtraParams; i++) {
      newCtx->extraParams[i].name = g_strdup(extraParams[i].name);
      newCtx->extraParams[i].value = g_strdup(extraParams[i].value);
   }

   err = VGAuth_InitConnection(newCtx);
   if (VGAUTH_E_OK != err) {
      return err;
   }

   err = VGAuthInitAuthentication(newCtx);
   if (VGAUTH_E_OK != err) {
      return err;
   }

   *ctx = newCtx;

   Log("VGAuth '%s' initialized for application '%s'.  Context created at %p\n",
       BUILD_NUMBER,
       newCtx->applicationName, newCtx);

   return err;
}


/*
 ******************************************************************************
 * VGAuth_Shutdown --                                                    */ /**
 *
 * @brief Cleans up a context and any associated data.
 *
 * @remark Can be called by any user.
 *
 * @param[in]  ctx        The VGAuthContext.
 *
 * @retval VGAUTH_E_INVALID_ARGUMENT If @a ctx is NULL.
 * @return VGAUTH_E_OK on success, VGAuthError on failure.
 *
 ******************************************************************************
 */

VGAuthError
VGAuth_Shutdown(VGAuthContext *ctx)
{
   int i;

   if (NULL == ctx) {
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   VGAuthShutdownAuthentication(ctx);

   VGAuth_CloseConnection(ctx);

   for (i = 0; i < ctx->numExtraParams; i++) {
      g_free(ctx->extraParams[i].name);
      g_free(ctx->extraParams[i].value);
   }
   g_free(ctx->extraParams);

   Log("VGAuth context at %p shutdown for application '%s'\n",
       ctx, ctx->applicationName);
   g_free(ctx->applicationName);
   g_free(ctx);

   return VGAUTH_E_OK;
}


/*
 ******************************************************************************
 * VGAuth_InstallClient --                                               */ /**
 *
 * @brief Provides any OS specific support that may be required:
 * system config entries, registry tweaks, etc.
 *
 * Note that on Posix, PAM configuration files are case-insensitive.  The
 * application name will be lower-cased to create a PAM configuration
 * filename.
 *
 * Note that there can be issues running 32 bit code in a 64 bit OS.
 * On at least one tested system, a 32 bit test on a 64 bit OS failed
 * to load PAM modules with ELF errors.  Users should always try to match
 * the native OS.  The vgauth installer should enforce this.
 *
 * @remark Must be called by root.
 *
 * @param[in]  ctx               The VGAuthContext.
 * @param[in]  numExtraParams    The number of elements in extraParams.
 * @param[in]  extraParams       Any optional, additional paramaters to the
 *                               function. Currently none are supported, so
 *                               this must be NULL.
 *
 * @retval VGAUTH_E_PERMISSION_DENIED If not called as root.
 * @retval VGAUTH_ERROR_SET_SYSTEM_ERRNO If a syscall fails.  Use
 *    VGAUTH_ERROR_EXTRA_ERROR on the return value to get the errno.
 * @retval VGAUTH_E_INVALID_ARGUMENT If @a ctx is NULL or one of the extra
 *                                   parameters is invalid.
 * @return VGAUTH_E_OK on success, VGAuthError on failure.
 *
 ******************************************************************************
 */

VGAuthError
VGAuth_InstallClient(VGAuthContext *ctx,
                     int numExtraParams,
                     const VGAuthExtraParams *extraParams)
{
   VGAuthError err;

   if (NULL == ctx) {
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   err = VGAuthValidateExtraParams(numExtraParams, extraParams);
   if (VGAUTH_E_OK != err) {
      return err;
   }

#ifdef _WIN32
   return VGAUTH_E_OK;
#elif defined(__linux__)
   {
   gchar *fileName;
   gchar *lowAppName;
   FILE *fp;
   /*
    * XXX
    *
    * This has worked for currently tested distros, but could
    * be improved.  I stole it from the tools installer, but they've
    * since improved it further to use 'include' statements, and do different
    * things depending on the distro.  It'd also be nice to somehow
    * share code with the installer.  See bug 889444.
    */

   static char *fileContents =
"#%PAM-1.0\n"
"# \n"
"# This file was generated by vgauth\n"
"# \n"
"auth           sufficient   pam_unix2.so shadow\n"
"auth           sufficient   pam_unix.so shadow\n"
"auth           required     pam_unix_auth.so shadow\n"
"account        sufficient   pam_unix2.so\n"
"account        sufficient   pam_unix.so\n"
"account        required     pam_unix_auth.so\n";

   if (!VGAuth_IsRunningAsRoot()) {
      return VGAUTH_E_PERMISSION_DENIED;
   }

   /*
    * PAM will convert a mixed-case application name into all lower case,
    * so make the lowercase version of the appName.
    */
   lowAppName = g_ascii_strdown(ctx->applicationName, -1);

   fileName = g_strdup_printf(PAM_DIRECTORY"/%s", lowAppName);

   /*
    * XXX add NO_CLOBBER check to catch some app that already has the same name?
    * Some concern that we can't do anything about it on Windows.
    */

   fp = g_fopen(fileName, "w+");
   if (NULL == fp) {
      VGAUTH_ERROR_SET_SYSTEM_ERRNO(err, errno);
      Warning("%s: Unable to open PAM config file %s for creation\n",
              __FUNCTION__, fileName);
      goto done;
   }
   if (g_fprintf(fp, "%s", fileContents) < 0) {
      VGAUTH_ERROR_SET_SYSTEM_ERRNO(err, errno);
      Warning("%s: Unable to fprintf() PAM config file contents\n",
              __FUNCTION__);
      goto done;
   }
   err = VGAUTH_E_OK;
done:
   if (fp != NULL) {
      if (fclose(fp) != 0) {
         if (err == VGAUTH_E_OK) {
            VGAUTH_ERROR_SET_SYSTEM_ERRNO(err, errno);
         }
         Warning("%s: Unable to close PAM config file\n", __FUNCTION__);
      }
   }
   g_free(fileName);
   g_free(lowAppName);

   return err;
   }
#elif defined(sun)
   return VGAUTH_E_OK;
#else
#error VGAuth_InstallClient unsupported on this platform.
#endif
}


/*
 ******************************************************************************
 * VGAuth_UninstallClient --                                             */ /**
 *
 * @brief Removes any OS specific support that may be required:
 * system config entries, registry tweaks, etc.
 *
 * @remark Must be called by root.
 *
 * @param[in]  ctx               The VGAuthContext.
 * @param[in]  numExtraParams    The number of elements in extraParams.
 * @param[in]  extraParams       Any optional, additional paramaters to the
 *                               function. Currently none are supported, so
 *                               this must be NULL.
 *
 * @retval VGAUTH_E_PERMISSION_DENIED If not called as root.
 * @retval VGAUTH_ERROR_SET_SYSTEM_ERRNO If a syscall fails.  Use
 *    VGAUTH_ERROR_EXTRA_ERROR on the return value to get the @a errno.
 * @retval VGAUTH_E_INVALID_ARGUMENT If @a ctx is NULL or if one of the extra
 *                                   parameters is invalid.
 * @return VGAUTH_E_OK on success, VGAuthError on failure.
 *
 ******************************************************************************
 */

VGAuthError
VGAuth_UninstallClient(VGAuthContext *ctx,
                       int numExtraParams,
                       const VGAuthExtraParams *extraParams)
{
   VGAuthError err;

   if (NULL == ctx) {
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   err = VGAuthValidateExtraParams(numExtraParams, extraParams);
   if (VGAUTH_E_OK != err) {
      return err;
   }

#ifdef _WIN32
   return VGAUTH_E_OK;
#elif defined(__linux__)
   {
   gchar *fileName;
   gchar *lowAppName;

   if (!VGAuth_IsRunningAsRoot()) {
      return VGAUTH_E_PERMISSION_DENIED;
   }

   /*
    * PAM will convert a mixed-case application name into all lower case,
    * so make the lowercase version of the file.
    */
   lowAppName = g_ascii_strdown(ctx->applicationName, -1);
   fileName = g_strdup_printf(PAM_DIRECTORY"/%s", lowAppName);

   if (g_unlink(fileName) != 0) {
      VGAUTH_ERROR_SET_SYSTEM_ERRNO(err, errno);
      Warning("%s: Unable to remove PAM config file '%s'\n",
              __FUNCTION__, fileName);
      goto done;
   }

   err = VGAUTH_E_OK;
done:
   g_free(fileName);
   g_free(lowAppName);
   return err;
   }
#elif defined(sun)
   return VGAUTH_E_OK;
#else
#error VGAuth_UninstallClient unsupported on this platform.
#endif   // linux
}


/*
 ******************************************************************************
 * VGAuth_SetLogHandler --                                               */ /**
 *
 * @brief Sets the global log handler.
 *
 * All VGAuth and glib errors, warnings and debug messages will go through
 * @a logFunc.
 *
 * @li VGAuth errors will use the glib loglevel @b G_LOG_LEVEL_WARNING.
 * @li VGAuth information messages will use the glib loglevel @b G_LOG_LEVEL_MESSAGE.
 * @li VGAuth debug messages will use the glib loglevel @b G_LOG_LEVEL_DEBUG.
 *
 * Note that any bad utf8 string arguments will be passed through
 * unmodified, so an error handler may want to sanity check the data.
 *
 * @remark Can be called by any user.
 *
 * @param[in]  logFunc          The function to be called for logging messages.
 * @param[in]  userData         The data to be passed to the logFunc.
 * @param[in]  numExtraParams   The number of elements in extraParams.
 * @param[in]  extraParams      Any optional, additional paramaters to the
 *                              function. Currently none are supported, so
 *                              this must be NULL.
 *
 * @retval VGAUTH_E_INVALID_ARGUMENT For a NULL @a logFunc or if one of the
 *                                   extra parameters is invalid.
 * @return VGAUTH_E_OK on success, VGAuthError on failure.
 *
 ******************************************************************************
 */

VGAuthError
VGAuth_SetLogHandler(VGAuthLogFunc logFunc,
                     void *userData,
                     int numExtraParams,
                     const VGAuthExtraParams *extraParams)
{
   VGAuthError err;

   if (NULL == logFunc) {
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   err = VGAuthValidateExtraParams(numExtraParams, extraParams);
   if (VGAUTH_E_OK != err) {
      return err;
   }

   /*
    * This makes everything using glib, no matter what log domain is
    * used, go through logFunc.
    */
   (void) g_log_set_default_handler((GLogFunc) logFunc, userData);

   return VGAUTH_E_OK;
}


/*
 ******************************************************************************
 * VGAuth_FreeBuffer --                                                  */ /**
 *
 * Frees a buffer returned from the VGAuth library.
 *
 * @param[in]  buffer  The buffer to be freed.
 *
 ******************************************************************************
 */

void
VGAuth_FreeBuffer(void *buffer)
{
   if (buffer != NULL) {
      g_free(buffer);
   }
}


/*
 ******************************************************************************
 * VGAuth_AuditEvent --                                                  */ /**
 *
 * This is a wrapper on AuditEvent to deal with the issue that an
 * app could have multiple VGAuthContexts, with differing applicationNames.
 * Rather than re-initing the Audit system each time (which can be racy
 * without adding locks), we'll just init once, then prepend the
 * applicationName to each message.
 *
 * @param[in] ctx         The VGAuthContext.
 * @param[in] isSuccess   If true, the message is a successful event.
 * @param[in] fmt         The format message for the event.
 *
 ******************************************************************************
 */

void
VGAuth_AuditEvent(VGAuthContext *ctx,
                  gboolean isSuccess,
                  const char *fmt,
                  ...)

{
   gchar *msg;
   va_list args;

   /*
    * If we ever expose a VGAuthExtraParams to toggle successful audits,
    * we'll have to look at isSuccess here and possibly drop successful
    * events.
    */
   va_start(args, fmt);
   msg = g_strdup_vprintf(fmt, args);
   Audit_Event(isSuccess, "%s: %s", ctx->applicationName, msg);
   va_end(args);

   g_free(msg);
}
