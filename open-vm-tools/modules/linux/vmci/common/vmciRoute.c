/*********************************************************
 * Copyright (C) 2011-2012,2014 VMware, Inc. All rights reserved.
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
 * vmciRoute.c --
 *
 *     Implementation of VMCI routing rules.
 */

#include "vmci_kernel_if.h"
#include "vm_assert.h"
#include "vmci_defs.h"
#include "vmci_infrastructure.h"
#include "vmciCommonInt.h"
#include "vmciContext.h"
#include "vmciDriver.h"
#include "vmciKernelAPI.h"
#include "vmciRoute.h"
#if defined(VMKERNEL)
#  include "vmciVmkInt.h"
#  include "vm_libc.h"
#  include "helper_ext.h"
#endif

#define LGPFX "VMCIRoute: "


/*
 *------------------------------------------------------------------------------
 *
 *  VMCI_Route --
 *
 *     Make a routing decision for the given source and destination handles.
 *     This will try to determine the route using the handles and the available
 *     devices.
 *
 *  Result:
 *     A VMCIRoute value.
 *
 *  Side effects:
 *     Sets the source context if it is invalid.
 *
 *------------------------------------------------------------------------------
 */

int
VMCI_Route(VMCIHandle *src,       // IN/OUT
           const VMCIHandle *dst, // IN
           Bool fromGuest,        // IN
           VMCIRoute *route)      // OUT
{
   Bool hasHostDevice;
   Bool hasGuestDevice;

   ASSERT(src);
   ASSERT(dst);
   ASSERT(route);

   *route = VMCI_ROUTE_NONE;

   /*
    * "fromGuest" is only ever set to TRUE by IOCTL_VMCI_DATAGRAM_SEND (or by
    * the vmkernel equivalent), which comes from the VMX, so we know it is
    * coming from a guest.
    */

   /*
    * To avoid inconsistencies, test these once.  We will test them again
    * when we do the actual send to ensure that we do not touch a non-existent
    * device.
    */

   hasHostDevice = VMCI_HostPersonalityActive();
   hasGuestDevice = VMCI_GuestPersonalityActive();

   /* Must have a valid destination context. */
   if (VMCI_INVALID_ID == dst->context) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   /* Anywhere to hypervisor. */
   if (VMCI_HYPERVISOR_CONTEXT_ID == dst->context) {
      /*
       * If this message already came from a guest then we cannot send it
       * to the hypervisor.  It must come from a local client.
       */

      if (fromGuest) {
         return VMCI_ERROR_DST_UNREACHABLE;
      }

      /* We must be acting as a guest in order to send to the hypervisor. */
      if (!hasGuestDevice) {
         return VMCI_ERROR_DEVICE_NOT_FOUND;
      }

      /* And we cannot send if the source is the host context. */
      if (VMCI_HOST_CONTEXT_ID == src->context) {
         return VMCI_ERROR_INVALID_ARGS;
      }

      /* Send from local client down to the hypervisor. */
      *route = VMCI_ROUTE_AS_GUEST;
      return VMCI_SUCCESS;
   }

   /* Anywhere to local client on host. */
   if (VMCI_HOST_CONTEXT_ID == dst->context) {
      /*
       * If it is not from a guest but we are acting as a guest, then we need
       * to send it down to the host.  Note that if we are also acting as a
       * host then this will prevent us from sending from local client to
       * local client, but we accept that restriction as a way to remove
       * any ambiguity from the host context.
       */

      if (src->context == VMCI_HYPERVISOR_CONTEXT_ID) {
         /*
          * If the hypervisor is the source, this is host local
          * communication. The hypervisor may send vmci event
          * datagrams to the host itself, but it will never send
          * datagrams to an "outer host" through the guest device.
          */

         if (hasHostDevice) {
            *route = VMCI_ROUTE_AS_HOST;
            return VMCI_SUCCESS;
         } else {
            return VMCI_ERROR_DEVICE_NOT_FOUND;
         }
      }

      if (!fromGuest && hasGuestDevice) {
         /* If no source context then use the current. */
         if (VMCI_INVALID_ID == src->context) {
            src->context = vmci_get_context_id();
         }

         /* Send it from local client down to the host. */
         *route = VMCI_ROUTE_AS_GUEST;
         return VMCI_SUCCESS;
      }

      /*
       * Otherwise we already received it from a guest and it is destined
       * for a local client on this host, or it is from another local client
       * on this host.  We must be acting as a host to service it.
       */

      if (!hasHostDevice) {
         return VMCI_ERROR_DEVICE_NOT_FOUND;
      }

      if (VMCI_INVALID_ID == src->context) {
         /*
          * If it came from a guest then it must have a valid context.
          * Otherwise we can use the host context.
          */

         if (fromGuest) {
            return VMCI_ERROR_INVALID_ARGS;
         }
         src->context = VMCI_HOST_CONTEXT_ID;
      }

      /* Route to local client. */
      *route = VMCI_ROUTE_AS_HOST;
      return VMCI_SUCCESS;
   }

   /* If we are acting as a host then this might be destined for a guest. */
   if (hasHostDevice) {
      /* It will have a context if it is meant for a guest. */
      if (VMCIContext_Exists(dst->context)) {
         if (VMCI_INVALID_ID == src->context) {
            /*
             * If it came from a guest then it must have a valid context.
             * Otherwise we can use the host context.
             */

            if (fromGuest) {
               return VMCI_ERROR_INVALID_ARGS;
            }
            src->context = VMCI_HOST_CONTEXT_ID;
         } else if (VMCI_CONTEXT_IS_VM(src->context) &&
                    src->context != dst->context) {
            /*
             * VM to VM communication is not allowed. Since we catch
             * all communication destined for the host above, this
             * must be destined for a VM since there is a valid
             * context.
             */

            ASSERT(VMCI_CONTEXT_IS_VM(dst->context));

            return VMCI_ERROR_DST_UNREACHABLE;
         }

         /* Pass it up to the guest. */
         *route = VMCI_ROUTE_AS_HOST;
         return VMCI_SUCCESS;
      } else if (!hasGuestDevice) {
         /*
          * The host is attempting to reach a CID without an active context, and
          * we can't send it down, since we have no guest device.
          */

         return VMCI_ERROR_DST_UNREACHABLE;
      }
   }

   /*
    * We must be a guest trying to send to another guest, which means
    * we need to send it down to the host. We do not filter out VM to
    * VM communication here, since we want to be able to use the guest
    * driver on older versions that do support VM to VM communication.
    */

   if (!hasGuestDevice) {
      /*
       * Ending up here means we have neither guest nor host device. That
       * shouldn't happen, since any VMCI client in the kernel should have done
       * a successful VMCI_DeviceGet.
       */

      ASSERT(FALSE);

      return VMCI_ERROR_DEVICE_NOT_FOUND;
   }

   /* If no source context then use the current context. */
   if (VMCI_INVALID_ID == src->context) {
      src->context = vmci_get_context_id();
   }

   /*
    * Send it from local client down to the host, which will route it to
    * the other guest for us.
    */

   *route = VMCI_ROUTE_AS_GUEST;
   return VMCI_SUCCESS;
}


/*
 *------------------------------------------------------------------------------
 *
 *  VMCI_RouteString --
 *
 *     Get a string for the given route.
 *
 *  Result:
 *     A string representing the route, if the route is valid, otherwise an
 *     empty string.
 *
 *  Side effects:
 *     None.
 *
 *------------------------------------------------------------------------------
 */

const char *
VMCI_RouteString(VMCIRoute route) // IN
{
   const char *vmciRouteStrings[] = {
      "none",
      "as host",
      "as guest",
   };
   if (route >= VMCI_ROUTE_NONE && route <= VMCI_ROUTE_AS_GUEST) {
      return vmciRouteStrings[route];
   }
   return "";
}
