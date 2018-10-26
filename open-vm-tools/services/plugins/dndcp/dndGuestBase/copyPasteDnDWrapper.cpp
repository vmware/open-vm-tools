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
 * @file copyPasteDnDWrapper.cpp
 *
 * This singleton class implements a wrapper around various versions of
 * copy and paste and dnd protocols, and provides some convenience functions
 * that help to make VMwareUser a bit cleaner.
 */

#define G_LOG_DOMAIN "dndcp"

#if defined(HAVE_GTKMM)
#include "copyPasteDnDX11.h"
#endif

#if defined(WIN32)
#include "copyPasteDnDWin32.h"
#ifdef DND_VM
#include "vmCopyPasteDnDWin32.h"
#else
#include "crtCopyPasteDnDWin32.h"
#endif
#endif

#if defined(__APPLE__)
#include "copyPasteDnDMac.h"
#endif

#ifdef DND_VM
#include "vmCopyPasteDnDWrapper.h"
#else
#include "crtCopyPasteDnDWrapper.h"
#endif

#include "copyPasteDnDWrapper.h"
#include "guestDnDCPMgr.hh"
#include "vmware.h"


/**
 * CopyPasteDnDWrapper is a singleton, here is a pointer to its only instance.
 */

CopyPasteDnDWrapper *CopyPasteDnDWrapper::m_instance = NULL;

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
#ifdef DND_VM
      m_instance = VMCopyPasteDnDWrapper::CreateInstance();
#else
      m_instance = CRTCopyPasteDnDWrapper::CreateInstance();
#endif
   }
   ASSERT(m_instance);
   return m_instance;
}


/**
 *
 * Destroy the singleton object.
 */

void
CopyPasteDnDWrapper::Destroy()
{
   if (m_instance) {
      g_debug("%s: destroying self\n", __FUNCTION__);
      delete m_instance;
      m_instance = NULL;
   }
}


/**
 *
 * Constructor.
 */

CopyPasteDnDWrapper::CopyPasteDnDWrapper() :
   m_isCPEnabled(FALSE),
   m_isDnDEnabled(FALSE),
   m_isCPRegistered(FALSE),
   m_isDnDRegistered(FALSE),
   m_cpVersion(0),
   m_dndVersion(0),
   m_pimpl(NULL)
{
}


/**
 *
 * Call the implementation class pointer/grab initialization code. See
 * the implementation code for further details.
 */

void
CopyPasteDnDWrapper::PointerInit()
{
   ASSERT(m_pimpl);

   m_pimpl->PointerInit();
}


/**
 *
 * Initialize the wrapper by instantiating the platform specific impl
 * class. Effectively, this function is a factory that produces a
 * platform implementation of the DnD/Copy Paste UI layer.
 *
 * @param[in] ctx tools app context.
 */

void
CopyPasteDnDWrapper::Init(ToolsAppCtx *ctx)
{
   GuestDnDCPMgr *p = GuestDnDCPMgr::GetInstance();
   ASSERT(p);
   p->Init(ctx);

   if (!m_pimpl) {
#if defined(HAVE_GTKMM)
      m_pimpl = new CopyPasteDnDX11();
#endif
#if defined(WIN32)
#ifdef DND_VM
      m_pimpl = new VMCopyPasteDnDWin32();
#else
      m_pimpl = new CRTCopyPasteDnDWin32();
#endif
#endif
#if defined(__APPLE__)
      m_pimpl = new CopyPasteDnDMac();
#endif

      if (m_pimpl) {
         m_pimpl->Init(ctx);
         /*
          * Tell the Guest DnD Manager what capabilities we support.
          */
         p->SetCaps(m_pimpl->GetCaps());
      }
   }
}


/**
 *
 * Destructor.
 */

CopyPasteDnDWrapper::~CopyPasteDnDWrapper()
{
   g_debug("%s: enter.\n", __FUNCTION__);
   if (m_pimpl) {
      if (IsCPRegistered()) {
         m_pimpl->UnregisterCP();
      }
      if (IsDnDRegistered()) {
         m_pimpl->UnregisterDnD();
      }
      delete m_pimpl;
   }
   GuestDnDCPMgr::Destroy();
}


/**
 *
 * Register copy and paste capabilities with the VMX. Try newest version
 * first, then fall back to the legacy implementation.
 *
 * @return TRUE on success, FALSE on failure
 */

gboolean
CopyPasteDnDWrapper::RegisterCP()
{
   g_debug("%s: enter.\n", __FUNCTION__);
   ASSERT(m_pimpl);
   if (IsCPEnabled()) {
      return m_pimpl->RegisterCP();
   }
   return false;
}


/**
 *
 * Register DnD capabilities with the VMX. Handled by the platform layer.
 *
 * @return TRUE on success, FALSE on failure
 */

gboolean
CopyPasteDnDWrapper::RegisterDnD()
{
   g_debug("%s: enter.\n", __FUNCTION__);
   ASSERT(m_pimpl);
   if (IsDnDEnabled()) {
      return m_pimpl->RegisterDnD();
   }
   return false;
}


