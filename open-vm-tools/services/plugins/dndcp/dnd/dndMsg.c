/*********************************************************
 * Copyright (C) 2007-2019 VMware, Inc. All rights reserved.
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

/*
 * dndMsg.c --
 *
 *      DnDMsg represents an rpc message which is sent across the
 *      wire. Any args that it holds will be written out exactly as stored.
 *
 *      To protect itself there are many checks to ensure the data which is
 *      serialized and unserialized is sane. Defines and asserts are used to
 *      ensure the message stays under these limits when serializing out and
 *      checks are enforced to ensure that the data to be unserialized remains
 *      under these limits.
 */

#include <stdlib.h>
#include <string.h>
#include "vm_assert.h"

#include "dndMsg.h"
#include "dndInt.h"

#define LOGLEVEL_MODULE dnd
#include "loglevel_user.h"


/*
 *----------------------------------------------------------------------------
 *
 * DnDMsg_Init --
 *
 *      DnDMsg constructor.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------------
 */

void
DnDMsg_Init(DnDMsg *msg)   // IN/OUT: the message
{
   ASSERT(msg);

   msg->ver = 3;
   msg->cmd = 0;
   msg->nargs = 0;
   DynBufArray_Init(&msg->args, 0);
   msg->expectedArgsSz = 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDMsg_Destroy --
 *
 *      Destroys a message by clearing any of the data that is contained in it.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Frees the arguments' memory.
 *
 *----------------------------------------------------------------------------
 */

void
DnDMsg_Destroy(DnDMsg *msg)  // IN/OUT: the message
{
   uint32 i;
   uint32 count;

   ASSERT(msg);

   count = DynArray_Count(&msg->args);

   msg->ver = 0;
   msg->cmd = 0;
   msg->nargs = 0;
   msg->expectedArgsSz = 0;

   for (i = 0; i < count; ++i) {
      DynBuf *b = DynArray_AddressOf(&msg->args, i);
      DynBuf_Destroy(b);
   }
   DynArray_SetCount(&msg->args, 0);
   DynBufArray_Destroy(&msg->args);
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDMsg_Cmd --
 *
 *      Gets the dnd/copy paste command from the header.
 *
 * Results:
 *      An uint32 representing the command.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

uint32
DnDMsg_GetCmd(DnDMsg *msg)      // IN/OUT: the message
{
   ASSERT(msg);

   return msg->cmd;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDMsg_SetCmd --
 *
 *      Sets the command for the message.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
DnDMsg_SetCmd(DnDMsg *msg,      // IN/OUT: the message
              uint32 cmd)       // IN: the command
{
   ASSERT(msg);
   ASSERT((DND_INVALID < cmd && cmd < DND_MAX) ||
          (CP_INVALID < cmd && cmd < CP_MAX));

   msg->cmd = cmd;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDMsg_NumArgs --
 *
 *      Determines the number of arguments currently in the DnDMsg.
 *
 * Results:
 *      The number of arguments.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

uint32
DnDMsg_NumArgs(DnDMsg *msg)     // IN/OUT: the message
{
   ASSERT(msg);

   return DynBufArray_Count(&msg->args);
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDMsg_GetArg --
 *
 *      Gets an argument stored in DnDMsg.
 *
 * Results:
 *      Null if the argument is out of bounds, otherwise a pointer to a dynbuf
 *      containing the argument.
 *       This dynbuf is still
 *      managed by the DnDMsg and should NOT be destroyed.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

DynBuf *
DnDMsg_GetArg(DnDMsg *msg,      // IN/OUT: the message
              uint32 idx)       // IN: the argument to return
{
   ASSERT(msg);
   ASSERT(0 <= idx && idx < DynBufArray_Count(&msg->args));

   return DynArray_AddressOf(&msg->args, idx);
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDMsg_AppendArg --
 *
 *      Adds the data to the end of the argument list in the message. It will
 *      create a copy of the data to be mananged by DnDMsg until the message is
 *      destroyed.
 *
 * Results:
 *      TRUE on success, FALSE on failure.
 *
 * Side effects:
 *      Increases the internal arg size counter.
 *
 *----------------------------------------------------------------------------
 */

Bool
DnDMsg_AppendArg(DnDMsg *msg,   // IN/OUT: the message
                 void *buf,     // IN: the input buffer
                 size_t len)    // IN: the length of the input buffer
{
   DynBuf clonebuf;

   ASSERT(msg);
   ASSERT(buf);

   if (DynBufArray_Count(&msg->args) >= DNDMSG_MAX_ARGS) {
      return FALSE;
   }

   DynBuf_Init(&clonebuf);
   if (!DynBuf_Append(&clonebuf, buf, len)) {
      goto err;
   }

   /* The dynbufarray now owns the clonebuf data. */
   if (!DynBufArray_Push(&msg->args, clonebuf)) {
      goto err;
   }
   return TRUE;

err:
   DynBuf_Destroy(&clonebuf);
   return FALSE;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDMsg_Serialize --
 *
 *      Serialize the contents of the DnDMsg out to the provided dynbuf. It
 *      will ASSERT if any invariants are broken.
 *
 * Results:
 *      TRUE on success.
 *      FALSE on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

Bool
DnDMsg_Serialize(DnDMsg *msg,   // IN/OUT: the message
                 DynBuf* buf)   // OUT: the output buffer
{
   uint32 nargs;
   uint32 i;
   uint32 serializeArgsSz = 0;

   ASSERT(msg);
   ASSERT(buf);
   ASSERT((DND_INVALID < msg->cmd && msg->cmd < DND_MAX) ||
          (CP_INVALID < msg->cmd && msg->cmd < CP_MAX));

   nargs = DynBufArray_Count(&msg->args);

   for (i = 0; i < nargs; ++i) {
      DynBuf *b = DynArray_AddressOf(&msg->args, i);
      serializeArgsSz += sizeof(uint32) + DynBuf_GetSize(b);
   }

   if (DynBuf_Append(buf, &msg->ver, sizeof msg->ver) &&
       DynBuf_Append(buf, &msg->cmd, sizeof msg->cmd) &&
       DynBuf_Append(buf, &nargs, sizeof nargs) &&
       DynBuf_Append(buf, &serializeArgsSz, sizeof serializeArgsSz)) {
      int i;
      uint32 curArgsSz;

      for (i = 0; i < nargs; i++) {
         DynBuf *curArg = DynBufArray_AddressOf(&msg->args, i);

         curArgsSz = DynBuf_GetSize(curArg);

         if (!DynBuf_Append(buf, &curArgsSz, sizeof curArgsSz) ||
             !DynBuf_Append(buf, DynBuf_Get(curArg), curArgsSz)) {
            return FALSE;
         }
      }
   } else {
      return FALSE;
   }

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDMsg_UnserializeHeader --
 *
 *      Read the header from the buffer into a DnDMsg. Any contents in the
 *      DnDMsg will be destroyed. This allows you to retrieve header
 *      information. These functions are specified in the dndMsg.h. Most
 *      notably, you can retrieve the size of the arguments so that you can
 *      pass a properly sized buffer to DnDMsg_UnserializeArgs.
 *
 *      This is the one of the two places that nargs is set. The other is
 *      implicitly set by DnDMsg_AppendArg with the push and only ever
 *      realized through the DnDMsg_Serialize function. expectedArgsSz,
 *      curArgSz follows the same idea.
 *
 * Results:
 *      DNDMSG_SUCCESS on success.
 *      DNDMSG_INPUT_TOO_SMALL when provided buffer is too small.
 *      DNDMSG_INPUT_ERR when the provided buffer is inconsistant.
 *      DNDMSG_NOMEM when we run out of memory.
 *      DNDMSG_ERR on any other error.
 *
 * Side effects:
 *      On success the msg's header will be filled. On failure the msg will be
 *      destroyed.
 *
 *----------------------------------------------------------------------------
 */

DnDMsgErr
DnDMsg_UnserializeHeader(DnDMsg *msg,   // IN/OUT: the message
                         void *buf,     // IN: the input buffer
                         size_t len)    // IN: the buffer length
{
   BufRead r;

   ASSERT(msg);
   ASSERT(buf);

   r.pos = buf;
   r.unreadLen = len;

   if (len < DNDMSG_HEADERSIZE_V3) {
      return DNDMSG_INPUT_TOO_SMALL;
   }

   /* Read buffer into msg. */
   if (DnDReadBuffer(&r, &msg->ver, sizeof msg->ver) &&
       DnDReadBuffer(&r, &msg->cmd, sizeof msg->cmd) &&
       DnDReadBuffer(&r, &msg->nargs, sizeof msg->nargs) &&
       DnDReadBuffer(&r, &msg->expectedArgsSz, sizeof msg->expectedArgsSz)) {
      /* Sanity checks. */
      if (msg->expectedArgsSz < DNDMSG_MAX_ARGSZ &&
          (msg->cmd < DND_MAX || msg->cmd < CP_MAX) &&
          0 < msg->cmd &&
          msg->ver >= 3 &&
          msg->nargs < DNDMSG_MAX_ARGS) {
         return DNDMSG_SUCCESS;
      } else {
         return DNDMSG_INPUT_ERR;
      }
   } else {
      return DNDMSG_INPUT_TOO_SMALL;
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDMsg_UnserializeArgs --
 *
 *      Unserialize the arguments of the message provided by the buffer.
 *      Each argument is a uint32 of the size followed by the buffer. On
 *      failure the message will revert to the state which was passed into the
 *      function.
 *
 * Results:
 *      DNDMSG_SUCCESS on success.
 *      DNDMSG_INPUT_TOO_SMALL when provided buffer is too small.
 *      DNDMSG_INPUT_ERR when the provided buffer is inconsistant.
 *      DNDMSG_NOMEM when we run out of memory.
 *      DNDMSG_ERR on any other error.
 *
 * Side effects:
 *      On success, arguments found in buf are unserialized into msg.
 *
 *----------------------------------------------------------------------------
 */

DnDMsgErr
DnDMsg_UnserializeArgs(DnDMsg *msg,     // IN/OUT: the message
                       void *buf,       // IN: input buffer
                       size_t len)      // IN: buffer length
{
   uint32 i;
   uint32 count;
   BufRead r;
   uint32 readArgsSz = 0;

   void *data = NULL;
   DnDMsgErr ret = DNDMSG_SUCCESS;

   ASSERT(msg);
   ASSERT(DynBufArray_Count(&msg->args) == 0);
   ASSERT(buf);

   r.pos = buf;
   r.unreadLen = len;

   if (len < msg->expectedArgsSz) {
      return DNDMSG_INPUT_TOO_SMALL;
   }

   for (i = 0; i < msg->nargs; ++i) {
      uint32 argSz;
      if (!DnDReadBuffer(&r, &argSz, sizeof argSz)) {
         ret = DNDMSG_INPUT_TOO_SMALL;
         goto err;
      }

      if (argSz > DNDMSG_MAX_ARGSZ ||
          readArgsSz + sizeof (uint32) + argSz > msg->expectedArgsSz) {
         ret = DNDMSG_INPUT_ERR;
         goto err;
      }

      data = malloc(argSz);
      if (!data) {
         ret = DNDMSG_NOMEM;
         goto err;
      }

      if (!DnDReadBuffer(&r, data, argSz)) {
         ret = DNDMSG_ERR;
         goto err;
      }

      if (!DnDMsg_AppendArg(msg, data, argSz)) {
         ret = DNDMSG_NOMEM;
         goto err;
      }
      readArgsSz += argSz + sizeof (uint32);

      free(data);
      data = NULL;
   }

   ASSERT(ret == DNDMSG_SUCCESS);
   return ret;

err:
   free(data);

   count = DynBufArray_Count(&msg->args);
   for (i = 0; i < count; ++i) {
      DynBuf *b = DynArray_AddressOf(&msg->args, i);
      DynBuf_Destroy(b);
   }
   /*
    * DnDMsg_AppendArg relies on DynBufArray_Push, hence the count needs to be
    * reset.
    */
   DynBufArray_SetCount(&msg->args, 0);

   return ret;
}
