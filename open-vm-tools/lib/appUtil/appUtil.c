/*********************************************************
 * Copyright (C) 2008-2019 VMware, Inc. All rights reserved.
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
 * appUtil.c --
 *
 *    Utility functions for guest applications.
 */


#include <stdlib.h>
#include <string.h>

#include "appUtil.h"
#include "debug.h"
#include "rpcout.h"
#include "str.h"


/*
 *----------------------------------------------------------------------------
 *
 * AppUtil_SendGuestCaps --
 *
 *     Send a list of guest capabilities to the host.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------------
 */

void
AppUtil_SendGuestCaps(const GuestCapabilities *caps, // IN: array of capabilities
                      size_t numCaps,                // IN: number of capabilities
                      Bool enabled)                  // IN: capabilities status
{
   char *capsStr;
   size_t capIdx;

   ASSERT(caps);
   ASSERT(numCaps > 0);

   capsStr = strdup(GUEST_CAP_FEATURES);
   for (capIdx = 0; capIdx < numCaps; capIdx++) {
      char *capsTemp;
      if (!capsStr) {
         Debug("%s: Not enough memory to create capabilities string\n", __FUNCTION__);
         return;
      }
      capsTemp = Str_Asprintf(NULL,
                              "%s %d=%d",
                              capsStr,
                              caps[capIdx],
                              (int)enabled);
      free(capsStr);
      capsStr = capsTemp;
   }

   if (!RpcOut_sendOne(NULL, NULL, capsStr)) {
      Debug("%s: could not set capabilities: older vmx?\n", __FUNCTION__);
   }

   free(capsStr);
}
