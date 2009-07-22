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
 * copy and paste and dnd protocols, and provides some convenience functions
 * that help to make VMwareUser a bit cleaner.
 */

#include "copyPasteDnDWrapper.h"

extern "C" {
#include "vmwareuserInt.h"
#include "debug.h"
#include "dndGuest.h"
#include "unity.h"
}

class DragDetWnd;

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

#if defined(HAVE_GTKMM)
extern "C" {

/**
 *
 * Enter or leave unity mode.
 *
 * @param[in] mode enter unity mode if TRUE, else leave.
 */

void
CopyPasteDnDWrapper_SetUnityMode(Bool mode)
{
   CopyPasteDnDWrapper *wrapper = CopyPasteDnDWrapper::GetInstance();

   if (wrapper) {
      wrapper->SetUnityMode(mode);
   }
}

}
#endif

/**
 *
 * Constructor.
 */

CopyPasteDnDWrapper::CopyPasteDnDWrapper() :
#if defined(HAVE_GTKMM)
   m_copyPasteUI(NULL),
   m_dndUI(NULL),
#endif
   m_isCPRegistered(FALSE),
   m_isDnDRegistered(FALSE),
   m_userData(NULL),
   m_cpVersion(-1),
   m_dndVersion(-1),
   m_isLegacy(false),
   m_hgWnd(NULL),
   m_ghWnd(NULL),
   m_eventQueue(NULL)
{
}


/**
 *
 * Destructor.
 */

