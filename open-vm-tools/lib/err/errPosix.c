/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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
 * errPosix.c --
 *
 *      Posix error handling library
 *
 */

#include <errno.h>
#include <string.h>
#include <locale.h>

#include "vmware.h"
#include "errInt.h"
#include "util.h"
#include "str.h"


/*
 *----------------------------------------------------------------------
 *
 * ErrErrno2String --
 *
 *      Convert an error number to a string in English.
 *	The returned string may use the supplied buffer or may be
 *	a static string.
 *
 * Results:
 *      The error string.
 *      
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

const char *
ErrErrno2String(Err_Number errorNumber, // IN
                char *buf,		// OUT: return buffer
		size_t bufSize)		// IN: size of buffer
{
   char *p;

#if defined(linux) && !defined(N_PLAT_NLM)
   p = strerror_r(errorNumber, buf, bufSize);
#else
   p = strerror(errorNumber);
#endif
   ASSERT(p != NULL);
   return p;
}


/*
 *----------------------------------------------------------------------
 *
 * Err_Errno2LocalString --
 *
 *      Returns a localized string in UTF-8 that corresponds to the
 *	passed error number.
 *
 *	XXX doesn't actually work
 *
 * Results:
 *      Allocated error message string, caller must free.
 *	NULL on failure.
 *      
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

char *
Err_Errno2LocalString(Err_Number errorNumber) // IN
{
   /*
    * XXX The version of glibc (2.2.x) we use doesn't support
    * uselocal() or strerror_l(), and we can't set LC_MESSAGES
    * globally, so we can't implement this function.
    * Instead, we return NULL and the upper level code
    * (Msg_LocalizeList1) generates some sort of message.
    * We can't do that here because it would require using
    * Msg_Format, which is a layer violation (and a linking
    * problem).
    */

#if 0
   // XXX need to cache newLocale if we ever actually use this code
   locale_t newLocale = newlocale(1 << LC_MESSAGES, NULL, NULL);
   locale_t oldLocale = uselocale(newLocale);
   char buf[2048];
   char *p;

   p = strerror_r(errorNumber, buf, sizeof buf);
   ASSERT_NOT_IMPLEMENTED(p != NULL);
   uselocale(oldLocale);
   return Util_SafeStrdup(p);
#else
   return NULL;
#endif
}
