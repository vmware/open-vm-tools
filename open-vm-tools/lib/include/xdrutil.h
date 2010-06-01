/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
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

#ifndef _XDRUTIL_H_
#define _XDRUTIL_H_

/*
 * xdrutil.h --
 *
 *    Utility functions for code that uses XDR to encode/decode data.
 */

#include <rpc/rpc.h>
#include "vm_basic_types.h"

/*
 * Helper macros for iterating over an rpcgen-generated array. Given a struct S:
 *
 * struct S {
 *    struct {
 *       u_int f_len;
 *       T *f_val;
 *    } f;
 * };
 *
 * Iterate over the array like this:
 *
 * S s;
 * u_int i;
 *
 * XDRUTIL_FOREACH(i, &s, f) {
 *    T *t = XDRUTIL_GETITEM(&s, f, i);
 * }
 *
 * 'f' should be a string with the field name.
 */
#define XDRUTIL_FOREACH(counter, ptr, field)                               \
   for ((counter) = 0; (counter) < (ptr)->field.field##_len; (counter)++)

#define XDRUTIL_GETITEM(ptr, field, idx) &((ptr)->field.field##_val[idx])

/*
 * Wrapper for XdrUtil_ArrayAppend that automatically populates the arguments
 * from a given XDR array.
 */
#define XDRUTIL_ARRAYAPPEND(ptr, field, cnt)                \
   XdrUtil_ArrayAppend((void **) &(ptr)->field.field##_val, \
                       &(ptr)->field.field##_len,           \
                       sizeof *(ptr)->field.field##_val,    \
                       (cnt))

void *
XdrUtil_ArrayAppend(void **array, u_int *arrayLen, size_t elemSz, u_int elemCnt);

Bool
XdrUtil_Deserialize(const void *data, size_t dataLen, void *xdrProc, void *dest);

#endif /* _XDRUTIL_H_ */

