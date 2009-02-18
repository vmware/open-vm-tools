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
 * copyPasteRpcV3.hh --
 *
 *     Rpc layer object for CopyPaste.
 */

#ifndef COPY_PASTE_RPC_V3_HH
#define COPY_PASTE_RPC_V3_HH

#include <sigc++/trackable.h>
#include "copyPasteRpc.hh"
#include "dndTransport.hh"

class CopyPasteRpcV3
   : public CopyPasteRpc,
     public sigc::trackable
{
   public:
      CopyPasteRpcV3(struct RpcIn *rpcIn);
      virtual ~CopyPasteRpcV3(void);

      /* CopyPaste Rpc functions. */
      virtual bool GHGetClipboardDone(const CPClipboard* clip);
      virtual bool HGStartFileCopy(const char *stagingDirCP, size_t sz);

   private:
      void OnRecvMsg(const uint8 *data, size_t dataSize);
      DnDTransport* mTransport;
};

#endif // COPY_PASTE_RPC_V3_HH
