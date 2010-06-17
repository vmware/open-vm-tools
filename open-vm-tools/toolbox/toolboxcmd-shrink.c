/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
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
 * toolboxcmd-shrink.c --
 *
 *     The shrink operations for toolbox-cmd
 */


#include <string.h>
#include <stdlib.h>

#ifndef _WIN32
#   include <signal.h>
#endif

#include "toolboxCmdInt.h"
#include "rpcout.h"
#include "vmware/guestrpc/tclodefs.h"
#include "vmware/tools/i18n.h"

#ifndef _WIN32
static void ShrinkWiperDestroy(int signal);
#endif

static Wiper_State *wiper = NULL;


/*
 *-----------------------------------------------------------------------------
 *
 * ShrinkGetMountPoints  --
 *
 *      Gets a list of wiper partitions.
 *
 * Results:
 *      The WiperPartion_List.
 *
 * Side effects:
 *      Prints to stderr on errors.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
ShrinkGetMountPoints(WiperPartition_List *pl) // OUT: Known mount points
{
   if (!GuestApp_IsDiskShrinkCapable()) {
      ToolsCmd_PrintErr("%s",
                        SU_(disk.shrink.unavailable, SHRINK_FEATURE_ERR));
   } else if (!GuestApp_IsDiskShrinkEnabled()) {
      ToolsCmd_PrintErr("%s",
                        SU_(disk.shrink.disabled, SHRINK_DISABLED_ERR));
   } else if (!WiperPartition_Open(pl)) {
      ToolsCmd_PrintErr("%s",
                        SU_(disk.shrink.partition.error,
                            "Unable to collect partition data.\n"));
   } else {
      return TRUE;
   }
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ShrinkGetPartition  --
 *
 *      Finds the WiperPartion whose mountpoint is given.
 *
 * Results:
 *      The WiperPatition.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static WiperPartition*
ShrinkGetPartition(char *mountPoint)
{
   WiperPartition_List plist;
   WiperPartition *p, *part = NULL;
   DblLnkLst_Links *curr;

   if (!ShrinkGetMountPoints(&plist)) {
      return NULL;
   }

   DblLnkLst_ForEach(curr, &plist.link) {
      p = DblLnkLst_Container(curr, WiperPartition, link);
      if (toolbox_strcmp(p->mountPoint, mountPoint) == 0) {
         part = p;
         /*
          * Detach the element we are interested in so it is not
          * destroyed when we call WiperPartition_Close.
          */
         DblLnkLst_Unlink1(&part->link);
         break;
      }
   }

   WiperPartition_Close(&plist);

   return part;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ShrinkList  --
 *
 *      Prints mount points to stdout.
 *
 * Results:
 *      EXIT_SUCCESS.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
