/*********************************************************
 * Copyright (C) 2011-2016 VMware, Inc. All rights reserved.
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

#ifndef _DEPLOYPKGINT_H_
#define _DEPLOYPKGINT_H_

/**
 * @file deployPkgInt.h
 *
 * Internal definitions for the deployPkg plugin.
 */

#define G_LOG_DOMAIN "deployPkg"
#include <glib.h>
#include "vmware/guestrpc/deploypkg.h"
#include "vmware/tools/guestrpc.h"

G_BEGIN_DECLS

gboolean DeployPkg_TcloBegin(RpcInData *data);
gboolean DeployPkg_TcloDeploy(RpcInData *data);

/* Functions to manage the log */
void DeployPkgLog_Open(void);
void DeployPkgLog_Close(void);
void DeployPkgLog_Log(int level, const char *fmtstr, ...);

G_END_DECLS

#endif /* _DEPLOYPKGINT_H_ */

