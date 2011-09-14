/*********************************************************
 * Copyright (C) 2009-2011 VMware, Inc. All rights reserved.
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


/*
 * Data structures, types, and constants
 */


typedef struct MsgList MsgList;
struct MsgList {
   MsgList *next;
   char *id;
   char *format;
   MsgFmt_Arg *args;
   int numArgs;
};

/*
 * Functions
 */

EXTERN MsgList *MsgList_Create(const char *idFmt, ...);
EXTERN MsgList *MsgList_VCreate(const char *idFmt, va_list args);

EXTERN void MsgList_Append(MsgList **tail, const char *idFmt, ...);
EXTERN void MsgList_VAppend(MsgList **tail, const char *idFmt, va_list args);

EXTERN void MsgList_Log(const MsgList *messages);
EXTERN char *MsgList_ToString(const MsgList *messages);
EXTERN MsgList *MsgList_Copy(const MsgList *src);
EXTERN void MsgList_Free(MsgList *messages);

EXTERN const char *MsgList_GetMsgID(const MsgList *messages);

#endif // ifndef _MSGLIST_H_
