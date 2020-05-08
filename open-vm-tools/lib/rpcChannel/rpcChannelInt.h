/*********************************************************
 * Copyright (C) 2008-2016,2018-2020 VMware, Inc. All rights reserved.
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

#ifndef _RPCCHANNELINT_H_
#define _RPCCHANNELINT_H_

/**
 * @file rpcChannelInt.h
 *
 *    Internal definitions for the RPC channel library.
 */

#include "vmware/tools/guestrpc.h"

#if defined(USE_RPCI_ONLY)

#undef NEED_RPCIN

#else

#define NEED_RPCIN

/** Max amount of time (in .01s) that the RpcIn loop will sleep for. */
#define RPCIN_MAX_DELAY    10

struct RpcIn;
#endif

/*
 * Flags associated with the RPC Channel
 */

/* Channel will be usaed for a single RPC */
#define RPCCHANNEL_FLAGS_SEND_ONE     0x1
/* VMX should close channel after sending reply */
#define RPCCHANNEL_FLAGS_FAST_CLOSE   0x2

/** a list of interface functions for a channel implementation */
typedef struct _RpcChannelFuncs{
   gboolean (*start)(RpcChannel *);
   void (*stop)(RpcChannel *);
   gboolean (*send)(RpcChannel *, char const *data, size_t dataLen,
                    Bool *rpcStatus, char **result, size_t *resultLen);
   void (*setup)(RpcChannel *chan, GMainContext *mainCtx,
                 const char *appName, gpointer appCtx);
   void (*shutdown)(RpcChannel *);
   RpcChannelType (*getType)(RpcChannel *chan);
   void (*destroy)(RpcChannel *);
} RpcChannelFuncs;

/**
 * Defines the interface between the application and the RPC channel.
 *
 * XXX- outLock is badly named and is used to protect the in and out
 * channels, their state (inStarted/outStarted) and _private data.
 */
struct _RpcChannel {
   const RpcChannelFuncs *funcs;
   gpointer _private;
#if defined(NEED_RPCIN)
   GMainContext *mainCtx;
   const char *appName;
   gpointer appCtx;
   struct RpcIn *in;
   gboolean inStarted;
#endif
   GMutex outLock;
   gboolean outStarted;
   int vsockChannelFlags;
   /*
    * Only vsocket channel is mutable as it can fallback to Backdoor.
    * If a channel is created as Backdoor, it will not be mutable.
    */
   gboolean isMutable;
   /*
    * Track the last vsocket connection failure timestamp.
    * Avoid using vsocket until a channel reset/restart
    * occurs, the service restarts or retry delay has passed
    * since the last failure.
    */
   uint64 vsockFailureTS;
   /*
    * Amount of time to delay next vsocket retry attempt.
    * It varies between RPCCHANNEL_VSOCKET_RETRY_MIN_DELAY
    * and RPCCHANNEL_VSOCKET_RETRY_MAX_DELAY.
    */
   uint32 vsockRetryDelay;
};

void BackdoorChannel_Fallback(RpcChannel *chan);
void VSockChannel_Restore(RpcChannel *chan, int flags);

#endif /* _RPCCHANNELINT_H_ */
