/*********************************************************
 * Copyright (C) 1999 VMware, Inc. All rights reserved.
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

#ifdef __cplusplus
extern "C" {
#endif

#include "vm_basic_types.h"


typedef struct Message_Channel Message_Channel;


/*
 * These functions must be implemented by any external Message
 * transport implementation. Some examples include crossTalk,
 * a network socket, or a Microsoft Hypervisor backdoor.
 *
 * These external functions mirror the same corresponding Message_* 
 * functions below.
 */
typedef Message_Channel *(*MessageOpenProcType)(uint32 proto);

typedef Bool (*MessageGetReadEventProcType)(Message_Channel *chan,
                                            int64 *readEvent);

typedef Bool (*MessageSendProcType)(Message_Channel *chan,
                                    const unsigned char *buf,
                                    size_t bufSize);
typedef Bool (*MessageReceiveProcType)(Message_Channel *chan,
                                       unsigned char **buf,
                                       size_t *bufSize);
typedef Bool (*MessageCloseProcType)(Message_Channel *chan);


Message_Channel *
Message_Open(uint32 proto); // IN

Bool
Message_Send(Message_Channel *chan,    // IN/OUT
             const unsigned char *buf, // IN
             size_t bufSize);          // IN

Bool
Message_Receive(Message_Channel *chan, // IN/OUT
                unsigned char **buf,   // OUT
                size_t *bufSize);      // OUT

Bool
Message_Close(Message_Channel *chan); // IN/OUT

#ifdef __cplusplus
}
#endif

#endif /* __MESSAGE_H__ */
