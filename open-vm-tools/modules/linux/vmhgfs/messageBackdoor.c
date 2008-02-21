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
 * messageBackdoor.c --
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

#ifdef MESSAGE_DEBUG
#   define MESSAGE_LOG(args) Debug args
#else /* MESSAGE_DEBUG */
#   define MESSAGE_LOG(args)
#endif /* MESSAGE_DEBUG */


#include "backdoor_def.h"
#include "guest_msg_def.h"
#include "backdoor.h"
#include "message.h"


/* The channel object */
struct Message_Channel {
   /* Identifier */
   uint16 id;

   /* Reception buffer */
   /*  Data */
   unsigned char *in;
   /*  Allocated size */
   size_t inAlloc;

   /* The cookie */
   uint32 cookieHigh;
   uint32 cookieLow;
};


/*
 *-----------------------------------------------------------------------------
 *
 * MessageBackdoor_Open --
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
MessageBackdoor_Open(uint32 proto) // IN
{
   Message_Channel *chan;
   uint32 flags;
   Backdoor_proto bp;

   chan = (Message_Channel *)malloc(sizeof(*chan));
   if (chan == NULL) {
      MESSAGE_LOG(("Message: Not enough memory\n"));
      goto error_quit;
   }

   flags = GUESTMSG_FLAG_COOKIE;
retry:
   /* IN: Type */
   bp.in.cx.halfs.high = MESSAGE_TYPE_OPEN;
   /* IN: Magic number of the protocol and flags */
   bp.in.size = proto | flags;

   bp.in.cx.halfs.low = BDOOR_CMD_MESSAGE;
   Backdoor(&bp);

   /* OUT: Status */
   if ((bp.in.cx.halfs.high & MESSAGE_STATUS_SUCCESS) == 0) {
      if (flags) {
         /* Cookies not supported. Fall back to no cookie. --hpreg */
         flags = 0;
         goto retry;
      }

      MESSAGE_LOG(("Message: Unable to open a communication channel\n"));
      goto error_quit;
   }

   /* OUT: Id and cookie */
   chan->id = bp.in.dx.halfs.high;
   chan->cookieHigh = bp.out.si.word;
   chan->cookieLow = bp.out.di.word;

   /* Initialize the channel */
   chan->in = NULL;
   chan->inAlloc = 0;

   MESSAGE_LOG(("Message: Communication channel %u opened\n", chan->id));
   return chan;

