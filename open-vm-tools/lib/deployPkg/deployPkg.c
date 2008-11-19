/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
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

#include <time.h>

#include "debug.h"
#include "vm_assert.h"
#if defined(VMTOOLS_USE_GLIB)
#  define G_LOG_DOMAIN  "deployPkg"
#  define Debug         g_debug
#  define Warning       g_warning
#  include "vmtoolsApp.h"
#endif
#include "rpcout.h"
#include "util.h"
#include "str.h"
#include "strutil.h"
#include "file.h"
#include "codeset.h"
#include "toolsDeployPkg.h"
#include "deployPkg.h"
#include "runDeployPkgInt.h"

static char *DeployPkgGetTempDir(void);


#if !defined(VMTOOLS_USE_GLIB)
/*
 *-----------------------------------------------------------------------------
 *
 * DeployPkg_Register --
 *
 *    Register TCLO handlers.
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
DeployPkg_Register(RpcIn *in)    // IN
{
   Debug("DeployPkg_Register got called\n");

   RpcIn_RegisterCallbackEx(in, "deployPkg.begin", DeployPkg_TcloBegin, NULL);
   RpcIn_RegisterCallbackEx(in, "deployPkg.deploy", DeployPkg_TcloDeploy, NULL);

   srand(time(NULL));
}
#endif


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

Bool
DeployPkg_TcloBegin(RpcInData *data)   // IN
{
   static char resultBuffer[FILE_MAXPATH];
   char *tempDir = DeployPkgGetTempDir();

   Debug("DeployPkgTcloBegin got call\n");

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

Bool
DeployPkg_TcloDeploy(RpcInData *data)  // IN
{
   char errMsg[2048];
   ToolsDeployPkgError ret;
   char *argCopy, *pkgStart, *pkgEnd;
   char *white = " \t\r\n";

   /* Set state to DEPLOYING. */
#if defined(VMTOOLS_USE_GLIB)
   gchar *msg;
   ToolsAppCtx *ctx = data->appCtx;

   msg = g_strdup_printf("deployPkg.update.state %d",
                         TOOLSDEPLOYPKG_DEPLOYING);
   if (!RpcChannel_Send(ctx->rpc, msg, strlen(msg) + 1, NULL, NULL)) {
      g_warning("DeployPkgTcloDeploy failed update state to "
              "TOOLSDEPLOYPKG_DEPLOYING\n");
   }
   g_free(msg);
#else
   if (!RpcOut_sendOne(NULL, NULL, "deployPkg.update.state %d",
                       TOOLSDEPLOYPKG_DEPLOYING)) {
      Warning("DeployPkgTcloDeploy failed update state to "
              "TOOLSDEPLOYPKG_DEPLOYING\n");
   }
#endif

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
#if defined(VMTOOLS_USE_GLIB)
      msg = g_strdup_printf("deployPkg.update.state %d %d Package file %s not found",
                            TOOLSDEPLOYPKG_DEPLOYING,
                            TOOLSDEPLOYPKG_ERROR_DEPLOY_FAILED,
                            pkgStart);
      if (!RpcChannel_Send(ctx->rpc, msg, strlen(msg) + 1, NULL, NULL)) {
         g_warning("DeployPkgTcloDeploy failed update state to "
                   "TOOLSDEPLOYPKG_DEPLOYING\n");
      }
      g_free(msg);
#else
      if (!RpcOut_sendOne(NULL, NULL, 
                          "deployPkg.update.state %d %d Package file %s not found",
                          TOOLSDEPLOYPKG_DEPLOYING, 
                          TOOLSDEPLOYPKG_ERROR_DEPLOY_FAILED,
                          pkgStart)) {
         Warning("DeployPkgTcloDeploy failed update state to "
                 "TOOLSDEPLOYPKG_DEPLOYING\n");
      }
#endif
      Warning("Package file '%s' doesn't exist!!\n", pkgStart);
      goto ExitPoint;
   }

   /* Unpack the package and run the command. */
   ret = DeployPkgDeployPkgInGuest(pkgStart, errMsg, sizeof errMsg);
   if (ret != TOOLSDEPLOYPKG_ERROR_SUCCESS) {
#if defined(VMTOOLS_USE_GLIB)
      msg = g_strdup_printf("deployPkg.update.state %d %d %s",
                            TOOLSDEPLOYPKG_DEPLOYING,
                            TOOLSDEPLOYPKG_ERROR_DEPLOY_FAILED,
                            errMsg);
      if (!RpcChannel_Send(ctx->rpc, msg, strlen(msg) + 1, NULL, NULL)) {
         g_warning("DeployPkgTcloDeploy failed update state to "
                   "TOOLSDEPLOYPKG_DEPLOYING\n");
      }
      g_free(msg);
#else
      if (!RpcOut_sendOne(NULL, NULL, "deployPkg.update.state %d %d %s",
                          TOOLSDEPLOYPKG_DEPLOYING,
                          TOOLSDEPLOYPKG_ERROR_DEPLOY_FAILED,
                          errMsg)) {
         Warning("DeployPkgTcloDeploy failed update state to "
                 "TOOLSDEPLOYPKG_DEPLOYING\n");
      }
#endif
      Warning("DeployPkgInGuest failed, error = %d\n", ret);
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
    * Get system temporary directory. We can't use Util_GetSafeTmpDir 
    * because much of win32util.c which gets used in that call creates 
    * dependencies on code that won't run on win9x.
    */
   if ((dir = File_GetTmpDir(TRUE)) == NULL) {
      Warning("DeployPkgGetTempDir File_GetTmpDir failed\n");
      goto exit;
   }

   /* Make a temporary directory to hold the package. */
   while (!found && i < 10) {
      free(newDir);
      newDir = Str_Asprintf(NULL, "%s%s%08x%s", 
                            dir, DIRSEPS, rand(), DIRSEPS);
      if (newDir == NULL) {
         Warning("DeployPkgGetTempDir Str_Asprintf failed\n");
         goto exit;
      }
      found = File_CreateDirectory(newDir);
      i++;
   }

   if (found == FALSE) {
      Warning("DeployPkgGetTempDir Could not create temp directory\n");
      goto exit;
   } 
exit:
   free(dir);
   return newDir;
}
