/*********************************************************
 * Copyright (C) 2007-2020 VMware, Inc. All rights reserved.
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
#include <glib.h>

#if !defined(_WIN32)
#include <unistd.h>
#endif

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
 *  Start of main program.  Check if we are in a VM, by reading
 *  a backdoor port.  Then process any other commands.
 */
int
main(int argc,
     char *argv[])
{
   uint32 version[2];
   gchar *gAppName;
   GError *gErr = NULL;
   gboolean product = FALSE;
   int success = 1;

   GOptionEntry options[] = {
      {"prod", 'p', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &product,
       "print VMware hypervisor product.", NULL},
      {NULL}
   };
   GOptionContext *optCtx;

#if defined(_WIN32)
   WinUtil_EnableSafePathSearching(TRUE);
#endif

   /*
    * set up glib options context.
    */
   gAppName = g_path_get_basename(argv[0]);

   g_set_prgname(gAppName);
   optCtx = g_option_context_new(NULL);
   g_option_context_add_main_entries(optCtx, options, NULL);

   if (!VmCheck_IsVirtualWorld()) {
      g_printerr("Error: %s must be run inside a virtual machine"
                 " on a VMware hypervisor product.\n", gAppName);
      goto out;
   }

   if (!VmCheck_GetVersion(&version[0], &version[1])) {
      g_printerr("%s: Couldn't get version\n", gAppName);
      goto out;
   }

   if (!g_option_context_parse(optCtx, &argc, &argv, &gErr)) {
      g_printerr("%s: %s\n", gAppName, gErr->message);
      g_error_free(gErr);
      goto out;
   }

   /*
    * product is true if 'p' option was passed to parser
    */
   if (product) {
      switch (version[1]) {
      case VMX_TYPE_SCALABLE_SERVER:
         g_print("ESX Server\n");
         break;

      case VMX_TYPE_WORKSTATION:
         g_print("Workstation\n");
         break;

      default:
         g_print("Unknown\n");
         break;
      }
      success = 0;
      goto out;
   }

   g_print("%s version %d (good)\n", PRODUCT_LINE_NAME, version[0]);
   success = 0;

out:
   g_option_context_free(optCtx);
   g_free(gAppName);
   return success;
}
