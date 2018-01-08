/*********************************************************
 * Copyright (C) 2010-2017 VMware, Inc. All rights reserved.
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
 * @guestCopyPasteSrc.cc --
 *
 * Implementation of common layer GuestCopyPasteSrc object for guest.
 */

#include "guestCopyPaste.hh"

extern "C" {
   #include <glib.h>

   #include "dndClipboard.h"
   #include "debug.h"
   #include "cpNameUtil.h"
}

#include "file.h"
#include "util.h"
#include "str.h"


/**
 * Constructor.
 *
 * @param[in] mgr guest CP manager
 */

GuestCopyPasteSrc::GuestCopyPasteSrc(GuestCopyPasteMgr *mgr)
 : mMgr(mgr)
{
   ASSERT(mMgr);
   mMgr->GetRpc()->getFilesDoneChanged.connect(
      sigc::mem_fun(this, &GuestCopyPasteSrc::OnRpcGetFilesDone));
   CPClipboard_Init(&mClipboard);
}


/**
 * Destructor.
 */

GuestCopyPasteSrc::~GuestCopyPasteSrc(void)
{
   ASSERT(mMgr);
   CPClipboard_Destroy(&mClipboard);
   /* Reset current session id after finished. */
   mMgr->SetSessionId(0);
}


/**
 * Got valid clipboard data from host. Emit srcRecvClipChanged signal if state
 * machine is right.
 *
 * @param[in] isActive active or passive CopyPaste.
 * @param[in] clip cross-platform clipboard data.
 */

void
GuestCopyPasteSrc::OnRpcRecvClip(bool isActive,
                                 const CPClipboard *clip)
{
   ASSERT(mMgr);
   ASSERT(clip);

   g_debug("%s: state is %d\n", __FUNCTION__, mMgr->GetState());

   CPClipboard_Clear(&mClipboard);
   CPClipboard_Copy(&mClipboard, clip);

   mMgr->srcRecvClipChanged.emit(clip);
}


/**
 * UI is asking for files. Send requestFiles cmd to controller.
 *
 * @param[in] dir staging directory in local format.
 *
 * @return The staging directory if succeed, otherwise empty string.
 */

const std::string
GuestCopyPasteSrc::UIRequestFiles(const std::string &dir)
{
   std::string destDir;
   char cpName[FILE_MAXPATH];
   int32 cpNameSize;

   if (mMgr->GetState() != GUEST_CP_READY) {
      /* Reset DnD for any wrong state. */
      g_debug("%s: Bad state: %d\n", __FUNCTION__, mMgr->GetState());
      goto error;
   }

   /* Setup staging directory. */
   destDir = SetupDestDir(dir);
   if (destDir.empty()) {
      goto error;
   }

   /* Convert staging name to CP format. */
   cpNameSize = CPNameUtil_ConvertToRoot(destDir.c_str(),
                                         sizeof cpName,
                                         cpName);
   if (cpNameSize < 0) {
      g_debug("%s: Error, could not convert to CPName.\n", __FUNCTION__);
      goto error;
   }

   if (!mMgr->GetRpc()->RequestFiles(mMgr->GetSessionId(),
                                     (const uint8 *)cpName,
                                     cpNameSize)) {
      goto error;
   }

   mStagingDir = destDir;
   mMgr->SetState(GUEST_CP_HG_FILE_COPYING);
   g_debug("%s: state changed to GUEST_CP_HG_FILE_COPYING\n", __FUNCTION__);

   return destDir;

error:
   mMgr->ResetCopyPaste();
   return "";
}


/**
 * The file transfer is finished. Emit getFilesDoneChanged signal and reset
 * local state.
 *
 * @param[in] sessionId active DnD session id
 * @param[in] success if the file transfer is successful or not
 * @param[in] stagingDirCP staging dir name in cross-platform format
 * @param[in] sz the staging dir name size
 */

void
GuestCopyPasteSrc::OnRpcGetFilesDone(uint32 sessionId,
                                     bool success,
                                     const uint8 *stagingDirCP,
                                     uint32 sz)
{
   if (!success && !mStagingDir.empty()) {
      /* Delete all files if host canceled the file transfer. */
      DnD_DeleteStagingFiles(mStagingDir.c_str(), FALSE);
      mStagingDir.clear();
   }
   /* UI should remove block with this signal. */
   mMgr->getFilesDoneChanged.emit(success);
   mMgr->SetState(GUEST_CP_READY);
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
GuestCopyPasteSrc::SetupDestDir(const std::string &destDir)
{
   mStagingDir = "";

   if (!destDir.empty() && File_Exists(destDir.c_str())) {
      mStagingDir = destDir;
      const char *lastSep = Str_Strrchr(mStagingDir.c_str(), DIRSEPC);
      if (lastSep && lastSep[1] != '\0') {
         mStagingDir += DIRSEPS;
      }
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
         g_debug("%s: destdir: %s", __FUNCTION__, mStagingDir.c_str());
      } else {
         g_debug("%s: destdir not created", __FUNCTION__);
      }
   }
   return mStagingDir;
}


