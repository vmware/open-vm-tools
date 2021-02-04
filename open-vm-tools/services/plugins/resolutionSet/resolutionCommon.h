/*********************************************************
 * Copyright (C) 2016-2021 VMware, Inc. All rights reserved.
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
/* Authors:
 * Thomas Hellstrom <thellstrom@vmware.com>
 */

#ifndef _RESOLUTION_COMMON_H_
#define _RESOLUTION_COMMON_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_DISTRIBUTE
#include "includeCheck.h"
#include "vmware/tools/plugin.h"
#ifdef ENABLE_RESOLUTIONKMS

int resolutionCheckForKMS(ToolsAppCtx *ctx);
void resolutionDRMClose(int fd);

#else

static inline int resolutionCheckForKMS(ToolsAppCtx *ctx)
{
    return -1;
}

static inline void resolutionDRMClose(int fd) {}

#endif /* !ENABLE_RESOLUTIONKMS */

#endif /*  _RESOLUTION_COMMON_H_ */
