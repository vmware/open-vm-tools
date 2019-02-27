/*********************************************************
 * Copyright (C) 2003-2019 VMware, Inc. All rights reserved.
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
#if defined(__linux__)
#include <sys/wait.h>
#include <mntent.h>
#include <paths.h>
#endif


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
#   include "windowsu.h"
#elif _WIN32
#   include "win95.h"
#endif

#include "vmware.h"
#include "procMgr.h"
#include "vm_version.h"
#include "message.h"

#define G_LOG_DOMAIN  "vix"
#include <glib.h>
#include <glib/gstdio.h>

#include "vixPluginInt.h"
#include "vmware/tools/utils.h"

#include "util.h"
#include "strutil.h"
#include "str.h"
#include "file.h"
#include "err.h"
#include "hostinfo.h"
#include "guest_os.h"
#include "guest_msg_def.h"
#include "conf.h"
#include "vixCommands.h"
#include "base64.h"
#include "syncDriver.h"
#include "hgfsServerManager.h"
#include "hgfs.h"
#include "system.h"
#include "codeset.h"
#include "vixToolsInt.h"

#if defined(__linux__)
#include "mntinfo.h"
#include "hgfsDevLinux.h"
#endif

/* Only Win32, Linux, Solaris and FreeBSD use impersonation functions. */
#if !defined(__APPLE__)
#include "impersonate.h"
#endif

#include "vixOpenSource.h"

#define MAX64_DECIMAL_DIGITS 20          /* 2^64 = 18,446,744,073,709,551,616 */

#if defined(__linux__) || defined(_WIN32)

# if defined(_WIN32)
#  define DECLARE_SYNCDRIVER_ERROR(name) DWORD name = ERROR_SUCCESS
#  define SYNCDRIVERERROR ERROR_GEN_FAILURE
# else
#  define DECLARE_SYNCDRIVER_ERROR(name) int name = 0
#  define SYNCDRIVERERROR errno
# endif

static SyncDriverHandle gSyncDriverHandle = SYNCDRIVER_INVALID_HANDLE;

static Bool ToolsDaemonSyncDriverThawCallback(void *clientData);
#endif

static char *ToolsDaemonTcloGetQuotedString(const char *args,
                                            const char **endOfArg);
  
static VixError ToolsDaemonTcloGetEncodedQuotedString(const char *args,
                                                      const char **endOfArg,
                                                      char **result);

gboolean ToolsDaemonTcloReceiveVixCommand(RpcInData *data);

#if defined(__linux__) || defined(_WIN32)
gboolean ToolsDaemonTcloSyncDriverFreeze(RpcInData *data);

gboolean ToolsDaemonTcloSyncDriverThaw(RpcInData *data);
#endif

gboolean ToolsDaemonTcloMountHGFS(RpcInData *data);

void ToolsDaemonTcloReportProgramCompleted(const char *requestName,
                                           VixError err,
                                           int exitCode,
                                           int64 pid,
                                           void *clientData);

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

gboolean
FoundryToolsDaemonRunProgram(RpcInData *data) // IN
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
   ProcMgr_Pid pid = -1;
   GMainLoop *eventQueue = ((ToolsAppCtx *)data->appCtx)->mainLoop;

   /*
    * Parse the arguments. Some of these are optional, so they
    * may be NULL.
    */
   requestName = ToolsDaemonTcloGetQuotedString(data->args, &data->args);

   err = ToolsDaemonTcloGetEncodedQuotedString(data->args, &data->args,
                                               &commandLine);
   if (err != VIX_OK) {
      goto abort;
   }

   err = ToolsDaemonTcloGetEncodedQuotedString(data->args, &data->args,
                                               &commandLineArgs);
   if (err != VIX_OK) {
      goto abort;
   }

   credentialTypeStr = ToolsDaemonTcloGetQuotedString(data->args, &data->args);
   obfuscatedNamePassword = ToolsDaemonTcloGetQuotedString(data->args, &data->args);
   directoryPath = ToolsDaemonTcloGetQuotedString(data->args, &data->args);
   environmentVariables = ToolsDaemonTcloGetQuotedString(data->args, &data->args);

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
                                eventQueue,
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
   RPCIN_SETRETVALS(data, resultBuffer, TRUE);

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

