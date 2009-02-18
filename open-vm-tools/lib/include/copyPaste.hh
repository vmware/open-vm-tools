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
 * copyPaste.hh --
 *
 *     CopyPaste common layer object for guest.
 */

#ifndef COPY_PASTE_HH
#define COPY_PASTE_HH

#include <string>
#include <sigc++/trackable.h>
#include "copyPasteBase.h"
#include "copyPasteRpc.hh"

class CopyPaste
   : public CopyPasteBase,
     public sigc::trackable
{
   public:
      CopyPaste(void);
      virtual ~CopyPaste(void);

      /* Common CopyPaste layer API exposed to UI (all platforms). */
      /* Local UI as CopyPaste source. */
      virtual bool SetRemoteClipboard(const CPClipboard *clip);

      /* Local UI as CopyPaste target. */
      virtual bool GetRemoteClipboard(void) {return false;}
      virtual std::string GetFiles(std::string dir = "");

      virtual bool IsCopyPasteAllowed(void) { return mCopyPasteAllowed; }

      void VmxCopyPasteVersionChanged(struct RpcIn *rpcIn,
                                      uint32 version);
      void SetCopyPasteAllowed(bool isCopyPasteAllowed)
         { mCopyPasteAllowed = isCopyPasteAllowed; }

   private:
      /* Callbacks from rpc. */
      void OnGetLocalClipboard(void);
      void OnGetRemoteClipboardDone(const CPClipboard *clip);
      void OnHGFileCopyDone(bool success);

      std::string SetupDestDir(const std::string &destDir);

      bool mCanCopyPaste;
      bool mCopyPasteAllowed;

      CopyPasteRpc *mRpc;
      uint32 mVmxCopyPasteVersion;
      VmTimeType mStateChangeTime;

      std::string mStagingDir;
};

#endif // COPY_PASTE_HH

