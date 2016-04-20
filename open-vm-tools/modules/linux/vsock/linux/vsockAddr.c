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
 * vsockAddr.c --
 *
 *    VSockets address implementation.
 */

/*
 * These includes come before vsockCommon.h to ensure that VMware's ASSERT
 * macro is used instead of Linux's irda.h definition.
 */
#if defined(__linux__) && !defined(VMKERNEL)
#  if defined(__KERNEL__)
#    include "driver-config.h"
#    include <linux/socket.h>
#    include "compat_sock.h"
#  else
#    include <string.h>
#    include <errno.h>
#  endif
#elif defined(VMKERNEL)
# include "vm_libc.h"
# include "return_status.h"
#elif defined(__APPLE__)
# include <sys/errno.h>
#endif

#include "vsockCommon.h"


/*
 *-----------------------------------------------------------------------------
 *
 * VSockAddr_Init --
 *
 *      Initialize the given address with the given context id and port. This
 *      will clear the address, set the correct family, and add the given
 *      values.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
VSockAddr_Init(struct sockaddr_vm *addr, // OUT
               uint32 cid,               // IN
               uint32 port)              // IN
{
   ASSERT(addr);
   VSockAddr_InitNoFamily(addr, cid, port);
   addr->svm_family = VMCISockGetAFValueInt();
   VSOCK_ADDR_ASSERT(addr);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VSockAddr_InitNoFamily --
 *
 *      Initialize the given address with the given context id and port. This
 *      will clear the address and add the given values, but not set the
 *      family.  Note that this is needed because in some places we don't want
 *      to re-register the address family in the Linux kernel and all we need
 *      is to check the context id and port.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
VSockAddr_InitNoFamily(struct sockaddr_vm *addr, // OUT
                       uint32 cid,               // IN
                       uint32 port)              // IN
{
   ASSERT(addr);

   memset(addr, 0, sizeof *addr);
#if defined(__APPLE__)
   addr->svm_len = sizeof *addr;
#endif
   addr->svm_cid = cid;
   addr->svm_port = port;
   VSOCK_ADDR_NOFAMILY_ASSERT(addr);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VSockAddr_Validate --
 *
 *      Try to validate the given address.  The address must not be null and
 *      must have the correct address family.  Any reserved fields must be
 *      zero.
 *
 * Results:
 *      0 on success, EFAULT if the address is null, EAFNOSUPPORT if the
 *      address is of the wrong family, and EINVAL if the reserved fields are
 *      not zero.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int32
VSockAddr_Validate(const struct sockaddr_vm *addr) // IN
{
   int32 err;

   if (NULL == addr) {
      err = EFAULT;
      goto exit;
   }

   if (VMCISockGetAFValueInt() != addr->svm_family) {
      err = EAFNOSUPPORT;
      goto exit;
   }

   if (0 != addr->svm_zero[0]) {
      err = EINVAL;
      goto exit;
   }

   err = 0;

exit:
   return sockerr2err(err);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VSockAddr_ValidateNoFamily --
 *
 *      Try to validate the given address.  The address must not be null and
 *      any reserved fields must be zero, but the address family is not
 *      checked.  Note that this is needed because in some places we don't want
 *      to re-register the address family with the Linux kernel.
 *
 *      Also note that we duplicate the code from _Validate() since we want to
 *      retain the ordering or the error return values.
 *
 * Results:
 *      0 on success, EFAULT if the address is null and EINVAL if the reserved
 *      fields are not zero.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int32
VSockAddr_ValidateNoFamily(const struct sockaddr_vm *addr) // IN
{
   int32 err;

   if (NULL == addr) {
      err = EFAULT;
      goto exit;
   }

   if (0 != addr->svm_zero[0]) {
      err = EINVAL;
      goto exit;
   }

   err = 0;

exit:
   return sockerr2err(err);
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockAddr_Bound --
 *
 *    Determines whether the provided address is bound.
 *
 * Results:
 *    TRUE if the address structure is bound, FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

Bool
VSockAddr_Bound(struct sockaddr_vm *addr) // IN: socket address to check
{
   ASSERT(addr);
   return addr->svm_port != VMADDR_PORT_ANY;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockAddr_Unbind --
 *
 *    Unbind the given addresss.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

void
VSockAddr_Unbind(struct sockaddr_vm *addr) // IN
{
   VSockAddr_Init(addr, VMADDR_CID_ANY, VMADDR_PORT_ANY);
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockAddr_EqualsAddr --
 *
 *    Determine if the given addresses are equal.
 *
 * Results:
 *    TRUE if the addresses are equal, FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

Bool
VSockAddr_EqualsAddr(struct sockaddr_vm *addr,  // IN
                     struct sockaddr_vm *other) // IN
{
   /*
    * XXX We don't ASSERT on the family here since this is used on the receive
    * path in Linux and we don't want to re-register the address family
    * unnecessarily.
    */
   VSOCK_ADDR_NOFAMILY_ASSERT(addr);
   VSOCK_ADDR_NOFAMILY_ASSERT(other);
   return (addr->svm_cid == other->svm_cid &&
           addr->svm_port == other->svm_port);
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockAddr_EqualsHandlePort --
 *
 *    Determines if the given address matches the given handle and port.
 *
 * Results:
 *    TRUE if the address matches the handle and port, FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

