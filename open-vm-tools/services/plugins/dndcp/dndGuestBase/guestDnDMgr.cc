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
 * @guestDnDMgr.cc --
 *
 * Implementation of common layer GuestDnDMgr object for guest.
 */

#include "tracer.hh"
#include "guestDnD.hh"
#include "guestDnDCPMgr.hh"

extern "C" {
   #include "debug.h"
}


/**
 * Constructor.
 *
 * @param[in] transport for sending/receiving packets.
 * @param[in] eventQueue for event management
 */

GuestDnDMgr::GuestDnDMgr(DnDCPTransport *transport,
                         ToolsAppCtx *ctx)
 : mSrc(NULL),
   mDest(NULL),
   mRpc(NULL),
   mDnDState(GUEST_DND_READY),
   mSessionId(0),
   mHideDetWndTimer(NULL),
   mUnityDnDDetTimeout(NULL),
   mUngrabTimeout(NULL),
   mDnDAllowed(false),
   mDnDTransport(transport),
   mCapabilities(0xffffffff)
{
   ASSERT(transport);
}


/**
 * Destructor.
 */

GuestDnDMgr::~GuestDnDMgr(void)
{
   delete mRpc;
   mRpc = NULL;

   /* Remove untriggered timers. */
   if (mHideDetWndTimer) {
      g_source_destroy(mHideDetWndTimer);
      mHideDetWndTimer = NULL;
   }
   if (mUnityDnDDetTimeout) {
      g_source_destroy(mUnityDnDDetTimeout);
      mUnityDnDDetTimeout = NULL;
   }
   RemoveUngrabTimeout();
}


/**
 * Reset state machine and session id. Delete mSrc and mDest.
 */

void
GuestDnDMgr::ResetDnD(void)
{
   TRACE_CALL();

   if (mSrc) {
      srcCancelChanged.emit();
      DelayHideDetWnd();
      delete mSrc;
      mSrc = NULL;
   }
   if (mDest) {
      DelayHideDetWnd();
      RemoveUngrabTimeout();
      destCancelChanged.emit();
      delete mDest;
      mDest = NULL;
   }

   SetState(GUEST_DND_READY);

   g_debug("%s: change to state %d, session id %d\n", __FUNCTION__, mDnDState,
           mSessionId);
}


/**
 * Guest UI got dragBeginDone. Wrapper for mSrc->UIDragBeginDone.
 */

void
GuestDnDMgr::SrcUIDragBeginDone(void)
{
   TRACE_CALL();

   if (mSrc) {
      mSrc->UIDragBeginDone();
   } else {
      g_debug("%s: mSrc is NULL\n", __FUNCTION__);
   }
}


/**
 * Guest UI got DnD feedback. Wrapper for mSrc->UIUpdateFeedback.
 *
 * @param[in] feedback
 */

void
GuestDnDMgr::SrcUIUpdateFeedback(DND_DROPEFFECT feedback)
{
   TRACE_CALL();

   if (mSrc) {
      mSrc->UIUpdateFeedback(feedback);
   } else {
      g_debug("%s: mSrc is NULL\n", __FUNCTION__);
   }
}


/**
 * Guest UI got dragEnter with valid data. Create mDest if the state machine
 * is ready.
 *
 * @param[in] clip cross-platform clipboard data.
 */

void
GuestDnDMgr::DestUIDragEnter(const CPClipboard *clip)
{
   TRACE_CALL();

   /* Remove untriggered ungrab timer. */
   RemoveUngrabTimeout();

   if (GUEST_DND_SRC_DRAGGING == mDnDState ||
       GUEST_DND_DEST_DRAGGING == mDnDState) {
      /*
       * In GH DnD case, if DnD already happened, user may drag back into guest
       * VM and drag into the detection window again, and trigger the
       * DragEnter. In this case, ignore the DragEnter.
       *
       * In HG DnD case, if DnD already happened, user may also drag into the
       * detection window again. The DragEnter should also be ignored.
       */
      return;
   }

   /*
    * In Unity mode, there is no QueryPendingDrag signal, so may get called
    * with state READY.
    */
   if (mDnDState != GUEST_DND_QUERY_EXITING &&
       mDnDState != GUEST_DND_READY) {
      g_debug("%s: Bad state: %d, reset\n", __FUNCTION__, mDnDState);
      ResetDnD();
      return;
   }

   /* Remove untriggered ungrab timer. */
   if (mUngrabTimeout) {
      g_source_destroy(mUngrabTimeout);
      mUngrabTimeout = NULL;
   }

   if (mDest) {
      g_debug("%s: mDest is not NULL\n", __FUNCTION__);
      delete mDest;
      mDest = NULL;
   }

   ASSERT(clip);
   mDest = new GuestDnDDest(this);
   mDest->UIDragEnter(clip);
}


