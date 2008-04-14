/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
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
 * err.c --
 *
 *      General error handling library
 *
 */

#ifdef _WIN32
#  include "win32util.h"
#endif

#include "err.h"


/*
 *----------------------------------------------------------------------
 *
 * Err_ErrString --
 *
 *      Returns a string that corresponds to the last error message.
 *
 *      TODO: Return UTF8 or a Unicode object.
 *
 * Results:
 *      Error message string.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

const char *
Err_ErrString(void)
{
   return Err_Errno2String(Err_Errno());
}
