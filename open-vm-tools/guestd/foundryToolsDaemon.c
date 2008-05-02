/*********************************************************
 * Copyright (C) 2003 VMware, Inc. All rights reserved.
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
 * foundryToolsDaemon.c --
 *
 *    VIX-specific TCLO cmds that are called through the backdoor
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>

#ifdef _WIN32
#include <io.h>
#else
#include <errno.h>
#include <unistd.h>
#endif

#ifdef _MSC_VER
#   include <Windows.h>
#   include <WinSock2.h>
#   include <WinSpool.h>
#   include "win32u.h"
#elif _WIN32
#   include "win95.h"
#endif

#include "vmware.h"
#include "procMgr.h"
#include "vm_version.h"
#include "vm_app.h"
#include "message.h"
#include "eventManager.h"
#include "debug.h"
#include "rpcout.h"
#include "rpcin.h"
#include "util.h"
#include "strutil.h"
#include "str.h"
#include "file.h"
#include "err.h"
#include "hostinfo.h"
#include "guest_os.h"
#include "conf.h"
#include "vixCommands.h"
#include "foundryToolsDaemon.h"
#include "printer.h"
#include "base64.h"
#include "guestInfoInt.h"
#include "syncDriver.h"
#include "hgfsServer.h"
#include "hgfs.h"
#include "system.h"
#include "codeset.h"

#ifndef __FreeBSD__
#include "netutil.h"
#endif

/* Only Win32 and Linux use impersonation functions. */
#if !defined(__FreeBSD__) && !defined(sun)
#include "impersonate.h"
#endif

#include "vixTools.h"
#include "vixOpenSource.h"

static DblLnkLst_Links *globalEventQueue;   // event queue for main event loop

#define GUESTMSG_MAX_IN_SIZE (64 * 1024) /* vmx/main/guest_msg.c */
#define MAX64_DECIMAL_DIGITS 20          /* 2^64 = 18,446,744,073,709,551,616 */

#if defined(linux) || defined(_WIN32)

# if defined(_WIN32)
#  define DECLARE_SYNCDRIVER_ERROR(name) DWORD name = ERROR_SUCCESS;
#  define SYNCDRIVERERROR ERROR_GEN_FAILURE
# else
#  define DECLARE_SYNCDRIVER_ERROR(name) int name = 0;
#  define SYNCDRIVERERROR errno
# endif

static SyncDriverHandle gSyncDriverHandle = SYNCDRIVER_INVALID_HANDLE;

static Bool ToolsDaemonSyncDriverThawCallback(void *clientData);
#endif

static char *ToolsDaemonTcloGetQuotedString(const char *args,
                                            const char **endOfArg);
  
static char * ToolsDaemonTcloGetEncodedQuotedString(const char *args,
                                                    const char **endOfArg);

Bool ToolsDaemonTcloReceiveVixCommand(char const **result,
                                      size_t *resultLen,
                                      const char *name,
                                      const char *args,
                                      size_t argsSize,
                                      void *clientData);

static Bool ToolsDaemonTcloCheckUserAccount(char const **result,
                                            size_t *resultLen,
                                            const char *name,
                                            const char *args,
                                            size_t argsSize,
                                            void *clientData);

#if !defined(N_PLAT_NLM)
static Bool ToolsDaemonHgfsImpersonated(char const **result,
                                        size_t *resultLen,
                                        const char *name,
                                        const char *args,
                                        size_t argsSize,
                                        void *clientData);
#endif

#if defined(linux) || defined(_WIN32)
static Bool ToolsDaemonTcloSyncDriverFreeze(char const **result,
                                            size_t *resultLen,
                                            const char *name,
                                            const char *args,
                                            size_t argsSize,
                                            void *clientData);

static Bool ToolsDaemonTcloSyncDriverThaw(char const **result,
                                          size_t *resultLen,
                                          const char *name,
                                          const char *args,
                                          size_t argsSize,
                                          void *clientData);
#endif

static Bool ToolsDaemonTcloMountHGFS(char const **result,
                                     size_t *resultLen,
                                     const char *name,
                                     const char *args,
                                     size_t argsSize,
                                     void *clientData);

void ToolsDaemonTcloReportProgramCompleted(const char *requestName,
                                           VixError err,
                                           int exitCode,
                                           int64 pid);

/*
 * These constants are a bad hack. I really should generate the result 
 * strings twice, once to compute the length and then allocate the buffer, 
 * and a second time to write the buffer.
 */
#define DEFAULT_RESULT_MSG_MAX_LENGTH     1024

static Bool thisProcessRunsAsRoot = FALSE;



/*
 *-----------------------------------------------------------------------------
 *
 * FoundryToolsDaemonRunProgram --
 *
 *    Run a named program on the guest.
 *
 * Return value:
 *    TRUE on success
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
FoundryToolsDaemonRunProgram(char const **result,     // OUT
                             size_t *resultLen,       // OUT
                             const char *name,        // IN
                             const char *args,        // IN
                             size_t argsSize,         // Ignored
                             void *clientData)        // IN
{
   VixError err = VIX_OK;
   char *requestName = NULL;
   char *commandLine = NULL;
   char *commandLineArgs = NULL;
   char *credentialTypeStr = NULL;
   char *obfuscatedNamePassword = NULL;
   char *directoryPath = NULL;
   char *environmentVariables = NULL;
   static char resultBuffer[DEFAULT_RESULT_MSG_MAX_LENGTH];
   Bool impersonatingVMWareUser = FALSE;
   void *userToken = NULL;
   ProcMgr_Pid pid;

   /*
    * Parse the arguments. Some of these are optional, so they
    * may be NULL.
    */
   requestName = ToolsDaemonTcloGetQuotedString(args, &args);
   commandLine = ToolsDaemonTcloGetEncodedQuotedString(args, &args);
   commandLineArgs = ToolsDaemonTcloGetEncodedQuotedString(args, &args);
   credentialTypeStr = ToolsDaemonTcloGetQuotedString(args, &args);
   obfuscatedNamePassword = ToolsDaemonTcloGetQuotedString(args, &args);
   directoryPath = ToolsDaemonTcloGetQuotedString(args, &args);
   environmentVariables = ToolsDaemonTcloGetQuotedString(args, &args);

   /*
    * Make sure we are passed the correct arguments.
    * Some of these arguments (like credentialTypeStr and obfuscatedNamePassword) are optional,
    * so they may be NULL.
    */
   if ((NULL == requestName) || (NULL == commandLine)) {
      err = VIX_E_INVALID_ARG;
      goto abort;
   }

   if ((NULL != credentialTypeStr)
         && (*credentialTypeStr) 
         && (thisProcessRunsAsRoot)) {
      impersonatingVMWareUser = VixToolsImpersonateUserImpl(credentialTypeStr, 
                                                            VIX_USER_CREDENTIAL_NONE,
                                                            obfuscatedNamePassword, 
                                                            &userToken);
      if (!impersonatingVMWareUser) {
         err = VIX_E_GUEST_USER_PERMISSIONS;
         goto abort;
      }
   }

   err = VixToolsRunProgramImpl(requestName,
                                commandLine,
                                commandLineArgs,
                                0,
                                userToken,
                                (int64 *) &pid);

