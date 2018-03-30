/*********************************************************
 * Copyright (C) 2006-2018 VMware, Inc. All rights reserved.
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

#include "vm_assert.h"
#include "file.h"
#include "str.h"
#include "util.h"
#include "unicodeBase.h"

#ifdef __cplusplus
extern "C" {
#endif
   #include "vmware/tools/plugin.h"
   #include "vmware/tools/threadPool.h"
#ifdef __cplusplus
}
#endif

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
DeployPkgDeployPkgInGuest(const char* pkgFile, // IN: the package filename
                          char* errBuf,        // OUT: buffer for msg on fail
                          int errBufSize)      // IN: size of errBuf
{
   char *tempFileName = NULL;
   ToolsDeployPkgError ret = TOOLSDEPLOYPKG_ERROR_SUCCESS;

   /* Init the logger */
   DeployPkgLog_Open();
   DeployPkg_SetLogger(DeployPkgLog_Log);

   DeployPkgLog_Log(0, "Deploying %s", pkgFile);

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
      DeployPkgLog_Log(3, errBuf);
      ret = TOOLSDEPLOYPKG_ERROR_DEPLOY_FAILED;
      goto ExitPoint;
   }
   pkgFile = tempFileName;
#endif

   if (0 != DeployPkg_DeployPackageFromFile(pkgFile)) {
      Str_Snprintf(errBuf, errBufSize,
                   "Package deploy failed in DeployPkg_DeployPackageFromFile");
      DeployPkgLog_Log(3, errBuf);
      ret = TOOLSDEPLOYPKG_ERROR_DEPLOY_FAILED;
      goto ExitPoint;
   }
   DeployPkgLog_Log(0, "Ran DeployPkg_DeployPackageFromFile successfully");

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
   static char resultBuffer[FILE_MAXPATH];
   char *tempDir = DeployPkgGetTempDir();

   g_debug("DeployPkgTcloBegin got call\n");

   if (tempDir) {
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
   char errMsg[2048];
   ToolsDeployPkgError ret;
   gchar *msg;
   char *pkgNameStr = (char *) pkgName;

   g_debug("%s: Deploypkg deploy task started.\n", __FUNCTION__);

   /* Unpack the package and run the command. */
   ret = DeployPkgDeployPkgInGuest(pkgNameStr, errMsg, sizeof errMsg);
   if (ret != TOOLSDEPLOYPKG_ERROR_SUCCESS) {
      msg = g_strdup_printf("deployPkg.update.state %d %d %s",
                            TOOLSDEPLOYPKG_DEPLOYING,
                            TOOLSDEPLOYPKG_ERROR_DEPLOY_FAILED,
                            errMsg);
      if (!RpcChannel_Send(ctx->rpc, msg, strlen(msg), NULL, NULL)) {
         g_warning("%s: failed to send error code %d for state TOOLSDEPLOYPKG_DEPLOYING\n",
                   __FUNCTION__,
                   TOOLSDEPLOYPKG_ERROR_DEPLOY_FAILED);
      }
      g_free(msg);
      g_warning("DeployPkgInGuest failed, error = %d\n", ret);
   }

   /* Attempt to delete the package file and tempdir. */
   Log("Deleting file %s\n", pkgNameStr);
   if (File_Unlink(pkgNameStr) == 0) {
      char *vol, *dir, *path;
      File_SplitName(pkgNameStr, &vol, &dir, NULL);
      path = Str_Asprintf(NULL, "%s%s", vol, dir);
      if (path != NULL) {
         Log("Deleting directory %s\n", path);
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

   /*
    * Get system temporary directory.
    */

   if ((dir = File_GetSafeRandomTmpDir(TRUE)) == NULL) {
      g_warning("%s: File_GetSafeRandomTmpDir failed\n", __FUNCTION__);
      goto exit;
   }

   /* Make a temporary directory to hold the package. */
   while (!found && i < 10) {
      free(newDir);
      newDir = Str_Asprintf(NULL, "%s%s%08x%s",
                            dir, DIRSEPS, rand(), DIRSEPS);
      if (newDir == NULL) {
         g_warning("%s: Str_Asprintf failed\n", __FUNCTION__);
         goto exit;
      }
      found = File_CreateDirectory(newDir);
      i++;
   }

   if (found == FALSE) {
      g_warning("%s: could not create temp directory\n", __FUNCTION__);
      goto exit;
   }
exit:
   free(dir);
   return newDir;
}
