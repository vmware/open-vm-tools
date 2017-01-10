/*********************************************************
 * Copyright (C) 2014-2016 VMware, Inc. All rights reserved.
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

/**
 * @file util.c
 *
 * Random GuestInfo type utilities.
 */

#include <string.h>

#include "vmware.h"
#include "nicInfoInt.h"
#include "xdrutil.h"


/*
 ******************************************************************************
 * GuestInfo_Util_FindNicByMac --                                        */ /**
 *
 * Searches a NIC list for the NIC identified by a MAC address.
 *
 * @param[in]   nicInfo NIC container.
 * @param[in]   mac     MAC address.
 *
 * @retval GuestNicV3* if found.
 * @retval NULL if not found.
 *
 ******************************************************************************
 */

GuestNicV3 *
GuestInfoUtilFindNicByMac(const NicInfoV3 *nicInfo,
                          const char *macAddress)
{
   u_int i;

   ASSERT(nicInfo);
   ASSERT(macAddress);

   XDRUTIL_FOREACH(i, nicInfo, nics) {
      GuestNicV3 *nic;
      nic = XDRUTIL_GETITEM(nicInfo, nics, i);
      if (strcasecmp(nic->macAddress, macAddress) == 0) {
         return nic;
      }
   }

   return NULL;
}