abort:
   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   /*
    * All VMXI tools commands return results that start with a VMXI error
    * and a guest-OS-specific error.
    */
   Str_Sprintf(resultBuffer,
               sizeof(resultBuffer),
               "%"FMT64"d %d %"FMT64"d",
               err,
               Err_Errno(),
               (int64) pid);
   RpcIn_SetRetVals(result, resultLen, resultBuffer, TRUE);

   /*
    * These were allocated by ToolsDaemonTcloGetQuotedString.
    */
   free(requestName);
   free(commandLine);
   free(credentialTypeStr);
   free(obfuscatedNamePassword);
   free(directoryPath);
   free(environmentVariables);
   free(commandLineArgs);

   return TRUE;
} // FoundryToolsDaemonRunProgram


/*
 *-----------------------------------------------------------------------------
 *
 * FoundryToolsDaemonGetToolsProperties --
 *
 *    Get information about test features.
 *
 * Return value:
 *    TRUE on success
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
FoundryToolsDaemonGetToolsProperties(char const **result,     // OUT
                                     size_t *resultLen,       // OUT
                                     const char *name,        // IN
                                     const char *args,        // IN
                                     size_t argsSize,         // Ignored
                                     void *clientData)        // IN
{
   VixError err = VIX_OK;
   GuestApp_Dict **confDictRef;
   int additionalError = 0;
   static char resultBuffer[DEFAULT_RESULT_MSG_MAX_LENGTH];
   char *serializedBuffer = NULL;
   size_t serializedBufferLength = 0;
   char *base64Buffer = NULL;
   size_t base64BufferLength = 0;
   Bool success;
   char *returnBuffer = NULL;
   
   /*
    * Collect some values about the host.
    */
   confDictRef = (GuestApp_Dict **) clientData;

   err = VixTools_GetToolsPropertiesImpl(confDictRef,
                                         &serializedBuffer,
                                         &serializedBufferLength);
   if (VIX_OK == err) {
      base64BufferLength = Base64_EncodedLength(serializedBuffer, serializedBufferLength) + 1;
      base64Buffer = Util_SafeMalloc(base64BufferLength);
      success = Base64_Encode(serializedBuffer, 
                              serializedBufferLength, 
                              base64Buffer, 
                              base64BufferLength, 
                              &base64BufferLength);
      if (!success) {
         base64Buffer[0] = 0;
         err = VIX_E_FAIL;
         goto abort;
      }
      base64Buffer[base64BufferLength] = 0;
   }


abort:
   returnBuffer = base64Buffer;
   if (NULL == base64Buffer) {
      returnBuffer = "";
   }
   if (VIX_OK != err) {
      additionalError = Err_Errno();
   }

   /*
    * All VMXI tools commands return results that start with a VMXI error
    * and a guest-OS-specific error.
    */
   Str_Sprintf(resultBuffer,
               sizeof(resultBuffer),
               "%"FMT64"d %d %s",
               err,
               additionalError,
               returnBuffer);
   RpcIn_SetRetVals(result, resultLen, resultBuffer, TRUE);

   free(serializedBuffer);
   free(base64Buffer);
   
   return TRUE;
} // FoundryToolsDaemonGetToolsProperties


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsDaemonTcloCheckUserAccount --
 *
 *
 * Return value:
 *    TRUE on success
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
ToolsDaemonTcloCheckUserAccount(char const **result,     // OUT
                                size_t *resultLen,       // OUT
                                const char *name,        // IN
                                const char *args,        // IN
                                size_t argsSize,         // Ignored
                                void *clientData)        // IN
{
   VixError err = VIX_OK;
   char *credentialTypeStr = NULL;
   char *obfuscatedNamePassword = NULL;
   static char resultBuffer[DEFAULT_RESULT_MSG_MAX_LENGTH];
   Bool impersonatingVMWareUser = FALSE;
   void *userToken = NULL;
   Debug(">ToolsDaemonTcloCheckUserAccount\n");

   /*
    * Parse the argument
    */
   credentialTypeStr = ToolsDaemonTcloGetQuotedString(args, &args);
   obfuscatedNamePassword = ToolsDaemonTcloGetQuotedString(args, &args);

   /*
    * Make sure we are passed the correct arguments.
    */
   if ((NULL == credentialTypeStr) || (NULL == obfuscatedNamePassword)) {
      err = VIX_E_INVALID_ARG;
      goto abort;
   }

   if (thisProcessRunsAsRoot) {
      impersonatingVMWareUser = VixToolsImpersonateUserImpl(credentialTypeStr, 
                                                            VIX_USER_CREDENTIAL_NONE,
                                                            obfuscatedNamePassword, 
                                                            &userToken);
      if (!impersonatingVMWareUser) {
         err = VIX_E_GUEST_USER_PERMISSIONS;
         goto abort;
      }
   }

abort:
   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   /*
    * All VMXI tools commands return results that start with a VMXI error
    * and a guest-OS-specific error.
    */
   Str_Sprintf(resultBuffer,
               sizeof(resultBuffer),
               "%"FMT64"d %d",
               err,
               Err_Errno());
   RpcIn_SetRetVals(result, resultLen, resultBuffer, TRUE);

   /*
    * These were allocated by ToolsDaemonTcloGetQuotedString.
    */
   free(credentialTypeStr);
   free(obfuscatedNamePassword);

   return TRUE;
} // ToolsDaemonTcloCheckUserAccount