gboolean
FoundryToolsDaemonGetToolsProperties(RpcInData *data) // IN
{
   VixError err = VIX_OK;
   int additionalError = 0;
   static char resultBuffer[DEFAULT_RESULT_MSG_MAX_LENGTH];
   char *serializedBuffer = NULL;
   size_t serializedBufferLength = 0;
   char *base64Buffer = NULL;
   size_t base64BufferLength = 0;
   Bool success;
   char *returnBuffer = NULL;
   GKeyFile *confDictRef;
   
   /*
    * Collect some values about the host.
    */
   confDictRef = data->clientData;

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
   RPCIN_SETRETVALS(data, resultBuffer, TRUE);

   free(serializedBuffer);
   free(base64Buffer);
   
   return TRUE;
} // FoundryToolsDaemonGetToolsProperties


/**
 * Initializes internal state of the Foundry daemon.
 *
 * @param[in]  ctx      Application context.
 */

void
FoundryToolsDaemon_Initialize(ToolsAppCtx *ctx)
{
   thisProcessRunsAsRoot = TOOLS_IS_MAIN_SERVICE(ctx);

   /*
    * TODO: Add the original/native environment (envp) to ToolsAppContext so
    * we can know what the environment variables were before the loader scripts
    * changed them.
    */
   (void) VixTools_Initialize(thisProcessRunsAsRoot,
#if defined(__FreeBSD__)
                              ctx->envp,   // envp
#else
                              NULL,        // envp
#endif
                              ToolsDaemonTcloReportProgramCompleted,
                              ctx);

#if !defined(__APPLE__)
   if (thisProcessRunsAsRoot) {
      Impersonate_Init();
   }
#endif

}


/**
 * Uninitializes internal state of the Foundry daemon.
 *
 * @param[in]  ctx      Application context.
 */

void
FoundryToolsDaemon_Uninitialize(ToolsAppCtx *ctx)
{
   VixTools_Uninitialize();
}


/**
 * Restrict VIX commands in Foundry daemon.
 *
 * @param[in]  ctx        Application context.
 * @param[in]  restricted TRUE/FALSE=>enable/disable restriction.
 */

void
FoundryToolsDaemon_RestrictVixCommands(ToolsAppCtx *ctx, gboolean restricted)
{
   VixTools_RestrictCommands(restricted);
}


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

static VixError
ToolsDaemonTcloGetEncodedQuotedString(const char *args,      // IN
                                      const char **endOfArg, // OUT
                                      char **result)         // OUT
{
   VixError err = VIX_OK;
   char *rawResultStr = NULL;
   char *resultStr = NULL;

   rawResultStr = ToolsDaemonTcloGetQuotedString(args, endOfArg);
   if (NULL == rawResultStr) {
      goto abort;
   }

   err = VixMsg_DecodeString(rawResultStr, &resultStr);

abort:
   free(rawResultStr);
   *result = resultStr;

   return err;
}

#if defined(__linux__) || defined(_WIN32)

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

