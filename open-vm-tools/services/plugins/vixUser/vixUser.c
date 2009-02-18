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
 * @file vixUser.c
 *
 * Tools Service entry point for the VIX userd plugin.
 */

#define G_LOG_DOMAIN "vixUser"

#include <string.h>

#include "vmware.h"
#include "err.h"
#include "guestApp.h"
#include "printer.h"
#include "str.h"
#include "strutil.h"
#include "vixCommands.h"
#include "vixTools.h"
#include "vmtools.h"
#include "vmtoolsApp.h"
#include "util.h"

#if defined(_WIN32)
#  include "win32u.h"
#endif

#if !defined(__APPLE__)
#include "embed_version.h"
#include "vmtoolsd_version.h"
VM_EMBED_VERSION(VMTOOLSD_VERSION_STRING);
#endif

/*
 * These constants are a bad hack. I really should generate the result
 * strings twice, once to compute the length and then allocate the buffer,
 * and a second time to write the buffer. (XXX: From foundryToolsDaemon.c.)
 */
#define DEFAULT_RESULT_MSG_MAX_LENGTH     1024


/**
 * Extract a quoted string from the middle of an argument string.
 * This is different than normal tokenizing in a few ways:
 *   - Whitespace is a separator outside quotes, but not inside quotes.
 *   - Quotes always come in pairs, so "" is am empty string. An empty
 *     string may appear anywhere in the string, even at the end, so
 *     a string that is "" contains 1 empty string, not 2.
 *   - The string may use whitespace to separate the op-name from the params,
 *     and then quoted params to skip whitespace inside a param.
 *
 * XXX: this is copied from foundryToolsDaemon.c. It's easier to copy than
 * to recompile that file and pull in all its dependencies into this plugin.
 *
 * @param[in]  args
 * @param[in]  engOfArg
 *
 * @return An allocated string.
 */

static char *
ToolsDaemonTcloGetQuotedString(const char *args,
                               const char **endOfArg)
{
   char *resultStr = NULL;
   char *endStr;
   g_debug(">ToolsDaemonTcloGetQuotedString\n");

   while ((*args) && ('\"' != *args)) {
      args++;
   }
   if ('\"' == *args) {
      args++;
   }

   resultStr = Util_SafeStrdup(args);

   endStr = resultStr;
   while (*endStr) {
      if (('\\' == *endStr) && (*(endStr + 1))) {
         endStr += 2;
      } else if ('\"' == *endStr) {
         *endStr = 0;
         endStr++;
         break;
      } else {
         endStr++;
      }
   }

   if (NULL != endOfArg) {
      args += (endStr - resultStr);
      while (' ' == *args) {
         args++;
      }
      *endOfArg = args;
   }

   g_debug("<ToolsDaemonTcloGetQuotedString\n");
   return resultStr;
}


/**
 * This is a wrapper for ToolsDaemonTcloGetQuotedString.
 * It just decodes the string.
 *
 * XXX: this is copied from foundryToolsDaemon.c. It's easier to copy than
 * to recompile that file and pull in all its dependencies into this plugin.
 *
 * @param[in]  args
 * @param[in]  engOfArg
 *
 * @return An allocated string.
 */

static char *
ToolsDaemonTcloGetEncodedQuotedString(const char *args,
                                      const char **endOfArg)
{
   char *rawResultStr = NULL;
   char *resultStr = NULL;

   rawResultStr = ToolsDaemonTcloGetQuotedString(args, endOfArg);
   if (NULL == rawResultStr) {
      return(NULL);
   }

   resultStr = VixMsg_DecodeString(rawResultStr);
   free(rawResultStr);

   return resultStr;
}


/**
 * Handle the command to open a URL in the guest.
 *
 * XXX: This code is copied from foundryToolsDaemon.c. The original code
 * should be cleaned up when the old service code is phased out.
 *
 * @param[in]  data  RPC request data.
 *
 * @return TRUE on success, FALSE on failure.
 */

static Bool
VixUserOpenUrl(RpcInData *data)
{
   static char resultBuffer[DEFAULT_RESULT_MSG_MAX_LENGTH];
   VixError err = VIX_OK;
   char *url = NULL;
   char *windowState = NULL;
   char *credentialTypeStr = NULL;
   char *obfuscatedNamePassword = NULL;
   uint32 sysError = 0;
   g_debug(">ToolsDaemonTcloOpenUrl\n");

   /*
    * Parse the arguments
    */
   url = ToolsDaemonTcloGetEncodedQuotedString(data->args, &data->args);
   windowState = ToolsDaemonTcloGetQuotedString(data->args, &data->args);
   // These parameters at the end are optional, so they may be NULL.
   credentialTypeStr = ToolsDaemonTcloGetQuotedString(data->args, &data->args);
   obfuscatedNamePassword = ToolsDaemonTcloGetQuotedString(data->args, &data->args);

   /*
    * Validate the arguments.
    */
   if ((NULL == url) || (NULL == windowState)) {
      err = VIX_E_INVALID_ARG;
      g_debug("Failed to get string args\n");
      goto abort;
   }

   g_debug("Opening URL: \"%s\"\n", url);

   /* Actually open the URL. */
   if (!GuestApp_OpenUrl(url, strcmp(windowState, "maximize") == 0)) {
      err = VIX_E_FAIL;
      g_debug("Failed to open the url \"%s\"\n", url);
      goto abort;
   }

abort:
   /*
    * All Foundry tools commands return results that start with a
    * foundry error and a guest-OS-specific error.
    */
   Str_Sprintf(resultBuffer, sizeof resultBuffer, "%"FMT64"d %d", err, sysError);
   RPCIN_SETRETVALS(data, resultBuffer, TRUE);

   /*
    * These were allocated by ToolsDaemonTcloGetQuotedString.
    */
   free(url);
   free(windowState);
   free(credentialTypeStr);
   free(obfuscatedNamePassword);

   g_debug("<ToolsDaemonTcloOpenUrl\n");
   return TRUE;
}


