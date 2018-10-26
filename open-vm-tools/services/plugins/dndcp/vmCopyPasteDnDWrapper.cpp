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
 * @file vmCopyPasteDnDWrapper.cpp
 *
 * The inherited implementation of common class CopyPasteDnDWrapper in VM side.
 */

#define G_LOG_DOMAIN "dndcp"

#include "guestDnDCPMgr.hh"
#include "vmCopyPasteDnDWrapper.h"
#include "vmware.h"

extern "C" {
   #include "rpcout.h"
   #include "vmware/guestrpc/tclodefs.h"
   #include "vmware/tools/plugin.h"
   #include "vmware/tools/utils.h"
   #include <string.h>
}


/**
 *
 * Timer callback for reset. Handle it by calling the member function
 * that handles reset.
 *
 * @param[in] clientData pointer to the VMCopyPasteDnDWrapper instance that
 * issued the timer.
 *
 * @return FALSE always.
 */

static gboolean
DnDPluginResetSent(void *clientData)
{
   VMCopyPasteDnDWrapper *p = reinterpret_cast<VMCopyPasteDnDWrapper *>(clientData);

   g_debug("%s: enter\n", __FUNCTION__);
   ASSERT(p);
   p->OnResetInternal();
   p->RemoveDnDPluginResetTimer();
   return FALSE;
}


/**
 *
 * Create an instance for class VMCopyPasteDnDWrapper.
 *
 * @return a pointer to the VMCopyPasteDnDWrapper instance.
 */

VMCopyPasteDnDWrapper *
VMCopyPasteDnDWrapper::CreateInstance(void)
{
   return new VMCopyPasteDnDWrapper();
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
VMCopyPasteDnDWrapper::Init(ToolsAppCtx *ctx)
{
   m_ctx = ctx;
   CopyPasteDnDWrapper::Init(ctx);
}


/**
 *
 * Add the DnD plugin reset timer.
 */

void
VMCopyPasteDnDWrapper::AddDnDPluginResetTimer(void)
{
   g_debug("%s: enter\n", __FUNCTION__);

   ASSERT(m_resetTimer == NULL);

   m_resetTimer = VMTools_CreateTimer(RPC_POLL_TIME * 30);
   if (m_resetTimer) {
      VMTOOLSAPP_ATTACH_SOURCE(m_ctx, m_resetTimer,
                               DnDPluginResetSent, this, NULL);
   }
}


/**
 *
 * Remove the DnD plugin reset timer.
 */

void
VMCopyPasteDnDWrapper::RemoveDnDPluginResetTimer(void)
{
   g_debug("%s: enter\n", __FUNCTION__);
   if (m_resetTimer) {
      g_source_destroy(m_resetTimer);
      g_source_unref(m_resetTimer);
      m_resetTimer = NULL;
   }
}


/**
 *
 * Handle cap reg. This is cross-platform so handle here instead of the
 * platform implementation.
 */

void
VMCopyPasteDnDWrapper::OnCapReg(gboolean set)
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
 * Get the version of the copy paste protocol being wrapped.
 *
 * @return copy paste protocol version.
 */

int
VMCopyPasteDnDWrapper::GetCPVersion()
{
   g_debug("%s: enter\n", __FUNCTION__);
   if (IsCPRegistered()) {
      char *reply = NULL;
      size_t replyLen;

      ToolsAppCtx *ctx = GetToolsAppCtx();
      if (!RpcChannel_Send(ctx->rpc, QUERY_VMX_COPYPASTE_VERSION,
         strlen(QUERY_VMX_COPYPASTE_VERSION), &reply, &replyLen)) {
            g_debug("%s: could not get VMX copyPaste "
                    "version capability: %s\n", __FUNCTION__,
                    reply ? reply : "NULL");
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
VMCopyPasteDnDWrapper::GetDnDVersion()
{
   g_debug("%s: enter\n", __FUNCTION__);
   if (IsDnDRegistered()) {
      char *reply = NULL;
      size_t replyLen;

      ToolsAppCtx *ctx = GetToolsAppCtx();
      if (!RpcChannel_Send(ctx->rpc, QUERY_VMX_DND_VERSION,
         strlen(QUERY_VMX_DND_VERSION), &reply, &replyLen)) {
            g_debug("%s: could not get VMX dnd "
                    "version capability: %s\n", __FUNCTION__,
                    reply ? reply : "NULL");
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
 * Handle reset.
 */

void
VMCopyPasteDnDWrapper::OnResetInternal()
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
 * Handle SetOption.
 *
 * @param[in] option option name
 * @param[in] option option value
 * @return TRUE on success, FALSE on failure
 */

gboolean
VMCopyPasteDnDWrapper::OnSetOption(const char *option,
                                   const char *value)
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
