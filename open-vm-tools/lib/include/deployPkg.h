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
 * deployPkg.h --
 *
 *    Deploy a package into the guests through tools.
 *
 */


#ifndef __TOOLS_DEPLOY_H__
#define __TOOLS_DEPLOY_H__


#if defined(VMTOOLS_USE_GLIB)
#  include "rpcChannel.h"
#else
#  include "rpcin.h"

void DeployPkg_Register(RpcIn *in); // IN

#endif

/* TCLO handlers */
Bool DeployPkg_TcloBegin(RpcInData *data);
Bool DeployPkg_TcloDeploy(RpcInData *data);

#endif /* __TOOLS_DEPLOY_H__ */
