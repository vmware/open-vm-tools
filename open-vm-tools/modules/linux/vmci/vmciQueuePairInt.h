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
 * vmciQueuePairInt.h --
 *
 *     Helper function declarations for VMCI QueuePair API.
 */

#ifndef _VMCI_QUEUE_PAIR_INT_H_
#define _VMCI_QUEUE_PAIR_INT_H_

#include "vmciGuestKernelAPI.h"

void VMCIQueuePair_Init(void);
void VMCIQueuePair_Exit(void);
int VMCIQueuePair_Alloc(VMCIHandle *handle, VMCIQueue **produceQ,
                        uint64 produceSize, VMCIQueue **consumeQ,
                        uint64 consumeSize, VMCIId peer, uint32 flags);
int VMCIQueuePair_AllocPriv(VMCIHandle *handle, VMCIQueue **produceQ,
                            uint64 produceSize, VMCIQueue **consumeQ,
                            uint64 consumeSize, VMCIId peer, uint32 flags,
                            VMCIPrivilegeFlags privFlags);
int VMCIQueuePair_Detach(VMCIHandle handle);


#endif /* !_VMCI_QUEUE_PAIR_INT_H_ */

