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
 * @guestDnDCPMgr.hh --
 *
 * Common layer management object for guest DnD/CP. It is a singleton.
 */

#ifndef GUEST_DND_CP_HH
#define GUEST_DND_CP_HH

#include "guestDnD.hh"
#include "guestCopyPaste.hh"
#include "guestFileTransfer.hh"

struct DblLnkLst_Links;
struct ToolsAppCtx;

class GuestDnDCPMgr
{
public:
   virtual ~GuestDnDCPMgr();
   static GuestDnDCPMgr *GetInstance(void);
   static void Destroy();
   virtual GuestDnDMgr *GetDnDMgr(void) { return NULL; }
   virtual GuestCopyPasteMgr *GetCopyPasteMgr(void) { return NULL; }
   virtual DnDCPTransport *GetTransport(void) { return NULL; }
   void StartLoop();
   void EndLoop();
   void IterateLoop();
   virtual void Init(ToolsAppCtx *ctx) { }
   void SetCaps(uint32 caps) {mLocalCaps = caps;};
   uint32 GetCaps() {return mLocalCaps;};

protected:
   /* We're a singleton, so it is a compile time error to call these. */
   GuestDnDCPMgr(void);
   GuestDnDCPMgr(const GuestDnDCPMgr &mgr);
   GuestDnDCPMgr& operator=(const GuestDnDCPMgr &mgr);

   static GuestDnDCPMgr *m_instance;
   GuestDnDMgr *mDnDMgr;
   GuestCopyPasteMgr *mCPMgr;
   GuestFileTransfer *mFileTransfer;
   DnDCPTransport *mTransport;
   uint32 mLocalCaps;
};

#endif // GUEST_DND_CP_HH

