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
 * @copyPasteRpc.hh --
 *
 * Rpc layer object for CopyPaste.
 */

#ifndef COPY_PASTE_RPC_HH
#define COPY_PASTE_RPC_HH

#include <sigc++/sigc++.h>
#include <sigc++2to3.h>
#include "dndCPLibExport.hh"
#include "rpcBase.h"

#include "dnd.h"

class LIB_EXPORT CopyPasteRpc
   : public RpcBase
{
public:
   virtual ~CopyPasteRpc(void) {};

   /* sigc signals for CopyPaste source callback. */
   sigc::signal<void, uint32, bool, const CPClipboard*> srcRecvClipChanged;
   sigc::signal<void, uint32, const uint8 *, uint32> requestFilesChanged;
   sigc::signal<void, uint32, bool, const uint8 *, uint32> getFilesDoneChanged;

   /* sigc signal for CopyPaste destination callback. */
   sigc::signal<void, uint32, bool> destRequestClipChanged;

   /* sigc signal for ping reply callback. */
   sigc::signal<void, uint32> pingReplyChanged;

   /* sigc signal for rpc command reply received. */
   sigc::signal<void, uint32, uint32> cmdReplyChanged;

   virtual void Init(void) = 0;
   virtual void SendPing(uint32 caps) = 0;

   /* CopyPaste Rpc functions. */
   virtual bool SrcRequestClip(uint32 sessionId,
                               bool isActive) = 0;
   virtual bool DestSendClip(uint32 sessionId,
                             bool isActive,
                             const CPClipboard* clip) = 0;
   virtual bool RequestFiles(uint32 sessionId,
                             const uint8 *stagingDirCP,
                             uint32 sz) = 0;
   virtual bool SendFilesDone(uint32 sessionId,
                              bool success,
                              const uint8 *stagingDirCP,
                              uint32 sz) = 0;
   virtual bool GetFilesDone(uint32 sessionId,
                             bool success) = 0;
};

#endif // COPY_PASTE_RPC_HH
