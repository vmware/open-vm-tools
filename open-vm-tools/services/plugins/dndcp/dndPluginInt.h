/*********************************************************
 * Copyright (C) 2010-2016 VMware, Inc. All rights reserved.
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
 * dndPluginInt.h
 *
 *    Various DnD plugin defines.
 *
 */

#if !defined (__DNDPLUGIN_INT_H__)
#define __DNDPLUGIN_INT_H__

extern "C" {
   #include "conf.h"
}

#define DEBUG_PREFIX             "vmusr"
#define RPC_POLL_TIME            10
#define POINTER_POLL_TIME        10
#define UNGRABBED_POS (-100)
#define VMWARE_CLIP_FORMAT_NAME     L"VMwareClipFormat"
#define TOOLS_DND_VERSION_3         "tools.capability.dnd_version 3"
#define TOOLS_DND_VERSION_4         "tools.capability.dnd_version 4"
#define QUERY_VMX_DND_VERSION       "vmx.capability.dnd_version"
#define TOOLS_COPYPASTE_VERSION     "tools.capability.copypaste_version"
#define QUERY_VMX_COPYPASTE_VERSION "vmx.capability.copypaste_version"

#endif
