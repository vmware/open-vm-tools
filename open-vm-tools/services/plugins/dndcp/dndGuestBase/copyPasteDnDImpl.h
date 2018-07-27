/*********************************************************
 * Copyright (C) 2010-2018 VMware, Inc. All rights reserved.
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
 * @file copyPasteDnDImpl.h
 *
 * Interface for concrete classes that implement the DnD, Copy paste plugin
 * abstraction. Implementing DnD/CP for a new guest platform involves creating
 * a class that inherits CopyPasteDnDImpl.
 */

#ifndef __COPYPASTEDNDIMPL_H__
#define __COPYPASTEDNDIMPL_H__

#include "vmware.h"
#include "vm_basic_types.h"

extern "C" {
#include "vmware/tools/plugin.h"
}

typedef struct ToolsAppCtx ToolsAppCtx;
class CopyPasteDnDImpl
{
public:
   virtual ~CopyPasteDnDImpl() {};
   virtual gboolean Init(ToolsAppCtx *ctx) = 0;
   virtual void PointerInit() = 0;
   virtual gboolean RegisterCP() = 0;
   virtual void UnregisterCP() = 0;
   virtual gboolean RegisterDnD() = 0;
   virtual void UnregisterDnD() = 0;
   virtual void CopyPasteVersionChanged(const int version) = 0;
   virtual void DnDVersionChanged(const int version) = 0;
   virtual void SetCopyPasteAllowed(bool allowed) = 0;
   virtual void SetDnDAllowed(bool allowed) = 0;
   virtual uint32 GetCaps() = 0;
};

#endif // __COPYPASTEDNDIMPL_H__
