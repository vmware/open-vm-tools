/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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

/*
 * dnd.cc --
 *
 *     Implementation of common layer DnD object.
 */

#include "dnd.hh"
#include "dndRpcV3.hh"

extern "C" {
   #include "util.h"
   #include "debug.h"
   #include "file.h"
   #include "cpNameUtil.h"
   #include "str.h"
   #include "eventManager.h"
}

#define UNGRAB_TIMEOUT 50        // 0.5 s
#define UNITY_DND_DET_TIMEOUT 50 // 0.5 s
#define HIDE_DET_WND_TIMER 50    // 0.5s


/*
 *---------------------------------------------------------------------
 *
 * DnDUngrabTimeout --
 *
 *      UngrabTimeout callback for GH DnD.
 *
 * Results:
 *      Always TRUE.
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------
 */

static Bool
DnDUngrabTimeout(void *clientData) // IN
{
   ASSERT(clientData);
   DnD *dnd = (DnD *)clientData;
   dnd->UngrabTimeout();
   return TRUE;
}


/*
 *---------------------------------------------------------------------
 *
 * DnDUnityDetTimeout --
 *
 *      UnityDnDDetTimeout callback for GH DnD.
 *
 * Results:
 *      Always TRUE.
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------
 */

static Bool
DnDUnityDetTimeout(void *clientData)      // IN
{
   ASSERT(clientData);
   DnD *dnd = (DnD *)clientData;
   dnd->UnityDnDDetTimeout();
   return TRUE;
}


/*
 *---------------------------------------------------------------------
 *
 * DnDHideDetWndTimer --
 *
 *      HideDetWndTimer callback.
 *
 * Results:
 *      Always TRUE.
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------
 */

static Bool
DnDHideDetWndTimer(void *clientData)      // IN
{
   Debug("%s: entering\n", __FUNCTION__);
   ASSERT(clientData);
   DnD *dnd = (DnD *)clientData;
   dnd->SetHideDetWndTimer(NULL);
   dnd->UpdateDetWnd(false, 0, 0);
   return TRUE;
}


/*
 *---------------------------------------------------------------------
 *
 * DnD::DnD --
 *
 *      Constructor for DnD.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------
 */

DnD::DnD(DblLnkLst_Links *eventQueue) // IN
   : mRpc(NULL),
     mVmxDnDVersion(0),
     mDnDAllowed(false),
     mStagingDir(""),
     mUngrabTimeout(NULL),
     mUnityDnDDetTimeout(NULL),
     mHideDetWndTimer(NULL),
     mEventQueue(eventQueue)
{
   mState = DNDSTATE_INVALID;
   mFeedback = DROP_UNKNOWN;
   CPClipboard_Init(&mClipboard);
}


/*
 *---------------------------------------------------------------------
 *
 * DnD::~DnD --
 *
 *      Destructor.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------
 */

