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
 * dndBase.h --
 *
 *     Base object for DnD. This is the common interface between UI and 
 *     cui DnD protocol layer. Both host side and guest side, and also
 *     different platforms, share this same interface.
 */

#ifndef DND_BASE_H
#define DND_BASE_H

#include <sigc++/connection.h>
#include <vector>

#ifdef VMX86_TOOLS
   /*
    * LIB_EXPORT definition is not needed on guest. 
    */
#   define LIB_EXPORT
#else
   /*
    * The interface class on host side for Windows is in dll which needs
    * LIB_EXPORT definition. 
    */
#   include "libExport.hh"
#endif

extern "C" {
#include "dnd.h"
#include "unityCommon.h"
}

class LIB_EXPORT DnDBase
{
   public:
      virtual ~DnDBase(void) {};

      /* sigc signals for local UI callbacks. */
      /* Local UI as DnD source. */
      sigc::signal<void, const CPClipboard*> dragExitChanged;
      sigc::signal<void> dndUngrabChanged; /* X11's notion of ungrab. */
      sigc::signal<void, bool, std::vector<uint8> > fileCopyDoneChanged;
      sigc::signal<void> sourceDropChanged;
      sigc::signal<void> sourceCancelChanged;
      sigc::signal<void, const CPClipboard*, std::string> dragStartChanged;

      /* Local UI as DnD target. */
      sigc::signal<void, DND_DROPEFFECT> updateFeedbackChanged;
      sigc::signal<void, bool, int, int> updateDetWndChanged;
      sigc::signal<void, bool, uint32, bool> updateUnityDetWndChanged;
      sigc::signal<void, int32, int32> targetPrivateDropChanged;

      sigc::signal<void> reset;
      sigc::signal<void> disable;
      sigc::signal<void> enable;
      sigc::signal<void, int, int> updateMouseChanged;
      sigc::signal<void> moveDetWndToMousePos;

      /* Guest cancel signals. */
      sigc::signal<void> ghCancel;

      /* cui DnD protocol layer API exposed to UI (all platforms). */

      /* Local UI as DnD source. */
      virtual void UpdateUnityDetWnd(bool bShow, uint32 unityWndId) {};
      virtual void DragLeave(int32 x, int32 y) = 0;
      virtual void SourceCancel(void) = 0;
      virtual void SourceDrop(DND_DROPEFFECT feedback) = 0;

      /* Local UI as DnD target. */
      virtual void DragEnter(const CPClipboard* clip) = 0;
      /* Host only. */
      virtual void SetMouse(int32 x, int32 y, bool down = true) = 0;
      /* Guest only. */
      virtual void SetFeedback(DND_DROPEFFECT feedback) = 0;
      virtual void TargetCancel(void) = 0;
      virtual void TargetDrop(const CPClipboard *clip,
                              int32 x,
                              int32 y) = 0;

      const CPClipboard* GetClipboard(void) { return &mClipboard; }

      virtual bool IsDnDAllowed(void) = 0;

   protected:
      /*
       * For details about state machine definition, please refer to
       * https://wiki.eng.vmware.com/ToolsDnDV3FlowChart
       */
      enum DND_STATE {
         DNDSTATE_INVALID = 0,
         DNDSTATE_READY,
         /* H->G */
         DNDSTATE_ENTERING,
         DNDSTATE_STARTING,
         DNDSTATE_DRAGGING_INSIDE,
         /* G->H */
         DNDSTATE_QUERY_EXITING,
         DNDSTATE_UNGRABING, // ask mks to ungrab, v2 only
         DNDSTATE_DRAGGING_OUTSIDE,
         DNDSTATE_DRAGGING_PRIVATE,
      };

      DND_STATE mState;
      DND_DROPEFFECT mFeedback;
      CPClipboard mClipboard;
};

#endif // DND_BASE_H

