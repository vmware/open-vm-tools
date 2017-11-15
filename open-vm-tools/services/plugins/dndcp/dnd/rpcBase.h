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
 * rpcBase.h --
 *
 *     Rpc layer object for DnD/CP.
 */

#ifndef RPC_BASE_H
#define RPC_BASE_H

#ifndef LIB_EXPORT
#define LIB_EXPORT
#endif

#include "vm_basic_types.h"

typedef struct RpcParams {
   uint32 addrId;          /* Destination address id. */
   uint32 cmd;             /* DnD/CP message command. */
   uint32 sessionId;       /* DnD/CP session ID. */
   uint32 status;          /* Status for last operation. */
   union {
      struct {
         uint32 major;
         uint32 minor;
         uint32 capability;
      } version;

      struct {
         uint32 x;
         uint32 y;
      } mouseInfo;

      struct {
         uint32 cmd;
      } replyToCmd;

      struct {
         uint32 cmd;
         uint32 binarySize;
         uint32 payloadOffset;
      } requestNextCmd;

      struct {
         uint32 feedback;
      } feedback;

      struct {
         uint32 major;
         uint32 minor;
         uint32 capability;
         uint32 x;
         uint32 y;
      } queryExiting;

      struct {
         uint32 major;
         uint32 minor;
         uint32 capability;
         uint32 show;
         uint32 unityWndId;
      } updateUnityDetWnd;

      struct {
         uint32 major;
         uint32 minor;
         uint32 capability;
         uint32 isActive;
      } cpInfo;

      struct {
         uint32 param1;
         uint32 param2;
         uint32 param3;
         uint32 param4;
         uint32 param5;
         uint32 param6;
      } genericParams;
   } optional;
} RpcParams;


class LIB_EXPORT RpcBase {
public:
   virtual ~RpcBase(void) {};
   virtual void OnRecvPacket(uint32 srcId,
                             const uint8 *packet,
                             size_t packetSize) = 0;
   virtual bool SendPacket(uint32 destId,
                           const uint8 *packet,
                           size_t length) = 0;
   virtual void HandleMsg(RpcParams *params,
                          const uint8 *binary,
                          uint32 binarySize) = 0;
};
#endif // RPC_BASE_H
