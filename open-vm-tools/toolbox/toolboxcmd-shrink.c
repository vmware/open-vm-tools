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
 * toolboxcmd-shrink.c --
 *
 *     The shrink operations for toolbox-cmd
 */


#include <string.h>
#include <stdlib.h>

#ifndef _WIN32
#   include <signal.h>
#endif

#include "vm_assert.h"
#include "toolboxCmdInt.h"
#include "guestApp.h"
#include "wiper.h"
#include "vmware/guestrpc/tclodefs.h"
#include "vmware/tools/i18n.h"

#ifndef _WIN32
static void ShrinkWiperDestroy(int signal);
#endif

#define SHRINK_DISABLED_ERR                                       \
   "Shrink disk is disabled for this virtual machine.\n\n"        \
   "Shrinking is disabled for linked clones, parents of "         \
   "linked clones, \npre-allocated disks, snapshots, or due to "  \
   "other factors. \nSee the User's manual for more "             \
   "information.\n"

#define SHRINK_FEATURE_ERR                                                    \
   "The shrink feature is not available,\n\n"                                 \
   "either because you are running an old version of a VMware product, "      \
   "or because too many communication channels are open.\n\n"                 \
   "If you are running an old version of a VMware product, you should "       \
   "consider upgrading.\n\n"                                                  \
   "If too many communication channels are open, you should power off your "  \
   "virtual machine and then power it back on.\n"

#define SHRINK_CONFLICT_ERR                                 \
   "Error, The Toolbox believes disk shrinking is "         \
   "enabled while the host believes it is disabled.\n\n "   \
   "Please close and reopen the Toolbox to synchronize "    \
   "it with the host.\n"

static Wiper_State *wiper = NULL;

#define WIPER_STATE_CMD "disk.wiper.enable"

typedef enum {
   WIPER_UNAVAILABLE,
   WIPER_DISABLED,
   WIPER_ENABLED,
} WiperState;


/*
 *-----------------------------------------------------------------------------
 *
 * ShrinkGetWiperState  --
 *
 *      Gets the state of the shrink backend in the host.
 *
 * Results:
 *      The shrink backend state.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static WiperState
ShrinkGetWiperState(void)
{
   char *result = NULL;
   size_t resultLen;
   WiperState state = WIPER_UNAVAILABLE;

   if (ToolsCmd_SendRPC(WIPER_STATE_CMD, sizeof WIPER_STATE_CMD - 1,
                        &result, &resultLen)) {
      if (resultLen == 1 && strcmp(result, "1") == 0) {
         state = WIPER_ENABLED;
      } else {
         state = WIPER_DISABLED;
      }
   }
   ToolsCmd_FreeRPC(result);
   return state;
}


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
   WiperState state = ShrinkGetWiperState();

   switch (state) {
   case WIPER_UNAVAILABLE:
      ToolsCmd_PrintErr("%s",
                        SU_(disk.shrink.unavailable, SHRINK_FEATURE_ERR));
      break;

   case WIPER_DISABLED:
      ToolsCmd_PrintErr("%s",
                        SU_(disk.shrink.disabled, SHRINK_DISABLED_ERR));
      break;

   case WIPER_ENABLED:
      if (WiperPartition_Open(pl, TRUE)) {
         return TRUE;
      }

      ToolsCmd_PrintErr("%s",
                        SU_(disk.shrink.partition.error,
                           "Unable to collect partition data.\n"));
      break;

   default:
      NOT_REACHED();
   }

   return FALSE;
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
   uint32 countShrink = 0;
   WiperState wstate = ShrinkGetWiperState();

   if (!ShrinkGetMountPoints(&plist)) {
      return EX_TEMPFAIL;
   }

   DblLnkLst_ForEach(curr, &plist.link) {
      WiperPartition *p = DblLnkLst_Container(curr, WiperPartition, link);
      if (p->type != PARTITION_UNSUPPORTED &&
          (wstate == WIPER_ENABLED || Wiper_IsWipeSupported(p))) {
         g_print("%s\n", p->mountPoint);
         countShrink++;
      }
   }

   WiperPartition_Close(&plist);

   /*
    * No shrinkable/wipable disks found.
    */
   if (countShrink == 0) {
      g_debug("No shrinkable disks found\n");
      ToolsCmd_PrintErr("%s",
                        SU_(disk.shrink.disabled, SHRINK_DISABLED_ERR));
      return EX_TEMPFAIL;
   }

   return EXIT_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ShrinkDiskSendRPC  --
 *
 *      Shrink all shrinkable disks/partitions, returning only when the shrink
 *      RPC operation is done or canceled.
 *
 * Results:
 *      EXIT_SUCCESS on success.
 *      EX_TEMPFAIL on failure.
 *
 * Side effects:
 *      Prints to stderr on errors.
 *
 *-----------------------------------------------------------------------------
 */

