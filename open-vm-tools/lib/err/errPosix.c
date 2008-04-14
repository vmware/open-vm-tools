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

#include "str.h"
#include "err.h"


/*
 *----------------------------------------------------------------------
 *
 * Err_Errno2String --
 *
 *      Returns a string that corresponds to the passed error number.
 *
 * Results:
 *      Error message string.
 *      
 * Side effects:
 *      None.
 *
 * The result should be printed or copied before calling again.
 *
 *----------------------------------------------------------------------
 */

const char *
Err_Errno2String(Err_Number errorNumber) // IN
{
   return strerror(errorNumber);
}
