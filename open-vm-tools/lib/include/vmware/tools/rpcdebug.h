/*********************************************************
 * Copyright (C) 2008-2016 VMware, Inc. All rights reserved.
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

#ifndef _VMWARE_TOOLS_RPCDEBUG_H_
#define _VMWARE_TOOLS_RPCDEBUG_H_

/**
 * @file rpcdebug.h
 *
 * Defines the public API for the "GuestRPC Debug Channel" implementation, and
 * the interface between the debug library and debug plugins.
 *
 * @addtogroup vmtools_debug
 * @{
 */

#include "vmware/tools/plugin.h"


/**
 * Shorthand macro to both call CU_ASSERT() and return from the
 * function if the assertion fails. Note that this file doesn't
 * include CUnit.h, so you'll need to include that header to use
 * this macro.
 */
#define RPCDEBUG_ASSERT(test, retval) do {   \
   CU_ASSERT(test);                          \
   g_return_val_if_fail(test, retval);       \
} while (0)


struct RpcDebugPlugin;

/**
 * Signature for the plugin's "receive" function, to validate the data
 * applications send using RpcChannel_Send.
 */
typedef gboolean (*RpcDebugRecvFn)(char *data,
                                   size_t dataLen,
                                   char **result,
                                   size_t *resultLen);

/** Defines a mapping between a message and a "receive" function. */
typedef struct RpcDebugRecvMapping {
   gchar            *name;
   RpcDebugRecvFn    recvFn;
   /**
    * If not NULL, should be a xdrproc_t function for deserializing the data
    * in the received message.
    */
   gpointer          xdrProc;
   /** If xdrProc is provided, should be the size of the structure to allocate. */
   size_t            xdrSize;
} RpcDebugRecvMapping;


/**
 * Signature for validation functions. Validation functions are called after
 * an application has processed an "incoming" RPC, so that the plugin can
 * validate the response.
 */
typedef gboolean (*RpcDebugValidateFn)(RpcInData *data,
                                       gboolean ret);

/** Defines a mapping between a message and a "validate" function. */
typedef struct RpcDebugMsgMapping {
   gchar                  *message;
   size_t                  messageLen;
   RpcDebugValidateFn      validateFn;
   gboolean                freeMsg;
} RpcDebugMsgMapping;

/** Defines a (NULL-terminated) list of message / validator mappings. */
typedef struct RpcDebugMsgList {
   RpcDebugMsgMapping     *mappings;
   size_t                  index;
} RpcDebugMsgList;


/**
 * Signature for the plugin's "send" function, which provides the data
 * to be sent when the service tries to read from the RPC Channel.
 *
 * The function should return FALSE if the service should finish the
 * test (any data provided when this function returns FALSE is ignored).
 */
typedef gboolean (*RpcDebugSendFn)(RpcDebugMsgMapping *rpcdata);

/** Signature for the plugin's "shutdown" function. */
typedef void (*RpcDebugShutdownFn)(ToolsAppCtx *ctx,
                                   struct RpcDebugPlugin *plugin);

/**
 * Registration data for debug plugins, should be returned by the plugin's
 * entry point function.
 */
typedef struct RpcDebugPlugin {
   /** Maps "incoming" RPCs to specific receive functions. NULL-terminated. */
   RpcDebugRecvMapping *recvFns;
   /**
    * Default receive function for when no mapping matches the incoming command.
    * May be NULL.
    */
   RpcDebugRecvFn       dfltRecvFn;
   /** Send function. */
   RpcDebugSendFn       sendFn;
   /** Shutdown function. */
   RpcDebugShutdownFn   shutdownFn;
   /** Plugin data that debug plugins can also export. */
   ToolsPluginData     *plugin;
} RpcDebugPlugin;


/**
 * Signature for the plugin's entry point. The function works in a similar
 * way to the "ToolsOnLoad" function for regular plugins.
 */
typedef RpcDebugPlugin *(*RpcDebugOnLoadFn)(ToolsAppCtx *ctx);

struct RpcDebugLibData;

/**
 * Describes the external interface of the library. An instance of this struct
 * is returned by RpcDebug_Initialize() and can be used by applications using
 * the library to use the debugging functionality.
 */
typedef struct RpcDebugLibData {
   RpcChannel *    (*newDebugChannel)  (ToolsAppCtx *,
                                        struct RpcDebugLibData *);
   int             (*run)              (ToolsAppCtx *,
                                        gpointer runMainLoop,
                                        gpointer runData,
                                        struct RpcDebugLibData *);
   RpcDebugPlugin   *debugPlugin;
} RpcDebugLibData;

/** Signature of the library's initialization function. */
typedef RpcDebugLibData *(* RpcDebugInitializeFn)(ToolsAppCtx *, gchar *);


G_BEGIN_DECLS

void
RpcDebug_DecRef(ToolsAppCtx *ctx);

void
RpcDebug_IncRef(void);

RpcDebugLibData *
RpcDebug_Initialize(ToolsAppCtx *ctx,
                    gchar *dbgPlugin);

gboolean
RpcDebug_SendNext(RpcDebugMsgMapping *rpcdata,
                  RpcDebugMsgList *list);

void
RpcDebug_SetResult(const char *str,
                   char **res,
                   size_t *len);

G_END_DECLS

/** @} */

#endif /* _VMWARE_TOOLS_RPCDEBUG_H_ */

