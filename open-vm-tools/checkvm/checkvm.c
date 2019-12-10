/*********************************************************
 * Copyright (C) 2007-2019 VMware, Inc. All rights reserved.
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
 * checkvm.c --
 *
 *      Check if we are running in a VM or not
 */

#include <stdlib.h>
#include <stdio.h>

#if !defined(_WIN32)
#include <unistd.h>
#endif

#include "vm_version.h"
#include "backdoor.h"
#include "backdoor_def.h"
#include "vm_basic_types.h"
#include "vmcheck.h"
#if defined(_WIN32)
#include "getoptwin32.h"
#include "vmware/tools/win32util.h"
#endif

#include "checkvm_version.h"
#include "vm_version.h"
#include "embed_version.h"
VM_EMBED_VERSION(CHECKVM_VERSION_STRING);


/*
 *  getHWVersion  -  Read VM HW version through backdoor
 */
void
getHWVersion(uint32 *hwVersion)
{
   Backdoor_proto bp;
   bp.in.cx.halfs.low = BDOOR_CMD_GETHWVERSION;
   Backdoor(&bp);
   *hwVersion = bp.out.ax.word;
}


/*
 *  getScreenSize  -  Get screen size of the host
 */
void
getScreenSize(uint32 *screensize)
{
   Backdoor_proto bp;
   bp.in.cx.halfs.low = BDOOR_CMD_GETSCREENSIZE;
   Backdoor(&bp);
   *screensize = bp.out.ax.word;
}


/*
 *  Start of main program.  Check if we are in a VM, by reading
 *  a backdoor port.  Then process any other commands.
 */
int
main(int argc,
     char *argv[])
{
   uint32 version[2];
   int opt;
   int width, height;
   uint32 screensize = 0;
   uint32 hwVersion;

#if defined(_WIN32)
   WinUtil_EnableSafePathSearching(TRUE);
#endif

   if (!VmCheck_IsVirtualWorld()) {
      fprintf(stdout, "Not running in a virtual machine.\n");
      return 1;
   }

   if (!VmCheck_GetVersion(&version[0], &version[1])) {
      fprintf(stdout, "Couldn't get version\n");
      return 1;
   }

   /*
    *  OK, we're in a VM, check if there are any other requests
    */
   while ((opt = getopt(argc, argv, "rph")) != EOF) {
      switch (opt) {
      case 'r':
         getScreenSize(&screensize);
         width = (screensize >> 16) & 0xffff;
         height = screensize & 0xffff;
         if ((width <= 0x7fff) && (height <= 0x7fff)) {
            printf("%d %d\n", width, height);
         } else {
            printf("0 0\n");
         }
         return 0;

      case 'p':
         /*
         * Print out product that we're running on based on code
         * obtained from getVersion().
         */
         switch (version[1]) {
         case VMX_TYPE_SCALABLE_SERVER:
            printf("ESX Server\n");
            break;

         case VMX_TYPE_WORKSTATION:
            printf("Workstation\n");
            break;

         default:
            printf("Unknown\n");
            break;
         }
         return 0;

      case 'h':
         getHWVersion(&hwVersion);
         printf("VM's hw version is %u\n", hwVersion);
         break;

      default:
         break;
      }
   }

   printf("%s version %d (good)\n", PRODUCT_LINE_NAME, version[0]);
   return 0;
}
