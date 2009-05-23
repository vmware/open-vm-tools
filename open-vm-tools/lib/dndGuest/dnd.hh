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
 * dnd.hh --
 *
 *     DnD protocol object.
 */

#ifndef DND_HH
#define DND_HH

#include <string>
#include <sigc++/trackable.h>
#include "dndBase.h"
#include "dndRpc.hh"

struct Event;
struct DblLnkLst_Links;

class DnD
   : public DnDBase,
     public sigc::trackable
{
   public:
      DnD(DblLnkLst_Links *eventQueue);
      virtual ~DnD(void);

      /* Common DnD layer API exposed to UI (all platforms). */
      /* Local UI as DnD source. */
      virtual DND_DROPEFFECT GetFeedback(void) { return DROP_UNKNOWN; }
      virtual void SourceCancel(void) {}
      virtual void SourceDrop(DND_DROPEFFECT feedback) {}

      /* Local UI as DnD target. */
      virtual void DragEnter(const CPClipboard *clip);
      virtual void SetMouse(int32 x, int32 y, bool down) {}
      virtual void SetFeedback(DND_DROPEFFECT feedback);
      virtual void DragLeave(int32 x, int32 y) {}
      virtual void TargetCancel(void) {}
      virtual void TargetDrop(const CPClipboard *clip,
                              int32 x,
                              int32 y) {}

      virtual bool IsDnDAllowed(void) { return mDnDAllowed; }

      void VmxDnDVersionChanged(struct RpcIn *rpcIn,
                                uint32 version);

      void SetDnDAllowed(bool isDnDAllowed)
         { mDnDAllowed = isDnDAllowed; }
      void UngrabTimeout(void);
      void ResetDnD(void);
      void HGDragStartDone(void);
      void OnUpdateMouse(int32 x, int32 y);
      void UpdateDetWnd(bool show, int32 x, int32 y);
      void SetHideDetWndTimer(Event *e) { mHideDetWndTimer = e; }

   private:
      /* Callbacks from rpc. */
      void OnGHUpdateUnityDetWnd(bool bShow, uint32 unityWndId);
      void OnGHQueryPendingDrag(int x, int y);
      void OnGHCancel(void);
      void OnHGDragEnter(const CPClipboard *clip);
      void OnHGDragStart(void);
      void OnHGCancel(void);
      void OnHGDrop(void);
      void OnHGFileCopyDone(bool cancel,
                            std::vector<uint8> stagingDir);

      std::string SetupDestDir(const std::string &destDir);

      DnDRpc *mRpc;
      uint32 mVmxDnDVersion;
      bool mDnDAllowed;
      std::string mStagingDir;
      Event *mUngrabTimeout;
      Event *mHideDetWndTimer;
      DblLnkLst_Links *mEventQueue;
};

#endif // DND_HH

