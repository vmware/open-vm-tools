/*********************************************************
 * Copyright (C) 2009-2016 VMware, Inc. All rights reserved.
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
 * DeployPkg_DeployPackageFromFile --
 *
 *      C-style wrapper to decode a package from a file, extract its payload,
 *      expand the payload into a temporary directory, and then execute
 *      the command specified in the package.
 *
 * @param file IN: the package file
 * @return 0 on success
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
