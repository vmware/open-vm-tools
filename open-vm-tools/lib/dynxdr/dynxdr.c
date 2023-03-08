/*********************************************************
 * Copyright (C) 2008-2018, 2023 VMware, Inc. All rights reserved.
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

#include <stdlib.h>
#if defined(_WIN32)
#  include <winsock2.h>
#else
#  include <sys/param.h>
#  include <sys/types.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#endif

#include "dynxdr.h"
#include "dynbuf.h"
#include "util.h"

/*
 * dynxdr.c --
 *
 *    Implements an XDR stream backed by a DynBuf.
 */


typedef struct DynXdrData {
   DynBuf   data;
   Bool     freeMe;
} DynXdrData;

/*
 * Solaris does not declare some parameters as "const".
 */
#if defined(sun)
#  define DYNXDR_CONST
#else
#  define DYNXDR_CONST const
#endif

/*
 * Mac OS X, FreeBSD and Solaris don't take a const parameter to the
 * "x_getpostn" function.
 */
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(sun) || defined(USE_TIRPC)
#  define DYNXDR_GETPOS_CONST
#else
#  define DYNXDR_GETPOS_CONST const
#endif

/*
 * Solaris defines the parameter to "x_putbytes" as an int.
 */
#if defined(sun)
#  define DYNXDR_SIZE_T int
#else
#  define DYNXDR_SIZE_T u_int
#endif

/*
 * Mac OS 64-bit defines the parameter to "x_putlong" as an int.
 */
#if defined __APPLE__ && defined __LP64__
#  define DYNXDR_LONG int
#else
#  define DYNXDR_LONG long
#endif

#if defined(sun)
#   define DYNXDR_INLINE_T rpc_inline_t
#   define DYNXDR_INLINE_LEN_T int
#else
#   define DYNXDR_INLINE_T int32_t
#   define DYNXDR_INLINE_LEN_T u_int
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * DynXdrPutBytes --
 *
 *    Writes a byte array into the XDR stream.
 *
 * Results:
 *    TRUE: all ok
 *    FALSE: failed to add data do dynbuf.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static bool_t
DynXdrPutBytes(XDR *xdrs,                 // IN/OUT
               DYNXDR_CONST char *data,   // IN
               DYNXDR_SIZE_T len)         // IN
{
   DynXdrData *priv = (DynXdrData *) xdrs->x_private;
   return DynBuf_Append(&priv->data, data, len);
}


/*
 *-----------------------------------------------------------------------------
 *
 * DynXdrGetPos --
 *
 *    Returns the current position of the buffer, which is the same as
 *    the current buffer size.
 *
 *    XXX: The Mac OS X headers don't expect a "const" argument.
 *
 * Results:
 *    The size of the underlying buffer.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static u_int
DynXdrGetPos(DYNXDR_GETPOS_CONST XDR *xdrs) // IN
{
   DynXdrData *priv = (DynXdrData *) xdrs->x_private;
   return (u_int) DynBuf_GetSize(&priv->data);
}


/*
 *-----------------------------------------------------------------------------
 *
 * DynXdrSetPos --
 *
 *    Sets the position of the XDR stream. The current data in the buffer is
 *    not affected, just the pointer to the current position.
 *
 * Results:
 *    TRUE if pos is within the bounds of the backing buffer.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static bool_t
DynXdrSetPos(XDR *xdrs, // IN
             u_int pos) // IN
{
   DynXdrData *priv = (DynXdrData *) xdrs->x_private;
   if (pos <= DynBuf_GetAllocatedSize(&priv->data)) {
      DynBuf_SetSize(&priv->data, (size_t) pos);
      return TRUE;
   }
   return FALSE;
}


#if !defined(USE_TIRPC) &&    \
     defined(__GLIBC__) ||    \
     (defined(sun) &&         \
        (defined(_LP64) ||    \
           defined(_KERNEL)))
/*
 *-----------------------------------------------------------------------------
 *
 * DynXdrPutInt32 --
 *
 *    Writes a 32-bit int to the XDR stream.
 *
 *    XXX: This seems to be a glibc-only extenstion. It's present since at
 *    least glibc 2.1, according to their CVS.
 *
 *    XXX: Investigate this further.  This XDR operation exists in Solaris
 *    since at least Solaris 9.
 *
 * Results:
 *    TRUE: all ok
 *    FALSE: failed to add data do dynbuf.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static bool_t
DynXdrPutInt32(XDR *xdrs,                       // IN/OUT
               DYNXDR_CONST int32_t *ip)        // IN
{
   int32_t out = htonl(*ip);
   DynXdrData *priv = (DynXdrData *) xdrs->x_private;
   return DynBuf_Append(&priv->data, &out, sizeof out);
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * DynXdrPutLong --
 *
 *    Writes a 32-bit int to the XDR stream.
 *
 * Results:
 *    TRUE: all ok
 *    FALSE: failed to add data do dynbuf.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static bool_t
DynXdrPutLong(XDR *xdrs,                    // IN/OUT
              DYNXDR_CONST DYNXDR_LONG *lp) // IN
{
   int32 out;
   DynXdrData *priv = (DynXdrData *) xdrs->x_private;

#ifdef __APPLE__
   ASSERT_ON_COMPILE(sizeof *lp <= sizeof (int32));
#endif
   out = htonl((int32)*lp);
   return DynBuf_Append(&priv->data, &out, sizeof out);
}


/*
 *-----------------------------------------------------------------------------
 *
 * DynXdrInline --
 *
 *    Return a pointer to a contiguous buffer of len bytes.  On XDR_ENCODE,
 *    is used to preallocate chunks of the backing buffer such that the caller
 *    may set bulk 4-byte members w/o reallocating each time.
 *
 * Results:
 *    Valid pointer on success, NULL on failure.
 *
 * Side effects:
 *    Backing DynBuf may be enlarged.
 *
 *-----------------------------------------------------------------------------
 */