CopyPasteDnDWrapper::~CopyPasteDnDWrapper()
{
   if (IsCPRegistered()) {
      UnregisterCP();
   }
   if (IsDnDRegistered()) {
      UnregisterDnD();
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
CopyPasteDnDWrapper::SetBlockControl(DnDBlockControl *blockCtrl)
{
   Debug("%s: enter %p (%d)\n", __func__, blockCtrl, blockCtrl->fd);
   m_blockCtrl = blockCtrl;
}


/**
 *
 * Register copy and paste capabilities with the VMX. Try newest version
 * first, then fall back to the legacy implementation.
 *
 * @return TRUE on success, FALSE on failure
 */

bool
CopyPasteDnDWrapper::RegisterCP()
{
   Debug("%s: m_blockCtrl %p\n", __func__, m_blockCtrl);
   if (IsCPRegistered()) {
      return TRUE;
   }

   /*
    * Try to get version 3, and if that fails, go for the compatibility
    * versions (1 and 2).
    */

#if defined(HAVE_GTKMM)
   if (!IsCPRegistered()) {
      m_copyPasteUI = new CopyPasteUI();
      if (m_copyPasteUI) {
         Debug("%s: Setting block control to %p (fd %d)\n",
               __func__, m_blockCtrl, m_blockCtrl->fd);
         m_copyPasteUI->SetBlockControl(m_blockCtrl);
         if (m_copyPasteUI->Init()) {
            SetCPIsRegistered(TRUE);
            int version = GetCPVersion();
            Debug("%s: version is %d\n", __FUNCTION__, version);
            if (version >= 3) {
               m_copyPasteUI->VmxCopyPasteVersionChanged(gRpcIn, version);
               m_copyPasteUI->SetCopyPasteAllowed(TRUE);
               m_isLegacy = false;
            } else {
               Debug("%s: version < 3, unregistering.\n", __FUNCTION__);
               UnregisterCP();
            }
         } else {
            delete m_copyPasteUI;
            m_copyPasteUI = NULL;
         }
      }
   }

#endif
   if (!IsCPRegistered()) {
      Debug("%s: Registering legacy m_userData %lx\n",
            __func__, (long unsigned int) m_userData);
      SetCPIsRegistered(CopyPaste_Register((GtkWidget *)m_userData));
      if (IsCPRegistered()) {
         Debug("%s: Registering capability\n", __FUNCTION__);
         if (!CopyPaste_RegisterCapability()) {
            UnregisterCP();
         }
         else {
            m_isLegacy = true;
         }
      }
   }

   return IsCPRegistered();
}


/**
 *
 * Register DnD capabilities with the VMX. Try newest version
 * first, then fall back to the legacy implementation.
 *
 * @return TRUE on success, FALSE on failure
 */

bool
CopyPasteDnDWrapper::RegisterDnD()
{
   /*
    * Try to get version 3, and if that fails, go for the compatibility
    * versions (1 and 2).
    */

#if defined(HAVE_GTKMM)
   if (!IsDnDRegistered()) {
      m_dndUI = new DnDUI(m_eventQueue);
      if (m_dndUI) {
         Debug("%s: Setting block control to %p (fd %d)\n",
               __func__, m_blockCtrl, m_blockCtrl->fd);
         m_dndUI->SetBlockControl(m_blockCtrl);

         if (m_dndUI->Init()) {
            UnityDnD state;
            state.detWnd = m_dndUI->GetDetWndAsWidget();
            state.setMode = CopyPasteDnDWrapper_SetUnityMode;
            Unity_SetActiveDnDDetWnd(&state);

            SetDnDIsRegistered(TRUE);
            int version = GetDnDVersion();
            Debug("%s: dnd version is %d\n", __FUNCTION__, version);
            if (version >= 3) {
               Debug("%s: calling VmxDnDVersionChanged (version %d) and SetDnDAllowed\n",
                     __FUNCTION__, version);
               m_dndUI->VmxDnDVersionChanged(gRpcIn, version);
               m_dndUI->SetDnDAllowed(TRUE);
               m_isLegacy = false;
            } else {
               Debug("%s: version < 3, unregistering.\n", __FUNCTION__);
               UnregisterDnD();
            }
         } else {
            delete m_dndUI;
            m_dndUI = NULL;
         }
      }
   }

#endif
   if (!IsDnDRegistered()) {
      Debug("%s: legacy registering dnd capability\n", __FUNCTION__);
      if (m_isLegacy) {
         SetDnDIsRegistered(DnD_Register(m_hgWnd, m_ghWnd));
         if (IsDnDRegistered()) {
            Debug("%s: setting up detwnd for Unity\n", __FUNCTION__);
            UnityDnD state;
            state.detWnd = m_ghWnd;
            state.setMode = DnD_SetMode;
            Unity_SetActiveDnDDetWnd(&state);
         }
      }
   } else if (m_isLegacy && DnD_GetVmxDnDVersion() > 1) {
      Debug("%s: legacy registering dnd capability\n", __FUNCTION__);
      if (!DnD_RegisterCapability()) {
         Debug("%s: legacy unable to register dnd capability\n", __FUNCTION__);
         UnregisterDnD();
      }
   }
   Debug("%s: dnd is registered? %d\n", __FUNCTION__, (int) IsDnDRegistered());
   return IsDnDRegistered();
}


/**
 *
 * Unregister copy paste capabilities and do general cleanup.
 */

void
CopyPasteDnDWrapper::UnregisterCP()
{
   Debug("%s: enter\n", __FUNCTION__);
   if (IsCPRegistered()) {
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
      SetCPIsRegistered(FALSE);
      m_cpVersion = -1;
   }
}


/**
 *
 * Unregister DnD capabilities and do general cleanup.
 */

void
CopyPasteDnDWrapper::UnregisterDnD()
{
   Debug("%s: enter\n", __FUNCTION__);
   if (IsDnDRegistered()) {
      if (m_isLegacy) {
         DnD_Unregister(m_hgWnd, m_ghWnd);
#if defined(HAVE_GTKMM)
      } else if (m_dndUI) {
         delete m_dndUI;
         m_dndUI = NULL;
#endif
      }
      m_dndVersion = -1;
      SetDnDIsRegistered(false);
      return;
   }
}


/**
 *
 * Get the version of the copy paste protocol being wrapped.
 *
 * @return copy paste protocol version.
 */

int
CopyPasteDnDWrapper::GetCPVersion()
{
   if (IsCPRegistered()) {
      m_cpVersion = CopyPaste_GetVmxCopyPasteVersion();
   }
   Debug("%s: got version %d\n", __FUNCTION__, m_cpVersion);
   return m_cpVersion;
}


/**
 *
 * Get the version of the DnD protocol being wrapped.
 *
 * @return DnD protocol version.
 */

int
CopyPasteDnDWrapper::GetDnDVersion()
{
   if (IsDnDRegistered()) {
      m_dndVersion = DnD_GetVmxDnDVersion();
   }
   Debug("%s: got version %d\n", __FUNCTION__, m_dndVersion);
   return m_dndVersion;
}


/**
 *
 * Set a flag indicating that we are wrapping an initialized and registered
 * copy paste implementation, or not.
 *
 * @param[in] isRegistered If TRUE, protocol is registered, otherwise FALSE.
 */

void
CopyPasteDnDWrapper::SetCPIsRegistered(bool isRegistered)
{
   m_isCPRegistered = isRegistered;
}


/**
 *
 * Get the flag indicating that we are wrapping an initialized and registered
 * copy paste implementation, or not.
 *
 * @return TRUE if copy paste is initialized, otherwise FALSE.
 */

bool
CopyPasteDnDWrapper::IsCPRegistered()
{
   return m_isCPRegistered;
}


/**
 *
 * Set a flag indicating that we are wrapping an initialized and registered
 * DnD implementation, or not.
 *
 * @param[in] isRegistered If TRUE, protocol is registered, otherwise FALSE.
 */

void
CopyPasteDnDWrapper::SetDnDIsRegistered(bool isRegistered)
{
   m_isDnDRegistered = isRegistered;
}


/**
 *
 * Get the flag indicating that we are wrapping an initialized and registered
 * DnD implementation, or not.
 *
 * @return TRUE if DnD is initialized, otherwise FALSE.
 */

bool
CopyPasteDnDWrapper::IsDnDRegistered()
{
   return m_isDnDRegistered;
}


/**
 *
 * Handle reset by calling protocol dependent handlers.
 */

void
CopyPasteDnDWrapper::OnReset()
{
   Debug("%s: enter\n", __FUNCTION__);
   if (IsDnDRegistered()) {
      UnregisterDnD();
   }
   if (IsCPRegistered()) {
      UnregisterCP();
   }
   if (!IsCPRegistered()) {
      RegisterCP();
   }
   if (!IsDnDRegistered()) {
      RegisterDnD();
   }
   if (!IsDnDRegistered() || !IsCPRegistered()) {
      Debug("%s: unable to reset fully!\n", __FUNCTION__);
   }
   if (m_isLegacy) {
      if (IsCPRegistered()) {
         CopyPaste_OnReset();
      }
      if (IsDnDRegistered()) {
         DnD_OnReset(m_hgWnd, m_ghWnd);
      }
   }
}

