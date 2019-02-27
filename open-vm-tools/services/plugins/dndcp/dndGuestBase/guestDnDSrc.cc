/*********************************************************
 * Copyright (C) 2010-2019 VMware, Inc. All rights reserved.
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
 * @guestDnDSrc.cc --
 *
 * Implementation of common layer GuestDnDSrc object for guest.
 */

#include "guestDnD.hh"
#include "util.h"

extern "C" {
   #include "dndClipboard.h"
   #include "debug.h"
   #include "cpNameUtil.h"
}

#include "file.h"
#include "str.h"


/**
 * Constructor.
 *
 * @param[in] mgr guest DnD manager
 */

GuestDnDSrc::GuestDnDSrc(GuestDnDMgr *mgr)
 : mMgr(mgr)
{
   ASSERT(mMgr);
   mMgr->GetRpc()->srcDropChanged.connect(
      sigc::mem_fun(this, &GuestDnDSrc::OnRpcDrop));
   mMgr->GetRpc()->srcCancelChanged.connect(
      sigc::mem_fun(this, &GuestDnDSrc::OnRpcCancel));
   mMgr->GetRpc()->getFilesDoneChanged.connect(
      sigc::mem_fun(this, &GuestDnDSrc::OnRpcGetFilesDone));

   CPClipboard_Init(&mClipboard);
}


/**
 * Destructor.
 */

GuestDnDSrc::~GuestDnDSrc(void)
{
   ASSERT(mMgr);
   CPClipboard_Destroy(&mClipboard);
   /* Reset current session id after finished. */
   mMgr->SetSessionId(0);
}


/**
 * Rpc got dragBegin with valid data. Ask UI to show the detection window and
 * start host->guest DnD inside guest.
 *
 * @param[in] clip cross-platform clipboard data.
 */

void
GuestDnDSrc::OnRpcDragBegin(const CPClipboard *clip)
{
   ASSERT(mMgr);
   ASSERT(clip);

   g_debug("%s: state is %d\n", __FUNCTION__, mMgr->GetState());

   if (NeedSetupDestDir(clip)) {
      /* Setup staging directory. */
      mStagingDir = SetupDestDir("");
      if (mStagingDir.empty()) {
         g_debug("%s: SetupDestDir failed.\n", __FUNCTION__);
         return;
      }
   }

   /* Show detection window in (0, 0). */
   mMgr->ShowDetWnd(0, 0);

   CPClipboard_Clear(&mClipboard);
   CPClipboard_Copy(&mClipboard, clip);

   mMgr->SetState(GUEST_DND_SRC_DRAGBEGIN_PENDING);
   g_debug("%s: state changed to DRAGBEGIN_PENDING\n", __FUNCTION__);

   mMgr->srcDragBeginChanged.emit(&mClipboard, mStagingDir);
}


/**
 * Guest UI got dragBeginDone. Send dragBeginDone cmd to controller.
 */

void
GuestDnDSrc::UIDragBeginDone(void)
{
   ASSERT(mMgr);

   g_debug("%s: state is %d\n", __FUNCTION__, mMgr->GetState());
   if (mMgr->GetState() != GUEST_DND_SRC_DRAGBEGIN_PENDING) {
      /* Reset DnD for any wrong state. */
      g_debug("%s: Bad state: %d\n", __FUNCTION__, mMgr->GetState());
      goto error;
   }

   if (!mMgr->GetRpc()->SrcDragBeginDone(mMgr->GetSessionId())) {
      g_debug("%s: SrcDragBeginDone failed\n", __FUNCTION__);
      goto error;
   }

   mMgr->SetState(GUEST_DND_SRC_DRAGGING);
   g_debug("%s: state changed to DRAGGING\n", __FUNCTION__);
   return;

error:
   mMgr->ResetDnD();
}


/**
 * Guest UI got DnD feedback. Send updateFeedback cmd to controller.
 *
 * @param[in] feedback
 */