Bool
VSockAddr_EqualsHandlePort(struct sockaddr_vm *addr, // IN
                           VMCIHandle handle,        // IN
                           uint32 port)              // IN
{
   VSOCK_ADDR_ASSERT(addr);
   return (addr->svm_cid == VMCI_HANDLE_TO_CONTEXT_ID(handle) &&
           addr->svm_port == port);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VSockAddr_Cast --
 *
 *      Try to cast the given generic address to a VM address.  The given
 *      length must match that of a VM address and the address must be valid.
 *      The "outAddr" parameter contains the address if successful.
 *
 * Results:
 *      0 on success, EFAULT if the length is too small.  See
 *      VSockAddr_Validate() for other possible return codes.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int32
VSockAddr_Cast(const struct sockaddr *addr,  // IN
               int32 len,                    // IN
               struct sockaddr_vm **outAddr) // OUT
{
   int32 err;

   ASSERT(outAddr);

   if (len < sizeof **outAddr) {
      err = EFAULT;
      goto exit;
   }

   *outAddr = (struct sockaddr_vm *) addr;
   err = VSockAddr_Validate(*outAddr);

exit:
   return sockerr2err(err);
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockAddr_SocketContextStream --
 *
 *      Determines whether the provided context id represents a context that
 *      contains a stream socket endpoints.
 *
 * Results:
 *      TRUE if the context does have socket endpoints, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

Bool
VSockAddr_SocketContextStream(uint32 cid)  // IN
{
   uint32 i;
   VMCIId nonSocketContexts[] = {
      VMCI_WELL_KNOWN_CONTEXT_ID,
   };

   ASSERT_ON_COMPILE(sizeof cid == sizeof *nonSocketContexts);

   for (i = 0; i < ARRAYSIZE(nonSocketContexts); i++) {
      if (cid == nonSocketContexts[i]) {
         return FALSE;
      }
   }

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockAddr_SocketContextDgram --
 *
 *      Determines whether the provided <context id, resource id> represent
 *      a protected datagram endpoint.
 *
 * Results:
 *      TRUE if the context does have socket endpoints, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

Bool
VSockAddr_SocketContextDgram(uint32 cid,  // IN
                             uint32 rid)  // IN
{
   if (cid == VMCI_HYPERVISOR_CONTEXT_ID) {
      /*
       * Registrations of PBRPC Servers do not modify VMX/Hypervisor state and
       * are allowed.
       */
      if (rid == VMCI_UNITY_PBRPC_REGISTER) {
         return TRUE;
      } else {
         return FALSE;
      }
   }

   return TRUE;
}
