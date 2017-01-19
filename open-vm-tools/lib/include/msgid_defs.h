/*********************************************************
 * Copyright (C) 2011-2016 VMware, Inc. All rights reserved.
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
 * msgid_defs.h --
 *
 *	Message ID magic definitions
 */

#ifndef _MSGID_DEFS_H_
#define _MSGID_DEFS_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMKERNEL
#include "includeCheck.h"


/*
 * Message ID macros
 *
 * Use as in
 *	Msg_Append(MSGID(file.openFailed) "Failed to open file %s: %s.\n"
 *		   fileName, Msg_ErrString())
 *	Msg_Append(MSGID(mks.powerOnFailed) "Power on failed.\n")
 * or
 *	Msg_Hint(TRUE, HINT_OK,
 *		 MSGID(mks.noDGA) "No full screen mode.\n").
 *
 * Don't make MSG_MAGIC_LEN (sizeof MSG_MAGIC - 1), since
 * that may cause the string to be in the object file, even
 * when it's not used at run time.  And we are trying
 * to avoid littering the output with the magic string.
 *
 * -- edward
 */

#define MSG_MAGIC	"@&!*@*@"
#define MSG_MAGIC_LEN	7
#define MSGID(id)	MSG_MAGIC "(msg." #id ")"
#define MSG_BUTTON_ID "(button."
#define MSG_BUTTON_ID_LEN 8
#define BUTTONID(id)	MSG_MAGIC MSG_BUTTON_ID #id ")"

#endif // ifndef _MSGID_DEFS_H_