static int
ShrinkDiskSendRPC(void)
{
   char *result = NULL;
   size_t resultLen;
   int retVal;

   ToolsCmd_PrintErr("\n");

   if (ToolsCmd_SendRPC(DISK_SHRINK_CMD, sizeof DISK_SHRINK_CMD - 1,
                        &result, &resultLen)) {
      ToolsCmd_Print("%s",
                     SU_(disk.shrink.complete, "Disk shrinking complete.\n"));
      retVal =  EXIT_SUCCESS;
   } else {
      ToolsCmd_PrintErr(SU_(disk.shrink.error,
                        "Error while shrinking: %s\n"), result);
      retVal =  EX_TEMPFAIL;
   }

   ToolsCmd_FreeRPC(result);
   return retVal;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ShrinkDoAllDiskShrinkOnly  --
 *
 *      Shrink all shrinkable disks/partitions if it is enabled. Layered around
 *      ShrinkDoAllDiskShrinkCommon. This routine does not invoke the disk wipe
 *      operation step.
 *
 * Results:
 *      EXIT_SUCCESS on success.
 *      EX_TEMPFAIL on failure.
 *
 * Side effects:
 *      Prints to stderr on errors.
 *
 *-----------------------------------------------------------------------------
 */

static int
ShrinkDoAllDiskShrinkOnly(void)
{
   WiperPartition_List plist;
   DblLnkLst_Links *curr;
   Bool canShrink = FALSE;
   WiperState wstate = ShrinkGetWiperState();

#ifndef _WIN32
   signal(SIGINT, ShrinkWiperDestroy);
#endif

   if (!ShrinkGetMountPoints(&plist)) {
      return EX_TEMPFAIL;
   }

   DblLnkLst_ForEach(curr, &plist.link) {
      WiperPartition *p = DblLnkLst_Container(curr, WiperPartition, link);
      if (p->type != PARTITION_UNSUPPORTED &&
          (wstate == WIPER_ENABLED || Wiper_IsWipeSupported(p))) {
         canShrink = TRUE;
         break;
      }
   }

   WiperPartition_Close(&plist);

   /*
    * Verify that shrinking is permitted on at least 1 disk.
    */

   if (!canShrink) {
      g_debug("No shrinkable disks found\n");
      ToolsCmd_PrintErr("%s",
                        SU_(disk.shrink.disabled, SHRINK_DISABLED_ERR));
      return EX_TEMPFAIL;
   }

   return ShrinkDiskSendRPC();
}


/*
 *-----------------------------------------------------------------------------
 *
 * ShrinkDoWipeAndShrink  --
 *
 *      Wipe a single partition, returning only when the wiper
 *      operation is done or canceled.
 *      Caller can optionally indicate whether a disk shrink operation is required
 *      to be performed after the wipe operation or not.
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
ShrinkDoWipeAndShrink(char *mountPoint,         // IN: mount point
                      gboolean quiet,           // IN: verbosity flag
                      gboolean performShrink)   // IN: perform a shrink operation
{
   int i;
   int progress = 0;
   unsigned char *err;
   WiperPartition *part = NULL;
   WiperPartition_List plist;
   int rc;

#if defined(_WIN32)
   DWORD currPriority = GetPriorityClass(GetCurrentProcess());
#else
   signal(SIGINT, ShrinkWiperDestroy);
#endif

   if (ShrinkGetMountPoints(&plist)) {
      DblLnkLst_Links *curr, *nextElem;
      DblLnkLst_ForEachSafe(curr, nextElem, &plist.link) {
         WiperPartition *p = DblLnkLst_Container(curr, WiperPartition, link);
         if (toolbox_strcmp(p->mountPoint, mountPoint) == 0) {
            WiperSinglePartition_Close(part);
            part = p;
            /*
             * Detach the element we are interested in so it is not
             * destroyed when we call WiperPartition_Close.
             */
            DblLnkLst_Unlink1(&part->link);
            if (part->type != PARTITION_UNSUPPORTED) {
               break;
            }
         }
      }
      WiperPartition_Close(&plist);
   }

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
    * Verify that wiping/shrinking are permitted before going through with the
    * wiping operation.
    */
   if (ShrinkGetWiperState() != WIPER_ENABLED && !Wiper_IsWipeSupported(part)) {
      g_debug("%s cannot be wiped / shrunk\n", mountPoint);
      ToolsCmd_PrintErr("%s",
                        SU_(disk.shrink.disabled, SHRINK_DISABLED_ERR));
      rc = EX_TEMPFAIL;
      goto out;
   }

   /*
    * During the initial 'wipe' process, the Toolbox CLI first fills the
    * entire guest's disk space with files filled with zeroes. During this step,
    * user may notice few warning messages related to 'low disk space' in the
    * guest operating system. We need to print a warning message to disregard
    * such warnings in the guest operating system.
    */
   if (performShrink) {
      ToolsCmd_Print("%s", SU_(disk.shrink.ignoreFreeSpaceWarnings,
                               "Please disregard any warnings about disk space "
                               "for the duration of shrink process.\n"));
   } else {
      ToolsCmd_Print("%s", SU_(disk.wipe.ignoreFreeSpaceWarnings,
                               "Please disregard any warnings about disk space "
                               "for the duration of wipe process.\n"));
   }

   wiper = Wiper_Start(part, MAX_WIPER_FILE_SIZE);

