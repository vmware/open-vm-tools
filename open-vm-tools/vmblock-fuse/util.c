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

#include <string.h>

/*
 *----------------------------------------------------------------------------
 *
 * strlcpy --
 *
 *    XXX: Copied from vmblock/linux/module.c. Share them.
 *
 *    Copies at most count - 1 bytes from src to dest, and ensures dest is NUL
 *    terminated.
 *
 * Results:
 *    Length of src.  If src >= count, src was truncated in copy.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

size_t
strlcpy(char *dest,         // OUT: destination to copy string to
        const char *src,    // IN : source to copy string from
        size_t count)       // IN : size of destination buffer
{
   size_t ret;
   size_t len;

   ret = strlen(src);
   len = ret >= count ? count - 1 : ret;
   memcpy(dest, src, len);
   dest[len] = '\0';
   return ret;
}
