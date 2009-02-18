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
 * dndRpcV3.hh --
 *
 *     Rpc layer object for DnD version 3.
 */

#ifndef DND_RPC_V3_HH
#define DND_RPC_V3_HH

#include <sigc++/trackable.h>
#include "dndRpc.hh"
#include "dndTransport.hh"

extern "C" {
   #include "dnd.h"
   #include "dndMsg.h"
}

class DnDRpcV3
   : public DnDRpc,
     public sigc::trackable
{
   public:
      DnDRpcV3(struct RpcIn *rpcIn);
      virtual ~DnDRpcV3(void);

      /* DnD functions. */
      /* GH DnD. */
      virtual bool GHDragEnter(const CPClipboard *clip);
      virtual bool GHUngrabTimeout(void);

      /* HG DnD. */
      virtual bool HGDragEnterDone(int32 x, int32 y);
      virtual bool HGDragStartDone(void);
      virtual bool HGUpdateFeedback(DND_DROPEFFECT feedback);
      virtual bool HGDropDone(const char *stagingDirCP, size_t sz);

   private:
      void OnRecvMsg(const uint8 *data, size_t dataSize);
      bool SendSingleCmd(DnDCommand cmd);
      bool SendCmdWithClip(DnDCommand cmd, const CPClipboard *clip);

      DnDTransport *mTransport;
};

#endif // DND_RPC_V3_HH