ShrinkList(void)
{
   WiperPartition_List plist;
   DblLnkLst_Links *curr;

   if (!ShrinkGetMountPoints(&plist)) {
      return EX_TEMPFAIL;
   }

   DblLnkLst_ForEach(curr, &plist.link) {
      WiperPartition *p = DblLnkLst_Container(curr, WiperPartition, link);
      if (p->type != PARTITION_UNSUPPORTED) {
         printf("%s\n", p->mountPoint);
      }
   }

   WiperPartition_Close(&plist);

   return EXIT_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ShrinkDoShrink  --
 *
 *      Wipe a single partition, returning only when the wiper
 *      operation is done or canceled.
 *
 * Results:
 *      EXIT_SUCCESS on success.
 *      EX_OSFILE if partition is not found.
 *      EX_TEMPFAIL on failure.
 *
 * Side effects:
 *      The wipe operation will fill the partition with dummy files.
 *      Prints to stderr on errors.
 *
 *-----------------------------------------------------------------------------
 */

static int
ShrinkDoShrink(char *mountPoint, // IN: mount point
               gboolean quiet)   // IN: verbosity flag
{
   int i;
   int progress = 0;
   unsigned char *err;
   WiperPartition *part;
   int rc;

#ifndef _WIN32
   signal(SIGINT, ShrinkWiperDestroy);
#endif

   part = ShrinkGetPartition(mountPoint);
   if (part == NULL) {
      ToolsCmd_PrintErr(SU_(disk.shrink.partition.notfound,
                            "Unable to find partition %s\n"),
                        mountPoint);
      return EX_OSFILE;
   }

   if (part->type == PARTITION_UNSUPPORTED) {
      ToolsCmd_PrintErr(SU_(disk.shrink.partition.unsupported,
                            "Partition %s is not shrinkable\n"),
                        part->mountPoint);
      rc = EX_UNAVAILABLE;
      goto out;
   }

   /*
    * Verify that shrinking is still possible before going through with the
    * wiping. This obviously isn't atomic, but it should take care of
    * the case where the user takes a snapshot with the toolbox open.
    */
   if (!GuestApp_IsDiskShrinkEnabled()) {
      ToolsCmd_PrintErr("%s",
                        SU_(disk.shrink.conflict, SHRINK_CONFLICT_ERR));
      rc = EX_TEMPFAIL;
      goto out;
   }

   wiper = Wiper_Start(part, MAX_WIPER_FILE_SIZE);

   while (progress < 100 && wiper != NULL) {
      err = Wiper_Next(&wiper, &progress);
      if (strlen(err) > 0) {
         if (strcmp(err, "error.create") == 0) {
            ToolsCmd_PrintErr("%s",
                              SU_(disk.wiper.file.error,
                                  "Error, Unable to create wiper file.\n"));
         } else {
            ToolsCmd_PrintErr(SU_(disk.wiper.error, "Error: %s"), err);
         }

         rc = EX_TEMPFAIL;
      }

      if (!quiet) {
         printf("\rProgress: %d [", progress);
         for (i = 0; i <= progress / 10; i++) {
            putchar('=');
         }
         printf(">%*c", 10 - i + 1, ']');
         fflush(stdout);
      }
   }

   if (progress >= 100) {
      char *result;
      size_t resultLen;

      ToolsCmd_PrintErr("\n");

      if (ToolsCmd_SendRPC(DISK_SHRINK_CMD, sizeof DISK_SHRINK_CMD - 1,
                           &result, &resultLen)) {
         ToolsCmd_Print("%s",
                        SU_(disk.shrink.complete, "Disk shrinking complete.\n"));
         rc = EXIT_SUCCESS;
         goto out;
      }

      ToolsCmd_PrintErr(SU_(disk.shrink.error, "Error while shrinking: %s\n"), result);
   }

   ToolsCmd_PrintErr("%s",
                     SU_(disk.shrink.incomplete, "Shrinking not completed.\n"));
   rc = EX_TEMPFAIL;

out:
   WiperSinglePartition_Close(part);
   free(wiper);
   wiper = NULL;
   return rc;
}


#ifndef _WIN32
/*
 *-----------------------------------------------------------------------------
 *
 * ShrinkWiperDestroy  --
 *
 *      Catch SIGINT and cancel wiper operation.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The wipe operation will be canceled, and "zero" files removed.
 *      We will also exit the vmware-toolbox-cmd program.
 *
 *-----------------------------------------------------------------------------
 */

void
ShrinkWiperDestroy(int signal)	// IN: Signal caught
{
   if (wiper != NULL) {
      Wiper_Cancel(&wiper);
      wiper = NULL;
   }
   ToolsCmd_Print("%s", SU_(disk.shrink.canceled, "Disk shrink canceled.\n"));
   exit(EXIT_SUCCESS);
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * Disk_Command --
 *
 *      Handle and parse disk commands.
 *
 * Results:
 *      Returns EXIT_SUCCESS on success.
 *      Returns the appropriate exit code on errors.
 *
 * Side effects:
 *      Might shrink disk
 *
 *-----------------------------------------------------------------------------
 */

int
Disk_Command(char **argv,      // IN: command line arguments
             int argc,         // IN: The length of the command line arguments
             gboolean quiet)   // IN
{
   if (toolbox_strcmp(argv[optind], "list") == 0) {
      return ShrinkList();
   } else if (toolbox_strcmp(argv[optind], "shrink") == 0) {
      if (++optind >= argc) {
         ToolsCmd_MissingEntityError(argv[0], SU_(arg.mountpoint, "mount point"));
      } else {
         return ShrinkDoShrink(argv[optind], quiet);
      }
   } else {
      ToolsCmd_UnknownEntityError(argv[0],
                                  SU_(arg.subcommand, "subcommand"),
                                  argv[optind]);
   }
   return EX_USAGE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Disk_Help --
 *
 *      Prints the help for the disk command.
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
Disk_Help(const char *progName, // IN: The name of the program obtained from argv[0]
          const char *cmd)      // IN
{
   g_print(SU_(help.disk, "%s: perform disk shrink operations\n"
                          "Usage: %s %s <subcommand> [args]\n\n"
                          "Subcommands:\n"
                          "   list: list available mountpoints\n"
                          "   shrink <mount-point>: shrinks a file system at the given mountpoint\n"),
           cmd, progName, cmd);
}

