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
 * rpcin.h --
 *
 *    Remote Procedure Call between VMware and guest applications
 *    C declarations
 */


#ifndef __RPCIN_H__
#   define __RPCIN_H__

#if defined(VMTOOLS_USE_GLIB)
#  include <glib.h>
#else
#  include "dbllnklst.h"
#endif

/* Helper macro for porting old callbacks that currently use RpcIn_SetRetVals. */
#define RPCIN_SETRETVALS(data, val, retVal)                                \
   RpcIn_SetRetVals((char const **) &(data)->result, &(data)->resultLen,   \
                    (val), (retVal))

typedef void RpcIn_ErrorFunc(void *clientData, char const *status);

typedef struct RpcIn RpcIn;

/* Data passed to new-style RpcIn callbacks. */
typedef struct RpcInData {
   /* Data from the host's RPC request. */
   const char *name;
   const char *args;
   size_t argsSize;
   /* Data to be returned to the host. */
   char *result;
   size_t resultLen;
   Bool freeResult;
   /* Client data. */
   void *appCtx;
   void *clientData;
} RpcInData;


/*
 * Type for RpcIn callbacks. The callback function is responsible for
 * allocating memory for the result string.
 */
typedef Bool (*RpcIn_Callback)(RpcInData *data);


#if defined(VMTOOLS_USE_GLIB)

RpcIn *RpcIn_Construct(GMainLoop *mainLoop,
                       RpcIn_Callback dispatch,
                       gpointer clientData);

Bool RpcIn_start(RpcIn *in, unsigned int delay,
                 RpcIn_ErrorFunc *errorFunc, void *errorData);

#else

/*
 * Type for old RpcIn callbacks. Don't use this anymore - this is here
 * for backwards compatibility.
 */
typedef Bool
(*RpcIn_CallbackOld)(char const **result,     // OUT
                     size_t *resultLen,       // OUT
                     const char *name,        // IN
                     const char *args,        // IN
                     size_t argsSize,         // IN
                     void *clientData);       // IN

RpcIn *RpcIn_Construct(DblLnkLst_Links *eventQueue);

Bool RpcIn_start(RpcIn *in, unsigned int delay,
                 RpcIn_Callback resetCallback, void *resetClientData,
                 RpcIn_ErrorFunc *errorFunc, void *errorData);

/*
 * Don't use this function anymore - it's here only for backwards compatibility.
 * Use RpcIn_RegisterCallbackEx() instead.
 */
void RpcIn_RegisterCallback(RpcIn *in, const char *name,
                            RpcIn_CallbackOld callback, void *clientData);

void RpcIn_RegisterCallbackEx(RpcIn *in, const char *name,
                              RpcIn_Callback callback, void *clientData);
void RpcIn_UnregisterCallback(RpcIn *in, const char *name);

#endif

void RpcIn_Destruct(RpcIn *in);
Bool RpcIn_restart(RpcIn *in);
Bool RpcIn_stop(RpcIn *in);

unsigned int RpcIn_SetRetVals(char const **result, size_t *resultLen,
                              const char *resultVal, Bool retVal);
#endif /* __RPCIN_H__ */

