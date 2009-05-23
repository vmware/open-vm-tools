/*********************************************************
 * Copyright (C) 2004 VMware, Inc. All rights reserved.
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
 * MessageStub.c --
 *
 * Implement the message interface that does nothing.
 *
 */

#if defined(__KERNEL__) || defined(_KERNEL) || defined(KERNEL)
#   include "kernelStubs.h"
#else
#   include <stdio.h>
#   include <stdlib.h>
#   include "debug.h"
#endif

#include "backdoor_def.h"
#include "guest_msg_def.h"
#include "backdoor.h"
#include "message.h"


static Message_Channel *MessageStub_Open(uint32 proto);

static Bool MessageStub_Close(Message_Channel *chan);

static Bool MessageStub_Receive(Message_Channel *chan,
                                  unsigned char **buf,
                                  size_t *bufSize);

static Bool MessageStub_Send(Message_Channel *chan,
                               const unsigned char *buf,
                               size_t  bufSize);

static Bool MessageStub_GetReadEvent(Message_Channel *chan,
                                       int64 *readEvent);


static int globalStubChannel = 0;



/*
 *-----------------------------------------------------------------------------
 *
 * MessageStub_RegisterTransport --
 *
 *
 * Results:
 *      Bool
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
MessageStub_RegisterTransport(void)
{
   Message_SetTransport(MessageStub_Open,
                        MessageStub_GetReadEvent,
                        MessageStub_Send,
                        MessageStub_Receive,
                        MessageStub_Close);
} // MessageStub_RegisterTransport


/*
 *-----------------------------------------------------------------------------
 *
 * MessageStub_Open --
 *
 *      Open a new channel, which can receive new socket connections.
 *
 * Results:
 *      Bool
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Message_Channel *
MessageStub_Open(uint32 proto) // IN
{
   return((Message_Channel *) &globalStubChannel);
} // MessageStub_Open


/*
 *-----------------------------------------------------------------------------
 *
 * MessageStub_Close --
 *
 *      Close the channel and all socket connections.
 *
 * Results:
 *      Bool
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
MessageStub_Close(Message_Channel *chan)  // IN
{
   return(TRUE);
} // MessageStub_Close


/*
 *-----------------------------------------------------------------------------
 *
 * MessageStub_GetReadEvent --
 * *
 * Results:
 *      Bool
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
MessageStub_GetReadEvent(Message_Channel *chan,       // IN
                         int64 *readEvent)            // OUT
{
   return(FALSE);
} // MessageStub_GetReadEvent


/*
 *----------------------------------------------------------------------------
 *
 * MessageStub_Receive --
 *
 * Results:
 *      Bool.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

Bool
MessageStub_Receive(Message_Channel *chan,     // IN
                    unsigned char **buf,       // IN
                    size_t *bufSize)           // IN
{
   if (NULL != buf) {
      *buf = NULL;
   }
   if (NULL != bufSize) {
      *bufSize = 0;
   }

   return(TRUE);
} // MessageStub_Receive


/*
 *----------------------------------------------------------------------------
 *
 * MessageStub_Send --
 *
 * Results:
 *      Bool.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

Bool
MessageStub_Send(Message_Channel *chan,     // IN
                 const unsigned char *buf,  // IN
                 size_t  bufSize)           // IN
{
   return(TRUE);
} // MessageStub_Send

