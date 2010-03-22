/*********************************************************
 * Copyright (C) 2010 VMware, Inc. All rights reserved.
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

#ifndef _VMCI_QPAIR_H_
#define _VMCI_QPAIR_H_

/*
 *
 * vmciQPair.h --
 *
 *    Defines the interface to the VMCI kernel module for clients.
 */

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_VMK_MODULE
#include "includeCheck.h"

#include "vmci_defs.h"

typedef struct VMCIQPair VMCIQPair;

int VMCIQPair_Alloc(VMCIQPair **qpair,
                    VMCIHandle *handle,
                    uint64 produceQSize,
                    uint64 consumeQSize,
                    VMCIId peer,
                    uint32 flags,
                    VMCIPrivilegeFlags privFlags);

void VMCIQPair_Detach(VMCIQPair **qpair);

void VMCIQPair_Init(VMCIQPair *qpair);
void VMCIQPair_GetProduceIndexes(const VMCIQPair *qpair,
                                 uint64 *producerTail,
                                 uint64 *consumerHead);
void VMCIQPair_GetConsumeIndexes(const VMCIQPair *qpair,
                                 uint64 *consumerTail,
                                 uint64 *producerHead);
int64 VMCIQPair_ProduceFreeSpace(const VMCIQPair *qpair);
int64 VMCIQPair_ProduceBufReady(const VMCIQPair *qpair);
int64 VMCIQPair_ConsumeFreeSpace(const VMCIQPair *qpair);
int64 VMCIQPair_ConsumeBufReady(const VMCIQPair *qpair);
ssize_t VMCIQPair_Enqueue(VMCIQPair *qpair,
                          const void *buf,
                          size_t bufSize,
                          int mode);
ssize_t VMCIQPair_Dequeue(VMCIQPair *qpair,
                          void *buf,
                          size_t bufSize,
                          int mode);
ssize_t VMCIQPair_Peek(VMCIQPair *qpair,
                       void *buf,
                       size_t bufSize,
                       int mode);

#if defined (SOLARIS) || (defined(__APPLE__) && !defined (VMX86_TOOLS)) || \
    (defined(__linux__) && defined(__KERNEL__))
/*
 * Environments that support struct iovec
 */

ssize_t VMCIQPair_EnqueueV(VMCIQPair *qpair,
                           void *iov,
                           size_t iovSize,
                           int mode);
ssize_t VMCIQPair_DequeueV(VMCIQPair *qpair,
                           void *iov,
                           size_t iovSize,
                           int mode);
ssize_t VMCIQPair_PeekV(VMCIQPair *qpair,
                        void *iov,
                        size_t iovSize,
                        int mode);
#endif /* Systems that support struct iovec */


#endif /* _VMCIQPAIR_H_ */

/*********************************************************
 * Copyright (C) 2010 VMware, Inc. All rights reserved.
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

#ifndef _VMCI_QPAIR_H_
#define _VMCI_QPAIR_H_

/*
 *
 * vmciQPair.h --
 *
 *    Defines the interface to the VMCI kernel module for clients.
 */

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_VMK_MODULE
#include "includeCheck.h"

#include "vmci_defs.h"

typedef struct VMCIQPair VMCIQPair;

int VMCIQPair_Alloc(VMCIQPair **qpair,
                    VMCIHandle *handle,
                    uint64 produceQSize,
                    uint64 consumeQSize,
                    VMCIId peer,
                    uint32 flags,
                    VMCIPrivilegeFlags privFlags);

void VMCIQPair_Detach(VMCIQPair **qpair);

void VMCIQPair_Init(VMCIQPair *qpair);
void VMCIQPair_GetProduceIndexes(const VMCIQPair *qpair,
                                 uint64 *producerTail,
                                 uint64 *consumerHead);
void VMCIQPair_GetConsumeIndexes(const VMCIQPair *qpair,
                                 uint64 *consumerTail,
                                 uint64 *producerHead);
int64 VMCIQPair_ProduceFreeSpace(const VMCIQPair *qpair);
int64 VMCIQPair_ProduceBufReady(const VMCIQPair *qpair);
int64 VMCIQPair_ConsumeFreeSpace(const VMCIQPair *qpair);
int64 VMCIQPair_ConsumeBufReady(const VMCIQPair *qpair);
ssize_t VMCIQPair_Enqueue(VMCIQPair *qpair,
                          const void *buf,
                          size_t bufSize,
                          int mode);
ssize_t VMCIQPair_Dequeue(VMCIQPair *qpair,
                          void *buf,
                          size_t bufSize,
                          int mode);
ssize_t VMCIQPair_Peek(VMCIQPair *qpair,
                       void *buf,
                       size_t bufSize,
                       int mode);

#if defined (SOLARIS) || (defined(__APPLE__) && !defined (VMX86_TOOLS)) || \
    (defined(__linux__) && defined(__KERNEL__))
/*
 * Environments that support struct iovec
 */

ssize_t VMCIQPair_EnqueueV(VMCIQPair *qpair,
                           void *iov,
                           size_t iovSize,
                           int mode);
ssize_t VMCIQPair_DequeueV(VMCIQPair *qpair,
                           void *iov,
                           size_t iovSize,
                           int mode);
ssize_t VMCIQPair_PeekV(VMCIQPair *qpair,
                        void *iov,
                        size_t iovSize,
                        int mode);
#endif /* Systems that support struct iovec */


#endif /* _VMCIQPAIR_H_ */