/**
 * Got queryExiting from rpc. Show the detection window on (x, y) to try to
 * detect any pending GH DnD.
 *
 * @param[in] sessionId active DnD session id
 * @param[in] x detection window position x.
 * @param[in] y detection window position y.
 */

void
GuestDnDMgr::OnRpcQueryExiting(uint32 sessionId,
                               int32 x,
                               int32 y)
{
   TRACE_CALL();

   if (!mDnDAllowed) {
      g_debug("%s: DnD is not allowed.\n", __FUNCTION__);
      return;
   }

   if (GUEST_DND_READY != mDnDState) {
      /* Reset DnD for any wrong state. */
      g_debug("%s: Bad state: %d\n", __FUNCTION__, mDnDState);
      ResetDnD();
      return;
   }

   /* Show detection window to detect pending GH DnD. */
   ShowDetWnd(x, y);
   SetSessionId(sessionId);
   SetState(GUEST_DND_QUERY_EXITING);

   /*
    * Add event to fire and hide our window if a DnD is not pending.  Note that
    * this is here in case the drag isn't picked up by our drag detection window
    * for some reason.
    */
   AddDnDUngrabTimeoutEvent();
}


/**
 * Callback for DnDUngrab timeout. This will be called if there is no pending
 * GH DnD when user dragged leaving the guest. Send dragNotPending command to
 * controller and reset local state machine.
 */

void
GuestDnDMgr::UngrabTimeout(void)
{
   TRACE_CALL();

   mUngrabTimeout = NULL;

   if (mDnDState != GUEST_DND_QUERY_EXITING) {
      /* Reset DnD for any wrong state. */
      g_debug("%s: Bad state: %d\n", __FUNCTION__, mDnDState);
      ResetDnD();
      return;
   }

   ASSERT(mRpc);
   mRpc->DragNotPending(mSessionId);

   HideDetWnd();
   SetState(GUEST_DND_READY);
}


/**
 * This callback is trigged when a user clicks into any Unity window or just
 * releases the mouse button. Either show the full-screen detection window
 * right after the Unity window, or hide the detection window.
 *
 * @param[in] sessionId  Active session id the controller assigned earlier.
 * @param[in] show       Show or hide unity DnD detection window.
 * @param[in] unityWndId The unity window id.
 */

void
GuestDnDMgr::OnRpcUpdateUnityDetWnd(uint32 sessionId,
                                    bool show,
                                    uint32 unityWndId)
{
   TRACE_CALL();

   if (show && mDnDState != GUEST_DND_READY) {
      /*
       * Reset DnD for any wrong state. Only do this when host asked to
       * show the window.
       */
      g_debug("%s: Bad state: %d\n", __FUNCTION__, mDnDState);
      ResetDnD();
      return;
   }

   if (mUnityDnDDetTimeout) {
      g_source_destroy(mUnityDnDDetTimeout);
      mUnityDnDDetTimeout = NULL;
   }

   if (show) {
      /*
       * When showing full screen window, also show the small top-most
       * window at (1, 1). After detected a GH DnD, the full screen
       * window will be hidden to avoid blocking other windows. So use
       * this window to accept drop in cancel case.
       */
      UpdateDetWnd(show, 1, 1);
      AddUnityDnDDetTimeoutEvent();
      SetSessionId(sessionId);
   } else {
      /*
       * If there is active DnD, the regular detection window will be hidden
       * after DnD is done.
       */
      if (mDnDState == GUEST_DND_READY) {
         UpdateDetWnd(false, 0, 0);
      }
   }

   /* Show/hide the full screen detection window. */
   updateUnityDetWndChanged.emit(show, unityWndId, false);
   g_debug("%s: updating Unity detection window, show %d, id %u\n",
           __FUNCTION__, show, unityWndId);
}