void
GuestDnDSrc::UIUpdateFeedback(DND_DROPEFFECT feedback)
{
   ASSERT(mMgr);

   g_debug("%s: state is %d\n", __FUNCTION__, mMgr->GetState());

   /* This operation needs a valid session id from controller. */
   if (0 == mMgr->GetSessionId()) {
      g_debug("%s: can not get a valid session id from controller.\n",
              __FUNCTION__);
      return;
   }
   if (!mMgr->GetRpc()->UpdateFeedback(mMgr->GetSessionId(), feedback)) {
      g_debug("%s: UpdateFeedback failed\n", __FUNCTION__);
      mMgr->ResetDnD();
   }
}


/**
 * Got drop cmd from rpc. Ask UI to simulate the drop at (x, y).
 *
 * @param[in] sessionId active DnD session id
 * @param[in] x mouse position x.
 * @param[in] y mouse position y.
 */

void
GuestDnDSrc::OnRpcDrop(uint32 sessionId,
                       int32 x,
                       int32 y)
{
   char cpName[FILE_MAXPATH];
   int32 cpNameSize;

   ASSERT(mMgr);

   g_debug("%s: state is %d\n", __FUNCTION__, mMgr->GetState());
   if (mMgr->GetState() != GUEST_DND_SRC_DRAGGING) {
      /* Reset DnD for any wrong state. */
      g_debug("%s: Bad state: %d\n", __FUNCTION__, mMgr->GetState());
      goto error;
   }
   mMgr->srcDropChanged.emit();

   if (CPClipboard_ItemExists(&mClipboard, CPFORMAT_FILELIST)) {
      /* Convert staging name to CP format. */
      cpNameSize = CPNameUtil_ConvertToRoot(mStagingDir.c_str(),
                                            sizeof cpName,
                                            cpName);
      if (cpNameSize < 0) {
         g_debug("%s: Error, could not convert to CPName.\n", __FUNCTION__);
         goto error;
      }

      if (!mMgr->GetRpc()->SrcDropDone(sessionId,
                                       (const uint8 *)cpName,
                                       cpNameSize)) {
         g_debug("%s: SrcDropDone failed\n", __FUNCTION__);
         goto error;
      }
   } else {
      /* For non-file formats, the DnD is done. Hide detection window. */
      mMgr->HideDetWnd();
      mMgr->SetState(GUEST_DND_READY);
      g_debug("%s: state changed to READY\n", __FUNCTION__);
   }
   return;

error:
   mMgr->ResetDnD();
}


/**
 * Got cancel cmd from rpc. Ask UI to cancel the DnD as source.
 *
 * @param[in] sessionId active DnD session id
 */

void
GuestDnDSrc::OnRpcCancel(uint32 sessionId)
{
   ASSERT(mMgr);

   g_debug("%s: state is %d\n", __FUNCTION__, mMgr->GetState());
   mMgr->srcCancelChanged.emit();
   mMgr->DelayHideDetWnd();
   mMgr->SetState(GUEST_DND_READY);
   g_debug("%s: state changed to READY\n", __FUNCTION__);
}


/**
 * Got getFileDone cmd from rpc. Reset state machine and hide detection window.
 *
 * @param[in] sessionId active DnD session id
 * @param[in] success if the file transfer is successful or not
 * @param[in] stagingCP staging dir name in cross-platform format
 * @param[in] sz the staging dir name size
 */

void
GuestDnDSrc::OnRpcGetFilesDone(uint32 sessionId,
                               bool success,
                               const uint8 *stagingDirCP,
                               uint32 sz)
{
   CleanStagingFiles(success);

   /* UI should remove block with this signal. */
   mMgr->getFilesDoneChanged.emit(success);
   mMgr->HideDetWnd();
   mMgr->SetState(GUEST_DND_READY);
   g_debug("%s: state changed to READY\n", __FUNCTION__);
}


/**
 * Creates a directory for file transfer. If the destination dir is provided,
 * we will attempt to copy files to that directory.
 *
 * @param[in] destDir the preferred destination directory
 *
 * @return the destination directory on success, an empty string on failure.
 */

const std::string &
GuestDnDSrc::SetupDestDir(const std::string &destDir)
{
   return mStagingDir;
}
