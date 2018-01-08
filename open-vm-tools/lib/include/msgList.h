/*********************************************************
 * Copyright (C) 2009-2017 VMware, Inc. All rights reserved.
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
 * msgList.h  --
 * 
 *   Utilities to manipulate (stateless) lists of messages.
 */

#ifndef _MSGLIST_H_
#define _MSGLIST_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include <string.h>
#include <stdarg.h>
#include "vm_basic_types.h"
#include "msgid.h"
#include "msgfmt.h"

#if defined(__cplusplus)
extern "C" {
#endif


/*
 * Data structures, types, and constants
 */


typedef struct MsgList MsgList;
struct MsgList {
   MsgList     *next;
   char        *id;
   char        *format;
   MsgFmt_Arg  *args;
   int          numArgs;
};

/*
 * Functions
 */

MsgList *MsgList_Create(const char *idFmt, ...) PRINTF_DECL(1, 2);
MsgList *MsgList_VCreate(const char *idFmt, va_list args);
MsgList *MsgList_CreateStr(const char *id);

void MsgList_Append(MsgList **tail, const char *idFmt, ...) PRINTF_DECL(2, 3);
void MsgList_VAppend(MsgList **tail, const char *idFmt, va_list args);
void MsgList_AppendStr(MsgList **tail, const char *id);
void MsgList_AppendMsgList(MsgList **tail, MsgList *messages);

void MsgList_Log(const MsgList *messages);
char *MsgList_ToEnglishString(const MsgList *messages);
MsgList *MsgList_Copy(const MsgList *src);
void MsgList_Free(MsgList *messages);

const char *MsgList_GetMsgID(const MsgList *messages);

Bool MsgList_Present(const MsgList *messages);

static INLINE void
MsgList_LogAndFree(MsgList *messages)
{
   MsgList_Log(messages);
   MsgList_Free(messages);
}

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif // ifndef _MSGLIST_H_
