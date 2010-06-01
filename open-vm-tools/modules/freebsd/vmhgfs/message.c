/*********************************************************
 * Copyright (C) 1999 VMware, Inc. All rights reserved.
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
 * message.c --
 *
 *    Second layer of the internal communication channel between guest
 *    applications and vmware
 *
 *    Build a generic messaging system between guest applications and vmware.
 *
 *    The protocol is not completely symmetrical, because:
 *     . basic requests can only be sent by guest applications (when vmware
 *       wants to post a message to a guest application, the message will be
 *       really fetched only when the guest application will poll for new
 *       available messages)
 *     . several guest applications can talk to vmware, while the contrary is
 *       not true
 *
 *    Operations that are not atomic (in terms of number of backdoor calls)
 *    can be aborted by vmware if a checkpoint/restore occurs in the middle of
 *    such an operation. This layer takes care of retrying those operations.
 */

#ifdef __cplusplus
extern "C" {
#endif

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

static MessageOpenProcType externalOpenProc = NULL;
static MessageGetReadEventProcType externalGetReadEventProc = NULL;
static MessageSendProcType externalSendProc = NULL;
static MessageReceiveProcType externalReceiveProc = NULL;
static MessageCloseProcType externalCloseProc = NULL;

/*
 * Currently, the default implementation is to use the backdoor. Soon,
 * this will not be the default, as we will explicitly set it when we
 * decide to use the backdoor.
 */
EXTERN Message_Channel *MessageBackdoor_Open(uint32 proto);

EXTERN Bool MessageBackdoor_GetReadEvent(Message_Channel *chan,
                                         int64 *event);

EXTERN Bool MessageBackdoor_Send(Message_Channel *chan,
                                 const unsigned char *buf,
                                 size_t bufSize);

EXTERN Bool MessageBackdoor_Receive(Message_Channel *chan,
                                    unsigned char **buf,
                                    size_t *bufSize);

EXTERN Bool MessageBackdoor_Close(Message_Channel *chan);



/*
 *-----------------------------------------------------------------------------
 *
 * Message_SetTransport --
 *
 *    This tells the message layer to use an alternate transport
 *    for messages. By default, we use the backdoor, so this function
 *    overrides that default at runtime and switches everything over to
 *    an alternate transport.
 *
 * Result:
 *    None
 *
 * Side-effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

void 
Message_SetTransport(MessageOpenProcType openProc,                   // IN
                     MessageGetReadEventProcType getReadEeventProc,  // IN
                     MessageSendProcType sendProc,                   // IN
                     MessageReceiveProcType receiveProc,             // IN
                     MessageCloseProcType closeProc)                 // IN
{
   externalOpenProc = openProc;
   externalGetReadEventProc = getReadEeventProc;
   externalSendProc = sendProc;
   externalReceiveProc = receiveProc;
   externalCloseProc = closeProc;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Message_Open --
 *
 *    Open a communication channel
 *
 * Result:
 *    An allocated Message_Channel on success
 *    NULL on failure
 *
 * Side-effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Message_Channel *
Message_Open(uint32 proto) // IN
{
   /*
    * If there is an alterate backdoor implementation, then call that.
    */
   if (NULL != externalOpenProc) {
      return((*externalOpenProc)(proto));
   }

   /*
    * Otherwise, we default to the backdoor.
    */
   return(MessageBackdoor_Open(proto));
}


/*
 *-----------------------------------------------------------------------------
 *
 * Message_GetReadEvent --
 *
 *    This allows higher levels of the IPC stack to use an event to detect
 *    when a message has arrived. This allows an asynchronous, event-based-model 
 *    rather than continually calling Message_Receive in a busy loop. This may 
 *    only be supported by some transports. The backdoor does not, so the IPC
 *    code will still have to poll in those cases.
 *
 * Result:
 *    Bool - whether this feature is supported by this transport.
 *
 * Side-effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool 
Message_GetReadEvent(Message_Channel *chan,  // IN
                     int64 *event)           // OUT
{
   /*
    * If there is an alterate backdoor implementation, then call that.
    */
   if (NULL != externalGetReadEventProc) {
      return((*externalGetReadEventProc)(chan, event));
   }

   /*
    * Otherwise, we default to the backdoor.
    */
   return(MessageBackdoor_GetReadEvent(chan, event));
}


/*
 *-----------------------------------------------------------------------------
 *
 * Message_Send --
 *
 *    Send a message over a communication channel
 *
 * Result:
 *    TRUE on success
 *    FALSE on failure (the message is discarded by vmware)
 *
 * Side-effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
Message_Send(Message_Channel *chan,    // IN/OUT
             const unsigned char *buf, // IN
             size_t bufSize)           // IN
{
   /*
    * If there is an alterate backdoor implementation, then call that.
    */
   if (NULL != externalSendProc) {
      return((*externalSendProc)(chan, buf, bufSize));
   }

   /*
    * Otherwise, we default to the backdoor.
    */
   return(MessageBackdoor_Send(chan, buf, bufSize));
}


/*
 *-----------------------------------------------------------------------------
 *
 * Message_Receive --
 *
 *    If vmware has posted a message for this channel, retrieve it
 *
 * Result:
 *    TRUE on success (bufSize is 0 if there is no message)
 *    FALSE on failure
 *
 * Side-effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
Message_Receive(Message_Channel *chan, // IN/OUT
                unsigned char **buf,   // OUT
                size_t *bufSize)       // OUT
{
   /*
    * If there is an alterate backdoor implementation, then call that.
    */
   if (NULL != externalReceiveProc) {
      return((*externalReceiveProc)(chan, buf, bufSize));
   }

   /*
    * Otherwise, we default to the backdoor.
    */
   return(MessageBackdoor_Receive(chan, buf, bufSize));
}


/*
 *-----------------------------------------------------------------------------
 *
 * Message_Close --
 *
 *    Close a communication channel
 *
 * Result:
 *    TRUE on success, the channel is destroyed
 *    FALSE on failure
 *
 * Side-effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
Message_Close(Message_Channel *chan) // IN/OUT
{
   /*
    * If there is an alterate backdoor implementation, then call that.
    */
   if (NULL != externalCloseProc) {
      return((*externalCloseProc)(chan));
   }

   /*
    * Otherwise, we default to the backdoor.
    */
   return(MessageBackdoor_Close(chan));
}

#ifdef __cplusplus
}
#endif
