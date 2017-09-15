/*********************************************************
 * Copyright (C) 2008-2016 VMware, Inc. All rights reserved.
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
 * stub-user-msg.c --
 *
 *   Stubs for Msg_* functions in lib/user.
 *
 */

#if defined(_WIN32)
#  include <windows.h>
#endif
#include "vm_assert.h"
#include "msg.h"
#include "str.h"


void
Msg_AppendMsgList(const MsgList *msgs)
{
   while (msgs != NULL) {
      Warning("%s [STUB]: %s\n", __FUNCTION__, msgs->id);
      msgs = msgs->next;
   }
}


void
Msg_Append(const char *fmt,
           ...)
{
   static char buf[1000];

   va_list args;
   va_start(args, fmt);
   Str_Vsnprintf(buf, sizeof buf, fmt, args);
   va_end(args);

   Warning("%s [STUB]: %s\n", __FUNCTION__, buf);
}


void
Msg_Post(MsgSeverity severity,
         const char *idFmt, ...)
{
   NOT_IMPLEMENTED();
}


unsigned int
Msg_Question(Msg_String const *buttons,
             int defaultAnswer,
             char const *fmt,
             ...)
{
   static char buf[1000];

   va_list args;
   va_start(args, fmt);
   Str_Vsnprintf(buf, sizeof buf, fmt, args);
   va_end(args);

   Warning("%s [STUB]: %s\n", __FUNCTION__, buf);

   return (unsigned int) defaultAnswer;
}


void
Msg_Reset(Bool log)
{
   NOT_IMPLEMENTED();
}

char *
Msg_FormatSizeInBytes(uint64 size)
{
   return NULL;
}