#if defined(_WIN32)
   /*
    * On Win32, lower the process priority during wipe, so other applications
    * can still run (sort of) normally while we're filling the disk.
    */
   if (!SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS)) {
      g_debug("Unable to lower process priority: %u.", GetLastError());
   }
#endif

   while (progress < 100 && wiper != NULL) {
      err = Wiper_Next(&wiper, &progress);
      if (strlen(err) > 0) {
         if (strcmp(err, "error.create") == 0) {
            ToolsCmd_PrintErr("%s",
                              SU_(disk.wiper.file.error,
                                  "Error, Unable to create wiper file.\n"));
         } else {
            ToolsCmd_PrintErr(SU_(error.message, "Error: %s\n"), err);
         }
         /* progress < 100 will result in "rc" of EX_TEMPFAIL */
         break;
      }

      if (!quiet) {
         g_print(SU_(disk.wiper.progress, "\rProgress: %d"), progress);
         g_print(" [");
         for (i = 0; i <= progress / 10; i++) {
            putchar('=');
         }
         g_print(">%*c", 10 - i + 1, ']');
         fflush(stdout);
      }
   }

#if defined(_WIN32)
   /* Go back to our original priority. */
   if (!SetPriorityClass(GetCurrentProcess(), currPriority)) {
      g_debug("Unable to restore process priority: %u.", GetLastError());
   }
#endif

   g_print("\n");
   if (progress < 100) {
      rc = EX_TEMPFAIL;
   } else if (performShrink) {
      rc = ShrinkDiskSendRPC();
   } else {
      rc = EXIT_SUCCESS;
      g_debug("Shrink skipped.\n");
   }

   if (rc != EXIT_SUCCESS) {
      ToolsCmd_PrintErr("%s",
                        SU_(disk.shrink.incomplete, "Shrinking not completed.\n"));
   }

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
         return ShrinkDoWipeAndShrink(argv[optind], quiet,
                                      TRUE /* perform shrink */);
      }
   } else if (toolbox_strcmp(argv[optind], "wipe") == 0) {
      if (++optind >= argc) {
         ToolsCmd_MissingEntityError(argv[0], SU_(arg.mountpoint, "mount point"));
      } else {
         return ShrinkDoWipeAndShrink(argv[optind], quiet,
                                      FALSE /* do not perform shrink */);
      }
   } else if (toolbox_strcmp(argv[optind], "shrinkonly") == 0) {
      return ShrinkDoAllDiskShrinkOnly();
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
                          "   list: list available locations\n"
                          "   shrink <location>: wipes and shrinks a file system at the given location\n"
                          "   shrinkonly: shrinks all disks\n"
                          "   wipe <location>: wipes a file system at the given location\n"),
           cmd, progName, cmd);
}