/**
 * Can not detect pending GH DnD within UNITY_DND_DET_TIMEOUT, put the full
 * screen detection window to bottom most.
 */

void
GuestDnDMgr::UnityDnDDetTimeout(void)
{
   TRACE_CALL();

   mUnityDnDDetTimeout = NULL;
   updateUnityDetWndChanged.emit(true, 0, true);
}


/**
 * Got moveMouse from rpc. Ask UI to update mouse position.
 *
 * @param[in] sessionId active DnD session id
 * @param[in] x mouse position x.
 * @param[in] y mouse position y.
 * @param[in] isButtonDown
 */

void
GuestDnDMgr::OnRpcMoveMouse(uint32 sessionId,
                            int32 x,
                            int32 y)
{
   TRACE_CALL();

   if (GUEST_DND_SRC_DRAGGING != mDnDState &&
       GUEST_DND_PRIV_DRAGGING != mDnDState) {
      g_debug("%s: not in valid state %d, ignoring\n", __FUNCTION__, mDnDState);
      return;
   }
   g_debug("%s: move to %d, %d\n", __FUNCTION__, x, y);
   moveMouseChanged.emit(x, y);
}


/**
 * Update the detection window.
 *
 * @param[in] show show/hide the detection window
 * @param[in] x detection window position x.
 * @param[in] y detection window position y.
 */

void
GuestDnDMgr::UpdateDetWnd(bool show,
                          int32 x,
                          int32 y)
{
   TRACE_CALL();

   if (mHideDetWndTimer) {
      g_source_destroy(mHideDetWndTimer);
      mHideDetWndTimer = NULL;
   }

   g_debug("%s: %s window at %d, %d\n", __FUNCTION__, show ? "show" : "hide",
           x, y);
   updateDetWndChanged.emit(show, x, y);
}


/**
 * Update the detection window.
 *
 * @param[in] show show/hide the detection window
 * @param[in] x detection window position x.
 * @param[in] y detection window position y.
 */

void
GuestDnDMgr::DelayHideDetWnd(void)
{
   TRACE_CALL();

   AddHideDetWndTimerEvent();
}


/**
 * Remove any pending mUngrabTimeout.
 */

void
GuestDnDMgr::RemoveUngrabTimeout(void)
{
   TRACE_CALL();

   if (mUngrabTimeout) {
      g_source_destroy(mUngrabTimeout);
      mUngrabTimeout = NULL;
   }
}


/**
 * Set state machine state.
 *
 * @param[in] state
 */

void
GuestDnDMgr::SetState(GUEST_DND_STATE state)
{
#ifdef VMX86_DEVEL
   static const char* states[] = {
      "GUEST_DND_INVALID",
      "GUEST_DND_READY",
      /* As destination. */
      "GUEST_DND_QUERY_EXITING",
      "GUEST_DND_DEST_DRAGGING",
      /* In private dragging mode. */
      "GUEST_DND_PRIV_DRAGGING",
      /* As source. */
      "GUEST_DND_SRC_DRAGBEGIN_PENDING",
      "GUEST_DND_SRC_CANCEL_PENDING",
      "GUEST_DND_SRC_DRAGGING",
   };
   g_debug("%s: %s => %s\n", __FUNCTION__, states[mDnDState], states[state]);
#endif

   mDnDState = state;
   stateChanged.emit(state);
   if (GUEST_DND_READY == state) {
      /* Reset sessionId if the state is reset. */
      SetSessionId(0);
   }
}


/**
 * Check if DragEnter is allowed.
 *
 * @return true if DragEnter is allowed, false otherwise.
 */

bool
GuestDnDMgr::IsDragEnterAllowed(void)
{
   /*
    * Right after any DnD is finished, there may be some unexpected
    * DragEnter from UI, and may disturb our state machine. The
    * mHideDetWndTimer will only be valid for 0.5 second after each
    * DnD, and during this time UI DragEnter is not allowed.
    */
   return mHideDetWndTimer == NULL;
}


