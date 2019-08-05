/*********************************************************
 * Copyright (C) 2008-2016,2018-2019 VMware, Inc. All rights reserved.
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
   void (*onStartErr)(RpcChannel *);
   gboolean (*stopRpcOut)(RpcChannel *);
} RpcChannelFuncs;

/**
 * Defines the interface between the application and the RPC channel.
 *
 * XXX- outLock is badly named and is used to protect the in and out
 * channels, their state (inStarted/outStarted) and _private data.
 */
struct _RpcChannel {
   const RpcChannelFuncs     *funcs;
   gpointer                  _private;
#if defined(NEED_RPCIN)
   GMainContext              *mainCtx;
   const char                *appName;
   gpointer                  appCtx;
#endif
   GMutex                    outLock;
#if defined(NEED_RPCIN)
   struct RpcIn              *in;
   gboolean                  inStarted;
#endif
   gboolean                  outStarted;
};

gboolean
BackdoorChannel_Fallback(RpcChannel *chan);

#endif /* _RPCCHANNELINT_H_ */

