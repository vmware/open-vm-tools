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
 * @vmGuestDnDMgr.cc --
 *
 * The inherited implementation of common class GuestDnDMgr in VM side.
 */

#include "dndRpcV3.hh"
#include "dndRpcV4.hh"
#include "tracer.hh"
#include "vmGuestDnDMgr.hh"
#include "vmGuestDnDSrc.hh"

extern "C" {
#include "debug.h"
}


/**
 * Constructor.
 *
 * @param[in] transport for sending/receiving packets.
 * @param[in] ctx ToolsAppCtx
 */

VMGuestDnDMgr::VMGuestDnDMgr(DnDCPTransport *transport,
                             ToolsAppCtx *ctx)
   : GuestDnDMgr(transport, ctx),
     mToolsAppCtx(ctx)
{
   ASSERT(mToolsAppCtx);
}


/**
 * Add DnD un-grab timeout event.
 */

void
VMGuestDnDMgr::AddDnDUngrabTimeoutEvent()
{
   if (NULL == mUngrabTimeout) {
      g_debug("%s: adding UngrabTimeout\n", __FUNCTION__);
      mUngrabTimeout = g_timeout_source_new(UNGRAB_TIMEOUT);
      VMTOOLSAPP_ATTACH_SOURCE(mToolsAppCtx, mUngrabTimeout,
                               GuestDnDMgr::DnDUngrabTimeout, this, NULL);
      g_source_unref(mUngrabTimeout);
   }
}


/**
 * Add Unity DnD detect timeout event.
 */

void
VMGuestDnDMgr::AddUnityDnDDetTimeoutEvent()
{
   mUnityDnDDetTimeout = g_timeout_source_new(UNITY_DND_DET_TIMEOUT);
   VMTOOLSAPP_ATTACH_SOURCE(mToolsAppCtx,
                            mUnityDnDDetTimeout,
                            GuestDnDMgr::DnDUnityDetTimeout,
                            this,
                            NULL);
   g_source_unref(mUnityDnDDetTimeout);
}


/**
 * Add hide detect window timer.
 */

void
VMGuestDnDMgr::AddHideDetWndTimerEvent()
{
   if (NULL == mHideDetWndTimer) {
      g_debug("%s: add timer to hide detection window.\n", __FUNCTION__);
      mHideDetWndTimer = g_timeout_source_new(HIDE_DET_WND_TIMER);
      VMTOOLSAPP_ATTACH_SOURCE(mToolsAppCtx, mHideDetWndTimer,
                               GuestDnDMgr::DnDHideDetWndTimer, this, NULL);
      g_source_unref(mHideDetWndTimer);
   } else {
      g_debug("%s: mHideDetWndTimer is not NULL, quit.\n", __FUNCTION__);
   }
}


/**
 * Create DnD rpc with input version.
 *
 * @param[in] version input version number
 */

void
VMGuestDnDMgr::CreateDnDRpcWithVersion(uint32 version)
{
   switch(version) {
   case 4:
      mRpc = new DnDRpcV4(mDnDTransport);
      break;
   case 3:
      mRpc = new DnDRpcV3(mDnDTransport);
      break;
   default:
      g_debug("%s: unsupported DnD version\n", __FUNCTION__);
      break;
   }
}


/**
 * Got srcDragBegin from rpc with valid data. Create mSrc if the state machine
 * is ready.
 *
 * @param[in] sessionId active DnD session id
 * @param[in] capability controller capability
 * @param[in] clip cross-platform clipboard data.
 */

void
VMGuestDnDMgr::OnRpcSrcDragBegin(uint32 sessionId,
                                 const CPClipboard *clip)
{
   TRACE_CALL();

   if (!mDnDAllowed) {
      g_debug("%s: DnD is not allowed.\n", __FUNCTION__);
      return;
   }

   if (GUEST_DND_READY != mDnDState) {
      g_debug("%s: Bad state: %d, reset\n", __FUNCTION__, mDnDState);
      ResetDnD();
      return;
   }

   if (mSrc) {
      g_debug("%s: mSrc is not NULL\n", __FUNCTION__);
      delete mSrc;
      mSrc = NULL;
   }

   SetSessionId(sessionId);

   ASSERT(clip);

   mSrc = new VMGuestDnDSrc(this);
   mSrc->OnRpcDragBegin(clip);
}
