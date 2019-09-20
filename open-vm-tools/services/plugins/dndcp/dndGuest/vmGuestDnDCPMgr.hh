/*********************************************************
 * Copyright (C) 2018 VMware, Inc. All rights reserved.
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
 * @vmGuestDnDCPMgr.hh --
 *
 * The inherited implementation of common class GuestDnDCPMgr in VM side.
 */

#ifndef VM_GUEST_DND_CP_MGR_HH
#define VM_GUEST_DND_CP_MGR_HH

#include "guestDnDCPMgr.hh"

extern "C" {
   #include "vmware/tools/guestrpc.h"
   #include "vmware/tools/plugin.h"
}

class VMGuestDnDCPMgr
   : public GuestDnDCPMgr
{
public:
   virtual ~VMGuestDnDCPMgr();
   void Init(ToolsAppCtx *ctx);
   virtual GuestDnDMgr *GetDnDMgr(void);
   virtual DnDCPTransport *GetTransport(void);
   virtual GuestCopyPasteMgr *GetCopyPasteMgr(void);

   static VMGuestDnDCPMgr *CreateInstance(void);

private:
   VMGuestDnDCPMgr(void) : GuestDnDCPMgr() { }
   VMGuestDnDCPMgr(const VMGuestDnDCPMgr &mgr);
   VMGuestDnDCPMgr& operator=(const VMGuestDnDCPMgr &mgr);

private:
   ToolsAppCtx *mToolsAppCtx;
};

#endif // VM_GUEST_DND_CP_MGR_HH