error_quit:
   free(chan);
   chan = NULL;
   return NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MessageBackdoor_GetReadEvent --
 *
 *    This allows higher levels of the IPC stack to use an event to detect
 *    when a message has arrived. This allows an interrupt-model rather than
 *    continually calling Message_Receive in a busy loop. This may only be supported
 *    by some transports. The backdoor does not, so the IPC code will still
 *    have to poll in those cases.
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
MessageBackdoor_GetReadEvent(Message_Channel *chan,  // IN
                             int64 *event)           // OUT
{
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MessageBackdoor_Send --
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
MessageBackdoor_Send(Message_Channel *chan,    // IN/OUT
                     const unsigned char *buf, // IN
                     size_t bufSize)           // IN
{
   const unsigned char *myBuf;
   size_t myBufSize;
   Backdoor_proto bp;

retry:
   myBuf = buf;
   myBufSize = bufSize;

   /*
    * Send the size.
    */

   /* IN: Type */
   bp.in.cx.halfs.high = MESSAGE_TYPE_SENDSIZE;
   /* IN: Id and cookie */
   bp.in.dx.halfs.high = chan->id;
   bp.in.si.word = chan->cookieHigh;
   bp.in.di.word = chan->cookieLow;
   /* IN: Size */
   bp.in.size = myBufSize;

   bp.in.cx.halfs.low = BDOOR_CMD_MESSAGE;
   Backdoor(&bp);

   /* OUT: Status */
   if ((bp.in.cx.halfs.high & MESSAGE_STATUS_SUCCESS) == 0) {
      MESSAGE_LOG(("Message: Unable to send a message over the communication "
                   "channel %u\n", chan->id));
      return FALSE;
   }

   if (bp.in.cx.halfs.high & MESSAGE_STATUS_HB) {
      /*
       * High-bandwidth backdoor port supported. Send the message in one
       * backdoor operation. --hpreg
       */

      if (myBufSize) {
         Backdoor_proto_hb bphb;

         bphb.in.bx.halfs.low = BDOORHB_CMD_MESSAGE;
         bphb.in.bx.halfs.high = MESSAGE_STATUS_SUCCESS;
         bphb.in.dx.halfs.high = chan->id;
         bphb.in.bp.word = chan->cookieHigh;
         bphb.in.dstAddr = chan->cookieLow;
         bphb.in.size = myBufSize;
         bphb.in.srcAddr = (uintptr_t) myBuf;
         Backdoor_HbOut(&bphb);
         if ((bphb.in.bx.halfs.high & MESSAGE_STATUS_SUCCESS) == 0) {
            if ((bphb.in.bx.halfs.high & MESSAGE_STATUS_CPT) != 0) {
               /* A checkpoint occurred. Retry the operation. --hpreg */
               goto retry;
            }

            MESSAGE_LOG(("Message: Unable to send a message over the "
                         "communication channel %u\n", chan->id));
            return FALSE;
         }
      }
   } else {
      /*
       * High-bandwidth backdoor port not supported. Send the message, 4 bytes
       * at a time. --hpreg
       */

      for (;;) {
         if (myBufSize == 0) {
            /* We are done */
	    break;
         }

         /* IN: Type */
         bp.in.cx.halfs.high = MESSAGE_TYPE_SENDPAYLOAD;
         /* IN: Id and cookie */
         bp.in.dx.halfs.high = chan->id;
         bp.in.si.word = chan->cookieHigh;
         bp.in.di.word = chan->cookieLow;
         /* IN: Piece of message */
         /*
          * Beware in case we are not allowed to read extra bytes beyond the
          * end of the buffer.
          */
         switch (myBufSize) {
         case 1:
            bp.in.size = myBuf[0];
            myBufSize -= 1;
            break;
         case 2:
            bp.in.size = myBuf[0] | myBuf[1] << 8;
            myBufSize -= 2;
            break;
         case 3:
            bp.in.size = myBuf[0] | myBuf[1] << 8 | myBuf[2] << 16;
            myBufSize -= 3;
            break;
         default:
            bp.in.size = *(const uint32 *)myBuf;
            myBufSize -= 4;
            break;
         }

         bp.in.cx.halfs.low = BDOOR_CMD_MESSAGE;
         Backdoor(&bp);

         /* OUT: Status */
         if ((bp.in.cx.halfs.high & MESSAGE_STATUS_SUCCESS) == 0) {
            if ((bp.in.cx.halfs.high & MESSAGE_STATUS_CPT) != 0) {
               /* A checkpoint occurred. Retry the operation. --hpreg */
               goto retry;
            }

            MESSAGE_LOG(("Message: Unable to send a message over the "
                         "communication channel %u\n", chan->id));
            return FALSE;
         }

         myBuf += 4;
      }
   }

   MESSAGE_LOG(("Message: Sent a message over the communication channel %u\n",
                chan->id));

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MessageBackdoor_Receive --
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
MessageBackdoor_Receive(Message_Channel *chan, // IN/OUT
                        unsigned char **buf,   // OUT
                        size_t *bufSize)       // OUT
{
   Backdoor_proto bp;
   size_t myBufSize;
   unsigned char *myBuf;

retry:
   /*
    * Is there a message waiting for our retrieval?
    */

   /* IN: Type */
   bp.in.cx.halfs.high = MESSAGE_TYPE_RECVSIZE;
   /* IN: Id and cookie */
   bp.in.dx.halfs.high = chan->id;
   bp.in.si.word = chan->cookieHigh;
   bp.in.di.word = chan->cookieLow;

   bp.in.cx.halfs.low = BDOOR_CMD_MESSAGE;
   Backdoor(&bp);

   /* OUT: Status */
   if ((bp.in.cx.halfs.high & MESSAGE_STATUS_SUCCESS) == 0) {
      MESSAGE_LOG(("Message: Unable to poll for messages over the "
                   "communication channel %u\n", chan->id));
      return FALSE;
   }

   if ((bp.in.cx.halfs.high & MESSAGE_STATUS_DORECV) == 0) {
      /* No message to retrieve */
      *bufSize = 0;
      return TRUE;
   }

   /*
    * Receive the size.
    */

   /* OUT: Type */
   if (bp.in.dx.halfs.high != MESSAGE_TYPE_SENDSIZE) {
      MESSAGE_LOG(("Message: Protocol error. Expected a "
                   "MESSAGE_TYPE_SENDSIZE request from vmware\n"));
      return FALSE;
   }

   /* OUT: Size */
   myBufSize = bp.out.bx.word;

   /*
    * Allocate an extra byte for a trailing NUL character. The code that will
    * deal with this message may not know about binary strings, and may expect
    * a C string instead. --hpreg
    */
   if (myBufSize + 1 > chan->inAlloc) {
      myBuf = (unsigned char *)realloc(chan->in, myBufSize + 1);
      if (myBuf == NULL) {
	 MESSAGE_LOG(("Message: Not enough memory to receive a message over "
                      "the communication channel %u\n", chan->id));
	 goto error_quit;
      }

      chan->in = myBuf;
      chan->inAlloc = myBufSize + 1;
   }
   *bufSize = myBufSize;
   myBuf = *buf = chan->in;

   if (bp.in.cx.halfs.high & MESSAGE_STATUS_HB) {
      /*
       * High-bandwidth backdoor port supported. Receive the message in one
       * backdoor operation. --hpreg
       */

      if (myBufSize) {
         Backdoor_proto_hb bphb;

         bphb.in.bx.halfs.low = BDOORHB_CMD_MESSAGE;
         bphb.in.bx.halfs.high = MESSAGE_STATUS_SUCCESS;
         bphb.in.dx.halfs.high = chan->id;
         bphb.in.srcAddr = chan->cookieHigh;
         bphb.in.bp.word = chan->cookieLow;
         bphb.in.size = myBufSize;
         bphb.in.dstAddr = (uintptr_t) myBuf;
         Backdoor_HbIn(&bphb);
         if ((bphb.in.bx.halfs.high & MESSAGE_STATUS_SUCCESS) == 0) {
            if ((bphb.in.bx.halfs.high & MESSAGE_STATUS_CPT) != 0) {
               /* A checkpoint occurred. Retry the operation. --hpreg */
               goto retry;
            }

            MESSAGE_LOG(("Message: Unable to receive a message over the "
                         "communication channel %u\n", chan->id));
            goto error_quit;
         }
      }
   } else {
      /*
       * High-bandwidth backdoor port not supported. Receive the message, 4
       * bytes at a time. --hpreg
       */

      for (;;) {
         if (myBufSize == 0) {
            /* We are done */
            break;
         }

         /* IN: Type */
         bp.in.cx.halfs.high = MESSAGE_TYPE_RECVPAYLOAD;
         /* IN: Id and cookie */
         bp.in.dx.halfs.high = chan->id;
         bp.in.si.word = chan->cookieHigh;
         bp.in.di.word = chan->cookieLow;
         /* IN: Status for the previous request (that succeeded) */
         bp.in.size = MESSAGE_STATUS_SUCCESS;

         bp.in.cx.halfs.low = BDOOR_CMD_MESSAGE;
         Backdoor(&bp);

         /* OUT: Status */
         if ((bp.in.cx.halfs.high & MESSAGE_STATUS_SUCCESS) == 0) {
            if ((bp.in.cx.halfs.high & MESSAGE_STATUS_CPT) != 0) {
               /* A checkpoint occurred. Retry the operation. --hpreg */
               goto retry;
            }

            MESSAGE_LOG(("Message: Unable to receive a message over the "
                         "communication channel %u\n", chan->id));
            goto error_quit;
         }

         /* OUT: Type */
         if (bp.in.dx.halfs.high != MESSAGE_TYPE_SENDPAYLOAD) {
            MESSAGE_LOG(("Message: Protocol error. Expected a "
                         "MESSAGE_TYPE_SENDPAYLOAD from vmware\n"));
            goto error_quit;
         }

         /* OUT: Piece of message */
         /*
          * Beware in case we are not allowed to write extra bytes beyond the
          * end of the buffer. --hpreg
          */
         switch (myBufSize) {
         case 1:
            myBuf[0] = bp.out.bx.word & 0xff;
            myBufSize -= 1;
            break;
         case 2:
            myBuf[0] = bp.out.bx.word & 0xff;
            myBuf[1] = (bp.out.bx.word >> 8) & 0xff;
            myBufSize -= 2;
            break;
         case 3:
            myBuf[0] = bp.out.bx.word & 0xff;
            myBuf[1] = (bp.out.bx.word >> 8) & 0xff;
            myBuf[2] = (bp.out.bx.word >> 16) & 0xff;
            myBufSize -= 3;
            break;
         default:
            *(uint32 *)myBuf = bp.out.bx.word;
            myBufSize -= 4;
            break;
         }

         myBuf += 4;
      }
   }

   /* Write a trailing NUL just after the message. --hpreg */
   chan->in[*bufSize] = '\0';

   /* IN: Type */
   bp.in.cx.halfs.high = MESSAGE_TYPE_RECVSTATUS;
   /* IN: Id and cookie */
   bp.in.dx.halfs.high = chan->id;
   bp.in.si.word = chan->cookieHigh;
   bp.in.di.word = chan->cookieLow;
   /* IN: Status for the previous request (that succeeded) */
   bp.in.size = MESSAGE_STATUS_SUCCESS;

   bp.in.cx.halfs.low = BDOOR_CMD_MESSAGE;
   Backdoor(&bp);

   /* OUT: Status */
   if ((bp.in.cx.halfs.high & MESSAGE_STATUS_SUCCESS) == 0) {
      if ((bp.in.cx.halfs.high & MESSAGE_STATUS_CPT) != 0) {
	 /* A checkpoint occurred. Retry the operation. --hpreg */
	 goto retry;
      }

      MESSAGE_LOG(("Message: Unable to receive a message over the "
                   "communication channel %u\n", chan->id));
      goto error_quit;
   }

   return TRUE;

error_quit:
   /* IN: Type */
   if (myBufSize == 0) {
      bp.in.cx.halfs.high = MESSAGE_TYPE_RECVSTATUS;
   } else {
      bp.in.cx.halfs.high = MESSAGE_TYPE_RECVPAYLOAD;
   }
   /* IN: Id and cookie */
   bp.in.dx.halfs.high = chan->id;
   bp.in.si.word = chan->cookieHigh;
   bp.in.di.word = chan->cookieLow;
   /* IN: Status for the previous request (that failed) */
   bp.in.size = 0;

   bp.in.cx.halfs.low = BDOOR_CMD_MESSAGE;
   Backdoor(&bp);

   /* OUT: Status */
   if ((bp.in.cx.halfs.high & MESSAGE_STATUS_SUCCESS) == 0) {
      MESSAGE_LOG(("Message: Unable to signal an error of reception over the "
                   "communication channel %u\n", chan->id));
      return FALSE;
   }

   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MessageBackdoor_Close --
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
MessageBackdoor_Close(Message_Channel *chan) // IN/OUT
{
   Backdoor_proto bp;
   Bool ret = TRUE;

   /* IN: Type */
   bp.in.cx.halfs.high = MESSAGE_TYPE_CLOSE;
   /* IN: Id and cookie */
   bp.in.dx.halfs.high = chan->id;
   bp.in.si.word = chan->cookieHigh;
   bp.in.di.word = chan->cookieLow;

   bp.in.cx.halfs.low = BDOOR_CMD_MESSAGE;
   Backdoor(&bp);

   /* OUT: Status */
   if ((bp.in.cx.halfs.high & MESSAGE_STATUS_SUCCESS) == 0) {
      MESSAGE_LOG(("Message: Unable to close the communication channel %u\n",
                  chan->id));
      ret = FALSE;
   } else {
      MESSAGE_LOG(("Message: Communication channel %u closed\n", chan->id));
   }

   free(chan->in);
   chan->in = NULL;

   free(chan);
   return ret;
}

#ifdef __cplusplus
}
#endif