/**
 * Handle version change in VMX.
 *
 * @param[in] version negotiated DnD version.
 */

void
GuestDnDMgr::VmxDnDVersionChanged(uint32 version)
{
   TRACE_CALL();

   g_debug("GuestDnDMgr::%s: enter version %d\n", __FUNCTION__, version);
   ASSERT(version >= 3);

   /* Remove untriggered timers. */
   if (mHideDetWndTimer) {
      g_source_destroy(mHideDetWndTimer);
      mHideDetWndTimer = NULL;
   }
   if (mRpc) {
      delete mRpc;
      mRpc = NULL;
   }

   CreateDnDRpcWithVersion(version);

   if (mRpc) {
      mRpc->pingReplyChanged.connect(
         sigc::mem_fun(this, &GuestDnDMgr::OnPingReply));
      mRpc->srcDragBeginChanged.connect(
         sigc::mem_fun(this, &GuestDnDMgr::OnRpcSrcDragBegin));
      mRpc->queryExitingChanged.connect(
         sigc::mem_fun(this, &GuestDnDMgr::OnRpcQueryExiting));
      mRpc->updateUnityDetWndChanged.connect(
         sigc::mem_fun(this, &GuestDnDMgr::OnRpcUpdateUnityDetWnd));
      mRpc->moveMouseChanged.connect(
         sigc::mem_fun(this, &GuestDnDMgr::OnRpcMoveMouse));
      mRpc->Init();
      mRpc->SendPing(GuestDnDCPMgr::GetInstance()->GetCaps() &
                     (DND_CP_CAP_DND | DND_CP_CAP_FORMATS_DND |
                      DND_CP_CAP_VALID));
   }

   ResetDnD();
}


/**
 * Check if a request is allowed based on resolved capabilities.
 *
 * @param[in] capsRequest requested capabilities.
 *
 * @return TRUE if allowed, FALSE otherwise.
 */

Bool
GuestDnDMgr::CheckCapability(uint32 capsRequest)
{
   Bool allowed = FALSE;

   if ((mCapabilities & capsRequest) == capsRequest) {
      allowed = TRUE;
   }
   return allowed;
}


/**
 * Got pingReplyChanged message. Update capabilities.
 *
 * @param[in] capability modified capabilities from VMX controller.
 */

void
GuestDnDMgr::OnPingReply(uint32 capabilities)
{
   TRACE_CALL();

   g_debug("%s: dnd ping reply caps are %x\n", __FUNCTION__, capabilities);
   mCapabilities = capabilities;
}


/**
 * Callback for DnDUngrab timeout. This will be called if there is no pending
 * GH DnD when user dragged leaving the guest.
 *
 * @param[in] clientData
 *
 * @return FALSE always
 */

gboolean
GuestDnDMgr::DnDUngrabTimeout(void *clientData)
{
   TRACE_CALL();

   ASSERT(clientData);
   GuestDnDMgr *dnd = (GuestDnDMgr *)clientData;
   /* Call actual callback. */
   dnd->UngrabTimeout();
   return FALSE;
}


/**
 * Callback for HideDetWndTimer.
 *
 * @param[in] clientData
 *
 * @return FALSE always
 */

gboolean
GuestDnDMgr::DnDHideDetWndTimer(void *clientData)
{
   TRACE_CALL();

   ASSERT(clientData);
   GuestDnDMgr *dnd = (GuestDnDMgr *)clientData;
   dnd->SetHideDetWndTimer(NULL);
   dnd->HideDetWnd();
   return FALSE;
}


/**
 * Callback for UnityDnDDetTimeout.
 *
 * @param[in] clientData
 *
 * @return FALSE always
 */

gboolean
GuestDnDMgr::DnDUnityDetTimeout(void *clientData)
{
   TRACE_CALL();

   ASSERT(clientData);
   GuestDnDMgr *dnd = (GuestDnDMgr *)clientData;
   dnd->UnityDnDDetTimeout();
   return FALSE;
}

