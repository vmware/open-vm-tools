/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/

/*
 * runDeployPkgInt.h --
 *
 *    c interface to load the deployPkg library and call the
 *    DeployPkg_DeployPackageFromFile export
 */

#ifndef _RUN_DEPLOYPKG_INT_H
#define _RUN_DEPLOYPKG_INT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "vm_basic_types.h"
#include "toolsDeployPkg.h"

/*
 * DeployPkg logger. In order to add the LogFunction pointer definition
 * to tools, we'd need to populate the component before the service
 * build, which would force anyone building tools to connect to the
 * component repo. To avoid that, duplicate the definition here:
 */
typedef void (*DeployPkgLogFunction) (int level, const char *fmtstr, ...);

/* Function pointers for exports in dll/so */
typedef int (*DeployPkgFromFileFn)(const char*);
typedef void (*DeployPkgSetLogFn)(DeployPkgLogFunction);

/* Decorated function names from deployPkg.dll */
#define FNAME_DEPLOYPKGFROMFILE "DeployPkg_DeployPackageFromFile"
#define FNAME_SETLOGGER "DeployPkg_SetLogger"

ToolsDeployPkgError
DeployPkgDeployPkgInGuest(const char* pkgFile, // IN: the package filename
                          char* errBuf,        // OUT: buffer for msg on fail
                          int errBufSize);     // IN: size of errBuf

#ifdef __cplusplus
}
#endif

#endif // _RUN_DEPLOYPKG_INT_H

