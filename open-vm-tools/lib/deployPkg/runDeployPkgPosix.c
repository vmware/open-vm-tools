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
 * runDeployPkgPosix.cpp --
 *
 *    c interface to load the deployPkg library and call the
 *    DeployPkg_DeployPackageFromFile export
 */
#include <dlfcn.h>
#include <unistd.h>

#include "str.h"
#include "file.h"
#include "vm_assert.h"

#include "runDeployPkgInt.h"
#include "deployPkgLog.h"

#define LIBPATH_DEPLOYPKG "/usr/lib/libDeployPkg.so"

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

ToolsDeployPkgError
DeployPkgDeployPkgInGuest(const char* pkgFile, // IN: the package filename
                          char* errBuf,        // OUT: buffer for msg on fail
                          int errBufSize)      // IN: size of errBuf
{
   ToolsDeployPkgError ret = TOOLSDEPLOYPKG_ERROR_SUCCESS;
   void* handle;
   DeployPkgFromFileFn fn;
   DeployPkgSetLogFn logFn;

   /* Init the logger */
   DeployPkgLog_Open();

   /* open the so module */
   handle = dlopen(LIBPATH_DEPLOYPKG, RTLD_LOCAL | RTLD_LAZY);
   if (handle == NULL) {
      const char* error = dlerror();
      Str_Snprintf(errBuf, errBufSize, "Failed to load " LIBPATH_DEPLOYPKG ": %s",
                   error == NULL ? "unknown error" : error);
      DeployPkgLog_Log(3, errBuf);
      DeployPkgLog_Close();
      return TOOLSDEPLOYPKG_ERROR_DEPLOY_FAILED;
   }
   DeployPkgLog_Log(0, LIBPATH_DEPLOYPKG " loaded successfully");


   /* Find the address of the function */
   fn =
      (DeployPkgFromFileFn)dlsym(handle, FNAME_DEPLOYPKGFROMFILE);
   logFn =
      (DeployPkgSetLogFn)dlsym(handle, FNAME_SETLOGGER);

   if (fn == NULL || logFn == NULL) {
      const char* error = dlerror();
      Str_Snprintf(errBuf, errBufSize, "Failed to find symbol in libDeployPkg.so: %s",
                   error == NULL ? "unknown error" : error);
      DeployPkgLog_Log(3, errBuf);
      ret = TOOLSDEPLOYPKG_ERROR_DEPLOY_FAILED;
      goto ExitPoint;
   }

   logFn(DeployPkgLog_Log);

   DeployPkgLog_Log(0, "Found DeployPkg_DeployPackageFromFile");

   DeployPkgLog_Log(0, "Deploying %s", pkgFile);
   if (0 != fn(pkgFile)) {
      Str_Snprintf(errBuf, errBufSize, 
                   "Package deploy failed in DeployPkg_DeployPackageFromFile");
      DeployPkgLog_Log(3, errBuf);
      ret = TOOLSDEPLOYPKG_ERROR_DEPLOY_FAILED;
      goto ExitPoint;
   }
   DeployPkgLog_Log(0, "Ran DeployPkg_DeployPackageFromFile successfully");

ExitPoint:
   dlclose(handle);
   DeployPkgLog_Close();
   return ret;
}