DnD::~DnD(void)
{
   delete mRpc;
   CPClipboard_Destroy(&mClipboard);

   /* Remove untriggered timers. */
   if (mHideDetWndTimer) {
      EventManager_Remove(mHideDetWndTimer);
      mHideDetWndTimer = NULL;
   }
   if (mUngrabTimeout) {
      EventManager_Remove(mUngrabTimeout);
      mUngrabTimeout = NULL;
   }
   if (mUnityDnDDetTimeout) {
      EventManager_Remove(mUnityDnDDetTimeout);
      mUnityDnDDetTimeout = NULL;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnD::VmxDnDVersionChanged --
 *
 *      Guest DnD version changed so should create corresponding Rpc and
 *      register callbacks for signals if version changed.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
DnD::VmxDnDVersionChanged(struct RpcIn *rpcIn, // IN
                          uint32 version)      // IN
{
   /* Do nothing if version is not changed. */
   if (mVmxDnDVersion == version) {
      return;
   }

   mVmxDnDVersion = version;

   delete mRpc;
   mRpc = NULL;
   mState = DNDSTATE_INVALID;
   Debug("%s: state changed to INVALID\n", __FUNCTION__);

   switch (version) {
   case 1:
   case 2:
      /* Here should create DnDRpcV2 for version 1 & 2. */
      break;
   case 3:
      mRpc = new DnDRpcV3(rpcIn);
      break;
   default:
      Debug("%s: got unsupported vmx DnD version %u.\n", __FUNCTION__, version);
      return;
   }

   if (mRpc) {
      mRpc->ghUpdateUnityDetWndChanged.connect(
         sigc::mem_fun(this, &DnD::OnGHUpdateUnityDetWnd));
      mRpc->ghQueryPendingDragChanged.connect(
         sigc::mem_fun(this, &DnD::OnGHQueryPendingDrag));
      mRpc->moveDetWndToMousePos.connect(
         sigc::mem_fun(this, &DnD::OnMoveDetWndToMousePos));
      mRpc->ghPrivateDropChanged.connect(
         sigc::mem_fun(this, &DnD::OnGHPrivateDrop));
      mRpc->ghCancelChanged.connect(
         sigc::mem_fun(this, &DnD::OnGHCancel));
      mRpc->hgDragEnterChanged.connect(
         sigc::mem_fun(this, &DnD::OnHGDragEnter));
      mRpc->hgDragStartChanged.connect(
         sigc::mem_fun(this, &DnD::OnHGDragStart));
      mRpc->hgCancelChanged.connect(
         sigc::mem_fun(this, &DnD::OnHGCancel));
      mRpc->hgDropChanged.connect(
         sigc::mem_fun(this, &DnD::OnHGDrop));
      mRpc->hgFileCopyDoneChanged.connect(
         sigc::mem_fun(this, &DnD::OnHGFileCopyDone));
      mRpc->updateMouseChanged.connect(
         sigc::mem_fun(this, &DnD::OnUpdateMouse));
      mState = DNDSTATE_READY;
      Debug("%s: state changed to READY\n", __FUNCTION__);
   }
}


/*
 *----------------------------------------------------------------------
 *
 * DnD::OnHGDragEnter --
 *
 *      This callback is trigged when host got dragEnter signal and
 *      related DnD data. Put data into local buffer, show detection
 *      window and inform host UI to simulate first mouse click in the
 *      guest.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
DnD::OnHGDragEnter(const CPClipboard *clip) // IN
{
   if (mState != DNDSTATE_READY &&
       mState != DNDSTATE_ENTERING) {
      /* Reset DnD for any wrong state. */
      Debug("%s: Bad state: %d\n", __FUNCTION__, mState);
      ResetDnD();
      return;
   }

   CPClipboard_Clear(&mClipboard);
   CPClipboard_Copy(&mClipboard, clip);

   /* Show detection window in (0, 0). */
   UpdateDetWnd(true, 0, 0);

   /*
    * Ask host to simulate mouse click inside the detection window.
    * Host should simulate all guest dragging for HG DnD.
    */
   mRpc->HGDragEnterDone(DRAG_DET_WINDOW_WIDTH / 2,
                         DRAG_DET_WINDOW_WIDTH / 2);
   mState = DNDSTATE_ENTERING;
   Debug("%s: state changed to ENTERING\n", __FUNCTION__);
}


/*
 *----------------------------------------------------------------------
 *
 * DnD::OnHGDragStart --
 *
 *      Host just simulated the first mouse click inside the detection
 *      window. Now guest can initialize HG DnD.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
DnD::OnHGDragStart(void) // IN
{
   if (mState != DNDSTATE_ENTERING) {
      /* Reset DnD for any wrong state. */
      Debug("%s: Bad state: %d\n", __FUNCTION__, mState);
      ResetDnD();
      return;
   }

   /* Setup staging directory. */
   mStagingDir = SetupDestDir("");
   if (mStagingDir.empty()) {
      return;
   }

   dragStartChanged.emit(&mClipboard, mStagingDir);
}


/*
 *----------------------------------------------------------------------
 *
 * DnD::HGDragStartDone --
 *
 *      Tell host we are done with DnD initialization.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
DnD::HGDragStartDone(void)
{
   if (mState != DNDSTATE_ENTERING) {
      /* Reset DnD for any wrong state. */
      Debug("%s: Bad state: %d\n", __FUNCTION__, mState);
      ResetDnD();
      return;
   }

   mRpc->HGDragStartDone();
   mState = DNDSTATE_DRAGGING_INSIDE;
   Debug("%s: state changed to DRAGGING\n", __FUNCTION__);
}


