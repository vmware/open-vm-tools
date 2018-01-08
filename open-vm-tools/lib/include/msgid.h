/*********************************************************
 * Copyright (C) 2008-2017 VMware, Inc. All rights reserved.
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
 * msgid.h --
 *
 *	Message ID magic
 */

#ifndef _MSGID_H_
#define _MSGID_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMKERNEL
#include "includeCheck.h"

#include "msgid_defs.h"
#include "vm_basic_defs.h"

#ifndef VMKERNEL
#include <string.h>
#endif

#if defined(__cplusplus)
extern "C" {
#endif


// the X hides MSG_MAGIC so it won't appear in the object file
#define MSG_MAGICAL(s) \
   (s != NULL && strncmp(s, MSG_MAGIC"X", MSG_MAGIC_LEN) == 0)

// Start after MSG_MAGIC so it won't appear in the object file either.
#define MSG_HAS_BUTTONID(s) \
   (MSG_MAGICAL(s) && \
    (strncmp(&(s)[MSG_MAGIC_LEN], MSG_BUTTON_ID, MSG_BUTTON_ID_LEN) == 0))


/*
 *-----------------------------------------------------------------------------
 *
 * Msg_HasMsgID --
 *
 *      Check that a string has a message ID.
 *	The full "MSG_MAGIC(...)" prefix is required, not just MSG_MAGIC.
 *
 * Results:
 *      True if string has a message ID.
 *
 * Side Effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Bool
Msg_HasMsgID(const char *s)
{
   return MSG_MAGICAL(s) &&
          *(s += MSG_MAGIC_LEN) == '(' &&
          strchr(s + 1, ')') != NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Msg_StripMSGID --
 *
 *      Returns the string that is inside the MSGID() or if it doesn't
 *      have a MSGID just return the string.
 *
 * Results:
 *      The unlocalized string.
 *
 * Side Effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE const char *
Msg_StripMSGID(const char *idString)    // IN
{
   const char *s = idString;

   if (MSG_MAGICAL(s) &&
       *(s += MSG_MAGIC_LEN) == '(' &&
       (s = strchr(s + 1, ')')) != NULL) {
      return s + 1;
   }
   return idString;
}

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif // ifndef _MSGID_H_
