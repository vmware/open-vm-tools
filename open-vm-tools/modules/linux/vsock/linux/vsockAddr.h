/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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
 * vsockAddr.h --
 *
 *    VSockets address constants, types and functions.
 */


#ifndef _VSOCK_ADDR_H_
#define _VSOCK_ADDR_H_


/* Assert that the given address is valid. */
#define VSOCK_ADDR_ASSERT(_a) \
   ASSERT(0 == VSockAddr_Validate((_a)))
#define VSOCK_ADDR_NOFAMILY_ASSERT(_a) \
   ASSERT(0 == VSockAddr_ValidateNoFamily((_a)))


void VSockAddr_Init(struct sockaddr_vm *addr, uint32 cid, uint32 port);
void VSockAddr_InitNoFamily(struct sockaddr_vm *addr, uint32 cid, uint32 port);
int32 VSockAddr_Validate(const struct sockaddr_vm *addr);
int32 VSockAddr_ValidateNoFamily(const struct sockaddr_vm *addr);
Bool VSockAddr_Bound(struct sockaddr_vm *addr);
void VSockAddr_Unbind(struct sockaddr_vm *addr);
Bool VSockAddr_EqualsAddr(struct sockaddr_vm *addr, struct sockaddr_vm *other);
Bool VSockAddr_EqualsHandlePort(struct sockaddr_vm *addr, VMCIHandle handle,
                                uint32 port);
int32 VSockAddr_Cast(const struct sockaddr *addr, int32 len,
                     struct sockaddr_vm **outAddr);
Bool VSockAddr_SocketContextStream(uint32 cid);
Bool VSockAddr_SocketContextDgram(uint32 cid, uint32 rid);

#endif // _VSOCK_ADDR_H_

