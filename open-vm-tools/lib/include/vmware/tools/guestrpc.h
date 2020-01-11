/*********************************************************
 * Copyright (C) 2008,2014-2016,2018-2019 VMware, Inc. All rights reserved.
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

#ifndef _VMWARE_TOOLS_GUESTRPC_H_
#define _VMWARE_TOOLS_GUESTRPC_H_

/**
 * @file guestrpc.h
 *
 *    Defines the interface between applications and the underlying GuestRPC
 *    channel. The goal is to have an abstraction so applications can run over
 *    the backdoor, vSockets or TCP/IP sockets by just picking up the
 *    desired channel at runtime, without the need to modify the code.
 *
 *    For this reason, the behavior of all channels is modeled after the RpcIn
 *    channel currently used in Tools, so the socket-based channels won't
 *    provide much better functionality than what the backdoor provides (aside
 *    from being interrupt-based rather than poll-based).
 *
 * @addtogroup vmtools_guestrpc
 * @{
 */

#include <glib.h>
#include "vmware/tools/utils.h"

G_BEGIN_DECLS

/** Aliases. */
#define RPCIN_SETRETVALS  RpcChannel_SetRetVals
#define RPCIN_SETRETVALSF RpcChannel_SetRetValsF

typedef struct _RpcChannel RpcChannel;

/** Data structure passed to RPC callbacks. */
typedef struct RpcInData {
   /** RPC name. */
   const char *name;
   /**
    * RPC arguments. Either the raw argument data, or de-serialized XDR data
    * in case @a xdrIn was provided in the registration data.
    */
   const char *args;
   /** Size of raw argument data, in bytes. */
   size_t argsSize;
   /**
    * Data to be returned to the caller, or pointer to XDR structure if
    * @a xdrOut was provided in the registration data.
    */
   char *result;
   /** Length in bytes of raw data being returned (ignored for XDR structures). */
   size_t resultLen;
   /**
    * Whether the RPC library should free the contents of the @a result
    * field (using vm_free()).
    */
   gboolean freeResult;
   /** Application context. */
   void *appCtx;
   /** Client data specified in the registration data. */
   void *clientData;
} RpcInData;

typedef enum RpcChannelType {
   RPCCHANNEL_TYPE_INACTIVE,
   RPCCHANNEL_TYPE_BKDOOR,
   RPCCHANNEL_TYPE_PRIV_VSOCK,
   RPCCHANNEL_TYPE_UNPRIV_VSOCK
} RpcChannelType;

/**
 * Type for RpcIn callbacks. The callback function is responsible for
 * allocating memory for the result string.
 */
typedef gboolean (*RpcIn_Callback)(RpcInData *data);


/** Defines the registration data for a GuestRPC application. */
typedef struct RpcChannelCallback {
   /** String identifying the RPC message. */
   const char       *name;
   /** Function to call when data arrives. */
   RpcIn_Callback    callback;
   /** Data to provide to callback function. */
   gpointer          clientData;
   /** If not NULL, the input data will be deserialized using this function. */
   gpointer          xdrIn;
   /**
    * If not NULL, the output data will be serialized using this function. The
    * output data should be stored in the @a result field of the RpcInData
    * structure, and should have been allocated with glib's @a g_malloc() if
    * @a freeResult is TRUE.
    */
   gpointer          xdrOut;
   /**
    * If xdrIn is not NULL, this should be the amount of memory to allocate
    * for deserializing the input data.
    */
   size_t            xdrInSize;
} RpcChannelCallback;

/**
 * Signature for the callback function called after a channel reset.
 *
 * @param[in]  chan     The RPC channel.
 * @param[in]  success  Whether reset was successful.
 * @param[in]  data     Client data.
 */
typedef void (*RpcChannelResetCb)(RpcChannel *chan,
                                  gboolean success,
                                  gpointer data);

/**
 * Signature for the application callback function when unable to establish
 * an RpcChannel connection.
 *
 * @param[in]  _state     Client data.
 */
typedef void (*RpcChannelFailureCb)(gpointer _state);


gboolean
RpcChannel_Start(RpcChannel *chan);

void
RpcChannel_Stop(RpcChannel *chan);

RpcChannelType
RpcChannel_GetType(RpcChannel *chan);

gboolean
RpcChannel_Send(RpcChannel *chan,
                char const *data,
                size_t dataLen,
                char **result,
                size_t *resultLen);

void
RpcChannel_Free(void *ptr);

#if !defined(USE_RPCI_ONLY)
gboolean
RpcChannel_BuildXdrCommand(const char *cmd,
                           void *xdrProc,
                           void *xdrData,
                           char **result,
                           size_t *resultLen);
gboolean
RpcChannel_Dispatch(RpcInData *data);

void
RpcChannel_Setup(RpcChannel *chan,
                 const gchar *appName,
                 GMainContext *mainCtx,
                 gpointer appCtx,
                 RpcChannelResetCb resetCb,
                 gpointer resetData,
                 RpcChannelFailureCb failureCb,
                 guint maxFailures);

void
RpcChannel_RegisterCallback(RpcChannel *chan,
                            RpcChannelCallback *rpc);

void
RpcChannel_UnregisterCallback(RpcChannel *chan,
                              RpcChannelCallback *rpc);
#endif

RpcChannel *
RpcChannel_Create(void);

void
RpcChannel_Destroy(RpcChannel *chan);

gboolean
RpcChannel_SetRetVals(RpcInData *data,
                      char const *result,
                      gboolean retVal);

gboolean
RpcChannel_SetRetValsF(RpcInData *data,
                       char *result,
                       gboolean retVal);

gboolean
RpcChannel_SendOneRaw(const char *data,
                      size_t dataLen,
                      char **result,
                      size_t *resultLen);

#if defined(__linux__) || defined(_WIN32)
gboolean
RpcChannel_SendOneRawPriv(const char *data,
                          size_t dataLen,
                          char **result,
                          size_t *resultLen);
#endif

gboolean
RpcChannel_SendOne(char **reply,
                   size_t *repLen,
                   const char *reqFmt,
                   ...);

#if defined(__linux__) || defined(_WIN32)
gboolean
RpcChannel_SendOnePriv(char **reply,
                       size_t *repLen,
                       const char *reqFmt,
                       ...);
#endif

RpcChannel *
RpcChannel_New(void);

#if defined(__linux__) || defined(_WIN32)
RpcChannel *
VSockChannel_New(void);
#endif

void
RpcChannel_SetBackdoorOnly(void);

RpcChannel *
BackdoorChannel_New(void);

G_END_DECLS

/** @} */

#endif

