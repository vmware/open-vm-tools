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
 * @GuestDnDCPMgr.cc --
 *
 * Implementation of common layer GuestDnDCPMgr object.
 */

#include "guestDnDCPMgr.hh"

// The MACRO DND_VM is only used for WS/FS
#ifdef DND_VM
#include "vmGuestDnDCPMgr.hh"
#else
#include "crtGuestDnDCPMgr.hh"
#endif

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
     mLocalCaps(0xffffffff)
{
}


/**
 * Destructor.
 */

GuestDnDCPMgr::~GuestDnDCPMgr(void)
{
   g_debug("%s: enter\n", __FUNCTION__);
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
#ifdef DND_VM
      m_instance = VMGuestDnDCPMgr::CreateInstance();
#else
      m_instance = CRTGuestDnDCPMgr::CreateInstance();
#endif
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
