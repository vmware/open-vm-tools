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

/**
 * @GuestDnDCPMgr.cc --
 *
 * Implementation of common layer GuestDnDCPMgr object.
 */

#include "guestDnDCPMgr.hh"
#include "dndCPTransportGuestRpc.hpp"

extern "C" {
   #include "debug.h"
   #include "guestApp.h"
}

GuestDnDCPMgr *GuestDnDCPMgr::m_instance = NULL;


/**
 *
 * Constructor.
 */

GuestDnDCPMgr::GuestDnDCPMgr()
   : mDnDMgr(NULL),
     mCPMgr(NULL),
     mFileTransfer(NULL),
     mTransport(NULL),
     mToolsAppCtx(NULL),
     mLocalCaps(0xffffffff)
{
}


/**
 * Destructor.
 */

GuestDnDCPMgr::~GuestDnDCPMgr(void)
{
   g_debug("%s: enter\n", __FUNCTION__);
   delete mDnDMgr;
   mDnDMgr = NULL;
   delete mFileTransfer;
   mFileTransfer = NULL;
   delete mTransport;
   mTransport = NULL;
}


/**
 * Get an instance of GuestDnDCPMgr, which is an application singleton.
 *
 * @return a pointer to the singleton GuestDnDCPMgr object, or NULL if
 * for some reason it could not be allocated.
 */

GuestDnDCPMgr *
GuestDnDCPMgr::GetInstance(void)
{
   if (!m_instance) {
      m_instance = new GuestDnDCPMgr;
   }
   return m_instance;
}


/**
 * Destroy the GuestDnDCPMgr singleton.
 */

void
GuestDnDCPMgr::Destroy(void)
{
   if (m_instance) {
      delete m_instance;
      m_instance = NULL;
   }
}


/**
 * Initialize the GuestDnDCPMgr object. All owner should call this first before
 * calling any other function.
 *
 * @param[in] ctx ToolsAppCtx
 */

void
GuestDnDCPMgr::Init(ToolsAppCtx *ctx)
{
   mToolsAppCtx = ctx;

   ASSERT(mToolsAppCtx);

   if (mFileTransfer) {
      delete mFileTransfer;
   }
   mFileTransfer = new GuestFileTransfer(GetTransport());
}


/**
 * Get the GuestDnDCPMgr object.
 *
 * @return a pointer to the GuestDnDCPMgr instance.
 */

GuestDnDMgr *
GuestDnDCPMgr::GetDnDMgr(void)
{
   if (!mDnDMgr) {
      /* mEventQueue must be set before this call. */
      mDnDMgr = new GuestDnDMgr(GetTransport(), mToolsAppCtx);
   }
   return mDnDMgr;
}


/**
 * Get the GuestCopyPasteMgr object.
 *
 * @return a pointer to the GuestCopyPasteMgr instance.
 */

GuestCopyPasteMgr *
GuestDnDCPMgr::GetCopyPasteMgr(void)
{
   if (!mCPMgr) {
      mCPMgr = new GuestCopyPasteMgr(GetTransport());
   }
   return mCPMgr;
}


/**
 * Get the DnDCPTransport object.
 *
 * XXX Implementation here is temporary and should be replaced with rpcChannel.
 *
 * @return a pointer to the manager's DnDCPTransport instance.
 */

DnDCPTransport *
GuestDnDCPMgr::GetTransport(void)
{
   if (!mTransport) {
      ASSERT(mToolsAppCtx);
      mTransport = new DnDCPTransportGuestRpc(mToolsAppCtx->rpc);
   }
   return mTransport;
}


/**
 * API for starting the transport main loop from python.
 */

void
GuestDnDCPMgr::StartLoop()
{
   (void) GetTransport();
   if (mTransport) {
      mTransport->StartLoop();
   }
}


/**
 * API for iterating the transport main loop from python.
 */

void
GuestDnDCPMgr::IterateLoop()
{
   (void) GetTransport();
   if (mTransport) {
      mTransport->IterateLoop();
   }
}


/**
 * API for ending the transport main loop from python.
 */

void
GuestDnDCPMgr::EndLoop()
{
   (void) GetTransport();
   if (mTransport) {
      mTransport->EndLoop();
   }
}
