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
 * @guestCopyPaste.hh --
 *
 * CopyPaste common layer classes for guest.
 */

#ifndef GUEST_COPY_PASTE_HH
#define GUEST_COPY_PASTE_HH

#include <sigc++/trackable.h>
#include <string>
#include "copyPasteRpc.hh"
#include "guestFileTransfer.hh"

#include "dnd.h"

enum GUEST_CP_STATE {
   GUEST_CP_INVALID = 0,
   GUEST_CP_READY,
   GUEST_CP_HG_FILE_COPYING,
};

class GuestCopyPasteSrc;
class GuestCopyPasteDest;

class GuestCopyPasteMgr
   : public sigc::trackable
{
public:
   GuestCopyPasteMgr(DnDCPTransport *transport);
   ~GuestCopyPasteMgr(void);

   sigc::signal<void, const CPClipboard*> srcRecvClipChanged;
   sigc::signal<void> destRequestClipChanged;
   sigc::signal<void, bool> getFilesDoneChanged;

   GUEST_CP_STATE GetState(void) { return mCPState; }
   void SetState(GUEST_CP_STATE state);
   CopyPasteRpc *GetRpc(void) { return mRpc; }
   GuestCopyPasteSrc *GetCopyPasteSrc(void)
      { return mSrc; }
   GuestCopyPasteDest *GetCopyPasteDest(void)
      { return mDest; }
   void ResetCopyPaste(void);

   uint32 GetSessionId(void) { return mSessionId; }
   void SetSessionId(uint32 id);

   void DestUISendClip(const CPClipboard *clip);
   const std::string SrcUIRequestFiles(const std::string &dir = "");
   bool IsCopyPasteAllowed (void) { return mCopyPasteAllowed; }
   void SetCopyPasteAllowed(bool isCopyPasteAllowed)
   { mCopyPasteAllowed = isCopyPasteAllowed; }
   void VmxCopyPasteVersionChanged(uint32 version);
   Bool CheckCapability(uint32 capsRequest);
private:
   void OnRpcSrcRecvClip(uint32 sessionId,
                         bool isActive,
                         const CPClipboard *clip);
   void OnRpcDestRequestClip(uint32 sessionId,
                             bool isActive);
   void OnPingReply(uint32 capabilities);
   GuestCopyPasteSrc *mSrc;
   GuestCopyPasteDest *mDest;
   CopyPasteRpc *mRpc;
   GUEST_CP_STATE mCPState;
   DnDCPTransport *mTransport;
   uint32 mSessionId;
   bool mCopyPasteAllowed;
   uint32 mResolvedCaps;       // caps as returned in ping reply, or default.
};


class GuestCopyPasteSrc
   : public sigc::trackable
{
public:
   GuestCopyPasteSrc(GuestCopyPasteMgr *mgr);
   ~GuestCopyPasteSrc(void);

   /* Common CopyPaste layer API exposed to UI for CopyPaste source. */
   const std::string UIRequestFiles(const std::string &dir = "");
   void OnRpcRecvClip(bool isActive,
                      const CPClipboard *clip);

private:
   /* Callbacks from rpc for CopyPaste source. */
   void OnRpcGetFilesDone(uint32 sessionId,
                          bool success,
                          const uint8 *stagingDirCP,
                          uint32 sz);
   const std::string &SetupDestDir(const std::string &destDir);

   GuestCopyPasteMgr *mMgr;
   CPClipboard mClipboard;
   std::string mStagingDir;
};


class GuestCopyPasteDest
   : public sigc::trackable
{
public:
   GuestCopyPasteDest(GuestCopyPasteMgr *mgr);

   /* Common CopyPaste layer API exposed to UI for CopyPaste destination. */
   void UISendClip(const CPClipboard *clip);
   /* Callbacks from rpc for CopyPaste destination. */
   void OnRpcRequestClip(bool isActive);

private:
   GuestCopyPasteMgr *mMgr;
   bool mIsActive;
};


#endif // GUEST_COPY_PASTE_HH

