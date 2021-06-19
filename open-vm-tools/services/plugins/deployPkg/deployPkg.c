/*********************************************************
 * Copyright (C) 2006-2021 VMware, Inc. All rights reserved.
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
 * deployPkg.c --
 *
 *    Support functions for guest package deployment.
 *
 */

#include "deployPkgInt.h"
#ifdef __cplusplus
#include "deployPkg/deployPkgDll.h"
#else
#include "deployPkg/linuxDeployment.h"
#endif

#include "file.h"
#include "random.h"
#include "str.h"
#include "util.h"
#include "unicodeBase.h"
#include "conf.h"

#ifdef __cplusplus
extern "C" {
#endif
   #include "vmware/tools/plugin.h"
   #include "vmware/tools/threadPool.h"
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
// deployPkg.c is compiled using c++ in Windows.
// For c++, LogLevel enum is defined in ImgCustCommon namespace.
using namespace ImgCustCommon;
#endif

// Using 3600s as the upper limit of timeout value in tools.conf.
#define MAX_TIMEOUT_FROM_TOOLCONF 3600

static char *DeployPkgGetTempDir(void);


/*
 *----------------------------------------------------------------------
 *
 * DeployPkgDeployPkgInGuest --
 *
 *    Load the deployPkg so, setup logging and do the job.
 *
 * Results:
 *    TOOLSDEPLOYPKG_ERROR_SUCCESS on success
 *    TOOLSDEPLOYPKG_ERROR_DEPLOY_FAILED on failure
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static ToolsDeployPkgError
DeployPkgDeployPkgInGuest(ToolsAppCtx *ctx,    // IN: app context
                          const char* pkgFile, // IN: the package filename
                          char* errBuf,        // OUT: buffer for msg on fail
                          int errBufSize)      // IN: size of errBuf
{
   char *tempFileName = NULL;
   ToolsDeployPkgError ret = TOOLSDEPLOYPKG_ERROR_SUCCESS;
#ifndef _WIN32
   int processTimeout;
#endif

   /*
    * Init the logger
    * PR 2109109. If the deployPkg log handler has been configured explicitly in
    * tools.conf, then output deployPkg log through the specified handler.
    * https://wiki.eng.vmware.com/Configuring_Logging_for_the_VMware_Tools
    * If not, output the log to the default log file defined in
    * function DeployPkgLog_Open.
    * The deployPkg log handler is mainly configured for debugging purpose.
    */
   char key[128];
   char *handler;
   snprintf(key, sizeof key, "%s.handler", G_LOG_DOMAIN);
   handler = VMTools_ConfigGetString(ctx->config,
                                     CONFGROUPNAME_LOGGING,
                                     key,
                                     NULL);
   if (handler != NULL &&
       (strcmp(handler, "vmx") == 0 || strcmp(handler, "file") == 0 ||
        strcmp(handler, "file+") == 0)) {
      g_debug("Using deployPkg log handler: %s", handler);
      free(handler);
   } else {
      DeployPkgLog_Open();

      if (handler != NULL) {
         DeployPkgLog_Log(log_debug,
                          "Log handler %s is not applicable for deployPkg,"
                          " ignore it and ouput the log in GOS customization"
                          " default log path.",
                          handler);
         free(handler);
      }
   }
   DeployPkg_SetLogger(DeployPkgLog_Log);

   DeployPkgLog_Log(log_debug, "Deploying %s", pkgFile);

#ifdef _WIN32
   /*
    * Because DeployPkg_DeployPackageFromFile can only accept file path of
    * local code page under Windows, convert pkgFile from utf8 to local code
    * page.
    *
    * PR 962946
    */
   tempFileName = (char *)Unicode_GetAllocBytes(pkgFile, STRING_ENCODING_DEFAULT);
   if (tempFileName == NULL) {
      Str_Snprintf(errBuf, errBufSize,
                   "Package deploy failed in Unicode_GetAllocBytes");
      DeployPkgLog_Log(log_error, errBuf);
      ret = TOOLSDEPLOYPKG_ERROR_DEPLOY_FAILED;
      goto ExitPoint;
   }
   pkgFile = tempFileName;
#else
   /*
    * Get processTimeout from tools.conf.
    * Only when we get valid 'timeout' value from tools.conf, we will call
    * DeployPkg_SetProcessTimeout to over-write the processTimeout in deployPkg
    * Using 0 as the default value of CONFNAME_DEPLOYPKG_PROCESSTIMEOUT in tools.conf
    */
   processTimeout =
        VMTools_ConfigGetInteger(ctx->config,
                                 CONFGROUPNAME_DEPLOYPKG,
                                 CONFNAME_DEPLOYPKG_PROCESSTIMEOUT,
                                 0);
   if (processTimeout > 0 && processTimeout <= MAX_TIMEOUT_FROM_TOOLCONF) {
      DeployPkgLog_Log(log_debug, "[%s] %s in tools.conf: %d",
                       CONFGROUPNAME_DEPLOYPKG,
                       CONFNAME_DEPLOYPKG_PROCESSTIMEOUT,
                       processTimeout);
      DeployPkg_SetProcessTimeout(processTimeout);
   } else if (processTimeout != 0) {
      DeployPkgLog_Log(log_debug, "Invalid value %d from tools.conf [%s] %s",
                       processTimeout,
                       CONFGROUPNAME_DEPLOYPKG,
                       CONFNAME_DEPLOYPKG_PROCESSTIMEOUT);
      DeployPkgLog_Log(log_debug, "The valid timeout value range: 1 ~ %d",
                       MAX_TIMEOUT_FROM_TOOLCONF);
   }
#endif

   if (0 != DeployPkg_DeployPackageFromFile(pkgFile)) {
      Str_Snprintf(errBuf, errBufSize,
                   "Package deploy failed in DeployPkg_DeployPackageFromFile");
      DeployPkgLog_Log(log_error, errBuf);
      ret = TOOLSDEPLOYPKG_ERROR_DEPLOY_FAILED;
      goto ExitPoint;
   }
   DeployPkgLog_Log(log_debug, "Ran DeployPkg_DeployPackageFromFile successfully");

ExitPoint:
   free(tempFileName);
   DeployPkgLog_Close();
   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DeployPkgTcloBegin --
 *
 *    TCLO handler for "deployPkg.begin". Try to get temporary directory for file
 *    copy and return the directory name to vmx as result.
 *
 * Return value:
 *    TRUE if get temp dir and send it back
 *    FALSE if something failed
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

gboolean
DeployPkg_TcloBegin(RpcInData *data)   // IN
{
   char *tempDir = DeployPkgGetTempDir();

   g_debug("DeployPkgTcloBegin got call\n");

   if (tempDir) {
      static char resultBuffer[FILE_MAXPATH];

      Str_Strcpy(resultBuffer, tempDir, sizeof resultBuffer);
      free(tempDir);
      return RPCIN_SETRETVALS(data, resultBuffer, TRUE);
   }
   return RPCIN_SETRETVALS(data, "failed to get temp dir", FALSE);
}


/*
 * ---------------------------------------------------------------------------
 * DeployPkgExecDeploy --
 *
 *    Start the deploy execution in a new thread.
 *
 * Return Value:
 *    None.
 *
 * Side effects:
 *    None.
 *
 * ---------------------------------------------------------------------------
 */

void
DeployPkgExecDeploy(ToolsAppCtx *ctx,   // IN: app context
                    void *pkgName)      // IN: pkg file name
{
   ToolsDeployPkgError ret;
   char *pkgNameStr = (char *) pkgName;
   Bool enableCust;

   g_debug("%s: Deploypkg deploy task started.\n", __FUNCTION__);

   /*
    * Check whether guest customization is enabled by VM Tools,
    * by default it is enabled.
    */
   enableCust = VMTools_ConfigGetBoolean(ctx->config,
                                         CONFGROUPNAME_DEPLOYPKG,
                                         CONFNAME_DEPLOYPKG_ENABLE_CUST,
                                         TRUE);
   if (!enableCust) {
      char *result = NULL;
      size_t resultLen;
      gchar *msg = g_strdup_printf("deployPkg.update.state %d %d %s",
                                   TOOLSDEPLOYPKG_DEPLOYING,
                                   TOOLSDEPLOYPKG_ERROR_CUST_DISABLED,
                                   "Customization is disabled by guest admin");

      g_warning("%s: Customization is disabled by guest admin.\n",
                __FUNCTION__);

      if (!RpcChannel_Send(ctx->rpc, msg, strlen(msg), &result, &resultLen)) {
         g_warning("%s: failed to send error code %d for state "
                   "TOOLSDEPLOYPKG_DEPLOYING, result: %s\n",
                   __FUNCTION__,
                   TOOLSDEPLOYPKG_ERROR_CUST_DISABLED,
                   result != NULL ? result : "");
      }
      g_free(msg);
      vm_free(result);
   } else {
      char errMsg[2048];
      /* Unpack the package and run the command. */
      ret = DeployPkgDeployPkgInGuest(ctx, pkgNameStr, errMsg, sizeof errMsg);
      if (ret != TOOLSDEPLOYPKG_ERROR_SUCCESS) {
#ifdef _WIN32
        /*
         * PR 1631160. for Linux, sysimage has sent failure status in vmx when
         * deploy pkg failed, to avoid sending failure events repeatedly, here
         * is only sending status in the case of windows.
         */
         gchar *msg = g_strdup_printf("deployPkg.update.state %d %d %s",
                                      TOOLSDEPLOYPKG_DEPLOYING,
                                      TOOLSDEPLOYPKG_ERROR_DEPLOY_FAILED,
                                      errMsg);

         if (!RpcChannel_Send(ctx->rpc, msg, strlen(msg), NULL, NULL)) {
            g_warning("%s: failed to send error code %d for state "
                      "TOOLSDEPLOYPKG_DEPLOYING\n",
                      __FUNCTION__,
                      TOOLSDEPLOYPKG_ERROR_DEPLOY_FAILED);
         }
         g_free(msg);
#endif
         g_warning("DeployPkgInGuest failed, error = %d\n", ret);
      }
   }

   /* Attempt to delete the package file and tempdir. */
   g_debug("Deleting file %s\n", pkgNameStr);
   if (File_Unlink(pkgNameStr) == 0) {
      char *vol, *dir, *path;
      File_SplitName(pkgNameStr, &vol, &dir, NULL);
      path = Str_Asprintf(NULL, "%s%s", vol, dir);
      if (path != NULL) {
         g_debug("Deleting directory %s\n", path);
         File_DeleteEmptyDirectory(path);
         free(path);
      }
      free(vol);
      free(dir);
   } else {
      g_warning("Unable to delete the file: %s\n", pkgNameStr);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * DeployPkg_TcloDeploy --
 *
 *    TCLO handler for "deployPkg.deploy". Start image guest package deployment.
 *
 * Return value:
 *    TRUE if success
 *    FALSE if pkg file path is not valid
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

gboolean
DeployPkg_TcloDeploy(RpcInData *data)  // IN
{
   char *argCopy, *pkgStart, *pkgEnd, *pkgName;
   const char *white = " \t\r\n";

   /* Set state to DEPLOYING. */
   gchar *msg;
   ToolsAppCtx *ctx = (ToolsAppCtx *)(data->appCtx);

   msg = g_strdup_printf("deployPkg.update.state %d",
                         TOOLSDEPLOYPKG_DEPLOYING);
   if (!RpcChannel_Send(ctx->rpc, msg, strlen(msg), NULL, NULL)) {
      g_warning("%s: failed to update state to TOOLSDEPLOYPKG_DEPLOYING\n",
                __FUNCTION__);
   }
   g_free(msg);

   /* The package filename is in args. First clean up extra whitespace: */
   argCopy = Util_SafeStrdup(data->args);
   pkgStart = argCopy;
   while (*pkgStart != '\0' && Str_Strchr(white, *pkgStart) != NULL) {
      pkgStart++;
   }

   pkgEnd = pkgStart + strlen(pkgStart);
   while (pkgEnd != pkgStart && Str_Strchr(white, *pkgEnd) != NULL) {
      *pkgEnd-- = '\0';
   }

   /* Now make sure the package exists */
   if (!File_Exists(pkgStart)) {
      msg = g_strdup_printf("deployPkg.update.state %d %d Package file %s not found",
                            TOOLSDEPLOYPKG_DEPLOYING,
                            TOOLSDEPLOYPKG_ERROR_DEPLOY_FAILED,
                            pkgStart);
      if (!RpcChannel_Send(ctx->rpc, msg, strlen(msg), NULL, NULL)) {
         g_warning("%s: failed to send error code %d for state TOOLSDEPLOYPKG_DEPLOYING\n",
                   __FUNCTION__,
                   TOOLSDEPLOYPKG_ERROR_DEPLOY_FAILED);
      }
      g_free(msg);
      g_warning("Package file '%s' doesn't exist!!\n", pkgStart);

      free(argCopy);
      return RPCIN_SETRETVALS(data, "failed to get package file", FALSE);
   }

   pkgName = Util_SafeStrdup(pkgStart);
   if (!ToolsCorePool_SubmitTask(ctx, DeployPkgExecDeploy, pkgName, free)) {
      g_warning("%s: failed to start deploy execution thread\n",
                __FUNCTION__);
      msg = g_strdup_printf("deployPkg.update.state %d %d %s",
                            TOOLSDEPLOYPKG_DEPLOYING,
                            TOOLSDEPLOYPKG_ERROR_DEPLOY_FAILED,
                            "failed to spawn deploy execution thread");
      if (!RpcChannel_Send(ctx->rpc, msg, strlen(msg), NULL, NULL)) {
         g_warning("%s: failed to send error code %d for state TOOLSDEPLOYPKG_DEPLOYING\n",
                   __FUNCTION__,
                   TOOLSDEPLOYPKG_ERROR_DEPLOY_FAILED);
      }
      g_free(msg);
      free(pkgName);
   }

   free(argCopy);
   return RPCIN_SETRETVALS(data, "", TRUE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * DeployPkgGetTempDir --
 *
 *    Try to create a staging directory for a package deployment.
 *
 * Results:
 *    Temporary directory path name in utf8 if success, NULL otherwise
 *
 * Side effects:
 *    Memory may be allocated for result.
 *
 *-----------------------------------------------------------------------------
 */

char *
DeployPkgGetTempDir(void)
{
   int i = 0;
   char *dir = NULL;
   char *newDir = NULL;
   Bool found = FALSE;
   int randIndex;
#ifndef _WIN32
   /*
    * PR 2115630. On Linux, use /var/run or /run directory
    * to hold the package.
    */
   const char *runDir = "/run";
   const char *varRunDir = "/var/run";

   if (File_IsDirectory(varRunDir)) {
      dir = strdup(varRunDir);
      if (dir == NULL) {
         g_warning("%s: strdup failed\n", __FUNCTION__);
         goto exit;
      }
   } else if (File_IsDirectory(runDir)) {
      dir = strdup(runDir);
      if (dir == NULL) {
         g_warning("%s: strdup failed\n", __FUNCTION__);
         goto exit;
      }
   }
#endif

   /*
    * Get system temporary directory.
    */

   if (dir == NULL && (dir = File_GetSafeRandomTmpDir(TRUE)) == NULL) {
      g_warning("%s: File_GetSafeRandomTmpDir failed\n", __FUNCTION__);
      goto exit;
   }

   /* Make a temporary directory to hold the package. */
   while (!found && i < 10) {
      free(newDir);
      if (!Random_Crypto(sizeof(randIndex), &randIndex)) {
         g_warning("%s: Random_Crypto failed\n", __FUNCTION__);
         newDir = NULL;
         goto exit;
      }
      newDir = Str_Asprintf(NULL, "%s%s%08x%s",
                            dir, DIRSEPS, randIndex, DIRSEPS);
      if (newDir == NULL) {
         g_warning("%s: Str_Asprintf failed\n", __FUNCTION__);
         goto exit;
      }
      found = File_CreateDirectory(newDir);
      i++;
   }

   if (!found) {
      g_warning("%s: could not create temp directory\n", __FUNCTION__);
      free(newDir);
      newDir = NULL;
   }
exit:
   free(dir);
   return newDir;
}
