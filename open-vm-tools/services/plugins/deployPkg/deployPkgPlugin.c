/*********************************************************
 * Copyright (C) 2008-2016 VMware, Inc. All rights reserved.
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

/**
 * @file deployPkgPlugin.c
 *
 * Wrapper around the deployPkg library for doing image customization.
 */

#include <stdlib.h>

#include "vm_basic_defs.h"
#include "deployPkgInt.h"
#include "vmware/tools/plugin.h"
#include "vmware/tools/utils.h"

#include "vm_version.h"
#include "embed_version.h"
#include "vmtoolsd_version.h"
VM_EMBED_VERSION(VMTOOLSD_VERSION_STRING);


/**
 * Called by the Tools service when loading the deployPkg plugin.
 *
 * @param[in]  ctx   The application context.
 *
 * @return Registration data for the plugin.
 */

TOOLS_MODULE_EXPORT ToolsPluginData *
ToolsOnLoad(ToolsAppCtx *ctx)
{
   static ToolsPluginData regData = {
      "deployPkg",
      NULL,
      NULL
   };

   RpcChannelCallback rpcs[] = {
      { "deployPkg.begin", DeployPkg_TcloBegin, NULL, NULL, NULL, 0 },
      { "deployPkg.deploy", DeployPkg_TcloDeploy, NULL, NULL, NULL, 0 }
   };
   ToolsAppReg regs[] = {
      { TOOLS_APP_GUESTRPC, VMTools_WrapArray(rpcs, sizeof *rpcs, ARRAYSIZE(rpcs)) }
   };

   srand(time(NULL));
   regData.regs = VMTools_WrapArray(regs, sizeof *regs, ARRAYSIZE(regs));

   return &regData;
}

