/*********************************************************
 * Copyright (C) 2016-2019 VMware, Inc. All rights reserved.
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

#ifndef _LINUXDEPLOYMENTUTILITIES_H_
#define _LINUXDEPLOYMENTUTILITIES_H_

#include <stdbool.h>
#include "imgcust-common/log.h"
#include "imgcust-common/imgcust-api.h"

IMGCUST_API bool
IsCloudInitEnabled(const char* configFile);

IMGCUST_API char *
GetCustomScript(const char* dirPath);
#endif //_LINUXDEPLOYMENTUTILITIES_H_

