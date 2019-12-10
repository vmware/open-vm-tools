/*********************************************************
 * Copyright (C) 2007-2016,2019 VMware, Inc. All rights reserved.
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

/*********************************************************
 * The contents of this file are subject to the terms of the Common
 * Development and Distribution License (the "License") version 1.0
 * and no later version.  You may not use this file except in
 * compliance with the License.
 *
 * You can obtain a copy of the License at
 *         http://www.opensource.org/licenses/cddl1.php
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 *********************************************************/

/*
 * rpcout.h --
 *
 *    Remote Procedure Call between VMware and guest applications
 *    C declarations
 */


#ifndef __RPCOUT_H__
#   define __RPCOUT_H__

#include "vm_basic_types.h"

#define RPCI_PROTOCOL_NUM       0x49435052 /* 'RPCI' ;-) */

typedef struct RpcOut RpcOut;

RpcOut *RpcOut_Construct(void);
void RpcOut_Destruct(RpcOut *out);
Bool RpcOut_start(RpcOut *out);
Bool RpcOut_send(RpcOut *out, char const *request, size_t reqLen,
                 Bool *rpcStatus, char const **reply, size_t *repLen);
Bool RpcOut_stop(RpcOut *out);


/*
 * This is the only method needed to send a message to vmware for
 * 99% of uses. I'm leaving the others defined here so people know
 * they can be exported again if the need arises. [greg]
 */
Bool RpcOut_sendOne(char **reply, size_t *repLen, char const *reqFmt, ...);

/* 
 * A version of the RpcOut_sendOne function that works with UTF-8
 * strings and other data that would be corrupted by Win32's
 * FormatMessage function (which is used by RpcOut_sendOne()).
 */

Bool RpcOut_SendOneRaw(void *request, size_t reqLen, char **reply, size_t *repLen);

/* 
 * A variant of the RpcOut_SendOneRaw in which the caller supplies the
 * receive buffer so as to avoid the need to call malloc internally.
 * Useful in situations where calling malloc is not allowed.
 */

Bool RpcOut_SendOneRawPreallocated(void *request, size_t reqLen, char *reply,
                                   size_t repLen);

#endif /* __RPCOUT_H__ */
