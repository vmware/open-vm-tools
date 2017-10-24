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

/*
 * dndCPTransport.h --
 *
 *      DnDCPTransport provides a data transportation interface for both dnd and copyPaste.
 */

#ifndef DND_CP_TRANSPORT_H
#define DND_CP_TRANSPORT_H

#ifndef LIB_EXPORT
#define LIB_EXPORT
#endif

#include "vm_basic_types.h"

/* Some definitions for addressId. */
#define MAX_NUM_OF_CONNECTIONS  50
#define BROADCAST_CONNECTION_ID 10000
#define DEFAULT_CONNECTION_ID   10001
#define INVALID_CONNECTION_ID   99999

typedef enum TransportInterfaceType {
   TRANSPORT_HOST_CONTROLLER_DND = 0,
   TRANSPORT_HOST_CONTROLLER_CP,
   TRANSPORT_HOST_CONTROLLER_FT,
   TRANSPORT_GUEST_CONTROLLER_DND,
   TRANSPORT_GUEST_CONTROLLER_CP,
   TRANSPORT_GUEST_CONTROLLER_FT,
   TRANSPORT_INTERFACE_MAX,
} TransportInterfaceType;

class RpcBase;
#if defined(SWIG)
class DnDCPTransport
#else
class LIB_EXPORT DnDCPTransport
#endif
{
public:
   virtual ~DnDCPTransport() {};

   virtual void StartLoop() {};
   virtual void EndLoop() {};
   virtual void IterateLoop() {};
   virtual bool RegisterRpc(RpcBase *rpc,
                            TransportInterfaceType type) = 0;
   virtual bool UnregisterRpc(TransportInterfaceType type) = 0;

   virtual bool SendPacket(uint32 destId,
                           TransportInterfaceType type,
                           const uint8 *msg,
                           size_t length) = 0;
};

#endif // DND_CP_TRANSPORT_H
