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
#endif

#if defined(__APPLE__)
#include "copyPasteDnDMac.h"
#endif

#include "copyPasteDnDWrapper.h"
#include "guestDnDCPMgr.hh"

extern "C" {
#include "vmware.h"
#include "rpcout.h"
#include "vmware/guestrpc/tclodefs.h"
#include "vmware/tools/plugin.h"
#include "vmware/tools/utils.h"
#include <string.h>
}

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
      m_instance = new CopyPasteDnDWrapper;
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
   m_ctx(NULL),
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
   m_ctx = ctx;

   GuestDnDCPMgr *p = GuestDnDCPMgr::GetInstance();
   ASSERT(p);
   p->Init(ctx);

   if (!m_pimpl) {
#if defined(HAVE_GTKMM)
      m_pimpl = new CopyPasteDnDX11();
#endif
#if defined(WIN32)
      m_pimpl = new CopyPasteDnDWin32();
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
   g_debug("%s: enter\n", __FUNCTION__);
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
   g_debug("%s: enter\n", __FUNCTION__);
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
   g_debug("%s: enter\n", __FUNCTION__);
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
   g_debug("%s: enter\n", __FUNCTION__);
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
   g_debug("%s: enter\n", __FUNCTION__);
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
   g_debug("%s: enter\n", __FUNCTION__);
   if (IsCPRegistered()) {
      char *reply = NULL;
      size_t replyLen;

      ToolsAppCtx *ctx = GetToolsAppCtx();
      if (!RpcChannel_Send(ctx->rpc, QUERY_VMX_COPYPASTE_VERSION,
                           strlen(QUERY_VMX_COPYPASTE_VERSION), &reply, &replyLen)) {
         g_debug("%s: could not get VMX copyPaste "
               "version capability: %s\n", __FUNCTION__, reply ? reply : "NULL");
         m_cpVersion = 1;
      } else {
         m_cpVersion = atoi(reply);
      }
      free(reply);
   }
   g_debug("%s: got version %d\n", __FUNCTION__, m_cpVersion);
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
   g_debug("%s: enter\n", __FUNCTION__);
   if (IsDnDRegistered()) {
      char *reply = NULL;
      size_t replyLen;

      ToolsAppCtx *ctx = GetToolsAppCtx();
      if (!RpcChannel_Send(ctx->rpc, QUERY_VMX_DND_VERSION,
                           strlen(QUERY_VMX_DND_VERSION), &reply, &replyLen)) {
         g_debug("%s: could not get VMX dnd "
               "version capability: %s\n", __FUNCTION__, reply ? reply : "NULL");
         m_dndVersion = 1;
      } else {
         m_dndVersion = atoi(reply);
      }
      free(reply);
   }
   g_debug("%s: got version %d\n", __FUNCTION__, m_dndVersion);
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
   g_debug("%s: enter\n", __FUNCTION__);
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
   g_debug("%s: enter\n", __FUNCTION__);
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
   g_debug("%s: enter\n", __FUNCTION__);
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
 * Timer callback for reset. Handle it by calling the member function
 * that handles reset.
 *
 * @param[in] clientData pointer to the CopyPasteDnDWrapper instance that
 * issued the timer.
 *
 * @return FALSE always.
 */

static gboolean
DnDPluginResetSent(void *clientData)
{
   CopyPasteDnDWrapper *p = reinterpret_cast<CopyPasteDnDWrapper *>(clientData);

   ASSERT(p);
   p->OnResetInternal();
   return FALSE;
}


/**
 *
 * Handle reset.
 */

void
CopyPasteDnDWrapper::OnResetInternal()
{
   g_debug("%s: enter\n", __FUNCTION__);

   /*
    * Reset DnD/Copy/Paste only if vmx said we can. The reason is that
    * we may also get reset request from vmx when user is taking snapshot
    * or recording. If there is an ongoing DnD/copy/paste, we should not
    * reset here. For details please refer to bug 375928.
    */
   char *reply = NULL;
   size_t replyLen;
   ToolsAppCtx *ctx = GetToolsAppCtx();
   if (RpcChannel_Send(ctx->rpc, "dnd.is.active",
                       strlen("dnd.is.active"), &reply, &replyLen) &&
       (1 == atoi(reply))) {
      g_debug("%s: ignore reset while file transfer is busy.\n", __FUNCTION__);
      goto exit;
   }

   if (IsDnDRegistered()) {
      UnregisterDnD();
   }
   if (IsCPRegistered()) {
      UnregisterCP();
   }
   if (IsCPEnabled() && !IsCPRegistered()) {
      RegisterCP();
   }
   if (IsDnDEnabled() && !IsDnDRegistered()) {
      RegisterDnD();
   }
   if (!IsDnDRegistered() || !IsCPRegistered()) {
      g_debug("%s: unable to reset fully DnD %d CP %d!\n",
            __FUNCTION__, IsDnDRegistered(), IsCPRegistered());
   }

exit:
   free(reply);
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
   GSource *src;

   g_debug("%s: enter\n", __FUNCTION__);
   src = VMTools_CreateTimer(RPC_POLL_TIME * 30);
   if (src) {
      VMTOOLSAPP_ATTACH_SOURCE(m_ctx, src, DnDPluginResetSent, this, NULL);
      g_source_unref(src);
   }
}


/**
 *
 * Handle cap reg. This is cross-platform so handle here instead of the
 * platform implementation.
 */

void
CopyPasteDnDWrapper::OnCapReg(gboolean set)
{
   g_debug("%s: enter\n", __FUNCTION__);
   char *reply = NULL;
   size_t replyLen;
   const char *toolsDnDVersion = TOOLS_DND_VERSION_4;
   char *toolsCopyPasteVersion = NULL;
   int version;

   ToolsAppCtx *ctx = GetToolsAppCtx();
   if (ctx) {
      /*
       * First DnD.
       */
      if (!RpcChannel_Send(ctx->rpc, toolsDnDVersion, strlen(toolsDnDVersion),
                           NULL, NULL)) {
         g_debug("%s: could not set guest dnd version capability\n",
               __FUNCTION__);
         version = 1;
         SetDnDVersion(version);
      } else {
         char const *vmxDnDVersion = QUERY_VMX_DND_VERSION;

         if (!RpcChannel_Send(ctx->rpc, vmxDnDVersion,
                              strlen(vmxDnDVersion), &reply, &replyLen)) {
            g_debug("%s: could not get VMX dnd version capability, assuming v1\n",
                  __FUNCTION__);
            version = 1;
            SetDnDVersion(version);
         } else {
            int version = atoi(reply);
            ASSERT(version >= 1);
            SetDnDVersion(version);
            g_debug("%s: VMX is dnd version %d\n", __FUNCTION__, GetDnDVersion());
            if (version == 3) {
               /*
                * VMDB still has version 4 in it, which will cause a V3
                * host to fail. So, change to version 3. Since we don't
                * support any other version, we only do this for V3.
                */
               toolsDnDVersion = TOOLS_DND_VERSION_3;
               if (!RpcChannel_Send(ctx->rpc, toolsDnDVersion,
                                    strlen(toolsDnDVersion), NULL, NULL)) {

                  g_debug("%s: could not set VMX dnd version capability, assuming v1\n",
                           __FUNCTION__);
                  version = 1;
                  SetDnDVersion(version);
               }
            }
         }
         vm_free(reply);
         reply = NULL;
       }

      /*
       * Now CopyPaste.
       */

      toolsCopyPasteVersion = g_strdup_printf(TOOLS_COPYPASTE_VERSION" %d", 4);
      if (!RpcChannel_Send(ctx->rpc, toolsCopyPasteVersion,
                           strlen(toolsCopyPasteVersion),
                           NULL, NULL)) {
         g_debug("%s: could not set guest copypaste version capability\n",
               __FUNCTION__);
         version = 1;
         SetCPVersion(version);
      } else {
         char const *vmxCopyPasteVersion = QUERY_VMX_COPYPASTE_VERSION;

         if (!RpcChannel_Send(ctx->rpc, vmxCopyPasteVersion,
                              strlen(vmxCopyPasteVersion), &reply, &replyLen)) {
            g_debug("%s: could not get VMX copypaste version capability, assuming v1\n",
                  __FUNCTION__);
            version = 1;
            SetCPVersion(version);
         } else {
            version = atoi(reply);
            ASSERT(version >= 1);
            SetCPVersion(version);
            g_debug("%s: VMX is copypaste version %d\n", __FUNCTION__,
                  GetCPVersion());
            if (version == 3) {
               /*
                * VMDB still has version 4 in it, which will cause a V3
                * host to fail. So, change to version 3. Since we don't
                * support any other version, we only do this for V3.
                */
               g_free(toolsCopyPasteVersion);
               toolsCopyPasteVersion = g_strdup_printf(TOOLS_COPYPASTE_VERSION" %d", 3);
               if (!RpcChannel_Send(ctx->rpc, toolsCopyPasteVersion,
                                    strlen(toolsCopyPasteVersion), NULL, NULL)) {

                  g_debug("%s: could not set VMX copypaste version, assuming v1\n",
                           __FUNCTION__);
                  version = 1;
                  SetCPVersion(version);
               }
            }
         }
         vm_free(reply);
      }
      g_free(toolsCopyPasteVersion);
   }
}


/**
 *
 * Handle SetOption
 */

gboolean
CopyPasteDnDWrapper::OnSetOption(const char *option, const char *value)
{
   gboolean ret = false;
   bool bEnable;

   ASSERT(option);
   ASSERT(value);

   bEnable = strcmp(value, "1") ? false : true;
   g_debug("%s: setting option '%s' to '%s'\n", __FUNCTION__, option, value);
   if (strcmp(option, TOOLSOPTION_ENABLEDND) == 0) {
      SetDnDIsEnabled(bEnable);
      ret = true;
   } else if (strcmp(option, TOOLSOPTION_COPYPASTE) == 0) {
      SetCPIsEnabled(bEnable);
      ret = true;
   }

   return ret;
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

   return m_pimpl->GetCaps();
}