/*
 *----------------------------------------------------------------------
 *
 * DnD::SetFeedback --
 *
 *      Got feedback from local UI. Notify host to update. For HG DnD.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
DnD::SetFeedback(DND_DROPEFFECT effect) // IN
{
   if (mState == DNDSTATE_INVALID) {
      /* Wrong state. */
      Debug("%s: Bad state: %d\n", __FUNCTION__, mState);
      return;
   }

   ASSERT(mRpc);
   Debug("%s: feedback is %d\n", __FUNCTION__, effect);
   mRpc->HGUpdateFeedback(effect);
}


/*
 *----------------------------------------------------------------------
 *
 * DnD::OnHGCancel --
 *
 *      Host just cancelled the HG DnD.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
DnD::OnHGCancel(void)
{
   /*
    * UI layer should do all cleanup, and simulate a mouse drop inside the
    * detection window to cancel the HG DnD. Because our detection window will
    * ignore the drop, There will be no further action with the drop.
    */
   sourceCancelChanged.emit();
   /*
    * Add a timer to hide the detection window after the drop. Without the
    * timer, the drop may happen after the window is hidden, and some other
    * applicationt may pick the drop up and cause unexpected action (like
    * file copy/move).
    */
   /* Hide detection window. */
   if (NULL == mHideDetWndTimer) {
      mHideDetWndTimer = EventManager_Add(mEventQueue, HIDE_DET_WND_TIMER,
                                          DnDHideDetWndTimer, this);
   }
   mState = DNDSTATE_READY;
   Debug("%s: state changed to READY\n", __FUNCTION__);
}


/*
 *----------------------------------------------------------------------
 *
 * DnD::OnHGDrop --
 *
 *      Host just drop the data but not copy file yet. Provide staging
 *      directory and add block.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
DnD::OnHGDrop(void)
{
   char cpName[FILE_MAXPATH];
   int32 cpNameSize;

   if (mState != DNDSTATE_DRAGGING_INSIDE) {
      /* Reset DnD for any wrong state. */
      Debug("%s: Bad state: %d\n", __FUNCTION__, mState);
      ResetDnD();
      return;
   }

   sourceDropChanged.emit();

   if (CPClipboard_ItemExists(&mClipboard, CPFORMAT_FILELIST)) {
      /* Convert staging name to CP format. */
      cpNameSize = CPNameUtil_ConvertToRoot(mStagingDir.c_str(),
                                            sizeof cpName,
                                            cpName);
      if (cpNameSize < 0) {
         Debug("%s: Error, could not convert to CPName.\n", __FUNCTION__);
         return;
      }

      mRpc->HGDropDone(cpName, cpNameSize);
   } else {
      /* For non-file formats, the DnD is done. Hide detection window. */
      UpdateDetWnd(false, 0, 0);
   }
   mState = DNDSTATE_READY;
   Debug("%s: state changed to READY\n", __FUNCTION__);
}


/*
 *----------------------------------------------------------------------
 *
 * DnD::OnHGFileCopyDone --
 *
 *      Host just finished file copy for HG DnD. Remove the block from
 *      staging directory.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
DnD::OnHGFileCopyDone(bool success,                  // IN
                      std::vector<uint8> stagingDir) // IN
{
   if (!success && !mStagingDir.empty()) {
      /* Delete all files if host canceled the file transfer. */
      DnD_DeleteStagingFiles(mStagingDir.c_str(), FALSE);
      mStagingDir.clear();
   }
   fileCopyDoneChanged.emit(success, stagingDir);
}


