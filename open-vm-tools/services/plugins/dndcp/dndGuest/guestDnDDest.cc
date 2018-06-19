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
 * @guestDnDDest.cc --
 *
 * Implementation of common layer GuestDnDDest object for guest.
 */


#include "guestDnD.hh"
#include "tracer.hh"

extern "C" {
   #include "dndClipboard.h"
   #include "debug.h"
}


/**
 * Constructor.
 *
 * @param[in] mgr guest DnD manager
 */

GuestDnDDest::GuestDnDDest(GuestDnDMgr *mgr)
 : mMgr(mgr)
{
   ASSERT(mMgr);
   mMgr->GetRpc()->destPrivDragEnterChanged.connect(
      sigc::mem_fun(this, &GuestDnDDest::OnRpcPrivDragEnter));
   mMgr->GetRpc()->destPrivDragLeaveChanged.connect(
      sigc::mem_fun(this, &GuestDnDDest::OnRpcPrivDragLeave));
   mMgr->GetRpc()->destPrivDropChanged.connect(
      sigc::mem_fun(this, &GuestDnDDest::OnRpcPrivDrop));
   mMgr->GetRpc()->destDropChanged.connect(
      sigc::mem_fun(this, &GuestDnDDest::OnRpcDrop));
   mMgr->GetRpc()->destCancelChanged.connect(
      sigc::mem_fun(this, &GuestDnDDest::OnRpcCancel));

   CPClipboard_Init(&mClipboard);
}


/**
 * Destructor.
 */

GuestDnDDest::~GuestDnDDest(void)
{
   CPClipboard_Destroy(&mClipboard);
}


/**
 * Guest UI got dragEnter with valid data. Send dragEnter cmd to controller.
 *
 * @param[in] clip cross-platform clipboard data.
 */

void
GuestDnDDest::UIDragEnter(const CPClipboard *clip)
{
   if (!mMgr->IsDragEnterAllowed()) {
      g_debug("%s: not allowed.\n", __FUNCTION__);
      return;
   }

   TRACE_CALL();

   if (GUEST_DND_DEST_DRAGGING == mMgr->GetState() ||
       GUEST_DND_PRIV_DRAGGING == mMgr->GetState()) {
      /*
       * In GH DnD case, if DnD already happened, user may drag back into guest
       * VM and drag into the detection window again, and trigger the
       * DragEnter. In this case, ignore the DragEnter.
       */
      g_debug("%s: already in state %d for GH DnD, ignoring.\n", __FUNCTION__,
              mMgr->GetState());
      return;
   }

   if (GUEST_DND_SRC_DRAGGING == mMgr->GetState()) {
      /*
       * In HG DnD case, if DnD already happened, user may also drag into the
       * detection window again. The DragEnter should also be ignored.
       */
      g_debug("%s: already in SRC_DRAGGING state, ignoring\n", __FUNCTION__);
      return;
   }

   /*
    * In Unity mode, there is no QueryPendingDrag signal, so may get called
    * with state READY.
    */
   if (mMgr->GetState() != GUEST_DND_QUERY_EXITING &&
       mMgr->GetState() != GUEST_DND_READY) {
      g_debug("%s: Bad state: %d, reset\n", __FUNCTION__, mMgr->GetState());
      goto error;
   }

   CPClipboard_Clear(&mClipboard);
   CPClipboard_Copy(&mClipboard, clip);

   if (!mMgr->GetRpc()->DestDragEnter(mMgr->GetSessionId(), clip)) {
      g_debug("%s: DestDragEnter failed\n", __FUNCTION__);
      goto error;
   }

   mMgr->SetState(GUEST_DND_DEST_DRAGGING);
   g_debug("%s: state changed to DEST_DRAGGING\n", __FUNCTION__);
   return;

error:
   mMgr->ResetDnD();
}


/**
 * User drags back to guest during GH DnD. Change state machine to
 * PRIV_DRAGGING state.
 *
 * @param[in] sessionId active session id the controller assigned.
 */

void
GuestDnDDest::OnRpcPrivDragEnter(uint32 sessionId)
{
   TRACE_CALL();

   if (GUEST_DND_DEST_DRAGGING != mMgr->GetState()) {
      g_debug("%s: Bad state: %d, reset\n", __FUNCTION__, mMgr->GetState());
      goto error;
   }

   mMgr->SetState(GUEST_DND_PRIV_DRAGGING);
   g_debug("%s: state changed to PRIV_DRAGGING\n", __FUNCTION__);
   return;

error:
   mMgr->ResetDnD();
}


/**
 * User drags away from guest during GH DnD. Change state machine to
 * SRC_DRAGGING state.
 *
 * @param[in] sessionId active session id the controller assigned.
 * @param[in] x mouse position x.
 * @param[in] y mouse position y.
 */

void
GuestDnDDest::OnRpcPrivDragLeave(uint32 sessionId,
                                 int32 x,
                                 int32 y)
{
   TRACE_CALL();

   if (GUEST_DND_PRIV_DRAGGING != mMgr->GetState()) {
      g_debug("%s: Bad state: %d, reset\n", __FUNCTION__, mMgr->GetState());
      goto error;
   }

   mMgr->SetState(GUEST_DND_DEST_DRAGGING);
   mMgr->destMoveDetWndToMousePosChanged.emit();
   g_debug("%s: state changed to DEST_DRAGGING\n", __FUNCTION__);
   return;

error:
   mMgr->ResetDnD();
}


/**
 * User drops inside guest during GH DnD. Simulate the mouse drop, hide
 * detection window, and reset state machine.
 *
 * @param[in] sessionId active session id the controller assigned.
 * @param[in] x mouse position x.
 * @param[in] y mouse position y.
 */

void
GuestDnDDest::OnRpcPrivDrop(uint32 sessionId,
                            int32 x,
                            int32 y)
{
   mMgr->privDropChanged.emit(x, y);
   mMgr->HideDetWnd();
   mMgr->SetState(GUEST_DND_READY);
   // XXX Trace.
   g_debug("%s: state changed to GUEST_DND_READY, session id changed to 0\n",
           __FUNCTION__);
}


/**
 * User drops outside of guest during GH DnD. Simply cancel the local DnD.
 *
 * @param[in] sessionId active session id the controller assigned.
 * @param[in] x mouse position x.
 * @param[in] y mouse position y.
 */

void
GuestDnDDest::OnRpcDrop(uint32 sessionId,
                        int32 x,
                        int32 y)
{
   OnRpcCancel(sessionId);
}


/**
 * Cancel current GH DnD.
 *
 * @param[in] sessionId active session id the controller assigned.
 */

void
GuestDnDDest::OnRpcCancel(uint32 sessionId)
{
   /*
    * For Windows, the detection window will hide when the drop
    * event occurs.
    * Please see bug 1750683.
    */
#if !defined(_WIN32)
   mMgr->DelayHideDetWnd();
#endif // _WIN32
   mMgr->RemoveUngrabTimeout();
   mMgr->destCancelChanged.emit();
   mMgr->SetState(GUEST_DND_READY);
   g_debug("%s: state changed to GUEST_DND_READY, session id changed to 0\n",
           __FUNCTION__);
}

