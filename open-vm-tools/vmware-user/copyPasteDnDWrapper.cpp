/*********************************************************
 * Copyright (C) 2009 VMware, Inc. All rights reserved.
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
 * @file copyPasteDnDWrapper.cpp
 * 
 * This singleton class implements a wrapper around various versions of
 * copy and paste protocols, and provides some convenience functions that
 * help to make VMwareUser a bit cleaner. 
 */

#include "copyPasteDnDWrapper.h"
extern "C" {
#include "vmwareuserInt.h"
#include "debug.h"
}


/**
 * CopyPasteDnDWrapper is a singleton, here is a pointer to its only instance.
 */
CopyPasteDnDWrapper *CopyPasteDnDWrapper::m_instance = 0;

/**
 *
 * Get an instance of CopyPasteDnDWrapper, which is an application singleton.  
 *
 * @return a pointer to the singleton CopyPasteDnDWrapper object, or NULL if
 * for some reason it could not be allocated.
 */

CopyPasteDnDWrapper *
CopyPasteDnDWrapper::GetInstance()
{
   if (!m_instance) {
      m_instance = new CopyPasteDnDWrapper;
   }
   return m_instance;
}


/**
 *
 * Constructor.
 */

CopyPasteDnDWrapper::CopyPasteDnDWrapper() :
#if defined(HAVE_GTKMM)
   m_copyPasteUI(NULL),
#endif
   m_isRegistered(FALSE),
   m_userData(NULL),
   m_version(-1)
{
}


/**
 *
 * Destructor.
 */

CopyPasteDnDWrapper::~CopyPasteDnDWrapper()
{
   if (IsRegistered()) {
      Unregister();
   }
}


/**
 *
 * Attach implementation-specific data (in reality, a GtkWidget * that is
 * needed by the legacy copy paste code. Going forward, any new protocol
 * versions should be implemented as classes, and should not need such a
 * crutch). 
 *
 * @param[in] userData  a GtkWidget created by VMwareUser and used by the
 * legacy copy and paste implementation.
 */

void
CopyPasteDnDWrapper::SetUserData(const void *userData)
{
   Debug("%s: enter %lx\n", __FUNCTION__, (unsigned long) userData);
   m_userData = userData;
}


/**
 *
 * Set block fd.
 *
 * @param[in] blockFd blockFd specified as command line arg by VMwareUser.
 */

void
CopyPasteDnDWrapper::SetBlockFd(int blockFd)
{
   Debug("%s: enter %d\n", __FUNCTION__, blockFd);
   mBlockFd = blockFd;
}


/**
 *
 * Register copy and paste capabilities with the VMX. Try newest version
 * first, then fall back to the legacy implementation.
 *
 * @return TRUE on success, FALSE on failure
 */

bool
CopyPasteDnDWrapper::Register()
{
   Debug("%s: mBlockFd %d\n", __FUNCTION__, mBlockFd);
   if (IsRegistered()) {
      return TRUE;
   }

   /*
    * Try to get version 3, and if that fails, go for the compatibility
    * versions (1 and 2).
    */

#if defined(HAVE_GTKMM)
   Debug("%s: enter\n", __FUNCTION__);
   m_copyPasteUI = new CopyPasteUI();
   if (m_copyPasteUI) {
      Debug("%s: Setting block fd to %d\n", __FUNCTION__, mBlockFd);
      m_copyPasteUI->SetBlockFd(mBlockFd);
      m_copyPasteUI->Init();
      SetIsRegistered(TRUE);
      int version = GetVersion();
      Debug("%s: version is %d\n", __FUNCTION__, version);
      if (version >= 3) {
         m_copyPasteUI->VmxCopyPasteVersionChanged(gRpcIn, version);
         m_copyPasteUI->SetCopyPasteAllowed(TRUE);
      } else {
         Debug("%s: version < 3, unregistering.\n", __FUNCTION__);
         Unregister();
      }
   }
#endif
   if (!IsRegistered()) {
      Debug("%s: Registering legacy m_userData %lx\n", __FUNCTION__, (long unsigned int) m_userData);
      SetIsRegistered(CopyPaste_Register((GtkWidget *)m_userData));
      if (IsRegistered()) {
      Debug("%s: Registering capability\n", __FUNCTION__);
         if (!CopyPaste_RegisterCapability()) {
            Unregister();
         }
      }
   }
   return IsRegistered();
}

/**
 *
 * Unregister copy paste capabilities and do general cleanup.
 */

void
CopyPasteDnDWrapper::Unregister()
{
   if (!IsRegistered()) {
      return;
   }
#if defined(HAVE_GTKMM)
   if (m_copyPasteUI) {
      delete m_copyPasteUI;
      m_copyPasteUI = NULL;
   } else {
#endif
      CopyPaste_Unregister((GtkWidget *)m_userData);
#if defined(HAVE_GTKMM)
   }
#endif
   SetIsRegistered(FALSE);
   m_version = -1;
}


/**
 *
 * Get the version of the copy paste protocol being wrapped.
 *
 * @return copy paste protocol version. 
 */

int
CopyPasteDnDWrapper::GetVersion()
{
   if (IsRegistered()) {
      m_version = CopyPaste_GetVmxCopyPasteVersion();
   }
   Debug("%s: got version %d\n", __FUNCTION__, m_version);
   return m_version;
}

/**
 *
 * Set a flag indicating that we are wrapping an initialized and registered
 * copy paste implementation, or not.
 *
 * @param[in] isRegistered If TRUE, protocol is registered, otherwise FALSE. 
 */

void
CopyPasteDnDWrapper::SetIsRegistered(bool isRegistered)
{
   m_isRegistered = isRegistered;
}

/**
 *
 * Get the flag indication that we are wrapping an initialized and registered
 * copy paste implementation, or not.
 *
 * @return TRUE if copy paste is initialized, otherwise FALSE. 
 */

bool
CopyPasteDnDWrapper::IsRegistered()
{
   return m_isRegistered;
}


/**
 *
 * Handle reset by calling protocol dependent handlers.
 */

void
CopyPasteDnDWrapper::OnReset()
{
   Debug("%s: enter\n", __FUNCTION__);
   if (IsRegistered()) {
      Unregister();
   }
   Register();
   if (!IsRegistered()) {
      Debug("%s: unable to reset!\n", __FUNCTION__);
   }
}


