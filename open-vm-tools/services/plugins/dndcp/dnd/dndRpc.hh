/*********************************************************
 * Copyright (C) 2007-2017,2022 VMware, Inc. All rights reserved.
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
 * @dndRpc.hh --
 *
 * Rpc layer object for DnD.
 */

#ifndef DND_RPC_HH
#define DND_RPC_HH

#include <sigc++/sigc++.h>
#include <sigc++2to3.h>
#include "dndCPLibExport.hh"
#include "rpcBase.h"

#include "dnd.h"

class LIB_EXPORT DnDRpc
   : public RpcBase
{
public:
   virtual ~DnDRpc(void) {};

   /* sigc signals for DnD source callback. */
   sigc::signal<void, uint32, const CPClipboard *> srcDragBeginChanged;
   sigc::signal<void, uint32> srcCancelChanged;
   sigc::signal<void, uint32, uint32, uint32> srcDropChanged;

   /* sigc signals for DnD destination callback. */
   sigc::signal<void, uint32, int32> destDragEnterReplyChanged;
   sigc::signal<void, uint32> destPrivDragEnterChanged;
   sigc::signal<void, uint32, int32, int32> destPrivDragLeaveChanged;
   sigc::signal<void, uint32, int32, int32> destPrivDropChanged;
   sigc::signal<void, uint32, int32, int32> destDropChanged;
   sigc::signal<void, uint32> destCancelChanged;

   sigc::signal<void, uint32, int32, int32> moveMouseChanged;
   sigc::signal<void, uint32, uint32> updateFeedbackChanged;
   sigc::signal<void, uint32, int32, int32> queryExitingChanged;
   sigc::signal<void, uint32> dragNotPendingChanged;
   sigc::signal<void, uint32, bool, uint32> updateUnityDetWndChanged;
   sigc::signal<void, uint32, const uint8 *, uint32> requestFileChanged;
   sigc::signal<void, uint32, bool, const uint8 *, uint32> getFilesDoneChanged;

   /* sigc signal for responding to ping reply */
   sigc::signal<void, uint32> pingReplyChanged;

   /* sigc signal for rpc cmd reply received. */
   sigc::signal<void, uint32, uint32> cmdReplyChanged;

   /* DnD source. */
   virtual bool SrcDragBeginDone(uint32 sessionId) = 0;
   virtual bool SrcDrop(uint32 sessionId, int32 x, int32 y) = 0;
   virtual bool SrcDropDone(uint32 sessionId,
                            const uint8 *stagingDirCP,
                            uint32 sz) = 0;

   virtual bool SrcPrivDragEnter(uint32 sessionId) = 0;
   virtual bool SrcPrivDragLeave(uint32 sessionId, int32 x, int32 y) = 0;
   virtual bool SrcPrivDrop(uint32 sessionId, int32 x, int32 y) = 0;
   virtual bool SrcCancel(uint32 sessionId) = 0;

   /* DnD destination. */
   virtual bool DestDragEnter(uint32 sessionId,
                              const CPClipboard *clip) = 0;
   virtual bool DestSendClip(uint32 sessionId,
                             const CPClipboard *clip) = 0;
   virtual bool DestDragLeave(uint32 sessionId, int32 x, int32 y) = 0;
   virtual bool DestDrop(uint32 sessionId, int32 x, int32 y) = 0;
   virtual bool DestCancel(uint32 sessionId) = 0;

   /* Common. */
   virtual void Init(void) = 0;
   virtual void SendPing(uint32 caps) = 0;
   virtual bool UpdateFeedback(uint32 sessionId, DND_DROPEFFECT feedback) = 0;
   virtual bool MoveMouse(uint32 sessionId,
                          int32 x,
                          int32 y) = 0;
   virtual bool QueryExiting(uint32 sessionId, int32 x, int32 y) = 0;
   virtual bool DragNotPending(uint32 sessionId) = 0;
   virtual bool UpdateUnityDetWnd(uint32 sessionId,
                                  bool show,
                                  uint32 unityWndId) = 0;
   virtual bool RequestFiles(uint32 sessionId) = 0;
   virtual bool SendFilesDone(uint32 sessionId,
                              bool success,
                              const uint8 *stagingDirCP,
                              uint32 sz) = 0;
   virtual bool GetFilesDone(uint32 sessionId,
                             bool success) = 0;
};

#endif // DND_RPC_HH