/*
 *-----------------------------------------------------------------------------
 *
 * FoundryToolsDaemon_RegisterRoutines --
 *
 *    Register the Foundry RPC callbacks
 *
 * Return value:
 *    None
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

void
FoundryToolsDaemon_RegisterRoutines(RpcIn *in,                    // IN
                                    GuestApp_Dict **confDictRef,  // IN
                                    DblLnkLst_Links *eventQueue,  // IN
                                    Bool runAsRoot)               // IN
{
   static Bool inited = FALSE;
#if defined(linux) || defined(_WIN32)
   static Bool sync_driver_inited = FALSE;
#endif

   ASSERT(in);
   ASSERT(confDictRef);
   ASSERT(*confDictRef);

   thisProcessRunsAsRoot = runAsRoot;
   globalEventQueue = eventQueue;

   (void) VixTools_Initialize(thisProcessRunsAsRoot, 
                              globalEventQueue,
                              ToolsDaemonTcloReportProgramCompleted);

#if defined(linux) || defined(_WIN32)
   /*
    * Be careful, Impersonate_Init should only be ever called once per process.
    *
    * We can get back here if the tools re-inits due to an error state,
    * such as hibernation.
    */
   if (!inited && thisProcessRunsAsRoot) {
      Impersonate_Init();
   }
#endif

   RpcIn_RegisterCallback(in,
                          VIX_BACKDOORCOMMAND_RUN_PROGRAM,
                          FoundryToolsDaemonRunProgram,
                          NULL);
   RpcIn_RegisterCallback(in,
                          VIX_BACKDOORCOMMAND_GET_PROPERTIES,
                          FoundryToolsDaemonGetToolsProperties,
                          confDictRef);
   RpcIn_RegisterCallback(in,
                          VIX_BACKDOORCOMMAND_CHECK_USER_ACCOUNT,
                          ToolsDaemonTcloCheckUserAccount,
                          NULL);
#if !defined(N_PLAT_NLM)
   RpcIn_RegisterCallback(in,
                          VIX_BACKDOORCOMMAND_SEND_HGFS_PACKET,
                          ToolsDaemonHgfsImpersonated,
                          NULL);
#endif
   RpcIn_RegisterCallback(in,
                          VIX_BACKDOORCOMMAND_COMMAND,
                          ToolsDaemonTcloReceiveVixCommand,
                          confDictRef);
   RpcIn_RegisterCallback(in,
                          VIX_BACKDOORCOMMAND_MOUNT_VOLUME_LIST,
                          ToolsDaemonTcloMountHGFS,
                          NULL);

#if defined(linux) || defined(_WIN32)

   /*
    * Only init once, but always register the RpcIn.
    */
   if (!sync_driver_inited) {
      sync_driver_inited = SyncDriver_Init();
   }
   if (sync_driver_inited) {
      /*
       * These only get registered if SyncDriver_Init succeeds. We do
       * support running the sync/thaw scripts even on guests where
       * the Sync driver is not supported (Linux, Windows older than
       * Win2k) but the running of the scripts is implemented using
       * VIX_BACKDOORCOMMAND_RUN_PROGRAM.
       */
      RpcIn_RegisterCallback(in,
                             VIX_BACKDOORCOMMAND_SYNCDRIVER_FREEZE,
                             ToolsDaemonTcloSyncDriverFreeze,
                             NULL);
      RpcIn_RegisterCallback(in,
                             VIX_BACKDOORCOMMAND_SYNCDRIVER_THAW,
                             ToolsDaemonTcloSyncDriverThaw,
                             NULL);
   } else {
      Debug("FoundryToolsDaemon: Failed to init SyncDriver, skipping command handlers.\n");
   }
#endif
   inited = TRUE;
} // FoundryToolsDaemon_RegisterRoutines


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsDaemonTcloGetQuotedString --
 *
 *    Extract a quoted string from the middle of an argument string.
 *    This is different than normal tokenizing in a few ways:
 *       * Whitespace is a separator outside quotes, but not inside quotes.
 *       * Quotes always come in pairs, so "" is am empty string. An empty
 *          string may appear anywhere in the string, even at the end, so
 *          a string that is "" contains 1 empty string, not 2.
 *       * The string may use whitespace to separate the op-name from the params,
 *          and then quoted params to skip whitespace inside a param.
 *
 * Return value:
 *    Allocates the string.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static char *
ToolsDaemonTcloGetQuotedString(const char *args,      // IN
                               const char **endOfArg) // OUT
{
   char *resultStr = NULL;
   char *endStr;
   Debug(">ToolsDaemonTcloGetQuotedString\n");

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

   Debug("<ToolsDaemonTcloGetQuotedString\n");
   return resultStr;
} // ToolsDaemonTcloGetQuotedString


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsDaemonTcloGetEncodedQuotedString --
 *
 *    This is a wrapper for ToolsDaemonTcloGetQuotedString.
 *    It just decoded the string.
 *
 * Return value:
 *    Allocates the string.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static char *
