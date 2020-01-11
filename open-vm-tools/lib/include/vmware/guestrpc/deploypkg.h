/*********************************************************
 * Copyright (C) 2006-2019 VMware, Inc. All rights reserved.
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

#ifndef __TOOLS_DEPLOYPKG_H_
#define __TOOLS_DEPLOYPKG_H_

#define INCLUDE_ALLOW_USERLEVEL
#include "includeCheck.h"


/*
 * deploypkg.h
 *
 *   -- Define constants related to tools package deployment.
 */

typedef enum {
   TOOLSDEPLOYPKG_IDLE = 0,
   TOOLSDEPLOYPKG_PENDING,
   TOOLSDEPLOYPKG_COPYING,
   TOOLSDEPLOYPKG_DEPLOYING,
   TOOLSDEPLOYPKG_RUNNING,
   TOOLSDEPLOYPKG_DONE
} ToolsDeployPackageState;

typedef enum {
   TOOLSDEPLOYPKG_ERROR_SUCCESS = 0,
   TOOLSDEPLOYPKG_ERROR_NOT_SUPPORT,       // Old tools do not support option
   TOOLSDEPLOYPKG_ERROR_PKG_NOT_FOUND,     // Specified pkg is not found
   TOOLSDEPLOYPKG_ERROR_RPC_INVALID,
   TOOLSDEPLOYPKG_ERROR_COPY_FAILED,
   TOOLSDEPLOYPKG_ERROR_DEPLOY_FAILED,
   TOOLSDEPLOYPKG_ERROR_CUST_SCRIPT_DISABLED // User defined script is disabled
} ToolsDeployPkgError;

#define QUERY_NICS_SUPPORTED  "queryNicsSupported"
#define NICS_STATUS_CONNECTED "connected"

#endif //__TOOLS_DEPLOYPKG_H_