/*
 *----------------------------------------------------------------------
 *
 * DnD::OnUpdateMouse --
 *
 *      Move mouse to position (x, y) with button down or up.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
DnD::OnUpdateMouse(int32 x,   // IN
                   int32 y)   // IN
{
   updateMouseChanged.emit(x, y);
}


/*
 *----------------------------------------------------------------------------
 *
 * DnD::SetupDestDir --
 *
 *      Creates a directory for file transfer. If the destination
 *      dir is provided, we will attempt to copy files to the directory.
 *
 *      Input directory will be in local format.
 *
 * Results:
 *      The destination directory on success, an empty string on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

std::string
DnD::SetupDestDir(const std::string &destDir) // IN
{
   static const std::string failDir = "";

   if (!destDir.empty() && File_Exists(destDir.c_str())) {
      mStagingDir = destDir;
      const char *lastSep = Str_Strrchr(mStagingDir.c_str(), DIRSEPC);
      if (lastSep && lastSep[1] != '\0') {
         mStagingDir += DIRSEPS;
      }

      return mStagingDir;
   } else {
      char *newDir;

      newDir = DnD_CreateStagingDirectory();
      if (newDir != NULL) {
         mStagingDir = newDir;

         char *lastSep = Str_Strrchr(newDir, DIRSEPC);
         if (lastSep && lastSep[1] != '\0') {
            mStagingDir += DIRSEPS;
         }
         free(newDir);
         Debug("%s: destdir: %s", __FUNCTION__, mStagingDir.c_str());

         return mStagingDir;
      } else {
         Debug("%s: destdir not created", __FUNCTION__);
         return failDir;
      }
   }
}


/*
 *----------------------------------------------------------------------
 *
 * DnD::OnGHUpdateUnityDetWnd --
 *
 *      This callback is trigged when users clicks into any Unity window
 *      or just release the mouse button. Either show the full-screen
 *      detection window right after the Unity window, or hide the
 *      detection window.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
DnD::OnGHUpdateUnityDetWnd(bool bShow,        // IN
                           uint32 unityWndId) // IN
{
   if (bShow && mState != DNDSTATE_READY) {
      /*
       * Reset DnD for any wrong state. Only do this when host asked to
       * show the window.
       */
      Debug("%s: Bad state: %d\n", __FUNCTION__, mState);
      ResetDnD();
      return;
   }

   if (bShow) {
      /*
       * When show full screen window, also show the small top-most
       * window in (1, 1). After detected a GH DnD, the full screen
       * window will be hidden to avoid blocking other windows. So use
       * this window to accept drop in cancel case.
       */
      UpdateDetWnd(bShow, 1, 1);
      if (mUnityDnDDetTimeout) {
         EventManager_Remove(mUnityDnDDetTimeout);
         mUnityDnDDetTimeout = NULL;
      }
      mUnityDnDDetTimeout = EventManager_Add(mEventQueue, UNITY_DND_DET_TIMEOUT,
                                             DnDUnityDetTimeout, this);
   } else {
      /*
       * If there is active DnD, the regular detection window will be hidden
       * after DnD is done.
       */
      if (mState == DNDSTATE_READY) {
         UpdateDetWnd(false, 0, 0);
      }
   }

   /* Show/hide the full screent detection window. */
   updateUnityDetWndChanged.emit(bShow, unityWndId, false);
   Debug("%s: updating Unity detection window, bShow %d, id %u\n",
         __FUNCTION__, bShow, unityWndId);
}


/*
 *----------------------------------------------------------------------
 *
 * DnD::UnityDnDDetTimeout --
 *
 *      Can not detect pending GH DnD within UNITY_DND_DET_TIMEOUT, put
 *      the full screen detection window to bottom most.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
DnD::UnityDnDDetTimeout(void)
{
   mUnityDnDDetTimeout = NULL;
   updateUnityDetWndChanged.emit(true, 0, true);
}


/*
 *----------------------------------------------------------------------
 *
 * DnD::OnGHQueryPendingDrag --
 *
 *      This callback is trigged when host got initial GH DnD signal.
 *      UI will check if there is any pending GH dragging.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
DnD::OnGHQueryPendingDrag(int32 x, // IN
                          int32 y) // IN
{
   if (mState != DNDSTATE_READY) {
      /* Reset DnD for any wrong state. */
      Debug("%s: Bad state: %d\n", __FUNCTION__, mState);
      ResetDnD();
      return;
   }

   /* Show detection window to detect pending GH DnD. */
   UpdateDetWnd(true, x, y);
   mState = DNDSTATE_QUERY_EXITING;
   Debug("%s: state changed to QUERY_EXITING\n", __FUNCTION__);

   /*
    * Add event to fire and hide our window if a DnD is not pending.  Note that
    * this is here in case the drag isn't picked up by our drag detection window
    * for some reason.
    */
   if (NULL == mUngrabTimeout) {
      mUngrabTimeout = EventManager_Add(mEventQueue, UNGRAB_TIMEOUT,
                                        DnDUngrabTimeout, this);
   }
}


/*
 *----------------------------------------------------------------------
 *
 * DnD::UngrabTimeout --
 *
 *      Can not detect pending GH DnD within UNGRAB_TIMEOUT, cancel it.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
DnD::UngrabTimeout(void)
{
   mUngrabTimeout = NULL;

   if (mState != DNDSTATE_QUERY_EXITING) {
      /* Reset DnD for any wrong state. */
      Debug("%s: Bad state: %d\n", __FUNCTION__, mState);
      ResetDnD();
      return;
   }

   ASSERT(mRpc);
   mRpc->GHUngrabTimeout();


   /* Hide detection window. */
   UpdateDetWnd(false, 0, 0);
   mState = DNDSTATE_READY;
}


