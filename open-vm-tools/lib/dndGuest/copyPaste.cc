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
 * copyPaste.cc --
 *
 *     Implementation of common layer CopyPaste object for guest.
 */

#include "copyPaste.hh"
#include "copyPasteRpcV3.hh"
#include "dndFileList.hh"

extern "C" {
   #include "util.h"
   #include "debug.h"
   #include "file.h"
   #include "str.h"
   #include "cpNameUtil.h"
   #include "hostinfo.h"
   #include "dndClipboard.h"
}

/*
 *---------------------------------------------------------------------
 *
 * CopyPaste::CopyPaste --
 *
 *      Constructor for CopyPaste.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------
 */

CopyPaste::CopyPaste(void) // IN
   : mRpc(NULL),
     mVmxCopyPasteVersion(0)
{
   mState = CPSTATE_INVALID;
}


/*
 *---------------------------------------------------------------------
 *
 * CopyPaste::~CopyPaste --
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

CopyPaste::~CopyPaste(void)
{
   delete mRpc;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPaste::VmxCopyPasteVersionChanged --
 *
 *      Vmx CopyPaste version changed so should create corresponding Rpc and
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
CopyPaste::VmxCopyPasteVersionChanged(struct RpcIn *rpcIn, // IN
                                      uint32 version)      // IN
{
   /* Do nothing if version is not changed. */
   if (mVmxCopyPasteVersion == version) {
      return;
   }

   mVmxCopyPasteVersion = version;
   Debug("%s: Version: %d", __FUNCTION__, version);

   delete mRpc;
   mRpc = NULL;

   mState = CPSTATE_INVALID;
   Hostinfo_GetTimeOfDay(&mStateChangeTime);

   switch (version) {
   case 1:
   case 2:
      /* Here should create CopyPasteRpcV2 for version 1 & 2. */
      break;
   case 3:
      mRpc = new CopyPasteRpcV3(rpcIn);
      break;
   default:
      Debug("%s: got unsupported guest CopyPaste version %u.\n",
            __FUNCTION__, version);
      return;
   }

   if (mRpc) {
      mRpc->ghGetClipboardChanged.connect(
         sigc::mem_fun(this, &CopyPaste::OnGetLocalClipboard));
      mRpc->hgSetClipboardChanged.connect(
         sigc::mem_fun(this, &CopyPaste::OnGetRemoteClipboardDone));
      mRpc->hgFileCopyDoneChanged.connect(
         sigc::mem_fun(this, &CopyPaste::OnHGFileCopyDone));
      mState = CPSTATE_READY;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * CopyPaste::OnGetRemoteClipboardDone --
 *
 *      Got clipboard data from host. Put it into local buffer and
 *      inform UI to process the clipboard data.
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
CopyPaste::OnGetRemoteClipboardDone(const CPClipboard *clip) // IN: new clipboard
{
   newClipboard.emit(clip);
   mState = CPSTATE_READY;
   Hostinfo_GetTimeOfDay(&mStateChangeTime);
}


/*
 *----------------------------------------------------------------------
 *
 * CopyPaste::OnGetLocalClipboard --
 *
 *      Host is asking for guest clipboard data. Try to get it from local
 *      UI and send to host with GHGetClipboardDone.
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
CopyPaste::OnGetLocalClipboard(void)
{
   CPClipboard clip;
   bool ready;

   CPClipboard_Init(&clip);
   ready = localGetClipboard.emit(&clip);
   /*
    * if caller decided to handle this asyncronously, it is responsible for
    * calling SetRemoteClipboard() when it has completed. Otherwise, call it
    * now.
    */
   if (ready) {
      SetRemoteClipboard(&clip);
   }
   CPClipboard_Destroy(&clip);
}


/*
 *----------------------------------------------------------------------
 *
 * CopyPaste::OnHGFileCopyDone --
 *
 *      Host finished file copy. Inform UI to remove the block.
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
CopyPaste::OnHGFileCopyDone(bool success)
{
   if (!success && !mStagingDir.empty()) {
      /* Delete all files if host canceled the file transfer. */
      DnD_DeleteStagingFiles(mStagingDir.c_str(), FALSE);
      mStagingDir.clear();
   }
   localGetFilesDoneChanged.emit(success);
}


/*
 *----------------------------------------------------------------------------
 *
 * CopyPaste::SetRemoteClipboard --
 *
 *      Sets the internal clipboard. Also synchronizes remote clipboard with
 *      the guest clipboard.
 *
 * Results:
 *      True if success, false otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

bool
CopyPaste::SetRemoteClipboard(const CPClipboard* clip)  // IN: new clipboard
{
   if (!mRpc) {
      Debug("%s: no valid rpc, guest version is %u.\n",
            __FUNCTION__, mVmxCopyPasteVersion);
      return false;
   }

   if (mState == CPSTATE_INVALID) {
      Debug("%s: Invalid state.", __FUNCTION__);
      return false;
   }

   if (!IsCopyPasteAllowed()) {
      Debug("%s: CopyPasteAllowed() returned false.", __FUNCTION__);
      return false;
   }

   /* Send clipboard data to remote side. */
   mRpc->GHGetClipboardDone(clip);

   return true;
}


/*
 *----------------------------------------------------------------------------
 *
 * CopyPaste::GetFiles --
 *
 *      Gets the files that are referenced on the clipboard.
 *
 * Results:
 *      The destination directory if we can start file copy, otherwise empty
 *      string.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

std::string
CopyPaste::GetFiles(std::string dir) // IN: suggested destination directory
                                     //     in local format
{
   std::string destDir;
   char cpName[FILE_MAXPATH];
   int32 cpNameSize;

   if (mState == CPSTATE_INVALID) {
      Debug("%s: Invalid state.", __FUNCTION__);
      return "";
   }

   /* Setup staging directory. */
   destDir = SetupDestDir(dir);
   if (destDir.empty()) {
      return "";
   }

   /* XXX Here should first convert encoding to precomposed UTF8. */

   /* Convert staging name to CP format. */
   cpNameSize = CPNameUtil_ConvertToRoot(destDir.c_str(),
                                         sizeof cpName,
                                         cpName);
   if (cpNameSize < 0) {
      Debug("%s: Error, could not convert to CPName.\n", __FUNCTION__);
      return "";
   }

   if (!mRpc->HGStartFileCopy(cpName, cpNameSize)) {
      return "";
   }

   mStagingDir = destDir;

   return destDir;
}


/*
 *----------------------------------------------------------------------------
 *
 * CopyPaste::SetupDestDir --
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
CopyPaste::SetupDestDir(const std::string &destDir)       // IN:
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