gboolean
ToolsDaemonTcloSyncDriverFreeze(RpcInData *data)
{
   static char resultBuffer[DEFAULT_RESULT_MSG_MAX_LENGTH];
   VixError err = VIX_OK;
   char *driveList = NULL;
   char *timeout = NULL;
   int timeoutVal;
   DECLARE_SYNCDRIVER_ERROR(sysError);
   ToolsAppCtx *ctx = data->appCtx;
   GKeyFile *confDictRef = ctx->config;
   Bool enableNullDriver;
   GSource *timer;

   /*
    * Parse the arguments
    */
   driveList = ToolsDaemonTcloGetQuotedString(data->args, &data->args);
   timeout = ToolsDaemonTcloGetQuotedString(data->args, &data->args);

   /*
    * Validate the arguments.
    */
   if (NULL == driveList || NULL == timeout) {
      err = VIX_E_INVALID_ARG;
      g_warning("%s: Failed to get string args\n", __FUNCTION__);
      goto abort;
   }

   if (!StrUtil_StrToInt(&timeoutVal, timeout) || timeoutVal < 0) {
      g_warning("%s: Bad args, timeout '%s'\n",
                __FUNCTION__, timeout);
      err = VIX_E_INVALID_ARG;
      goto abort;
   }

   g_debug("%s: Got request to freeze '%s', timeout %d\n",
           __FUNCTION__, driveList, timeoutVal);

   /* Disallow multiple freeze calls. */
   if (gSyncDriverHandle != SYNCDRIVER_INVALID_HANDLE) {
      err = VIX_E_OBJECT_IS_BUSY;
      goto abort;
   }

   enableNullDriver = VixTools_ConfigGetBoolean(confDictRef,
                                                "vmbackup",
                                                "enableNullDriver",
                                                FALSE);

   /* Perform the actual freeze. */
   if (!SyncDriver_Freeze(driveList, enableNullDriver, &gSyncDriverHandle,
                          NULL) ||
       SyncDriver_QueryStatus(gSyncDriverHandle, INFINITE) != SYNCDRIVER_IDLE) {
      g_warning("%s: Failed to Freeze drives '%s'\n",
                __FUNCTION__, driveList);
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
      g_debug("%s: Starting timer callback %d\n",
              __FUNCTION__, timeoutVal);
      timer = g_timeout_source_new(timeoutVal * 10);
      VMTOOLSAPP_ATTACH_SOURCE(ctx, timer, ToolsDaemonSyncDriverThawCallback, NULL, NULL);
      g_source_unref(timer);
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
   g_message("%s: returning %s\n", __FUNCTION__, resultBuffer);
   return RPCIN_SETRETVALS(data, resultBuffer, TRUE);
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

#if defined(__linux__) || defined(_WIN32)
static Bool
ToolsDaemonSyncDriverThawCallback(void *clientData) // IN (ignored)
{
   g_debug("%s: Timed out waiting for thaw.\n", __FUNCTION__);

   if (gSyncDriverHandle == SYNCDRIVER_INVALID_HANDLE) {
      g_warning("%s: No drives are frozen.\n", __FUNCTION__);
      goto exit;
   }
   if (!SyncDriver_Thaw(gSyncDriverHandle)) {
      g_warning("%s: Failed to thaw.\n", __FUNCTION__);
   }

exit:
   SyncDriver_CloseHandle(&gSyncDriverHandle);
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

#if defined(__linux__) || defined(_WIN32)
gboolean
ToolsDaemonTcloSyncDriverThaw(RpcInData *data) // IN
{
   static char resultBuffer[DEFAULT_RESULT_MSG_MAX_LENGTH];
   VixError err = VIX_OK;
   DECLARE_SYNCDRIVER_ERROR(sysError);

   /*
    * This function has no arguments that we care about.
    */

   g_debug("%s: Got request to thaw\n", __FUNCTION__);

   if (gSyncDriverHandle == SYNCDRIVER_INVALID_HANDLE) {
      err = VIX_E_GUEST_VOLUMES_NOT_FROZEN;
      sysError = SYNCDRIVERERROR;
      g_warning("%s: No drives are frozen.\n", __FUNCTION__);
   } else if (!SyncDriver_Thaw(gSyncDriverHandle)) {
      err = VIX_E_FAIL;
      sysError = SYNCDRIVERERROR;
      g_warning("%s: Failed to Thaw drives\n", __FUNCTION__);
   }

   SyncDriver_CloseHandle(&gSyncDriverHandle);

   /*
    * All Foundry tools commands return results that start with a
    * foundry error and a guest-OS-specific error.
    */
   Str_Sprintf(resultBuffer, sizeof resultBuffer, "%"FMT64"d %d", err, sysError);
   g_message("%s: returning %s\n", __FUNCTION__, resultBuffer);
   return RPCIN_SETRETVALS(data, resultBuffer, TRUE);
}
#endif


#if defined(__linux__)
/*
 *-----------------------------------------------------------------------------
 *
 * ToolsDaemonCheckMountedHGFS --
 *
 *    Check if the HGFS file system is already mounted.
 *
 * Return value:
 *    VIX_OK and vmhgfsMntFound is TRUE if mounted or FALSE if not.
 *    set VixError otherwise.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static VixError
ToolsDaemonCheckMountedHGFS(Bool isFuseEnabled,      // IN:
                            Bool *vmhgfsMntFound)    // OUT: HGFS is mounted
{
   MNTHANDLE mtab;
   DECLARE_MNTINFO(mnt);
   const char *fsName;
   const char *fsType;
   VixError err = VIX_OK;

   if ((mtab = OPEN_MNTFILE("r")) == NULL) {
      err = VIX_E_FAIL;
      g_warning("%s: ERROR: opening mounted file system table -> %d\n", __FUNCTION__, errno);
      goto exit;
   }

   *vmhgfsMntFound = FALSE;
   if (isFuseEnabled) {
      fsName = HGFS_FUSENAME;
      fsType = HGFS_FUSETYPE;
   } else {
      fsName = ".host:/";
      fsType = HGFS_NAME;
   }
   while (GETNEXT_MNTINFO(mtab, mnt)) {
      if ((strcmp(MNTINFO_NAME(mnt), fsName) == 0) &&
            (strcmp(MNTINFO_FSTYPE(mnt), fsType) == 0) &&
            (strcmp(MNTINFO_MNTPT(mnt), HGFS_MOUNT_POINT) == 0)) {
         *vmhgfsMntFound = TRUE;
         g_debug("%s: mnt fs \"%s\" type \"%s\" dir \"%s\"\n", __FUNCTION__,
                  MNTINFO_NAME(mnt), MNTINFO_FSTYPE(mnt), MNTINFO_MNTPT(mnt));
         break;
      }
   }
   CLOSE_MNTFILE(mtab);

exit:
   return err;
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsDaemonTcloMountHGFS --
 *
 *    Mount the HGFS file system.
 *
 *    This will do nothing if the file system is already mounted. In some cases
 *    it might be necessary to create the mount path too.
 *
 * Return value:
 *    TRUE always and VixError status for the RPC call reply.
 *    VIX_OK if mount succeeded or was already mounted
 *    VIX_E_FAIL if we couldn't check the mount was available
 *    VIX_E_HGFS_MOUNT_FAIL if the mount operation itself failed
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

gboolean
ToolsDaemonTcloMountHGFS(RpcInData *data) // IN
{
   VixError err = VIX_OK;
   static char resultBuffer[DEFAULT_RESULT_MSG_MAX_LENGTH];

#if defined(__linux__)
#define MOUNT_PATH_BIN       "/bin/mount"
#define MOUNT_PATH_USR_BIN   "/usr" MOUNT_PATH_BIN
#define MOUNT_HGFS_PATH      "/mnt/hgfs"
#define MOUNT_HGFS_ARGS      " -t vmhgfs .host:/ " MOUNT_HGFS_PATH

   /*
    * Look for a vmhgfs mount at /mnt/hgfs. If one exists, nothing
    * else needs to be done.  If one doesn't exist, then mount at
    * that location.
    */
   ProcMgr_ProcArgs vmhgfsExecProcArgs;
   Bool execRes;
   const char *mountCmd = NULL;
   Bool isFuseEnabled = TRUE;
   Bool vmhgfsMntFound = FALSE;
   Bool vmhgfsMntPointCreated = FALSE;
   Bool validFuseExitCode;
   int fuseExitCode;
   int ret;

   vmhgfsExecProcArgs.envp = NULL;
   vmhgfsExecProcArgs.workingDirectory = NULL;

   execRes = ProcMgr_ExecSyncWithExitCode("/usr/bin/vmhgfs-fuse --enabled",
                                          &vmhgfsExecProcArgs,
                                          &validFuseExitCode,
                                          &fuseExitCode);
   if (!execRes) {
      if (validFuseExitCode && fuseExitCode == 2) {
         g_warning("%s: vmhgfs-fuse -> FUSE not installed\n", __FUNCTION__);
         err = VIX_E_HGFS_MOUNT_FAIL;
         goto exit;
      }
      g_message("%s: vmhgfs-fuse -> %d: not supported on this kernel version\n",
                __FUNCTION__, validFuseExitCode ? fuseExitCode : 0);
      isFuseEnabled = FALSE;
   }

   err = ToolsDaemonCheckMountedHGFS(isFuseEnabled, &vmhgfsMntFound);
   if (err != VIX_OK) {
      goto exit;
   }

   if (vmhgfsMntFound) {
      g_message("%s: vmhgfs already mounted\n", __FUNCTION__);
      goto exit;
   }

   /* Verify that mount point exists, if not create it. */
   ret = g_access(MOUNT_HGFS_PATH, F_OK);
   if (ret != 0) {
      g_message("%s: no mount point found, create %s\n", __FUNCTION__, MOUNT_HGFS_PATH);
      ret = g_mkdir_with_parents(MOUNT_HGFS_PATH, 0755);
      if (ret != 0) {
         err = VIX_E_HGFS_MOUNT_FAIL;
         g_warning("%s: ERROR: vmhgfs mount point creation -> %d\n", __FUNCTION__, errno);
         goto exit;
      }
      vmhgfsMntPointCreated = TRUE;
   }

   /* Do the HGFS mount. */
   if (isFuseEnabled) {
      mountCmd = "/usr/bin/vmhgfs-fuse .host:/ /mnt/hgfs -o subtype=vmhgfs-fuse,allow_other";
   } else {
      /*
       * We need to call the mount program, not the mount system call. The
       * mount program does several additional things, like compute the mount
       * options from the contents of /etc/fstab, and invoke custom mount
       * programs like the one needed for HGFS.
       */
      ret = g_access(MOUNT_PATH_USR_BIN, F_OK);
      if (ret == 0) {
         mountCmd = MOUNT_PATH_USR_BIN MOUNT_HGFS_ARGS;
      } else {
         ret = g_access(MOUNT_PATH_BIN, F_OK);
         if (ret == 0) {
            mountCmd = MOUNT_PATH_BIN MOUNT_HGFS_ARGS;
         } else {
            g_warning("%s: failed to find mount -> %d\n", __FUNCTION__, errno);
            err = VIX_E_HGFS_MOUNT_FAIL;
            goto exit;
         }
      }
   }

   g_debug("%s: Mounting: %s\n", __FUNCTION__, mountCmd);
   execRes = ProcMgr_ExecSync(mountCmd, &vmhgfsExecProcArgs);
   if (!execRes) {
      err = VIX_E_HGFS_MOUNT_FAIL;
      g_warning("%s: ERROR: no vmhgfs mount\n", __FUNCTION__);
   }
exit:
   if (err != VIX_OK) {
      if (vmhgfsMntPointCreated) {
         ret = g_rmdir(MOUNT_HGFS_PATH);
         if (ret != 0) {
            g_warning("%s: vmhgfs mount point not deleted %d\n", __FUNCTION__, errno);
         }
      }
   }
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
   RPCIN_SETRETVALS(data, resultBuffer, TRUE);

   g_message("%s: returning %s\n", __FUNCTION__, resultBuffer);

   return TRUE;
} // ToolsDaemonTcloMountHGFS


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
                                      int64 pid,                  // IN
                                      void *clientData)           // IN
{
   Bool sentResult;
   ToolsAppCtx *ctx = clientData;
   gchar *msg = g_strdup_printf("%s %s %"FMT64"d %d %d %"FMT64"d",
                                VIX_BACKDOORCOMMAND_RUN_PROGRAM_DONE,
                                requestName,
                                err,
                                Err_Errno(),
                                exitCode,
                                (int64) pid);

   sentResult = RpcChannel_Send(ctx->rpc, msg, strlen(msg) + 1, NULL, NULL);
   g_free(msg);

   if (!sentResult) {
      g_warning("%s: Unable to send results from polling the result program.\n",
                __FUNCTION__);
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

gboolean
ToolsDaemonTcloReceiveVixCommand(RpcInData *data) // IN
{
   VixError err = VIX_OK;
   uint32 additionalError = 0;
   char *requestName = NULL;
   VixCommandRequestHeader *requestMsg = NULL;
   size_t maxResultBufferSize;
   size_t tcloBufferLen;
   char *resultValue = NULL;
   size_t resultValueLength = 0;
   Bool deleteResultValue = FALSE;
   char *destPtr = NULL;
   int vixPrefixDataSize = (MAX64_DECIMAL_DIGITS * 2)
                             + (sizeof(' ') * 2)
                             + sizeof('\0')
                             + sizeof(' ') * 10;   // for RPC header

   /*
    * Our temporary buffer will be the same size as what the
    * Tclo/RPC system can handle, which is GUESTMSG_MAX_IN_SIZE.
    */
   static char tcloBuffer[GUESTMSG_MAX_IN_SIZE];

   ToolsAppCtx *ctx = data->appCtx;
   GMainLoop *eventQueue = ctx->mainLoop;
   GKeyFile *confDictRef = ctx->config;

   requestName = ToolsDaemonTcloGetQuotedString(data->args, &data->args);

   /*
    * Skip the NULL, char, and then the rest of the buffer should just 
    * be a Vix command object.
    */
   while (*data->args) {
      data->args += 1;
   }
   data->args += 1;
   err = VixMsg_ValidateMessage((char *) data->args, data->argsSize);
   if (VIX_OK != err) {
      goto abort;
   }
   requestMsg = (VixCommandRequestHeader *) data->args;
   maxResultBufferSize = sizeof(tcloBuffer) - vixPrefixDataSize;

   err = VixTools_ProcessVixCommand(requestMsg,
                                    requestName,
                                    maxResultBufferSize,
                                    confDictRef,
                                    eventQueue,
                                    &resultValue,
                                    &resultValueLength,
                                    &deleteResultValue);

   /*
    * NOTE: We have always been returning an additional 32 bit error (errno,
    * or GetLastError() for Windows) along with the 64 bit VixError. The VMX
    * side has been dropping the higher order 32 bits of VixError (by copying
    * it onto a 32 bit error). They do save the additional error but as far
    * as we can tell, it was not getting used by foundry. So at this place,
    * for certain guest commands that have extra error information tucked into
    * the higher order 32 bits of the VixError, we use that extra error as the
    * additional error to be sent back to VMX.
    */
   additionalError = VixTools_GetAdditionalError(requestMsg->opCode, err);
   if (additionalError) {
      g_message("%s: command %u, additionalError = %u\n",
                __FUNCTION__, requestMsg->opCode, additionalError);
   } else {
      g_debug("%s: command %u, additionalError = %u\n",
              __FUNCTION__, requestMsg->opCode, additionalError);
   }

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
    * All Foundry tools commands return results that start with a foundry error
    * and a guest-OS-specific error.
    */
   Str_Sprintf(tcloBuffer,
               sizeof tcloBuffer,
               "%"FMT64"d %d ",
               err,
               additionalError);
   destPtr = tcloBuffer + strlen(tcloBuffer);

   /*
    * If this is a binary result, then we put a # at the end of the ascii to
    * mark the end of ascii and the start of the binary data. 
    */
   if ((NULL != requestMsg)
         && (requestMsg->commonHeader.commonFlags & VIX_COMMAND_GUEST_RETURNS_BINARY)) {
      *(destPtr++) = '#';
      data->resultLen = destPtr - tcloBuffer + resultValueLength;
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
      data->resultLen = strlen(tcloBuffer) + 1;
   }
   
   data->result = tcloBuffer;

   if (deleteResultValue) {
      free(resultValue);
   }
   free(requestName);

   return TRUE;
} // ToolsDaemonTcloReceiveVixCommand

