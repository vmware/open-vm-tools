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
 * @vmGuestDnDCPMgr.cc --
 *
 * The inherited implementation of common class GuestDnDCPMgr in VM side.
 */

#include "dndCPTransportGuestRpc.hpp"
#include "vmGuestDnDCPMgr.hh"
#include "vmGuestDnDMgr.hh"
#include "vmGuestFileTransfer.hh"


/**
 *
 * Destructor.
 */

VMGuestDnDCPMgr::~VMGuestDnDCPMgr()
{
   g_debug("%s: enter.\n", __FUNCTION__);
   delete mDnDMgr;
   mDnDMgr = NULL;
   delete mFileTransfer;
   mFileTransfer = NULL;
   delete mTransport;
   mTransport = NULL;
}


/**
 * Initialize the VMGuestDnDCPMgr object. All owner should call this first before
 * calling any other function.
 *
 * @param[in] ctx ToolsAppCtx
 */

void
VMGuestDnDCPMgr::Init(ToolsAppCtx *ctx)
{
   mToolsAppCtx = ctx;

   ASSERT(mToolsAppCtx);

   if (mFileTransfer) {
      delete mFileTransfer;
   }
   mFileTransfer = new VMGuestFileTransfer(GetTransport());
}


/**
 * Get the DnDCPTransport object.
 *
 * XXX Implementation here is temporary and should be replaced with rpcChannel.
 *
 * @return a pointer to the manager's DnDCPTransport instance.
 */

DnDCPTransport *
VMGuestDnDCPMgr::GetTransport(void)
{
   if (!mTransport) {
      ASSERT(mToolsAppCtx);
      mTransport = new DnDCPTransportGuestRpc(mToolsAppCtx->rpc);
   }
   return mTransport;
}


/**
 * Get the GuestDnDMgr object.
 *
 * @return a pointer to the GuestDnDMgr instance.
 */

GuestDnDMgr *
VMGuestDnDCPMgr::GetDnDMgr(void)
{
   if (!mDnDMgr) {
      /* mEventQueue must be set before this call. */
      mDnDMgr = new VMGuestDnDMgr(GetTransport(), mToolsAppCtx);
   }
   return mDnDMgr;
}


/**
 * Create an instance of class VMGuestDnDCPMgr.
 *
 * @return a pointer to the VMGuestDnDCPMgr instance.
 */

VMGuestDnDCPMgr *
VMGuestDnDCPMgr::CreateInstance(void)
{
   return new VMGuestDnDCPMgr();
}


/**
 * Get the GuestCopyPasteMgr object.
 *
 * @return a pointer to the GuestCopyPasteMgr instance.
 */

GuestCopyPasteMgr *
VMGuestDnDCPMgr::GetCopyPasteMgr(void)
{
   if (!mCPMgr) {
      mCPMgr = new GuestCopyPasteMgr(GetTransport());
   }
   return mCPMgr;
}
