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
 * @guestDnD.hh --
 *
 * DnD common layer classes for guest.
 */

#ifndef GUEST_DND_HH
#define GUEST_DND_HH

#include <sigc++/trackable.h>
#include "dndRpcV4.hh"
#include "guestFileTransfer.hh"

#include "capsProvider.h"

#include <string>

#include "dnd.h"

extern "C" {
   #include "vmware/tools/plugin.h"
}

#define UNGRAB_TIMEOUT 500        // 0.5s
#define HIDE_DET_WND_TIMER 500    // 0.5s
#define UNITY_DND_DET_TIMEOUT 500 // 0.5s

enum GUEST_DND_STATE {
   GUEST_DND_INVALID = 0,
   GUEST_DND_READY,
   /* As destination. */
   GUEST_DND_QUERY_EXITING,
   GUEST_DND_DEST_DRAGGING,
   /* In private dragging mode. */
   GUEST_DND_PRIV_DRAGGING,
   /* As source. */
   GUEST_DND_SRC_DRAGBEGIN_PENDING,
   GUEST_DND_SRC_CANCEL_PENDING,
   GUEST_DND_SRC_DRAGGING,
};

class GuestDnDSrc;
class GuestDnDDest;

class GuestDnDMgr
   : public sigc::trackable, public CapsProvider
{
public:
   GuestDnDMgr(DnDCPTransport *transport,
               ToolsAppCtx *ctx);
   virtual ~GuestDnDMgr(void);

   sigc::signal<void, int, int> moveMouseChanged;
   sigc::signal<void, bool, int, int> updateDetWndChanged;
   sigc::signal<void, bool, uint32, bool> updateUnityDetWndChanged;
   sigc::signal<void, GUEST_DND_STATE> stateChanged;

   sigc::signal<void, const CPClipboard*, std::string> srcDragBeginChanged;
   sigc::signal<void> srcDropChanged;
   sigc::signal<void> srcCancelChanged;
   sigc::signal<void, bool> getFilesDoneChanged;

   sigc::signal<void> destCancelChanged;
   sigc::signal<void, int32, int32> privDropChanged;
   sigc::signal<void> destMoveDetWndToMousePosChanged;

   GuestDnDSrc *GetDnDSrc(void) { return mSrc; }
   GuestDnDDest *GetDnDDest(void) { return mDest; }

   /* Common DnD layer API exposed to UI (all platforms) for DnD source. */
   void SrcUIDragBeginDone(void);
   void SrcUIUpdateFeedback(DND_DROPEFFECT feedback);

   void DestUIDragEnter(const CPClipboard *clip);
   DnDRpc *GetRpc(void) { return mRpc; }

   GUEST_DND_STATE GetState(void) { return mDnDState; }
   void SetState(GUEST_DND_STATE state);
   void UpdateDetWnd(bool show, int32 x, int32 y);
   void HideDetWnd(void) { UpdateDetWnd(false, 0, 0); }
   void ShowDetWnd(int32 x, int32 y) { UpdateDetWnd(true, x, y); }
   void UnityDnDDetTimeout(void);
   uint32 GetSessionId(void) { return mSessionId; }
   void SetSessionId(uint32 id) { mSessionId = id; }
   void ResetDnD(void);
   void DelayHideDetWnd(void);
   void SetHideDetWndTimer(GSource *gs) { mHideDetWndTimer = gs; }
   void UngrabTimeout(void);
   void RemoveUngrabTimeout(void);
   bool IsDnDAllowed (void) { return mDnDAllowed; }
   void SetDnDAllowed(bool isDnDAllowed)
   { mDnDAllowed = isDnDAllowed;}
   void VmxDnDVersionChanged(uint32 version);
   bool IsDragEnterAllowed(void);
   Bool CheckCapability(uint32 capsRequest);
   virtual bool NeedDoMouseCoordinateConversion() { return true; }

   static gboolean DnDUngrabTimeout(void *clientData);
   static gboolean DnDHideDetWndTimer(void *clientData);
   static gboolean DnDUnityDetTimeout(void *clientData);

protected:
   virtual void OnRpcSrcDragBegin(uint32 sessionId,
                                  const CPClipboard *clip) = 0;
   void OnRpcQueryExiting(uint32 sessionId, int32 x, int32 y);
   void OnRpcUpdateUnityDetWnd(uint32 sessionId,
                               bool show,
                               uint32 unityWndId);
   void OnRpcMoveMouse(uint32 sessionId,
                       int32 x,
                       int32 y);
   void OnPingReply(uint32 capabilities);

   virtual void AddDnDUngrabTimeoutEvent() = 0;
   virtual void AddUnityDnDDetTimeoutEvent() = 0;
   virtual void AddHideDetWndTimerEvent() = 0;
   virtual void CreateDnDRpcWithVersion(uint32 version) = 0;

   GuestDnDSrc *mSrc;
   GuestDnDDest *mDest;
   DnDRpc *mRpc;
   GUEST_DND_STATE mDnDState;
   uint32 mSessionId;
   GSource *mHideDetWndTimer;
   GSource *mUnityDnDDetTimeout;
   GSource *mUngrabTimeout;
   bool mDnDAllowed;
   DnDCPTransport *mDnDTransport;
   uint32 mCapabilities;
};


class GuestDnDSrc
   : public sigc::trackable
{
public:
   GuestDnDSrc(GuestDnDMgr *mgr);
   virtual ~GuestDnDSrc(void);

   /* Common DnD layer API exposed to UI (all platforms) for DnD source. */
   void UIDragBeginDone(void);
   void UIUpdateFeedback(DND_DROPEFFECT feedback);
   void OnRpcDragBegin(const CPClipboard *clip);

protected:
   /* Callbacks from rpc for DnD source. */
   void OnRpcUpdateMouse(uint32 sessionId, int32 x, int32 y);
   void OnRpcDrop(uint32 sessionId, int32 x, int32 y);
   virtual void OnRpcCancel(uint32 sessionId);
   void OnRpcGetFilesDone(uint32 sessionId,
                          bool success,
                          const uint8 *stagingDirCP,
                          uint32 sz);
   virtual const std::string& SetupDestDir(const std::string &destDir);
   virtual void CleanStagingFiles(bool fileTransferResult) { }
   virtual bool NeedSetupDestDir(const CPClipboard *clip) { return true; }

   GuestDnDMgr *mMgr;
   DnDRpc *mRpc;
   std::string mStagingDir;
   CPClipboard mClipboard;
};

class GuestDnDDest
   : public sigc::trackable
{
public:
   GuestDnDDest(GuestDnDMgr *mgr);
   ~GuestDnDDest(void);

   /* Common DnD layer API exposed to UI (all platforms) for DnD destination. */
   void UIDragEnter(const CPClipboard *clip);

private:
   /* Callbacks from rpc for DnD destination. */
   void OnRpcPrivDragEnter(uint32 sessionId);
   void OnRpcPrivDragLeave(uint32 sessionId, int32 x, int32 y);
   void OnRpcPrivDrop(uint32 sessionId, int32 x, int32 y);
   void OnRpcDrop(uint32 sessionId, int32 x, int32 y);
   void OnRpcCancel(uint32 sessionId);

   GuestDnDMgr *mMgr;
   CPClipboard mClipboard;
};

#endif // GUEST_DND_HH

