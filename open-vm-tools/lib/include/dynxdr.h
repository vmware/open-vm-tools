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

#ifndef _DYNXDR_H_
#define _DYNXDR_H_

/*
 * dynxdr.h --
 *
 *    Functions for creating and destroying an XDR stream that is backed
 *    by a dynamic memory buffer. Uses DynBuf, so requires code using it to
 *    link lib/misc.
 *
 *    This stream only does encoding. For decoding, we generally have data
 *    already available in the form of a pre-allocated buffer, in which
 *    case we can use the xdrmem_create() function.
 *
 *    Note: xdr_destroy() is a no-op for this stream. Use DynXdr_Destroy()
 *    instead. Also, XDR_SETPOS and XDR_INLINE are not supported.
 */

#include <rpc/types.h>
#include <rpc/xdr.h>
#include "vm_basic_types.h"

#if defined(__cplusplus)
extern "C" {
#endif

XDR *DynXdr_Create(XDR *in);
Bool DynXdr_AppendRaw(XDR *xdrs, const void *buf, size_t len);
void *DynXdr_AllocGet(XDR *xdrs);
void *DynXdr_Get(XDR *xdrs);
void DynXdr_Destroy(XDR *xdrs, Bool release);

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif /* _DYNXDR_H_ */