/**
 *
 * Unregister copy paste capabilities. Handled by platform layer.
 */

void
CopyPasteDnDWrapper::UnregisterCP()
{
   g_debug("%s: enter.\n", __FUNCTION__);
   ASSERT(m_pimpl);
   return m_pimpl->UnregisterCP();
}


/**
 *
 * Unregister DnD capabilities. Handled by platform layer.
 */

void
CopyPasteDnDWrapper::UnregisterDnD()
{
   g_debug("%s: enter.\n", __FUNCTION__);
   ASSERT(m_pimpl);
   return m_pimpl->UnregisterDnD();
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
   g_debug("%s: enter.\n", __FUNCTION__);
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
   g_debug("%s: enter.\n", __FUNCTION__);
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
CopyPasteDnDWrapper::SetCPIsRegistered(gboolean isRegistered)
{
   g_debug("%s: enter.\n", __FUNCTION__);
   m_isCPRegistered = isRegistered;
}


/**
 *
 * Get the flag indicating that we are wrapping an initialized and registered
 * copy paste implementation, or not.
 *
 * @return TRUE if copy paste is initialized, otherwise FALSE.
 */

gboolean
CopyPasteDnDWrapper::IsCPRegistered()
{
   g_debug("%s: enter.\n", __FUNCTION__);
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
CopyPasteDnDWrapper::SetDnDIsRegistered(gboolean isRegistered)
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

gboolean
CopyPasteDnDWrapper::IsDnDRegistered()
{
   return m_isDnDRegistered;
}


/**
 *
 * Set a flag indicating that copy paste is enabled, or not. This is called
 * in response to SetOption RPC being received.
 *
 * @param[in] isEnabled If TRUE, copy paste is enabled, otherwise FALSE.
 */

void
CopyPasteDnDWrapper::SetCPIsEnabled(gboolean isEnabled)
{
   g_debug("%s: enter.\n", __FUNCTION__);
   m_isCPEnabled = isEnabled;
   if (!isEnabled && IsCPRegistered()) {
      UnregisterCP();
   } else if (isEnabled && !IsCPRegistered()) {
      RegisterCP();
   }
}


/**
 *
 * Get the flag indicating that copy paste was enabled (via SetOption RPC).
 *
 * @return TRUE if copy paste is enabled, otherwise FALSE.
 */

gboolean
CopyPasteDnDWrapper::IsCPEnabled()
{
   return m_isCPEnabled;
}


/**
 *
 * Set a flag indicating that DnD is enabled, or not. This is called
 * in response to SetOption RPC being received.
 *
 * @param[in] isEnabled If TRUE, DnD is enabled, otherwise FALSE.
 */

void
CopyPasteDnDWrapper::SetDnDIsEnabled(gboolean isEnabled)
{
   g_debug("%s: enter.\n", __FUNCTION__);
   m_isDnDEnabled = isEnabled;
   if (!isEnabled && IsDnDRegistered()) {
      UnregisterDnD();
   } else if (isEnabled && !IsDnDRegistered()) {
      RegisterDnD();
   }
}


/**
 *
 * Get the flag indicating that DnD was enabled (via SetOption) or not.
 *
 * @return TRUE if DnD is enabled, otherwise FALSE.
 */

gboolean
CopyPasteDnDWrapper::IsDnDEnabled()
{
   return m_isDnDEnabled;
}


/**
 *
 * Handle reset.
 */

void
CopyPasteDnDWrapper::OnResetInternal()
{
   g_debug("%s: enter.\n", __FUNCTION__);
}


/**
 *
 * Handle reset.
 *
 * Schedule the post-reset actions to happen a little after one cycle of the
 * RpcIn loop. This will give vmware a chance to receive the ATR
 * reinitialize the channel if appropriate.
 */

void
CopyPasteDnDWrapper::OnReset()
{
   g_debug("%s: enter.\n", __FUNCTION__);
   AddDnDPluginResetTimer();
}


/**
 *
 * Handle no_rpc.
 *
 * Remove any actions that would need RPC channel.
 */

void
CopyPasteDnDWrapper::OnNoRpc()
{
   g_debug("%s: enter.\n", __FUNCTION__);
   RemoveDnDPluginResetTimer();
}


/**
 *
 * Handle cap reg. This is cross-platform so handle here instead of the
 * platform implementation.
 */

void
CopyPasteDnDWrapper::OnCapReg(gboolean set)
{
   g_debug("%s: enter.\n", __FUNCTION__);
}


/**
 *
 * Handle SetOption
 */

gboolean
CopyPasteDnDWrapper::OnSetOption(const char *option, const char *value)
{
   g_debug("%s: enter.\n", __FUNCTION__);
   return TRUE;
}


/**
 * Get capabilities by calling platform implementation.
 *
 * @return 32-bit mask of DnD/CP capabilities.
 */

uint32
CopyPasteDnDWrapper::GetCaps()
{
   ASSERT(m_pimpl);

   g_debug("%s: enter.\n", __FUNCTION__);
   return m_pimpl->GetCaps();
}
