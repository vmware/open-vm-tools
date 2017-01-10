/*********************************************************
 * Copyright (C) 2009-2016 VMware, Inc. All rights reserved.
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
 * msgList.c --
 *
 *   Utilities to manipulate (stateless) lists of messages.
 *   See also msg.h.
 */


#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "vmware.h"
#include "util.h"
#include "str.h"
#include "err.h"
#include "msgList.h"
#include "dynbuf.h"

#define LOGLEVEL_MODULE main
#include "loglevel_user.h"


/*
 *-----------------------------------------------------------------------------
 *
 * MsgId2MsgList --
 *
 *      Create a MsgList item from the input message. Does not handle arguments;
 *      the caller must handle those.
 *
 *      Performs any needed sanity checks as well.
 *
 * Results:
 *	A newly-allocated MsgList.
 *
 * Side effects:
 *	None.
 *
 *-----------------------------------------------------------------------------
 */

static MsgList *
MsgId2MsgList(const char *idFmt)  // IN message ID and English message
{
   MsgList *m;
   const char *idp, *strp;

   /* All message strings must be prefixed by the message ID. */
   ASSERT(Msg_HasMsgID(idFmt));

   /*
    * Find the beginning of the ID (idp) and the string (strp).
    * The string should have the correct MSG_MAGIC(...)... form.
    */

   idp = idFmt + MSG_MAGIC_LEN + 1;
   strp = strchr(idp, ')') + 1;

   m = Util_SafeMalloc(sizeof *m);
   m->format = Util_SafeStrdup(strp);
   m->next = NULL;
   m->args = NULL;
   m->numArgs = 0;

   if (vmx86_debug) {
      uint32 i;
      static const char *prfx[] = {
         "msg.",    // bora/lib, VMX, ...
         "vob.",    // Vmkernel OBservation
         "vpxa.",   // VirtualCenter host agent
         "vpxd.",   // VirtualCenter server
         "hostd.",  // Host agent
                    // Additional prefixes go here, but do not add "button."
      };

      for (i = 0; i < ARRAYSIZE(prfx); i++) {
         if (!Str_Strncasecmp(idp, prfx[i], strlen(prfx[i]))) {
            break;
         }
      }
      if (i >= ARRAYSIZE(prfx)) {
         Panic("%s error: Invalid msg prefix in <%s>\n", __FUNCTION__, idp);
      }
   }

   m->id = Util_SafeStrndup(idp, strp - idp - 1 /* ')' character */);

   return m;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MsgList_AppendStr --
 *
 *      Create a MsgList item from the input message. The input message MUST
 *      have no arguments. Do not pass in formatted messages; use MsgList_Append
 *      for that. This variant is only for MSGIDs that have no format arguments.
 *
 *      If the incoming list pointer reference is NULL, operate in 'silent'
 *      mode: skip all work (except preconditions).  Note that in silent +
 *      vmx86_debug mode, this code does all work and throws away the result,
 *      to make sure all messages are parseable.
 *
 * Results:
 *	New item is attached to 'list' (and '*list' is updated).
 *
 * Side effects:
 *	Callers are responsible to free the returned MsgList.
 *
 *-----------------------------------------------------------------------------
 */

void
MsgList_AppendStr(MsgList **list,  // IN reference to existing list
                  const char *id)  // IN message ID and English message
{
   ASSERT(id != NULL);

   /* Silently upgrade system errors to real MSGIDs. */
   if (!Msg_HasMsgID(id)) {
      ASSERT(Err_String2Errno(id) != ERR_INVALID);
      /* On release builds, tolerate other messages that lack MSGIDs. */
      MsgList_Append(list, MSGID(literal) "%s", id);
      return;
   }

   /*
    * The MsgList_AppendStr variant does not accept format strings. This
    * check disallows some legitimate strings, but it's probably easier
    * on the msgconv parser to just disallow all format-string-like things.
    */
   ASSERT(strchr(id, '%') == NULL);

   /*
    * In silent mode, skip processing in release builds. Debug
    * builds can afford the speed cost to verify message is constructable.
    */
   if (list != NULL || vmx86_debug) {
      MsgList *m = MsgId2MsgList(id);

      if (list != NULL) {
         m->next = *list;
         *list = m;
      } else {
         /* Silent mode, but constructed as a sanity test. Clean up. */
         ASSERT(vmx86_debug);
         MsgList_Free(m);
      }
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * MsgList_VAppend --
 *
 *      Create a MsgList item from the message with va_list,
 *      and attach it to the incoming list.
 *
 *      If the incoming list pointer reference is NULL, operate in 'silent'
 *      mode: skip all work (except preconditions).  Note that in silent +
 *      vmx86_debug mode, this code does all work and throws away the result,
 *      to make sure all messages are parseable.
 *
 * Results:
 *	New item is attached to 'list' (and '*list' is updated).
 *
 * Side effects:
 *	Callers are responsible to free the returned MsgList.
 *
 *-----------------------------------------------------------------------------
 */

void
MsgList_VAppend(MsgList **list,     // IN/OUT/OPT: reference to existing list
                const char *idFmt,  // IN: message ID and English message
                va_list args)       // IN: args
{
   ASSERT(idFmt != NULL);

   if (!Msg_HasMsgID(idFmt)) {
      ASSERT(Err_String2Errno(idFmt) != ERR_INVALID);
      /* On release builds, tolerate other messages that lack MSGIDs. */
      MsgList_Append(list, MSGID(literal) "%s", idFmt);
      return;
   }

   /*
    * In silent mode, skip processing in release builds. Debug
    * builds can afford the speed cost to verify message is constructable.
    */
   if (list != NULL || vmx86_debug) {
      MsgList *m = MsgId2MsgList(idFmt);
      Bool status;
      char *error;

      status = MsgFmt_GetArgs(m->format, args, &m->args, &m->numArgs, &error);
      if (!status) {
         Log("%s error: %s\nformat <%s>\n", __FUNCTION__, error, m->format);
         PANIC();
      }

      if (list != NULL) {
         m->next = *list;
         *list = m;
      } else {
         /* Silent mode, but constructed as a sanity test. Clean up. */
         ASSERT(vmx86_debug);
         MsgList_Free(m);
      }
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * MsgList_Append --
 *
 *      Create the MsgList item from the message with va_list.
 *
 * Results:
 *	New item is prepended to 'list' (and '*list' is new item).
 *
 * Side effects:
 *	Callers are responsible to free the returned MsgList.
 *
 *-----------------------------------------------------------------------------
 */

void
MsgList_Append(MsgList **list,     // IN/OUT/OPT: reference to existing list
               const char *idFmt,  // IN: message ID and English message
               ...)                // IN: args
{
   va_list args;

   va_start(args, idFmt);
   MsgList_VAppend(list, idFmt, args);
   va_end(args);
}


/*
 *-----------------------------------------------------------------------------
 *
 * MsgList_AppendMsgList --
 *
 *      Append the 'messages' to an existing MsgList, 'list'. Memory
 *      owner ship is transfered to 'list'.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Callers are responsible to free the returned MsgList.
 *
 *-----------------------------------------------------------------------------
 */

void
MsgList_AppendMsgList(MsgList **list,     // IN/OUT
                      MsgList *messages)  // IN
{
   if (list != NULL && messages != NULL) {
      MsgList *head = messages;
      while (messages->next != NULL) {
         messages = messages->next;
      }
      messages->next = *list;
      *list = head;
   } else {
      MsgList_Free(messages);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * MsgList_VCreate --
 *
 *     Create the MsgList item from the message.
 *
 * Results:
 *	New MsgList structure.
 *
 * Side effects:
 *	Callers are responsible to free the returned MsgList.
 *
 *-----------------------------------------------------------------------------
 */

MsgList *
MsgList_VCreate(const char *idFmt,  // IN message ID and English message
                va_list args)       // IN args
{
   MsgList *ml = NULL;

   MsgList_VAppend(&ml, idFmt, args);

   return ml;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MsgList_Create --
 *
 *     Create the MsgList item from the message with va_list.
 *
 * Results:
 *	New MsgList structure.
 *
 * Side effects:
 *	Callers are responsible to free the returned MsgList.
 *
 *-----------------------------------------------------------------------------
 */

MsgList *
MsgList_Create(const char *idFmt,  // IN message ID and English message
               ...)                // IN args
{
   MsgList *ml = NULL;
   va_list args;

   va_start(args, idFmt);
   MsgList_VAppend(&ml, idFmt, args);
   va_end(args);

   return ml;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MsgList_CreateStr --
 *
 *     Create the MsgList item from the message with no format arguments.
 *
 * Results:
 *	New MsgList structure.
 *
 * Side effects:
 *	Callers are responsible to free the returned MsgList.
 *
 *-----------------------------------------------------------------------------
 */

MsgList *
MsgList_CreateStr(const char *idFmt)  // IN message ID and English message
{
   MsgList *ml = NULL;

   MsgList_AppendStr(&ml, idFmt);

   return ml;
}


/*
 *----------------------------------------------------------------------
 *
 * MsgList_Copy --
 *
 *      Makes a deep copy of the MsgList.
 *
 * Results:
 *      Newly allocated MsgList.  Use MsgList_Free() to free.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

MsgList *
MsgList_Copy(const MsgList *src)  // IN:
{
   MsgList *result = NULL;
   MsgList **pdst = &result;

   while (src != NULL) {
      MsgList *dst = Util_SafeMalloc(sizeof *dst);

      dst->id = Util_SafeStrdup(src->id);
      dst->format = Util_SafeStrdup(src->format);
      dst->args = MsgFmt_CopyArgs(src->args, src->numArgs);
      dst->numArgs = src->numArgs;
      dst->next = NULL;
      src = src->next;
      *pdst = dst;
      pdst = &dst->next;
   }

   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * MsgList_Free --
 *
 *      Frees the full MsgList chain.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
MsgList_Free(MsgList *messages)  // IN:
{
   MsgList *m;
   MsgList *next;

   for (m = messages; m != NULL; m = next) {
      free(m->format);
      free(m->id);
      MsgFmt_FreeArgs(m->args, m->numArgs);
      next = m->next;
      free(m);
   }
}

/*
 *----------------------------------------------------------------------
 *
 * MsgList_GetMsgID --
 *
 *      Returns the "main" MSGID for the message stack.
 *
 *      This is useful for Msg_Post, Msg_Hint, and Msg_Question,
 *      all of which have the semantic that the generalized MSGID
 *      is the MSGID of the last message in the stack.
 *
 * Results:
 *	Returns pointer to something within the MsgList, or
 *	NULL if the MsgList doesn't exist.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

const char *
MsgList_GetMsgID(const MsgList *messages)  // IN:
{
   if (messages == NULL) {
      return NULL;
   }
   while (messages->next != NULL) {
      messages = messages->next;
   }

   return messages->id;
}


/*
 *----------------------------------------------------------------------
 *
 * MsgList_ToEnglishString --
 *
 *      Returns the English representation of a MsgList chain.  Does NOT
 *      localize. (Use Msg_LocalizeList to localize instead.)
 *
 * Results:
 *      Allocated memory containing message.  Successive messages
 *      are separated by newlines.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

char *
MsgList_ToEnglishString(const MsgList *messages)  // IN:
{
   char *result = NULL;

   if (messages != NULL) {
      size_t len = 0;
      char *formatted = MsgFmt_Asprintf(&len, messages->format, messages->args,
                                        messages->numArgs);
      const char *eol = (len > 0 && formatted != NULL &&
                         formatted[len - 1] == '\n') ? "" : "\n";
      char *tail;

      if (messages->next != NULL) {
         tail = MsgList_ToEnglishString(messages->next);
      } else {
         tail = Util_SafeStrdup("");
      }
      result = Str_SafeAsprintf(NULL, "%s%s%s", formatted, eol, tail);
      free(formatted);
      free(tail);
   }

   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * MsgList_Log --
 *
 *      Emits the English representation of a MsgList chain to Log().
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
MsgList_Log(const MsgList *messages)  // IN:
{
   const MsgList *m;

   for (m = messages; m != NULL; m = m->next) {
      size_t len = 0;
      char *formatted = MsgFmt_Asprintf(&len, m->format, m->args, m->numArgs);

      Log("[%s] %s%s",
          m->id, formatted,
          (len > 0 && formatted != NULL && formatted[len - 1] == '\n') ? ""
                                                                       : "\n");
      free(formatted);
   }
}


/*
 *----------------------------------------------------------------------
 *
 * MsgList_Present --
 *
 *      Tests if the MsgList is empty.
 *
 * Results:
 *      TRUE if there are appended messages; FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Bool
MsgList_Present(const MsgList *messages)  // IN:
{
   return messages != NULL;
}