/**
 * Handles the command to set the printer on the guest.
 *
 * XXX: This code is copied from foundryToolsDaemon.c. The original code
 * should be cleaned up when the old service code is phased out.
 *
 * @param[in]  data  RPC request data.
 *
 * @return TRUE on success, FALSE on failure.
 */

static Bool
VixUserSetPrinter(RpcInData *data)
{
   static char resultBuffer[DEFAULT_RESULT_MSG_MAX_LENGTH];
#if defined(_WIN32)
   VixError err = VIX_OK;
   char *printerName = NULL;
   char *defaultString = NULL;
   int defaultInt;
   DWORD sysError = ERROR_SUCCESS;
   g_debug(">ToolsDaemonTcloSetPrinter\n");

   /*
    * Parse the arguments
    */
   printerName = ToolsDaemonTcloGetQuotedString(data->args, &data->args);
   defaultString = ToolsDaemonTcloGetQuotedString(data->args, &data->args);

   /*
    * Validate the arguments.
    */
   if ((NULL == printerName) || (NULL == defaultString)) {
      err = VIX_E_INVALID_ARG;
      g_debug("Failed to get string args\n");
      goto abort;
   }

   if (!StrUtil_StrToInt(&defaultInt, defaultString)) {
      err = VIX_E_INVALID_ARG;
      g_debug("Failed to convert int arg\n");
      goto abort;
   }

   g_debug("Setting printer to: \"%s\", %ssetting as default\n",
           printerName, (defaultInt != 0) ? "" : "not ");

   /* Actually set the printer. */
   if (!Printer_AddConnection(printerName, &sysError)) {
      err = VIX_E_FAIL;
      g_debug("Failed to add printer %s : %d %s\n", printerName, sysError,
              Err_Errno2String(sysError));
      goto abort;
   }

   /* Set this printer as the default if requested. */
   if (defaultInt != 0) {
      if (!Win32U_SetDefaultPrinter(printerName)) {
         /*
          * We couldn't set this printer as default. Oh well. We'll
          * still report success or failure based purely on whether
          * the actual printer add succeeded or not.
          */
         g_debug("Unable to set \"%s\" as the default printer\n",
                 printerName);
      }
   }

abort:
   /*
    * All Foundry tools commands return results that start with a
    * foundry error and a guest-OS-specific error.
    */
   Str_Sprintf(resultBuffer, sizeof resultBuffer, "%"FMT64"d %d", err, sysError);
   RPCIN_SETRETVALS(data, resultBuffer, TRUE);

   /*
    * These were allocated by ToolsDaemonTcloGetQuotedString.
    */
   free(printerName);
   free(defaultString);

   g_debug("<ToolsDaemonTcloSetPrinter\n");
   return TRUE;

#else
   Str_Sprintf(resultBuffer,
               sizeof resultBuffer,
               "%d %d 0",
               VIX_E_OP_NOT_SUPPORTED_ON_GUEST,
               Err_Errno());
   RPCIN_SETRETVALS(data, resultBuffer, TRUE);
   return TRUE;
#endif
}


/**
 * Returns the list of the plugin's capabilities.
 *
 * @param[in]  src      The source object.
 * @param[in]  ctx      The app context.
 * @param[in]  set      Unused.
 * @param[in]  data     Unused.
 *
 * @return A list of capabilities.
 */

static GArray *
VixUserCapabilities(gpointer src,
                    ToolsAppCtx *ctx,
                    gboolean set,
                    gpointer data)
{
   const ToolsAppCapability caps[] = {
      { TOOLS_CAP_OLD, "open_url", 0, 1 },
      { TOOLS_CAP_OLD, "printer_set", 0, 1 }
   };

   return VMTools_WrapArray(caps, sizeof *caps, ARRAYSIZE(caps));
}


/**
 * Returns the registration data for either the guestd or userd process.
 *
 * @param[in]  ctx   The application context.
 *
 * @return The registration data.
 */

TOOLS_MODULE_EXPORT ToolsPluginData *
ToolsOnLoad(ToolsAppCtx *ctx)
{
   static ToolsPluginData regData = {
      "vixUser",
      NULL,
      NULL
   };

   RpcChannelCallback rpcs[] = {
      { VIX_BACKDOORCOMMAND_OPEN_URL, VixUserOpenUrl, NULL, NULL, NULL, 0 },
      { VIX_BACKDOORCOMMAND_SET_GUEST_PRINTER, VixUserSetPrinter, NULL, NULL, NULL, 0 }
   };
   ToolsPluginSignalCb sigs[] = {
      { TOOLS_CORE_SIG_CAPABILITIES, VixUserCapabilities, NULL }
   };
   ToolsAppReg regs[] = {
      { TOOLS_APP_GUESTRPC, VMTools_WrapArray(rpcs, sizeof *rpcs, ARRAYSIZE(rpcs)) },
      { TOOLS_APP_SIGNALS, VMTools_WrapArray(sigs, sizeof *sigs, ARRAYSIZE(sigs)) }
   };

   regData.regs = VMTools_WrapArray(regs, sizeof *regs, ARRAYSIZE(regs));

   return &regData;
}
