/*********************************************************
 * Copyright (C) 2007-2019 VMware, Inc. All rights reserved.
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

#ifdef __cplusplus
extern "C" {
#endif

typedef void RpcIn_ErrorFunc(void *clientData, char const *status);

typedef void RpcIn_ClearErrorFunc(void *clientData);

typedef struct RpcIn RpcIn;

#if defined(VMTOOLS_USE_GLIB) /* { */

#include "vmware/tools/guestrpc.h"

RpcIn *RpcIn_Construct(GMainContext *mainCtx,
                       RpcIn_Callback dispatch,
                       gpointer clientData);

Bool RpcIn_start(RpcIn *in, unsigned int delay,
                 RpcIn_ErrorFunc *errorFunc,
                 RpcIn_ClearErrorFunc *clearErrorFunc,
                 void *errorData);

#else /* } { */

#include "dbllnklst.h"

/*
 * Type for old RpcIn callbacks. Don't use this anymore - this is here
 * for backwards compatibility.
 */
typedef Bool
(*RpcIn_Callback)(char const **result,     // OUT
                  size_t *resultLen,       // OUT
                  const char *name,        // IN
                  const char *args,        // IN
                  size_t argsSize,         // IN
                  void *clientData);       // IN

RpcIn *RpcIn_Construct(DblLnkLst_Links *eventQueue);

Bool RpcIn_start(RpcIn *in, unsigned int delay,
                 RpcIn_Callback resetCallback, void *resetClientData,
                 RpcIn_ErrorFunc *errorFunc,
                 RpcIn_ClearErrorFunc *clearErrorFunc,
                 void *errorData);


/*
 * Don't use this function anymore - it's here only for backwards compatibility.
 * Use RpcIn_RegisterCallbackEx() instead.
 */
void RpcIn_RegisterCallback(RpcIn *in, const char *name,
                            RpcIn_Callback callback, void *clientData);

void RpcIn_UnregisterCallback(RpcIn *in, const char *name);

unsigned int RpcIn_SetRetVals(char const **result, size_t *resultLen,
                              const char *resultVal, Bool retVal);

#endif /* } */

void RpcIn_Destruct(RpcIn *in);
void RpcIn_stop(RpcIn *in);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* __RPCIN_H__ */
