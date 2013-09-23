/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 *********************************************************/

/*
 * vsockVmci.h --
 *
 *    VSockets VMCI constants, types and functions.
 */


#ifndef _VSOCK_VMCI_H_
#define _VSOCK_VMCI_H_


extern VMCIId VMCI_GetContextID(void);


/*
 *-----------------------------------------------------------------------------
 *
 * VSockVmci_IsLocal --
 *
 *      Determine if the given handle points to the local context.
 *
 * Results:
 *      TRUE if the given handle is for the local context, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Bool
VSockVmci_IsLocal(VMCIHandle handle) // IN
{
   return VMCI_GetContextID() == VMCI_HANDLE_TO_CONTEXT_ID(handle);
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmci_ErrorToVSockError --
 *
 *      Converts from a VMCI error code to a VSock error code.
 *
 * Results:
 *      Appropriate error code.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static INLINE int32
VSockVmci_ErrorToVSockError(int32 vmciError) // IN
{
   int32 err;
   switch (vmciError) {
   case VMCI_ERROR_NO_MEM:
#if defined(_WIN32)
      err = ENOBUFS;
#else // _WIN32
      err = ENOMEM;
#endif // _WIN32
      break;
   case VMCI_ERROR_DUPLICATE_ENTRY:
      err = EADDRINUSE;
      break;
   case VMCI_ERROR_NO_ACCESS:
      err = EPERM;
      break;
   case VMCI_ERROR_NO_RESOURCES:
      err = ENOBUFS;
      break;
   case VMCI_ERROR_INVALID_RESOURCE:
      err = EHOSTUNREACH;
      break;
   case VMCI_ERROR_MODULE_NOT_LOADED:
      err = ESYSNOTREADY;
      break;
   case VMCI_ERROR_NO_HANDLE:
      err = ENETUNREACH;
      break;
   case VMCI_ERROR_INVALID_ARGS:
   default:
      err = EINVAL;
   }

   return sockerr2err(err);
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmci_GetVmciObjSocket --
 *
 *      Get a socket from a VMCI object, but only if the object is of the
 *      appropriate type.
 *
 * Results:
 *      A socket if the object is of the correct type, NULL otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static INLINE void *
VSockVmci_GetVmciObjSocket(VMCIObj *obj) // IN
{
   ASSERT(obj);
   if (NULL != obj->ptr && VMCIOBJ_SOCKET == obj->type) {
      return obj->ptr;
   }
   return NULL;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmci_SetVmciObjSocket --
 *
 *      Set the socket in a VMCI object.  This will also set the type
 *      accordingly.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static INLINE void
VSockVmci_SetVmciObjSocket(VMCIObj *obj, // OUT
                           void *s)      // IN
{
   ASSERT(obj);
   ASSERT(s);
   obj->ptr = s;
   obj->type = VMCIOBJ_SOCKET;
}


#endif // _VSOCK_VMCI_H_