ToolsDaemonTcloGetEncodedQuotedString(const char *args,      // IN
                                      const char **endOfArg) // OUT
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


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsDaemonTcloOpenUrl --
 *
 *    Open a URL on the guest.
 *
 * Return value:
 *    TRUE on success
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
ToolsDaemonTcloOpenUrl(char const **result,     // OUT
                       size_t *resultLen,       // OUT
                       const char *name,        // IN
                       const char *args,        // IN
                       size_t argsSize,         // Ignored
                       void *clientData)        // IN
{
   static char resultBuffer[DEFAULT_RESULT_MSG_MAX_LENGTH];
   VixError err = VIX_OK;
   char *url = NULL;
   char *windowState = NULL;
   char *credentialTypeStr = NULL;
   char *obfuscatedNamePassword = NULL;
   uint32 sysError = 0;
   Bool impersonatingVMWareUser = FALSE;
   void *userToken = NULL;
   Debug(">ToolsDaemonTcloOpenUrl\n");

   /*
    * Parse the arguments
    */
   url = ToolsDaemonTcloGetEncodedQuotedString(args, &args);
   windowState = ToolsDaemonTcloGetQuotedString(args, &args);
   // These parameters at the end are optional, so they may be NULL.
   credentialTypeStr = ToolsDaemonTcloGetQuotedString(args, &args);
   obfuscatedNamePassword = ToolsDaemonTcloGetQuotedString(args, &args);

   /*
    * Validate the arguments.
    */
   if ((NULL == url) || (NULL == windowState)) {
      err = VIX_E_INVALID_ARG;
      Debug("Failed to get string args\n");
      goto abort;
   }

   if ((NULL != credentialTypeStr) 
         && (*credentialTypeStr)
         && (thisProcessRunsAsRoot)) {
      impersonatingVMWareUser = VixToolsImpersonateUserImpl(credentialTypeStr, 
                                                            VIX_USER_CREDENTIAL_NONE,
                                                            obfuscatedNamePassword, 
                                                            &userToken);
      if (!impersonatingVMWareUser) {
         err = VIX_E_GUEST_USER_PERMISSIONS;
         goto abort;
      }
   }

   Debug("Opening URL: \"%s\"\n", url);

   /* Actually open the URL. */
   if (!GuestApp_OpenUrl(url, strcmp(windowState, "maximize") == 0)) {
      err = VIX_E_FAIL;
      Debug("Failed to open the url \"%s\"\n", url);
      goto abort;
   }

abort:
   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   /*
    * All Foundry tools commands return results that start with a
    * foundry error and a guest-OS-specific error.
    */
   Str_Sprintf(resultBuffer, sizeof resultBuffer, "%"FMT64"d %d", err, sysError);
   RpcIn_SetRetVals(result, resultLen, resultBuffer, TRUE);

   /*
    * These were allocated by ToolsDaemonTcloGetQuotedString.
    */
   free(url);
   free(windowState);
   free(credentialTypeStr);
   free(obfuscatedNamePassword);

   Debug("<ToolsDaemonTcloOpenUrl\n");
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsDaemonTcloSetPrinter --
 *
 *    Set the printer on the guest.
 *
 * Return value:
 *    TRUE on success
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
ToolsDaemonTcloSetPrinter(char const **result,     // OUT
                          size_t *resultLen,       // OUT
                          const char *name,        // IN
                          const char *args,        // IN
                          size_t argsSize,         // Ignored
                          void *clientData)        // IN
{
   static char resultBuffer[DEFAULT_RESULT_MSG_MAX_LENGTH];
#if defined(_WIN32)
   VixError err = VIX_OK;
   char *printerName = NULL;
   char *defaultString = NULL;
   int defaultInt;
   DWORD sysError = ERROR_SUCCESS;
   Debug(">ToolsDaemonTcloSetPrinter\n");

   /*
    * Parse the arguments
    */
   printerName = ToolsDaemonTcloGetQuotedString(args, &args);
   defaultString = ToolsDaemonTcloGetQuotedString(args, &args);

   /*
    * Validate the arguments.
    */
   if ((NULL == printerName) || (NULL == defaultString)) {
      err = VIX_E_INVALID_ARG;
      Debug("Failed to get string args\n");
      goto abort;
   }

   if (!StrUtil_StrToInt(&defaultInt, defaultString)) {
      err = VIX_E_INVALID_ARG;
      Debug("Failed to convert int arg\n");
      goto abort;
   }

   Debug("Setting printer to: \"%s\", %ssetting as default\n",
         printerName, (defaultInt != 0) ? "" : "not ");

   /* Actually set the printer. */
   if (!Printer_AddConnection(printerName, &sysError)) {
      err = VIX_E_FAIL;
      Debug("Failed to add printer %s : %d %s\n", printerName, sysError,
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
         Debug("Unable to set \"%s\" as the default printer\n",
               printerName);
      }
   }

abort:
   /*
    * All Foundry tools commands return results that start with a
    * foundry error and a guest-OS-specific error.
    */
   Str_Sprintf(resultBuffer, sizeof resultBuffer, "%"FMT64"d %d", err, sysError);
   RpcIn_SetRetVals(result, resultLen, resultBuffer, TRUE);

   /*
    * These were allocated by ToolsDaemonTcloGetQuotedString.
    */
   free(printerName);
   free(defaultString);

   Debug("<ToolsDaemonTcloSetPrinter\n");
   return TRUE;

#else
   Str_Sprintf(resultBuffer,
               sizeof resultBuffer,
               "%d %d 0",
               VIX_E_OP_NOT_SUPPORTED_ON_GUEST,
               Err_Errno());
   RpcIn_SetRetVals(result, resultLen, resultBuffer, TRUE);
   return TRUE;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsDaemonTcloSyncDriverFreeze --
 *
 *    Use the Sync Driver to freeze I/O in the guest..
 *
 * Return value:
 *    TRUE on success
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

#if defined(linux) || defined(_WIN32)
static Bool
ToolsDaemonTcloSyncDriverFreeze(char const **result,     // OUT
                                size_t *resultLen,       // OUT
                                const char *name,        // IN
                                const char *args,        // IN
                                size_t argsSize,         // Ignored
                                void *clientData)        // IN
{
   static char resultBuffer[DEFAULT_RESULT_MSG_MAX_LENGTH];
   VixError err = VIX_OK;
   char *driveList = NULL;
   char *timeout = NULL;
   int timeoutVal;
   Event *cbEvent;
   DECLARE_SYNCDRIVER_ERROR(sysError);
   
   Debug(">ToolsDaemonTcloSyncDriverFreeze\n");

   /*
    * Parse the arguments
    */
   driveList = ToolsDaemonTcloGetQuotedString(args, &args);
   timeout = ToolsDaemonTcloGetQuotedString(args, &args);

   /*
    * Validate the arguments.
    */
   if (NULL == driveList || NULL == timeout) {
      err = VIX_E_INVALID_ARG;
      Debug("ToolsDaemonTcloSyncDriverFreeze: Failed to get string args\n");
      goto abort;
   }

   if (!StrUtil_StrToInt(&timeoutVal, timeout) || timeoutVal < 0) {
      Debug("ToolsDaemonTcloSyncDriverFreeze: Bad args, timeout '%s'\n", timeout);
      err = VIX_E_INVALID_ARG;
      goto abort;
   }

   Debug("SYNCDRIVE: Got request to freeze '%s', timeout %d\n", driveList,
         timeoutVal);

   /* Disallow multiple freeze calls. */
   if (gSyncDriverHandle != SYNCDRIVER_INVALID_HANDLE) {
      err = VIX_E_OBJECT_IS_BUSY;
      goto abort;
   }

   /* Perform the actual freeze. */
   if (!SyncDriver_Freeze(driveList, &gSyncDriverHandle) ||
       SyncDriver_QueryStatus(gSyncDriverHandle, INFINITE) != SYNCDRIVER_IDLE) {
      Debug("ToolsDaemonTcloSyncDriverFreeze: Failed to Freeze drives '%s'\n",
            driveList);
      err = VIX_E_FAIL;
      sysError = SYNCDRIVERERROR;
      if (gSyncDriverHandle != SYNCDRIVER_INVALID_HANDLE) {
         SyncDriver_Thaw(gSyncDriverHandle);
         SyncDriver_CloseHandle(&gSyncDriverHandle);
      }
      goto abort;
   }

   /* Start the timer callback to automatically thaw. */
   if (0 != timeoutVal) {
      Debug("ToolsDaemonTcloSyncDriverFreeze: Starting timer callback %d\n", timeoutVal);
      cbEvent = EventManager_Add(globalEventQueue,
                                 timeoutVal,
                                 ToolsDaemonSyncDriverThawCallback,
                                 NULL);
      if (!cbEvent) {
         Debug("ToolsDaemonTcloSyncDriverFreeze: Failed to start callback, aborting\n");
         if (!SyncDriver_Thaw(gSyncDriverHandle)) {
            Debug("ToolsDaemonTcloSyncDriverFreeze: Unable to abort freeze. Oh well.\n");
         }
         SyncDriver_CloseHandle(&gSyncDriverHandle);
         err = VIX_E_FAIL;
         sysError = SYNCDRIVERERROR;
         goto abort;
      }
   }

abort:
   /*
    * These were allocated by ToolsDaemonTcloGetQuotedString.
    */
   free(driveList);
   free(timeout);

   /*
    * All Foundry tools commands return results that start with a
    * foundry error and a guest-OS-specific error.
    */
   Str_Sprintf(resultBuffer, sizeof resultBuffer, "%"FMT64"d %d", err, sysError);
   Debug("<ToolsDaemonTcloSyncDriverFreeze\n");
   return RpcIn_SetRetVals(result, resultLen, resultBuffer, TRUE);
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsDaemonSyncDriverThawCallback --
 *
 *      Callback to thaw all currently frozen drives if they have not been
 *      thawed already.
 *
 * Results:
 *      TRUE (returning FALSE will stop the event loop)
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

#if defined(linux) || defined(_WIN32)
static Bool
ToolsDaemonSyncDriverThawCallback(void *clientData) // IN (ignored)
{
   Debug(">ToolsDaemonSyncDriverThawCallback\n");
   Debug("ToolsDaemonSyncDriverThawCallback: Timed out waiting for thaw.\n");

   /* Don't bother calling freeze if no drives are frozen. */
   if (gSyncDriverHandle == SYNCDRIVER_INVALID_HANDLE ||
       !SyncDriver_DrivesAreFrozen()) {
      Debug("<ToolsDaemonSyncDriverThawCallback\n");
      Debug("ToolsDaemonSyncDriverThawCallback: No drives are frozen.\n");
      goto exit;
   }

   if (!SyncDriver_Thaw(gSyncDriverHandle)) {
      Debug("ToolsDaemonSyncDriverThawCallback: Failed to thaw.\n");
   }

exit:
   SyncDriver_CloseHandle(&gSyncDriverHandle);
   Debug("<ToolsDaemonSyncDriverThawCallback\n");
   return TRUE;
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsDaemonTcloSyncDriverThaw --
 *
 *    Thaw I/O previously frozen by the Sync Driver.
 *
 * Return value:
 *    TRUE on success
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

#if defined(linux) || defined(_WIN32)
static Bool
ToolsDaemonTcloSyncDriverThaw(char const **result,     // OUT
                              size_t *resultLen,       // OUT
                              const char *name,        // IN
                              const char *args,        // IN
                              size_t argsSize,         // Ignored
                              void *clientData)        // IN
{
   static char resultBuffer[DEFAULT_RESULT_MSG_MAX_LENGTH];
   VixError err = VIX_OK;
   DECLARE_SYNCDRIVER_ERROR(sysError);
   Debug(">ToolsDaemonTcloSyncDriverThaw\n");

   /*
    * This function has no arguments that we care about.
    */

   Debug("SYNCDRIVE: Got request to thaw\n");

   if (gSyncDriverHandle == SYNCDRIVER_INVALID_HANDLE ||
       !SyncDriver_DrivesAreFrozen()) {
      err = VIX_E_GUEST_VOLUMES_NOT_FROZEN;
      sysError = SYNCDRIVERERROR;
      Debug("ToolsDaemonTcloSyncDriverThaw: No drives are frozen.\n");
   } else if (!SyncDriver_Thaw(gSyncDriverHandle)) {
      err = VIX_E_FAIL;
      sysError = SYNCDRIVERERROR;
      Debug("ToolsDaemonTcloSyncDriverThaw: Failed to Thaw drives\n");
   }

   SyncDriver_CloseHandle(&gSyncDriverHandle);

   /*
    * All Foundry tools commands return results that start with a
    * foundry error and a guest-OS-specific error.
    */
   Str_Sprintf(resultBuffer, sizeof resultBuffer, "%"FMT64"d %d", err, sysError);
   Debug("<ToolsDaemonTcloSyncDriverThaw\n");
   return RpcIn_SetRetVals(result, resultLen, resultBuffer, TRUE);
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * FoundryToolsDaemon_RegisterOpenUrlCapability --
 *
 *      Register the OpenUrl capability. Sometimes this needs to
 *      be done separately from the TCLO callback registration, so we
 *      provide it separately here.
 *
 * Results:
 *      TRUE on success
 *      FALSE on failure
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
FoundryToolsDaemon_RegisterOpenUrlCapability(void)
{
   if (!RpcOut_sendOne(NULL, NULL, "tools.capability.open_url 1")) {
      Debug("Unable to register open url capability\n");
      Debug("<FoundryToolsDaemon_RegisterOpenUrlCapability");
      return FALSE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FoundryToolsDaemon_RegisterOpenUrl --
 *
 *      Register the OpenUrl capability and TCLO handler.
 *
 * Results:
 *      TRUE on success
 *      FALSE on failure
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
FoundryToolsDaemon_RegisterOpenUrl(RpcIn *in) // IN
{
   /* Register the TCLO handler. */
   RpcIn_RegisterCallback(in,
                          VIX_BACKDOORCOMMAND_OPEN_URL,
                          ToolsDaemonTcloOpenUrl,
                          NULL);
   /*
    * Inform the VMX that we support opening urls in the guest; the UI
    * and VMX need to know about this capability in advance (rather than
    * simply trying and failing). I've put this here on the theory that
    * it's better to have it close to the command that handles the
    * actual request than it is to have it near the other capabilities
    * registration code, which is in toolsDaemon.c.
    *
    * Eventually, Foundry might want to have a unified way of
    * registering support for only a subset of the foundry commands in
    * the guest.
    */
   if (!FoundryToolsDaemon_RegisterOpenUrlCapability()) {
      return FALSE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FoundryToolsDaemon_UnregisterOpenUrl --
 *
 *      Unregister the "OpenUrl" capability.
 *
 * Results:
 *      TRUE on success
 *      FALSE on failure
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
FoundryToolsDaemon_UnregisterOpenUrl(void) 
{
   /*
    * RpcIn doesn't have an unregister facility, so all we need to do
    * here is unregister the capability.
    */

   /*
    * Report no longer supporting open url;
    */
   if (!RpcOut_sendOne(NULL, NULL, "tools.capability.open_url 0")) {
      Debug("<FoundryToolsDaemon_UnregisterOpenUrl\n");
      Debug("Unable to unregister OpenUrl capability\n");
      return FALSE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FoundryToolsDaemon_RegisterSetPrinterCapability --
 *
 *      Register the Set Printer capability. Sometimes this needs to
 *      be done separately from the TCLO callback registration, so we
 *      provide it separately here.
 *
 * Results:
 *      TRUE on success
 *      FALSE on failure
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
FoundryToolsDaemon_RegisterSetPrinterCapability(void)
{
   if (!RpcOut_sendOne(NULL, NULL, "tools.capability.printer_set 1")) {
      Debug("Unable to register printer set capability\n");
      Debug("<FoundryToolsDaemon_RegisterSetPrinterCapability\n");
      return FALSE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FoundryToolsDaemon_RegisterSetPrinter --
 *
 *      Register the Set Printer capability and TCLO handler.
 *
 * Results:
 *      TRUE on success
 *      FALSE on failure
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
FoundryToolsDaemon_RegisterSetPrinter(RpcIn *in) // IN
{
   /* First, try to load the printer library. */
   if (!Printer_Init()) {
      Debug("<FoundryToolsDaemon_RegisterSetPrinter\n");
      Debug("Unable to load printer library.\n");
      return FALSE;
   }

   /* Register the TCLO handler. */
   RpcIn_RegisterCallback(in,
                          VIX_BACKDOORCOMMAND_SET_GUEST_PRINTER,
                          ToolsDaemonTcloSetPrinter,
                          NULL);
   /*
    * Inform the VMX that we support setting the guest printer; the UI
    * and VMX need to know about this capability in advance (rather than
    * simply trying and failing). I've put this here on the theory that
    * it's better to have it close to the command that handles the
    * actual request than it is to have it near the other capabilities
    * registration code, which is in toolsDaemon.c.
    *
    * Eventually, Foundry might want to have a unified way of
    * registering support for only a subset of the foundry commands in
    * the guest.
    */
   if (!FoundryToolsDaemon_RegisterSetPrinterCapability()) {
      Debug("<FoundryToolsDaemon_RegisterSetPrinter\n");
      return FALSE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FoundryToolsDaemon_UnregisterSetPrinter --
 *
 *      Unregister the "Set Printer" capability.
 *
 * Results:
 *      TRUE on success
 *      FALSE on failure
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
FoundryToolsDaemon_UnregisterSetPrinter(RpcIn *in) // IN
{
   /*
    * RpcIn doesn't have an unregister facility, so all we need to do
    * here is unregister the capability.
    */

   /*
    * Report no longer supporting setting the guest printer.
    */
   if (!RpcOut_sendOne(NULL, NULL, "tools.capability.printer_set 0")) {
      Debug("<FoundryToolsDaemon_UnregisterSetPrinter\n");
      Debug("Unable to unregister printer set capability\n");
      return FALSE;
   }

   /* Cleanup the printer library. */
   if (!Printer_Cleanup()) {
      /*
       * Failed to cleanup, but we still unregistered the command so
       * we'll just warn and pretend we succeeded.
       */
      Debug("Unable to cleanup printer library\n");
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsDaemonTcloMountHGFS --
 *
 *
 * Return value:
 *    VixError
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
ToolsDaemonTcloMountHGFS(char const **result,     // OUT
                         size_t *resultLen,       // OUT
                         const char *name,        // IN
                         const char *args,        // IN
                         size_t argsSize,         // Ignored
                         void *clientData)        // IN
{
   VixError err = VIX_OK;
   static char resultBuffer[DEFAULT_RESULT_MSG_MAX_LENGTH];
   Debug(">ToolsDaemonTcloMountHGFS\n");

#if defined(linux)
   /*
    * We need to call the mount program, not the mount system call. The
    * mount program does several additional things, like compute the mount options
    * from the contents of /etc/fstab, and invoke custom mount programs like the
    * one needed for HGFS.
    */
   system("mount -a -t vmhgfs");
#endif

   /*
    * All tools commands return results that start with an error
    * and a guest-OS-specific error.
    */
   Str_Sprintf(resultBuffer,
               sizeof(resultBuffer),
               "%"FMT64"d %d",
               err,
               Err_Errno());
   RpcIn_SetRetVals(result, resultLen, resultBuffer, TRUE);

   Debug("<ToolsDaemonTcloMountHGFS\n");
   return TRUE;
} // ToolsDaemonTcloMountHGFS


#if !defined(N_PLAT_NLM)
/*
 *-----------------------------------------------------------------------------
 *
 * ToolsDaemonHgfsImpersonated --
 *
 *      Tclo cmd handler for hgfs requests.
 *
 *      Here we receive guest user credentials and an HGFS packet to
 *      be processed by the HGFS server under the context of
 *      the guest user credentials.
 *
 *      We pre-allocate a HGFS reply packet buffer and leave some space at
 *      the beginning of the buffer for foundry error codes.
 *      The format of the foundry error codes is a 64 bit number (as text),
 *      followed by a 32 bit number (as text), followed by a hash,
 *      all delimited by space (' ').  The hash is needed
 *      to make it easier for text parsers to know where the
 *      HGFS reply packet begins, since it can start with a space.
 *
 *      We do this funky "allocate an HGFS packet with extra
 *      room for foundry error codes" to avoid copying buffers
 *      around.  The HGFS packet buffer is roughly 6k, so it would
 *      be bad to copy that for every packet.
 *
 *      It is guaranteed that we will not be called twice
 *      at the same time, so it is safe for resultPacket to be static.
 *      The TCLO processing loop (RpcInLoop()) is synchronous.
 *
 *
 * Results:
 *      TRUE on TCLO success (*result contains the hgfs reply)
 *      FALSE on TCLO error (not supposed to happen)
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
ToolsDaemonHgfsImpersonated(char const **result,     // OUT
                            size_t *resultLen,       // OUT
                            const char *name,        // IN
                            const char *args,        // IN
                            size_t argsSize,         // IN: Size of args
                            void *clientData)        // Unused
{
   VixError err;
   size_t hgfsPacketSize = 0;
   const char *origArgs = args;
   Bool impersonatingVMWareUser = FALSE;
   char *credentialTypeStr = NULL;
   char *obfuscatedNamePassword = NULL;
   void *userToken = NULL;
   int actualUsed;
#define STRLEN_OF_MAX_64_BIT_NUMBER_AS_STRING 20
#define OTHER_TEXT_SIZE 4                /* strlen(space zero space quote) */
   static char resultPacket[STRLEN_OF_MAX_64_BIT_NUMBER_AS_STRING
                              + OTHER_TEXT_SIZE
                              + HGFS_PACKET_MAX];
   char *hgfsReplyPacket = resultPacket
                             + STRLEN_OF_MAX_64_BIT_NUMBER_AS_STRING
                             + OTHER_TEXT_SIZE;

   Debug(">ToolsDaemonHgfsImpersonated\n");

   err = VIX_OK;

   /*
    * We assume VixError is 64 bits.  If it changes, we need
    * to adjust STRLEN_OF_MAX_64_BIT_NUMBER_AS_STRING.
    *
    * There isn't much point trying to return gracefully
    * if sizeof(VixError) is larger than we expected: we didn't
    * allocate enough space to actually represent the error!
    * So we're stuck.  Panic at this point.
    */
   ASSERT_ON_COMPILE(sizeof (uint64) == sizeof err);

   /*
    * Get the authentication information.
    */
   credentialTypeStr = ToolsDaemonTcloGetQuotedString(args, &args);
   obfuscatedNamePassword = ToolsDaemonTcloGetQuotedString(args, &args);

   /*
    * Make sure we are passed the correct arguments.
    */
   if ((NULL == credentialTypeStr) || (NULL == obfuscatedNamePassword)) {
      err = VIX_E_INVALID_ARG;
      goto abort;
   }

   /*
    * Skip over our token that is right before the HGFS packet.
    * This makes ToolsDaemonTcloGetQuotedString parsing predictable,
    * since it will try to eat trailing spaces after a quoted string,
    * and the HGFS packet might begin with a space.
    */
   if (((args - origArgs) >= argsSize) || ('#' != *args)) {
      /*
       * Buffer too small or we got an unexpected token.
       */
      err = VIX_E_FAIL;
      goto abort;
   }
   args++;
   
   /*
    * At this point args points to the HGFS packet.
    * If we're pointing beyond the end of the buffer, we'll
    * get a negative HGFS packet length and abort.
    */
   hgfsPacketSize = argsSize - (args - origArgs);
   if (hgfsPacketSize <= 0) {
      err = VIX_E_FAIL;
      goto abort;
   }
   
   if (thisProcessRunsAsRoot) {
      impersonatingVMWareUser = VixToolsImpersonateUserImpl(credentialTypeStr,
                                                            VIX_USER_CREDENTIAL_NONE,
                                                            obfuscatedNamePassword,
                                                            &userToken);
      if (!impersonatingVMWareUser) {
         err = VIX_E_GUEST_USER_PERMISSIONS;
         hgfsPacketSize = 0;
         goto abort;
      }
   }

   /*
    * Impersonation was okay, so let's give our packet to
    * the HGFS server and forward the reply packet back.
    */
   HgfsServer_DispatchPacket(args,              // packet in buf
                             hgfsReplyPacket,   // packet out buf
                             &hgfsPacketSize);  // in/out size

abort:
   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   /*
    * These were allocated by ToolsDaemonTcloGetQuotedString.
    */
   free(credentialTypeStr);
   free(obfuscatedNamePassword);

   *result = resultPacket;
   *resultLen = STRLEN_OF_MAX_64_BIT_NUMBER_AS_STRING
                  + OTHER_TEXT_SIZE
                  + hgfsPacketSize;
   
   /*
    * Render the foundry error codes into the buffer.
    */
   actualUsed = Str_Snprintf(resultPacket,
                             STRLEN_OF_MAX_64_BIT_NUMBER_AS_STRING
                               + OTHER_TEXT_SIZE,
                             "%"FMT64"d 0 ",
                             err);
                             
   if (actualUsed < 0) {
      /*
       * We computed our string length wrong!  This should never happen.
       * But if it does, let's try to recover gracefully.  The "1" in
       * the string below is VIX_E_FAIL.  We don't try to use %d since
       * we couldn't even do that right the first time around.
       * That hash is needed for the parser on the other
       * end to stop before the HGFS packet, since the HGFS packet
       * can contain a space (and the parser can eat trailing spaces).
       */
      ASSERT(0);
      actualUsed = Str_Snprintf(resultPacket,
                                STRLEN_OF_MAX_64_BIT_NUMBER_AS_STRING,
                                "1 0 #");
      *resultLen = actualUsed;
   } else {
      /*
       * We computed the string length correctly.  Great!
       *
       * We allocated enough space to cover a large 64 bit number
       * for VixError.  Chances are we didn't use all that space.
       * Instead, pad it with whitespace so the text parser can skip
       * over it.
       */
      memset(resultPacket + actualUsed,
             ' ',
             STRLEN_OF_MAX_64_BIT_NUMBER_AS_STRING
                                 + OTHER_TEXT_SIZE
                                 - actualUsed);   
      /*
       * Put a hash right before the HGFS packet.
       * So the buffer will look something like this:
       * "0 0                        #" followed by the HGFS packet.
       */
      resultPacket[STRLEN_OF_MAX_64_BIT_NUMBER_AS_STRING
                    + OTHER_TEXT_SIZE - 1] = '#';
   }

   Debug("<<<ToolsDaemonHgfsImpersonated\n");
   return TRUE;
} // ToolsDaemonHgfsImpersonated

#undef STRLEN_OF_MAX_64_BIT_NUMBER_AS_STRING
#undef OTHER_TEXT_SIZE
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsDaemonTcloReportProgramCompleted --
 *
 *
 * Return value:
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

void
ToolsDaemonTcloReportProgramCompleted(const char *requestName,    // IN
                                      VixError err,               // IN
                                      int exitCode,               // IN
                                      int64 pid)                  // IN
{
   Bool sentResult;

   sentResult = RpcOut_sendOne(NULL,
                               NULL,
                               "%s %s %"FMT64"d %d %d %"FMT64"d",
                               VIX_BACKDOORCOMMAND_RUN_PROGRAM_DONE,
                               requestName,
                               err,
                               Err_Errno(),
                               exitCode,
                               (int64) pid);
   if (!sentResult) {
      Warning("Unable to send results from polling the result program.\n\n");
   }
} // ToolsDaemonTcloReportProgramCompleted


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsDaemonTcloReceiveVixCommand --
 *
 *
 * Return value:
 *    TRUE on success
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
ToolsDaemonTcloReceiveVixCommand(char const **result,     // OUT
                                 size_t *resultLen,       // OUT
                                 const char *name,        // IN
                                 const char *args,        // IN
                                 size_t argsSize,         // IN: Size of args
                                 void *clientData)        // IN
{
   VixError err = VIX_OK;
   char *requestName = NULL;
   VixCommandRequestHeader *requestMsg = NULL;
   size_t maxResultBufferSize;
   size_t tcloBufferLen;
   char *resultValue = NULL;
   size_t resultValueLength = 0;
   Bool deleteResultValue = FALSE;
   char *destPtr = NULL;
   GuestApp_Dict **confDictRef;
   int vixPrefixDataSize = (MAX64_DECIMAL_DIGITS * 2)
                             + (sizeof(' ') * 2)
                             + sizeof('\0');
   /*
    * Our temporary buffer will be the same size as what the 
    * Tclo/RPC system can handle, which is GUESTMSG_MAX_IN_SIZE.
    */
   static char tcloBuffer[GUESTMSG_MAX_IN_SIZE];
   
   
   requestName = ToolsDaemonTcloGetQuotedString(args, &args);

   /*
    * Skip the NULL, char, and then the rest of the buffer should just 
    * be a Vix command object.
    */
   while (*args) {
      args += 1;
   }
   args += 1;
   err = VixMsg_ValidateMessage((char *) args, argsSize);
   if (VIX_OK != err) {
      goto abort;
   }
   requestMsg = (VixCommandRequestHeader *) args;
   confDictRef = (GuestApp_Dict **) clientData;
   maxResultBufferSize = sizeof(tcloBuffer) - vixPrefixDataSize;

   err = VixTools_ProcessVixCommand(requestMsg,
                                    requestName,
                                    maxResultBufferSize,
                                    confDictRef,
                                    &resultValue,
                                    &resultValueLength,
                                    &deleteResultValue);


abort:
   tcloBufferLen = resultValueLength + vixPrefixDataSize;

   /*
    * If we generated a message larger than tclo/Rpc can handle,
    * we did something wrong.  Our code should never have done this.
    */
   if (tcloBufferLen > sizeof tcloBuffer) {
      ASSERT(0);
      resultValue[0] = 0;
      tcloBufferLen = tcloBufferLen - resultValueLength;
      err = VIX_E_OUT_OF_MEMORY;
   }

   /*
    * This should never happen since tcloBuffer is pretty huge.
    * But, there is no harm in being paranoid and I don't want to
    * find out the buffer is too small when we are halfway through
    * formatting the response.
    */
   if ((32 + resultValueLength + 32) >= sizeof(tcloBuffer)) {
      err = VIX_E_OUT_OF_MEMORY;
      resultValueLength = 0;
   }


   /*
    * All Foundry tools commands return results that start with a foundry error
    * and a guest-OS-specific error.
    */
   Str_Sprintf(tcloBuffer,
               sizeof tcloBuffer,
               "%"FMT64"d %d ",
               err,
               Err_Errno());
   destPtr = tcloBuffer + strlen(tcloBuffer);

   /*
    * If this is a binary result, then we put a # at the end of the ascii to
    * mark the end of ascii and the start of the binary data. 
    */
   if ((NULL != requestMsg)
         && (requestMsg->commonHeader.commonFlags & VIX_COMMAND_GUEST_RETURNS_BINARY)) {
      *(destPtr++) = '#';
      *resultLen = destPtr - tcloBuffer + sizeof '#' + resultValueLength;
   }

   /*
    * Copy the result. Don't use a strcpy, since this may be a binary buffer.
    */
   memcpy(destPtr, resultValue, resultValueLength);
   destPtr += resultValueLength;

   /*
    * If this is not binary data, then it should be a NULL terminated string.
    */
   if ((NULL == requestMsg)
         || !(requestMsg->commonHeader.commonFlags & VIX_COMMAND_GUEST_RETURNS_BINARY)) {
      *(destPtr++) = 0;
      *resultLen = strlen(tcloBuffer);
   }
   
   *result = tcloBuffer;

   if (deleteResultValue) {
      free(resultValue);
   }
   free(requestName);

   Debug("<ToolsDaemonTcloReceiveVixCommand\n");
   return TRUE;
} // ToolsDaemonTcloReceiveVixCommand

