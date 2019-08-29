/*********************************************************
 * Copyright (C) 2015-2019 VMware, Inc. All rights reserved.
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
 * toolboxcmd-info.c --
 *
 *    Various guest info operations for toolbox-cmd.
 */

#include <time.h>

#include "conf.h"
#include "nicInfo.h"
#include "dynxdr.h"
#include "xdrutil.h"
#include "toolboxCmdInt.h"
#include "vmware/tools/i18n.h"
#include "vmware/tools/utils.h"
#ifdef _WIN32
#include "netutil.h"
#endif

/*
 *-----------------------------------------------------------------------------
 *
 * InfoSendNetworkXdr --
 *
 *      Send network info to the VMX.
 *
 * Results:
 *      Returns TRUE on success.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
InfoSendNetworkXdr(GuestNicProto *message,
                   GuestInfoType type)
{
   Bool status = FALSE;
   XDR xdrs;
   gchar *request;
   char *reply = NULL;
   size_t replyLen;

   /* Add the RPC preamble: message name, and type. */
   request = g_strdup_printf("%s  %d ", GUEST_INFO_COMMAND, type);

   if (DynXdr_Create(&xdrs) == NULL) {
      goto exit;
   }

   /* Write preamble and serialized nic info to XDR stream. */
   if (!DynXdr_AppendRaw(&xdrs, request, strlen(request)) ||
       !xdr_GuestNicProto(&xdrs, message)) {
      g_warning("Error serializing nic info v%d data.", message->ver);
   } else {
      status = ToolsCmd_SendRPC(DynXdr_Get(&xdrs), xdr_getpos(&xdrs),
                                &reply, &replyLen);
      if (!status) {
         g_warning("%s: update failed: request \"%s\", reply \"%s\".\n",
                    __FUNCTION__, request, reply);
      }
      ToolsCmd_FreeRPC(reply);
   }
   DynXdr_Destroy(&xdrs, TRUE);

exit:
   g_free(request);
   return status;
}


/*
 *-----------------------------------------------------------------------------
 *
 * InfoUpdateNetwork --
 *
 *      Update network info.
 *
 * Results:
 *      Returns EXIT_SUCCESS on success.
 *      Returns the appropriate exit codes on errors.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
InfoUpdateNetwork(void)
{
   int ret = EXIT_SUCCESS;
   NicInfoV3 *info = NULL;
   GuestNicProto msg = { 0 };
   GuestInfoType type = INFO_IPADDRESS_V3;

   GKeyFile *confDict = NULL;
   int maxIPv4RoutesToGather = NICINFO_MAX_ROUTES;
   int maxIPv6RoutesToGather = NICINFO_MAX_ROUTES;

#ifdef _WIN32
   DWORD dwRet = NetUtil_LoadIpHlpApiDll();
   if (dwRet != ERROR_SUCCESS) {
      g_warning("NetUtil_LoadIpHlpApiDll() failed.\n");
      return EXIT_FAILURE;
   }
#endif

   // Get the config options of max IPv4/IPv6 routes to gather
   VMTools_LoadConfig(NULL, G_KEY_FILE_NONE, &confDict, NULL);

   if (confDict != NULL) {
      maxIPv4RoutesToGather =
               VMTools_ConfigGetInteger(confDict,
                                        CONFGROUPNAME_GUESTINFO,
                                        CONFNAME_GUESTINFO_MAXIPV4ROUTES,
                                        NICINFO_MAX_ROUTES);
      if (maxIPv4RoutesToGather < 0 ||
          maxIPv4RoutesToGather > NICINFO_MAX_ROUTES) {
         g_warning("Invalid %s.%s value: %d. Using default %u.\n",
                   CONFGROUPNAME_GUESTINFO,
                   CONFNAME_GUESTINFO_MAXIPV4ROUTES,
                   maxIPv4RoutesToGather,
                   NICINFO_MAX_ROUTES);
         maxIPv4RoutesToGather = NICINFO_MAX_ROUTES;
      }

      maxIPv6RoutesToGather =
               VMTools_ConfigGetInteger(confDict,
                                        CONFGROUPNAME_GUESTINFO,
                                        CONFNAME_GUESTINFO_MAXIPV6ROUTES,
                                        NICINFO_MAX_ROUTES);
      if (maxIPv6RoutesToGather < 0 ||
          maxIPv6RoutesToGather > NICINFO_MAX_ROUTES) {
         g_warning("Invalid %s.%s value: %d. Using default %u.\n",
                   CONFGROUPNAME_GUESTINFO,
                   CONFNAME_GUESTINFO_MAXIPV6ROUTES,
                   maxIPv6RoutesToGather,
                   NICINFO_MAX_ROUTES);
         maxIPv6RoutesToGather = NICINFO_MAX_ROUTES;
      }
      g_key_file_free(confDict);
   }

   if (!GuestInfo_GetNicInfo(maxIPv4RoutesToGather,
                             maxIPv6RoutesToGather,
                             &info)) {
      g_warning("Failed to get nic info.\n");
      ret = EXIT_FAILURE;
      goto done;
   }

   // Only useful for VMXs that support V3
   msg.ver = NIC_INFO_V3;
   msg.GuestNicProto_u.nicInfoV3 = info;
   if (!InfoSendNetworkXdr(&msg, type)) {
      ret = EXIT_FAILURE;
   }

   GuestInfo_FreeNicInfo(info);

done:
#ifdef _WIN32
   NetUtil_FreeIpHlpApiDll();
#endif

   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Info_Command --
 *
 *      Handle and parse info commands.
 *
 * Results:
 *      Returns EXIT_SUCCESS on success.
 *      Returns the appropriate exit codes on errors.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
Info_Command(char **argv,      // IN: Command line arguments
             int argc,         // IN: Length of command line arguments
             gboolean quiet)   // IN
{
   char *subcommand;
   char *class;

   // subcommand: 'update'
   if ((optind) >= argc) {
      ToolsCmd_MissingEntityError(argv[0],
                                  SU_(arg.info.subcommand, "info operation"));
      return EX_USAGE;
   }
   subcommand = argv[optind];

   // info class: 'network'
   if ((optind + 1) >= argc) {
      ToolsCmd_MissingEntityError(argv[0],
                                  SU_(arg.info.class, "info infoclass"));
      return EX_USAGE;
   }

   class = argv[optind + 1];

   if (toolbox_strcmp(subcommand, "update") == 0) {
      if (toolbox_strcmp(class, "network") == 0) {
         return InfoUpdateNetwork();
      } else {
         ToolsCmd_UnknownEntityError(argv[0],
                                     SU_(arg.info.class, "info infoclass"),
                                     class);
         return EX_USAGE;
      }
   } else {
      ToolsCmd_UnknownEntityError(argv[0],
                                  SU_(arg.subcommand, "subcommand"),
                                  subcommand);
      return EX_USAGE;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * Info_Help --
 *
 *      Prints the help for the info command.
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
Info_Help(const char *progName, // IN: The name of the program obtained from argv[0]
          const char *cmd)      // IN
{
   g_print(SU_(help.info,
               "%s: update guest information on the host\n"
               "Usage: %s %s update <infoclass>\n\n"
               "Subcommands:\n"
               "   update <infoclass>: update information identified by <infoclass>\n"
               "<infoclass> can be 'network'\n"),
           cmd, progName, cmd);
}

