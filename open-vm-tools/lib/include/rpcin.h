/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
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

#include "dbllnklst.h"

typedef void RpcIn_ErrorFunc(void *clientData, char const *status);

typedef struct RpcIn RpcIn;

/*
 * Type for RpcIn callbacks.  The command is the prefix of message and
 * that result may be NULL.  The callback function is responsible for
 * allocating memory for the result string.
 */
typedef Bool
(*RpcIn_Callback)(char const **result,     // OUT
                  size_t *resultLen,       // OUT
                  const char *name,        // IN
                  const char *args,        // IN
                  size_t argsSize,         // IN
                  void *clientData);       // IN

RpcIn *RpcIn_Construct(DblLnkLst_Links *eventQueue);
void RpcIn_Destruct(RpcIn *in);
Bool RpcIn_start(RpcIn *in, unsigned int delay,
                 RpcIn_Callback resetCallback, void *resetClientData,
                 RpcIn_ErrorFunc *errorFunc, void *errorData);
Bool RpcIn_restart(RpcIn *in);
Bool RpcIn_stop(RpcIn *in);
void RpcIn_RegisterCallback(RpcIn *in, const char *name,
                            RpcIn_Callback callback, void *clientData);
void RpcIn_UnregisterCallback(RpcIn *in, const char *name);
unsigned int RpcIn_SetRetVals(char const **result, size_t *resultLen,
                              const char *resultVal, Bool retVal);

#endif /* __RPCIN_H__ */
