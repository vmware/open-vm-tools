/*********************************************************
 * Copyright (C) 2008-2018 VMware, Inc. All rights reserved.
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
#include "util.h"

#if defined(__cplusplus)
extern "C" {
#endif

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
#define XDRUTIL_COUNT(ptr, field) ((ptr)->field.field##_len)
#define XDRUTIL_FOREACH(counter, ptr, field)                               \
   for ((counter) = 0; (counter) < XDRUTIL_COUNT(ptr, field); (counter)++)

#define XDRUTIL_GETITEM(ptr, field, idx) &((ptr)->field.field##_val[idx])

/*
 * Wrapper for XdrUtil_ArrayAppend that automatically populates the arguments
 * from a given XDR array.
 */
#ifdef __GNUC__
#   if defined(__cpp_decltype) || defined(__GXX_EXPERIMENTAL_CXX0X__)
#      define XDRUTIL_ARRAYAPPEND(ptr, field, cnt)                      \
          (decltype ((ptr)->field.field##_val))                         \
          XdrUtil_ArrayAppend((void **) &(ptr)->field.field##_val,      \
                              &(ptr)->field.field##_len,                \
                              sizeof *(ptr)->field.field##_val,         \
                              (cnt))
#   else
#      define XDRUTIL_ARRAYAPPEND(ptr, field, cnt)                      \
          (typeof ((ptr)->field.field##_val))                           \
          XdrUtil_ArrayAppend((void **) &(ptr)->field.field##_val,      \
                              &(ptr)->field.field##_len,                \
                              sizeof *(ptr)->field.field##_val,         \
                              (cnt))
#   endif
#else
#   define XDRUTIL_ARRAYAPPEND(ptr, field, cnt)                         \
       XdrUtil_ArrayAppend((void **) &(ptr)->field.field##_val,         \
                           &(ptr)->field.field##_len,                   \
                           sizeof *(ptr)->field.field##_val,            \
                           (cnt))
#endif

/*
 * Macros for assigning to XDR optional strings, opaque fields, and
 * optional opaque fields.
 *
 * Usage:
 * // XDR
 * struct MyFoo { string *foo; };
 * struct MyBar { opaque bar; };
 * struct MyOptBar { opaque *bar; };
 *
 * // C
 * char buf[] = { 0xca, 0xfe, 0xba, 0xbe, 0x80, 0x08 };
 *
 * MyFoo foo;
 * XDRUTIL_STRING_OPT(&foo.foo, "Hello, world!");
 *
 * MyBar bar;
 * XDRUTIL_OPAQUE(&bar.bar, buf, sizeof buf);
 *
 * MyOptBar obar;
 * XDRUTIL_OPAQUE_OPT(&obar.bar, buf, sizeof buf);
 */

#define XDRUTIL_STRING_OPT(ptr, src)                            do {    \
   (ptr) = Util_SafeMalloc(sizeof *(ptr));                              \
   *(ptr) = Util_SafeStrdup((src));                                     \
} while (0)

#define XDRUTIL_OPAQUE(ptr, src, srcSize)                       do {    \
   struct { u_int len; char *val; } __opaque_temp = {(srcSize), NULL};  \
   ASSERT_ON_COMPILE(sizeof(*(ptr)) == sizeof(__opaque_temp));          \
                                                                        \
   __opaque_temp.val = Util_SafeMalloc((srcSize));                      \
   memcpy(__opaque_temp.val, (src), (srcSize));                         \
   memcpy(ptr, &__opaque_temp, sizeof __opaque_temp);                   \
} while (0)

#define XDRUTIL_OPAQUE_OPT(ptr, src, srcSize)                   do {    \
   *(ptr) = Util_SafeMalloc(sizeof (struct { u_len; void*; }));         \
   XDRUTIL_OPAQUE(*(ptr), (src), (srcSize));                            \
} while(0)

void *
XdrUtil_ArrayAppend(void **array, u_int *arrayLen, size_t elemSz, u_int elemCnt);

Bool
XdrUtil_Deserialize(const void *data, size_t dataLen, void *xdrProc, void *dest);

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif /* _XDRUTIL_H_ */

