/*********************************************************
 * Copyright (C) 2006-2016 VMware, Inc. All rights reserved.
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

#ifdef __cplusplus
extern "C" {
#endif
   #include "vm_assert.h"
   #include "file.h"
   #include "str.h"
   #include "util.h"
   #include "unicodeBase.h"
   #include "vmware/tools/plugin.h"
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
 *-----------------------------------------------------------------------------
 *
 * DeployPkgTcloDeploy --
 *
 *    TCLO handler for "deployPkg.deploy". Start image guest package deployment.
 *
 * Return value:
 *    TRUE if success
 *    FALSE if anything failed
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

gboolean
DeployPkg_TcloDeploy(RpcInData *data)  // IN
{
   char errMsg[2048];
   ToolsDeployPkgError ret;
   char *argCopy, *pkgStart, *pkgEnd;
   const char *white = " \t\r\n";

   /* Set state to DEPLOYING. */
   gchar *msg;
   ToolsAppCtx *ctx = (ToolsAppCtx *)(data->appCtx);

   msg = g_strdup_printf("deployPkg.update.state %d",
                         TOOLSDEPLOYPKG_DEPLOYING);
   if (!RpcChannel_Send(ctx->rpc, msg, strlen(msg) + 1, NULL, NULL)) {
      g_warning("%s: failed update state to TOOLSDEPLOYPKG_DEPLOYING\n",
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
      if (!RpcChannel_Send(ctx->rpc, msg, strlen(msg) + 1, NULL, NULL)) {
         g_warning("%s: failed update state to TOOLSDEPLOYPKG_DEPLOYING\n",
                   __FUNCTION__);
      }
      g_free(msg);
      g_warning("Package file '%s' doesn't exist!!\n", pkgStart);
      goto ExitPoint;
   }

   /* Unpack the package and run the command. */
   ret = DeployPkgDeployPkgInGuest(pkgStart, errMsg, sizeof errMsg);
   if (ret != TOOLSDEPLOYPKG_ERROR_SUCCESS) {
      msg = g_strdup_printf("deployPkg.update.state %d %d %s",
                            TOOLSDEPLOYPKG_DEPLOYING,
                            TOOLSDEPLOYPKG_ERROR_DEPLOY_FAILED,
                            errMsg);
      if (!RpcChannel_Send(ctx->rpc, msg, strlen(msg) + 1, NULL, NULL)) {
         g_warning("%s: failed update state to TOOLSDEPLOYPKG_DEPLOYING\n",
                   __FUNCTION__);
      }
      g_free(msg);
      g_warning("DeployPkgInGuest failed, error = %d\n", ret);
   }

 ExitPoint:

   /* Attempt to delete the package file and tempdir. */
   Log("Deleting file %s\n", pkgStart);
   if (File_Unlink(pkgStart) == 0) {
      char *vol, *dir, *path;
      File_SplitName(pkgStart, &vol, &dir, NULL);
      path = Str_Asprintf(NULL, "%s%s", vol, dir);
      if (path != NULL) {
         Log("Deleting directory %s\n", path);
         File_DeleteEmptyDirectory(path);
         free(path);
      }
      free(vol);
      free(dir);
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

   if ((dir = File_GetSafeTmpDir(TRUE)) == NULL) {
      g_warning("%s: File_GetSafeTmpDir failed\n", __FUNCTION__);
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
