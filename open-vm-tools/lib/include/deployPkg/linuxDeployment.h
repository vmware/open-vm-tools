/*********************************************************
 * Copyright (C) 2009-2019 VMware, Inc. All rights reserved.
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
 * linuxDeployment.h --
 *
 *      C interface to package deployment.
 */

#ifndef LINUX_DEPLOYMENT_H
#define LINUX_DEPLOYMENT_H

#include "vm_basic_types.h"

#include "imgcust-common/log.h"
#include "imgcust-common/imgcust-api.h"

#define DEPLOYPKG_PROCESSTIMEOUT_DEFAULT 100

typedef enum {
   DEPLOYPKG_STATUS_SUCCESS,
   DEPLOYPKG_STATUS_CLOUD_INIT_DELEGATED,
   DEPLOYPKG_STATUS_ERROR,
   DEPLOYPKG_STATUS_CAB_ERROR
} DeployPkgStatus;

/*
 *------------------------------------------------------------------------------
 *
 * DeployPkg_SetTimeout --
 *
 *      Give the deploy package an application specific timeout value.
 *      Package deployment engines such as tools-deployPkg-plugin or standalone program
 * linuxDeployPkg can call this API to set gProcessTimeout.
 *      This API should be called before DeployPkg_DeployPackageFromFile or
 * DeployPkg_DeployPackageFromFileEx.
 *      If the package header includs valid 'timeout' value, then that value will
 * overwrite gProcessTimeout.
 *      If no valid 'timeout' value from both package header and deployment engine, then
 * default value 100s will be used.
 *
 * @param logger [in]
 *      timeout value to be used for process execution period control
 *
 *------------------------------------------------------------------------------
 */

IMGCUST_API void
DeployPkg_SetProcessTimeout(uint16 timeout);


/*
 *------------------------------------------------------------------------------
 *
 * DeployPkg_SetLogger --
 *
 *      Give the deploy package an application specific logger.
 *
 * @param logger [in] logger to be used for deploy operation
 *
 *------------------------------------------------------------------------------
 */

IMGCUST_API void
DeployPkg_SetLogger(LogFunction log);


/*
 *------------------------------------------------------------------------------
 *
 * DeployPkg_DeployPackageFromFileEx --
 *
 *      C-style wrapper to decode a package from a file, extract its payload,
 *      expand the payload into a temporary directory, and then execute
 *      the command specified in the package.
 *
 * @param file IN: the package file
 * @return DEPLOYPKG_STATUS_SUCCESS on success
 *         DEPLOYPKG_STATUS_CLOUD_INIT_DELEGATED if customization task is
 *         delegated to cloud-init
 *         DEPLOYPKG_STATUS_ERROR on failure
 *
 *------------------------------------------------------------------------------
 */

IMGCUST_API DeployPkgStatus
DeployPkg_DeployPackageFromFileEx(const char* file);


/*
 *------------------------------------------------------------------------------
 *
 * DeployPkg_DeployPackageFromFile --
 *
 *      C-style wrapper to decode a package from a file, extract its payload,
 *      expand the payload into a temporary directory, and then execute
 *      the command specified in the package.
 *
 * @param file IN: the package file
 * @return 0 on success, -1 on failure
 *
 *------------------------------------------------------------------------------
 */

IMGCUST_API int
DeployPkg_DeployPackageFromFile(const char* file);


/*
 *------------------------------------------------------------------------------
 *
 * ExtractCabPackage --
 *
 *      C-style wrapper to extract a package from a file using libmspack.
 *
 * @param[in]  cabFileName  the Cabinet file's path
 * @param[in]  destDir  a destination directory where to uncab
 *
 * @return TRUE on success, otherwise - FALSE.
 *
 *------------------------------------------------------------------------------
 */

IMGCUST_API Bool
ExtractCabPackage(const char* cabFileName, const char* destDir);

#endif // LINUX_DEPLOYMENT_H
