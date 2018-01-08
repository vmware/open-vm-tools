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
 * vmxrpc.h --
 *
 *    VMware extensions to XDR types. Wrappers to allow the use of types declared
 *    in vm_basic_types.h in structures.
 */

#ifndef _VMXRPC_H_
#define _VMXRPC_H_

#include <rpc/types.h>
#include <rpc/xdr.h>
#include "vm_basic_types.h"

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * glibc and Solaris headers seem to define functions for unsigned types with
 * slightly different names than all other platforms. Provide macros to
 * translate the names to the more common ones.
 */
#if defined(__GLIBC__) || defined(sun)
#  define xdr_u_int16_t(x, i)    xdr_uint16_t(x, i)
#  define xdr_u_int32_t(x, i)    xdr_uint32_t(x, i)
#  define xdr_u_int64_t(x, i)    xdr_uint64_t(x, i)
#endif


/* Wrapper around xdr_free that does casting automatically. */
#define VMX_XDR_FREE(func, data) xdr_free((xdrproc_t)(func), (char *)(data))


/*
 *-----------------------------------------------------------------------------
 *
 * xdr_int8 --
 *
 *    XDR function for encoding/decoding int8.
 *
 * Results:
 *    Whether the call succeeded.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE bool_t
xdr_int8(XDR *xdrs,  // IN
         int8 *ip)   // IN/OUT
{
   /* XDR doesn't seem to have a "signed char" representation. */
   return xdr_char(xdrs, (char*)ip);
}


/*
 *-----------------------------------------------------------------------------
 *
 * xdr_uint8 --
 *
 *    XDR function for encoding/decoding uint8.
 *
 * Results:
 *    Whether the call succeeded.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE bool_t
xdr_uint8(XDR *xdrs, // IN
          uint8 *ip) // IN/OUT
{
   return xdr_u_char(xdrs, ip);
}


/*
 *-----------------------------------------------------------------------------
 *
 * xdr_int16 --
 *
 *    XDR function for encoding/decoding int16.
 *
 * Results:
 *    Whether the call succeeded.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE bool_t
xdr_int16(XDR *xdrs, // IN
          int16 *ip) // IN/OUT
{
   return xdr_int16_t(xdrs, ip);
}


/*
 *-----------------------------------------------------------------------------
 *
 * xdr_uint16 --
 *
 *    XDR function for encoding/decoding uint16.
 *
 * Results:
 *    Whether the call succeeded.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE bool_t
xdr_uint16(XDR *xdrs,   // IN
           uint16 *ip)  // IN/OUT
{
   return xdr_u_int16_t(xdrs, ip);
}


/*
 *-----------------------------------------------------------------------------
 *
 * xdr_int32 --
 *
 *    XDR function for encoding/decoding int32.
 *
 * Results:
 *    Whether the call succeeded.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE bool_t
xdr_int32(XDR *xdrs, // IN
          int32 *ip) // IN/OUT
{
   return xdr_int32_t(xdrs, ip);
}


/*
 *-----------------------------------------------------------------------------
 *
 * xdr_uint32 --
 *
 *    XDR function for encoding/decoding uint32.
 *
 * Results:
 *    Whether the call succeeded.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE bool_t
xdr_uint32(XDR *xdrs,   // IN
           uint32 *ip)  // IN/OUT
{
   return xdr_u_int32_t(xdrs, ip);
}


/*
 *-----------------------------------------------------------------------------
 *
 * xdr_int64 --
 *
 *    XDR function for encoding/decoding int64.
 *
 * Results:
 *    Whether the call succeeded.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE bool_t
xdr_int64(XDR *xdrs, // IN
          int64 *ip) // IN/OUT
{
   return xdr_int64_t(xdrs, ip);
}


/*
 *-----------------------------------------------------------------------------
 *
 * xdr_uint64 --
 *
 *    XDR function for encoding/decoding uint64.
 *
 * Results:
 *    Whether the call succeeded.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE bool_t
xdr_uint64(XDR *xdrs,   // IN
           uint64 *ip)  // IN/OUT
{
   return xdr_u_int64_t(xdrs, ip);
}


/* See vm_basic_types.h. X defines a different Bool. */
#if !defined(__STRICT_ANSI__) || defined(__FreeBSD__)
/*
 *-----------------------------------------------------------------------------
 *
 * xdr_Bool --
 *
 *    XDR function for encoding/decoding Bool.
 *
 * Results:
 *    Whether the call succeeded.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */
static INLINE bool_t
xdr_Bool(XDR *xdrs,  // IN
         Bool *ip)   // IN/OUT
{
   return xdr_char(xdrs, ip);
}
#endif

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif /* _VMXRPC_H_ */

