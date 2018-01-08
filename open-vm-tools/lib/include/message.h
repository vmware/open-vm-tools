/*********************************************************
 * Copyright (C) 1999-2017 VMware, Inc. All rights reserved.
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
 * message.h --
 *
 *    Second layer of the internal communication channel between guest
 *    applications and vmware
 */

#ifndef __MESSAGE_H__
#   define __MESSAGE_H__

#include "vm_basic_types.h"

#ifdef __cplusplus
extern "C" {
#endif


/* The channel object */
typedef struct Message_Channel {
   /* Identifier */
   uint16 id;

   /* Reception buffer */
   /*  Data */
   unsigned char *in;
   /*  Allocated size */
   size_t inAlloc;
   Bool inPreallocated;

   /* The cookie */
   uint32 cookieHigh;
   uint32 cookieLow;
} Message_Channel;

Bool Message_OpenAllocated(uint32 proto, Message_Channel *chan,
                           char *receiveBuffer, size_t receiveBufferSize);
Message_Channel* Message_Open(uint32 proto);
Bool Message_Send(Message_Channel *chan, const unsigned char *buf,
                  size_t bufSize);
Bool Message_Receive(Message_Channel *chan, unsigned char **buf,
                     size_t *bufSize);
Bool Message_CloseAllocated(Message_Channel *chan);
Bool Message_Close(Message_Channel *chan);

#ifdef __cplusplus
}
#endif

#endif /* __MESSAGE_H__ */
