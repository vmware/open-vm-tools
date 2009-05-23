/*********************************************************
 * Copyright (C) 2009 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 *********************************************************/

/*
 * notify.h --
 *
 *      Notify functions for Linux VSocket module.
 */

#ifndef __NOTIFY_H__
#define __NOTIFY_H__

#include "driver-config.h"
#include "vsockCommon.h"
#include "vsockPacket.h"

/* Comment this out to compare with old protocol. */
#define VSOCK_OPTIMIZATION_WAITING_NOTIFY 1
#if defined(VSOCK_OPTIMIZATION_WAITING_NOTIFY)
/* Comment this out to remove flow control for "new" protocol */
#  define VSOCK_OPTIMIZATION_FLOW_CONTROL 1
#endif

#define NOTIFYCALLRET(vsk, rv, mod_fn, args...)                     \
do {                                                                \
  if (vsk->notifyOps->mod_fn != NULL) {                             \
        rv = (vsk->notifyOps->mod_fn)(args);                        \
  } else {                                                          \
        rv = 0;                                                     \
  }                                                                 \
} while (0)

#define NOTIFYCALL(vsk, mod_fn, args...)                            \
do {                                                                \
  if (vsk->notifyOps->mod_fn != NULL) {                             \
        (vsk->notifyOps->mod_fn)(args);                             \
  }                                                                 \
} while (0)


typedef struct VSockVmciNotify {
   uint64 writeNotifyWindow;
   uint64 writeNotifyMinWindow;
   Bool peerWaitingRead;
   Bool peerWaitingWrite;
   Bool peerWaitingWriteDetected;
   Bool sentWaitingRead;
   Bool sentWaitingWrite;
   VSockWaitingInfo peerWaitingReadInfo;
   VSockWaitingInfo peerWaitingWriteInfo;
   uint64 produceQGeneration;
   uint64 consumeQGeneration;
} VSockVmciNotify;

typedef struct VSockVmciRecvNotifyData {
#if defined(VSOCK_OPTIMIZATION_WAITING_NOTIFY)
   uint64 consumeHead;
   uint64 produceTail;
#ifdef VSOCK_OPTIMIZATION_FLOW_CONTROL
   Bool notifyOnBlock;
#endif
#endif
} VSockVmciRecvNotifyData;

typedef struct VSockVmciSendNotifyData {
#if defined(VSOCK_OPTIMIZATION_WAITING_NOTIFY)
   uint64 consumeHead;
   uint64 produceTail;
#endif
} VSockVmciSendNotifyData;

/* Socket notification callbacks. */
typedef struct VSockVmciNotifyOps {
   void  (*socketInit)(struct sock *sk);
   void  (*socketDestruct)(struct sock *sk);
   int32 (*pollIn)(struct sock *sk, int target, Bool *dataReadyNow);
   int32 (*pollOut)(struct sock *sk, int target, Bool *spaceAvailNow);
   void  (*handleNotifyPkt)(struct sock *sk, VSockPacket *pkt,
                            Bool bottomHalf, struct sockaddr_vm *dst,
                            struct sockaddr_vm *src, Bool *pktProcessed);
   int32 (*recvInit)(struct sock *sk, int target,
                     VSockVmciRecvNotifyData *data);
   int32 (*recvPreBlock)(struct sock *sk, int target,
                         VSockVmciRecvNotifyData *data);
   int32 (*recvPreDequeue)(struct sock *sk, int target,
                           VSockVmciRecvNotifyData *data);
   int32 (*recvPostDequeue)(struct sock *sk, int target, ssize_t copied,
                            Bool dataRead, VSockVmciRecvNotifyData *data);
   int32 (*sendInit)(struct sock *sk,
                     VSockVmciSendNotifyData *data);
   int32 (*sendPreBlock)(struct sock *sk,
                         VSockVmciSendNotifyData *data);
   int32 (*sendPreEnqueue)(struct sock *sk,
                           VSockVmciSendNotifyData *data);
   int32 (*sendPostEnqueue)(struct sock *sk,
                            ssize_t written,
                            VSockVmciSendNotifyData *data);
} VSockVmciNotifyOps;

extern VSockVmciNotifyOps vSockVmciNotifyPktOps;

#endif /* __NOTIFY_H__ */
