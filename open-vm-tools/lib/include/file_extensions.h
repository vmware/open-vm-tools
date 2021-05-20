/*********************************************************
 * Copyright (C) 1998-2016, 2020-2021 VMware, Inc. All rights reserved.
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

#ifndef _FILE_EXTENSIONS_H_
#define _FILE_EXTENSIONS_H_


#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

/*
 * Virtual disk and related file types.
 */

#define DISK_FILE_EXTENSION "vmdk"
#define REDO_FILE_EXTENSION "REDO"
#define SWAP_FILE_EXTENSION "vswp"

/*
 * VM configuration and related file types.
 */

#define CONFIG_FILE_EXTENSION "vmx"           // VM configuration file
#define CONFIG_ALT_FILE_EXTENSION "cfg"       // Obsolete synonym for .vmx
#define EXTENDED_CONFIG_FILE_EXTENSION "vmxf" // Foundry metadata
#define MANAGED_CONFIG_FILE_EXTENSION "vmxa"  // ACE Top Level
#define VRM_CONFIG_FILE_EXTENSION "vmx"       // ACE Instance
#define VPX_TEMPLATE_EXTENSION "vmtx"         // VirtualCenter template
#define TEAM_FILE_EXTENSION "vmtm"            // Foundry VM team
#define POLICY_FILE_EXTENSION "vmpl"          // ACE/VRM policy file
#define BUNDLE_FILE_EXTENSION "vmwarevm"      // VM configuration bundle directory
#define SIDECAR_FILE_EXTENSION "vmfd"         // Virtual machine filter data aka sidecar
#define HBRPERSIST_FILE_EXTENSION "psf"       // HBR/VR persistent state file

/*
 * Snapshot and related file types.
 */

#define MAINMEM_FILE_EXTENSION "vmem"
#define SUSPEND_FILE_EXTENSION "vmss"
#define CHECKPOINT_FILE_EXTENSION "vmsn"
#define VPLAY_FILE_EXTENSION "vmlog"
#define SNAPSHOT_METADATA_EXTENSION "vmsd"
#define CHECKPOINT_FILE_EXTENSION_OLD "cpt"   // Obsolete synonym for vmsn

/*
 * Foundry scripts.
 */

#define VIX_ACTION_FILE_EXTENSION "vmac"      // Foundry action
#define VIX_BATCH_FILE_EXTENSION "vmba"       // Foundry batch script

/*
 * ACE/VRM management transit files.
 */

#define VRM_HOTFIXREQ_FILE_EXTENSION "vmhr"   // ACE hotfix request
#define VRM_HOTFIX_FILE_EXTENSION "vmhf"      // ACE hotfix response

/*
 * VM Download.
 */

#define RVM_DOWNLOAD_FILE_EXTENSION "vmdownload"    // Downloaded file
#define RVM_STATE_FILE_EXTENSION "vmstate"          // Download metadata
#define DOWNLOAD_BUNDLE_FILE_EXTENSION "vmdownload" // VM download bundle directory

/*
 * Other file types.
 */

#define SCREENSHOT_EXTENSION "png"
#define NVRAM_EXTENSION "nvram"
#define LOCK_FILE_EXTENSION "lck"
#define VIRTUALPC_EXTENSION "vmc"
#define SYMANTEC_LIVESTATE_EXTENSION "sv2i"
#define STORAGECRAFT_SHADOWSTOR_EXTENSION "spf"
#define ACRONIS_EXTENSION "tib"
#define OPEN_VM_FORMAT_EXTENSION "ovf"
#define ARCHIVED_OPEN_VM_FORMAT_EXTENSION "ova"
#define NAMESPACEDB_EXTENSION "db"
#define DATASETSSTORE_DISKMODE_EXTENSION "dsd"
#define DATASETSSTORE_VMMODE_EXTENSION "dsv"
// "xvm" // VMware console configuration file

/*
 * Extensions repeated with leading period.
 * Moved from bora/public/dumper.h.
 */

#define STDPATH_EXT     "." SUSPEND_FILE_EXTENSION
#define CPTPATH_EXT     "." CHECKPOINT_FILE_EXTENSION
#define CPTPATH_EXT_OLD "." CHECKPOINT_FILE_EXTENSION_OLD
#define CONFIG_EXT      "." CONFIG_FILE_EXTENSION
#define CONFIG_EXT_ALT  "." CONFIG_ALT_FILE_EXTENSION
#define CONFIG_EXT_MGD  "." MANAGED_CONFIG_FILE_EXTENSION
#define CONFIG_EXT_TEAM "." TEAM_FILE_EXTENSION
#define VPX_TEMPL_EXT   "." VPX_TEMPLATE_EXTENSION
#define SCREENSHOT_EXT  "." SCREENSHOT_EXTENSION
#define SWAPPATH_EXT    "." SWAP_FILE_EXTENSION


#endif /* _FILE_EXTENSIONS_H_ */
