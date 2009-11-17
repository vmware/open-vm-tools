/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
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

void
RpcChannel_Error(void *_state,
                 char const *status);

#endif /* _RPCCHANNELINT_H_ */