/*
 *----------------------------------------------------------------------
 *
 * DnD::DragEnter --
 *
 *      This is reponse to OnGHQueryPendingDrag if there is a pending
 *      GH dragging.
 *
 *      If guest can not detect any pending GH dragging, UngrabTimeout
 *      will notify host to cancel GH DnD.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
DnD::DragEnter(const CPClipboard *clip)
{
   if (DNDSTATE_DRAGGING_OUTSIDE == mState ||
       DNDSTATE_DRAGGING_INSIDE == mState) {
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
   if (mState != DNDSTATE_QUERY_EXITING && mState != DNDSTATE_READY) {
      /* Reset DnD for any wrong state. */
      Debug("%s: Bad state: %d\n", __FUNCTION__, mState);
      ResetDnD();
      return;
   }

   /* Remove untriggered ungrab timer. */
   if (mUngrabTimeout) {
      EventManager_Remove(mUngrabTimeout);
      mUngrabTimeout = NULL;
   }

   ASSERT(mRpc);
   mRpc->GHDragEnter(clip);
   mState = DNDSTATE_DRAGGING_OUTSIDE;
   Debug("%s: state changed to DRAGGING_OUTSIDE\n", __FUNCTION__);
}


/*
 *----------------------------------------------------------------------
 *
 * DnD::OnMoveDetWndToMousePos --
 *
 *      Move detection windows to current cursor position to prevent
 *      unexpected drop.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
DnD::OnMoveDetWndToMousePos(void)
{
   Debug("%s: entering\n", __FUNCTION__);
   moveDetWndToMousePos.emit();
}


/*
 *----------------------------------------------------------------------
 *
 * DnD::OnGHPrivateDrop --
 *
 *      User started a GH DnD, dragged back to VM and dropped. In guest
 *      side we should simulate the drop, hide detection window and reset
 *      the state machine.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
DnD::OnGHPrivateDrop(int32 x, // IN
                     int32 y) // IN
{
   targetPrivateDropChanged.emit(x, y);
   updateDetWndChanged.emit(false, 0, 0);
   mState = DNDSTATE_READY;
   Debug("%s: state changed to READY\n", __FUNCTION__);
}


/*
 *----------------------------------------------------------------------
 *
 * DnD::OnGHCancel --
 *
 *      Host cancelled current GH DnD.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
DnD::OnGHCancel(void)
{
   /*
    * UI layer should do all cleanup, and simulate a mouse drop inside the
    * detection window to cancel the GH DnD. Because our detection window will
    * ignore the drop, There will be no further action with the drop.
    */
   ghCancel.emit();
   /*
    * Add a timer to hide the detection window after the drop. Without the
    * timer, the drop may happen after the window is hidden, and some other
    * applicationt may pick the drop up and cause unexpected action (like
    * file copy/move).
    */
   if (NULL == mHideDetWndTimer) {
      mHideDetWndTimer = EventManager_Add(mEventQueue, HIDE_DET_WND_TIMER,
                                          DnDHideDetWndTimer, this);
   }
   /* Remove the timer. */
   if (mUngrabTimeout) {
      EventManager_Remove(mUngrabTimeout);
      mUngrabTimeout = NULL;
   }
   mState = DNDSTATE_READY;
   Debug("%s: state changed to READY\n", __FUNCTION__);
}


/*
 *----------------------------------------------------------------------
 *
 * DnD::UpdateDetWnd --
 *
 *      Update the detection window.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
DnD::UpdateDetWnd(bool show, // IN
                  int32 x,   // IN
                  int32 y)   // IN
{
   if (mHideDetWndTimer) {
      EventManager_Remove(mHideDetWndTimer);
      mHideDetWndTimer = NULL;
   }
   updateDetWndChanged.emit(show, x, y);
}


/*
 *----------------------------------------------------------------------
 *
 * DnD::ResetDnD --
 *
 *      Cancel both HG and GH DnD.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
DnD::ResetDnD(void)
{
   OnHGCancel();
   OnGHCancel();
   reset.emit();
}

