/*********************************************************
 * Copyright (c) 2019-2020 VMware, Inc. All rights reserved.
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
 * toolboxcmd-gueststore.c --
 *
 *     GuestStore getcontent operation for toolbox-cmd.
 */


#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#include <dlfcn.h>
#endif

#include "vm_assert.h"
#include "vm_basic_defs.h"
#include "toolboxCmdInt.h"
#include "vmware/tools/i18n.h"
#include "guestStoreClient.h"


/*
 * GuestStore client library error messages
 *
 * Localization can be done in this way if needed:
 *
 * #define GUESTSTORE_LIB_ERR_ITEM(a, b, c) MSGID(b) c,
 *
 * call this to get localized error message:
 *
 * VMTools_GetString(VMW_TEXT_DOMAIN, guestStoreLibErrMsgs[errCode])
 */
#define GUESTSTORE_LIB_ERR_ITEM(a, b, c) c,
static const char * const guestStoreLibErrMsgs[] = {
GUESTSTORE_LIB_ERR_LIST
};
#undef GUESTSTORE_LIB_ERR_ITEM

/*
 * Passed in from main(), command line --quiet (q) option.
 */
static gboolean gQuiet = FALSE;


/*
 *-----------------------------------------------------------------------------
 *
 * GuestStoreReportProgress --
 *
 *      Report progress from GuestStore client library.
 *
 * Results:
 *      TRUE to continue getting content.
 *      FALSE to cancel getting content.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
GuestStoreReportProgress(int64 fileSize,       // IN
                         int64 bytesReceived,  // IN
                         void *clientData)     // IN
{
   static Bool first = TRUE;
   static int lastProgress = 0;
   int percentage;
   int progress;
   int i;

   if (gQuiet) {
      return TRUE;
   }

   if (first) {
      g_print("%s", SU_(gueststore.content_size,
                        "Content size in bytes: "));

      g_print("%" FMT64 "d\n", fileSize);

      first = FALSE;
   }

   percentage = (int)((bytesReceived * 100) / fileSize);
   progress = percentage / 5;  // increment by every 5%
   if (progress == lastProgress) {
      return TRUE;
   }

   lastProgress = progress;

   g_print(SU_(gueststore.progress,
               "\rProgress: %d%%"),
           percentage);
   g_print(" [");

   for (i = 0; i < progress; i++) {
      putchar('=');
   }

   g_print(">%*c", 20 - i, ']');
   fflush(stdout);

   if (percentage == 100) {
      g_print("\n");
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestStoreRemovePathEnclosingQuotes --
 *
 *      Remove the beginning and ending quotes in a path.
 *
 * Results:
 *      Path without enclosing quotes.
 *
 * Side effects:
 *      Input path may have been changed.
 *
 *-----------------------------------------------------------------------------
 */

static char *
GuestStoreRemovePathEnclosingQuotes(char *path)  // IN/OUT
{
   char *lastChar;

   if (*path != '"') {
      return path;
   }

   path++;
   lastChar = path + strlen(path) - 1;
   if (*lastChar == '"') {
      *lastChar = '\0';
   }

   return path;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestStore_Command --
 *
 *      Handle and parse gueststore command.
 *
 * Results:
 *      0 on success.
 *
 *      Error code from GuestStore client library or
 *      general process error exit code.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

int
GuestStore_Command(char **argv,      // IN: Command line arguments
                   int argc,         // IN: Length of the command line arguments
                   gboolean quiet)   // IN
{
   int exitCode;
   char *contentPath;
   char *outputPath;

   if (toolbox_strcmp(argv[optind], "getcontent") != 0) {
      ToolsCmd_UnknownEntityError(argv[0],
                                  SU_(arg.subcommand, "subcommand"),
                                  argv[optind]);
      return EX_USAGE;
   }

   if (optind != (argc - 3)) {
      return EX_USAGE;
   }

   gQuiet = quiet;

   if (!GuestStoreClient_Init()) {
      g_critical("GuestStoreClient_Init failed.\n");
      exitCode = EX_SOFTWARE;
      goto exit;
   }

   contentPath = GuestStoreRemovePathEnclosingQuotes(argv[argc - 2]);
   outputPath = GuestStoreRemovePathEnclosingQuotes(argv[argc - 1]);

   exitCode = GuestStoreClient_GetContent(contentPath, outputPath,
                                          GuestStoreReportProgress, NULL);
   if (exitCode != GSLIBERR_SUCCESS) {
      g_critical("GuestStoreClient_GetContent failed: error=%d.\n", exitCode);
   }

   if (!GuestStoreClient_DeInit()) {
      g_warning("GuestStoreClient_DeInit failed.\n");
   }

exit:

   if (exitCode == GSLIBERR_SUCCESS) {
      ToolsCmd_Print(SU_(result.succeeded,
                         "'%s' succeeded.\n"),
                     argv[optind]);
   } else if (exitCode < GUESTSTORE_LIB_ERR_MAX) {
      ToolsCmd_PrintErr(SU_(gueststore.error.client_lib,
                            "'%s' failed, GuestStore client library "
                            "error: %s.\n"),
                        argv[optind], guestStoreLibErrMsgs[exitCode]);
   } else {
      ToolsCmd_PrintErr(SU_(result.error.failed,
                            "'%s' failed, check %s log for "
                            "more information.\n"),
                        argv[optind], argv[0]);
   }

   return exitCode;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestStore_Help --
 *
 *      Prints the help for the gueststore command.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
GuestStore_Help(const char *progName, // IN: Name of the program from argv[0]
                const char *cmd)      // IN
{
   g_print(SU_(help.gueststore,
               "%s: get resource content from GuestStore\n"
               "Usage: %s %s <subcommand>\n\n"
               "ESX guests only subcommands:\n"
               "   getcontent <resource path> <output file>: "
               "get resource content from GuestStore and "
               "save to output file.\n\n"
               "<resource path> starts with / and represents a unique "
               "resource in GuestStore. If it ends with /, defaults to "
               "retrieve the underlying 'metadata.json' resource.\n"
               "<output file> is the path of a file "
               "to save resource content to.\n"),
           cmd, progName, cmd);
}
