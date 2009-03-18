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
 * dndTransportGuestRpc.hh --
 *
 *     GuestRpc implementation of the dndTransport interface.
 */


#ifndef DND_TRANSPORT_GUEST_RPC_HH
#define DND_TRANSPORT_GUEST_RPC_HH

#include <sigc++/trackable.h>
#include "dndTransport.hh"

extern "C" {
   #include "dnd.h"
}

class DnDTransportGuestRpc
   : public DnDTransport,
     public sigc::trackable
{
public:
   DnDTransportGuestRpc(struct RpcIn *rpcIn,
                        const char *rpcCmd);
   virtual ~DnDTransportGuestRpc(void);

   virtual bool SendMsg(uint8 *msg,
                        size_t length);
   void RecvMsg(DnDTransportPacketHeader *packet,
                size_t packetSize);

private:
   char *mRpcCmd;
   struct RpcIn *mRpcIn;

   DnDTransportBuffer mSendBuf;
   DnDTransportBuffer mRecvBuf;
   uint32 mSeqNum;

   bool SendPacket(uint8 *packet,
                   size_t packetSize);
};

#endif // DND_TRANSPORT_GUEST_RPC_HH
