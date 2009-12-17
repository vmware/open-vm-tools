/*********************************************************
 * Copyright (C) 2009 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 *********************************************************/

/*********************************************************
 * The contents of this file are subject to the terms of the Common
 * Development and Distribution License (the "License") version 1.0
 * and no later version.  You may not use this file except in
 * compliance with the License.
 *
 * You can obtain a copy of the License at
 *         http://www.opensource.org/licenses/cddl1.php
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 *********************************************************/

#ifndef _NPA_DEFS_H
#define _NPA_DEFS_H

#define NPA_PLUGIN_NUMPAGES      64
#define NPA_MEMIO_NUMPAGES       32
#define VMXNET3_NPA_CMD_SUCCESS   0
#define VMXNET3_NPA_CMD_FAILURE   1
#define VMXNET3_PLUGIN_INFO_LEN  32
// XXX: unify the definitions
#define VMXNET3_MAX_TX_DESC_SIZE 256
#define VMXNET3_MAX_RX_DESC_SIZE 256
#define VMXNET3_MAX_TX_RINGS 4
#define VMXNET3_MAX_RX_RINGS 4

/* these structure are versioned using the vmxnet3 version */

typedef 
#include "vmware_pack_begin.h"
struct NPA_PluginPages {
   uint64 vaddr;
   uint32 numPages;
   PPN64  pages[NPA_PLUGIN_NUMPAGES];
}
#include "vmware_pack_end.h"
NPA_PluginPages;

typedef
#include "vmware_pack_begin.h"
struct NPA_MemioPages {
   PPN64  startPPN;
   uint32 numPages;
}
#include "vmware_pack_end.h"
NPA_MemioPages;

typedef
#include "vmware_pack_begin.h"
struct NPA_PluginConf {
   NPA_PluginPages   pluginPages;
   NPA_MemioPages    memioPages;
   uint64            entryVA;    // address of entry function in the plugin 
   uint32            deviceInfo[VMXNET3_PLUGIN_INFO_LEN]; // opaque data returned by PF driver
}
#include "vmware_pack_end.h"
NPA_PluginConf;


/* vmkernel and device backend shared definitions */

#define VMXNET3_PLUGIN_NAME_LEN  256
#define NPA_MEMIO_REGIONS_MAX    6

typedef uint32 VF_ID;

typedef struct Vmxnet3_VFInfo {
   char     pluginName[VMXNET3_PLUGIN_NAME_LEN];
   uint32   deviceInfo[VMXNET3_PLUGIN_INFO_LEN];    // opaque data returned by PF driver
   MA       memioAddr;
   uint32   memioLen;
} Vmxnet3_VFInfo;

#endif // _NPA_DEFS_H
