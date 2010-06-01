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

#ifndef _WIN32
static void ShrinkWiperDestroy(int signal);
#endif

static Bool ShrinkGetMountPoints(WiperPartition_List *);
static WiperPartition * ShrinkGetPartition(char *mountPoint);
static Wiper_State *wiper = NULL;


/*
 *-----------------------------------------------------------------------------
 *
 * Shrink_List  --
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

int
Shrink_List(void)
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
      fprintf(stderr, SHRINK_FEATURE_ERR);
   } else if (!GuestApp_IsDiskShrinkEnabled()) {
      fprintf(stderr, SHRINK_DISABLED_ERR);
   } else if (!WiperPartition_Open(pl)) {
      fprintf(stderr, "Unable to collect partition data.\n");
   } else {
      return TRUE;
   }
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Shrink_DoShrink  --
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

int
Shrink_DoShrink(char *mountPoint, // IN: mount point
                int quiet_flag)   // IN: verbosity flag
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
      fprintf(stderr, "Unable to find partition %s\n", mountPoint);
      return EX_OSFILE;
   }

   if (part->type == PARTITION_UNSUPPORTED) {
      fprintf(stderr, "Partition %s is not shrinkable\n", part->mountPoint);
      rc = EX_UNAVAILABLE;
      goto out;
   }

   /*
    * Verify that shrinking is still possible before going through with the
    * wiping. This obviously isn't atomic, but it should take care of
    * the case where the user takes a snapshot with the toolbox open.
    */
   if (!GuestApp_IsDiskShrinkEnabled()) {
      fprintf(stderr, SHRINK_CONFLICT_ERR);
      rc = EX_TEMPFAIL;
      goto out;
   }

   wiper = Wiper_Start(part, MAX_WIPER_FILE_SIZE);

   while (progress < 100 && wiper != NULL) {
      err = Wiper_Next(&wiper, &progress);
      if (strlen(err) > 0) {
         if (strcmp(err, "error.create") == 0) {
            fprintf(stderr, "Error, Unable to create wiper file\n");
         } else {
            fprintf(stderr, "Error, %s", err);
         }

         rc = EX_TEMPFAIL;
      }

      if (!quiet_flag) {
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

      if (RpcOut_sendOne(&result, &resultLen, "disk.shrink")) {
         if (!quiet_flag) {
            printf("\nDisk shrinking complete\n");
         }
         rc = EXIT_SUCCESS;
         goto out;
      }

      fprintf(stderr, "%s\n", result);
   }

   fprintf(stderr, "Shrinking not completed\n");
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
   printf("Disk shrink canceled\n");
   exit(EXIT_SUCCESS);
}
#endif
