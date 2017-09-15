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
 * xdrutil.c --
 *
 *    Utility functions for code that uses XDR to encode/decode data.
 */

#include <stdlib.h>
#include <string.h>
#include "vm_assert.h"
#include "vmxrpc.h"
#include "xdrutil.h"


/*
 *-----------------------------------------------------------------------------
 *
 * XdrUtil_ArrayAppend --
 *
 *    Appends 'cnt' new elements of size 'sz' at the end of the given array.
 *    If successful, len will contain the count of elements in the new
 *    array.
 *
 *    The newly allocated memory is zeroed out.
 *
 * Results:
 *    NULL on allocation failure, pointer to the first new element otherwise.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

void *
XdrUtil_ArrayAppend(void **array,      // IN/OUT
                    u_int *arrayLen,   // IN/OUT
                    size_t elemSz,     // IN
                    u_int elemCnt)     // IN
{
   void *ret = NULL;
   void *newarray;

   newarray = realloc(*array, (*arrayLen + elemCnt) * elemSz);
   if (newarray != NULL) {
      ret = &((char *)newarray)[*arrayLen * elemSz];
      memset(ret, 0, elemSz * elemCnt);
      *array = newarray;
      *arrayLen = *arrayLen + elemCnt;
   }

   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * XdrUtil_Deserialize --
 *
 *    Deserializes the given data into the provided destination, using the
 *    given XDR function.
 *
 * Results:
 *    Whether deserialization was successful.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
XdrUtil_Deserialize(const void *data,  // IN
                    size_t dataLen,    // IN
                    void *xdrProc,     // IN
                    void *dest)        // IN
{
   Bool ret;
   xdrproc_t proc = xdrProc;
   XDR xdrs;

   ASSERT(data != NULL);
   ASSERT(xdrProc != NULL);
   ASSERT(dest != NULL);

   xdrmem_create(&xdrs, (char *) data, dataLen, XDR_DECODE);
   ret = (Bool) proc(&xdrs, dest, 0);
   xdr_destroy(&xdrs);

   if (!ret) {
      VMX_XDR_FREE(proc, dest);
   }

   return ret;
}