static DYNXDR_INLINE_T *
DynXdrInline(XDR *xdrs,                 // IN/OUT
             DYNXDR_INLINE_LEN_T len)   // IN
{
   DynXdrData *priv = (DynXdrData *)xdrs->x_private;
   DynBuf *buf = &priv->data;
   DYNXDR_INLINE_T *retAddr;

   ASSERT(len >= 0);
   ASSERT(xdrs->x_op == XDR_ENCODE);

   if (len == 0) {
      return (DYNXDR_INLINE_T *)&buf->data[buf->size];
   }

   if (buf->allocated - buf->size < len) {
      /* DynBuf too small.  Grow it. */
      if (!DynBuf_Enlarge(buf, buf->size + len)) {
         return NULL;
      }
   }

   retAddr = (DYNXDR_INLINE_T *)&buf->data[buf->size];
   buf->size += len;

   return retAddr;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DynXdr_Create --
 *
 *    Creates a new XDR struct backed by a DynBuf. The XDR stream is created
 *    in XDR_ENCODE mode. The "in" argument is optional - if NULL, a new XDR
 *    structure will be allocated.
 *
 * Results:
 *    The XDR struct, or NULL on failure.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

XDR *
DynXdr_Create(XDR *in)  // IN
{
   static struct xdr_ops dynXdrOps = {
      /*
       * Yes, these macros are a little redundant, but I figure it helps with
       * readability to group the sun/_KERNEL bits together.
       */
#if !defined(sun) || (defined(sun) && !defined(_KERNEL))
      NULL,             /* x_getlong */
      DynXdrPutLong,    /* x_putlong */
#endif
      NULL,             /* x_getbytes */
      DynXdrPutBytes,   /* x_putbytes */
      DynXdrGetPos,     /* x_getpostn */
      DynXdrSetPos,     /* x_setpostn */
      DynXdrInline,     /* x_inline */
      NULL,             /* x_destroy */
#if defined(__APPLE__) || defined(USE_TIRPC)
      NULL,             /* x_control */
#elif defined(__GLIBC__)
      NULL,             /* x_getint32 */
      DynXdrPutInt32,   /* x_putint32 */
#elif defined(sun) && (defined(_LP64) || defined(_KERNEL))
      NULL,             /* x_control */
      NULL,             /* x_getint32 */
      DynXdrPutInt32,   /* x_putint32 */
#endif
   };

   XDR *ret;
   DynXdrData *priv;

   if (in == NULL) {
      ret = malloc(sizeof *ret);
      if (ret == NULL) {
         goto error;
      }
   } else {
      ret = in;
   }

   priv = malloc(sizeof *priv);
   if (priv == NULL) {
      goto error;
   }

   priv->freeMe = (in == NULL);
   DynBuf_Init(&priv->data);

   ret->x_op = XDR_ENCODE;
   ret->x_public = NULL;
   ret->x_private = (char *) priv;
   ret->x_base = 0;
   ret->x_ops = &dynXdrOps;

   return ret;

error:
   if (in == NULL) {
      free(ret);
   }
   return NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DynXdr_AppendRaw --
 *
 *    Appends some raw bytes to the XDR stream's internal dynbuf. This is
 *    useful when non-XDR data may need to be added to the buffer, avoiding
 *    the need to create another buffer and copying the existing data.
 *
 * Results:
 *    Whether appending the data was successful.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */


Bool
DynXdr_AppendRaw(XDR *xdrs,         // IN
                 const void *buf,   // IN
                 size_t len)        // IN
{
   DynBuf *intbuf = &((DynXdrData *) xdrs->x_private)->data;
   return DynBuf_Append(intbuf, buf, len);
}


/*
 *-----------------------------------------------------------------------------
 *
 * DynXdr_GetData --
 *
 *    Returns a copy of the current data in the XDR buffer. Caller is
 *    responsible for freeing the data.
 *
 * Results:
 *    The current data in the buffer, or NULL if there's no data, or failed
 *    to allocate data.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

void *
DynXdr_AllocGet(XDR *xdrs) // IN
{
   DynBuf *buf = &((DynXdrData *) xdrs->x_private)->data;
   return Util_Memdup(DynBuf_Get(buf), DynBuf_GetSize(buf));
}


/*
 *-----------------------------------------------------------------------------
 *
 * DynXdr_Get --
 *
 *    Returns the current data in the XDR buffer.
 *
 * Results:
 *    The current data in the buffer, or NULL if there's no data.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

void *
DynXdr_Get(XDR *xdrs)  // IN
{
   DynBuf *buf = &((DynXdrData *) xdrs->x_private)->data;
   return DynBuf_Get(buf);
}


/*
 *-----------------------------------------------------------------------------
 *
 * DynXdr_Destroy --
 *
 *    Frees data in the XDR stream, optionally destroying the underlying
 *    DynBuf (if "release" is TRUE). If the XDR stream was dynamically
 *    allocated by DynXdr_Create(), it will be freed.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

void
DynXdr_Destroy(XDR *xdrs,     // IN
               Bool release)  // IN
{
   if (xdrs) {
      DynXdrData *priv = (DynXdrData *) xdrs->x_private;
      if (release) {
         DynBuf_Destroy(&priv->data);
      }
      if (priv->freeMe) {
         free(xdrs);
      }
      free(priv);
   }
}

