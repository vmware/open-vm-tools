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


#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <signal.h>
#include "toolboxCmdInt.h"


static void ShrinkWiperDestroy(int signal);
static WiperPartition_List * ShrinkGetMountPoints(void);
static WiperPartition * ShrinkGetPartition(char *mountPoint, int quiet_flag);
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
Shrink_List(void) // IN: Verbosity flag.
{
   int i;
   WiperPartition_List *plist = ShrinkGetMountPoints();
   if (plist == NULL) {
      return EX_TEMPFAIL;
   }
   for (i = 0; i < plist->size; i++) {
      if (strlen(plist->partitions[i].comment) == 0) {
         printf("%s\n", plist->partitions[i].mountPoint);
      }
   }
   WiperPartition_Close(plist);
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
ShrinkGetPartition(char *mountPoint, // IN: mount point
                   int quiet_flag)   // IN: Verbosity flag
{
   int i;
   WiperPartition_List *plist = ShrinkGetMountPoints();
   if (!plist) {
      return NULL;
   }
   WiperPartition *part;
   part = (WiperPartition *) malloc(sizeof *part);
   for (i = 0; i < plist->size; i++) {
      if (strcmp(plist->partitions[i].mountPoint, mountPoint) == 0) {
         memcpy(part, &plist->partitions[i], sizeof *part);
         WiperPartition_Close(plist);
         return part;
      }
   }
   WiperPartition_Close(plist);
   return NULL;
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

static WiperPartition_List*
ShrinkGetMountPoints(void) // IN: Verbosity flag
{
   if (GuestApp_IsDiskShrinkCapable()) {
      if (GuestApp_IsDiskShrinkEnabled()) {
         Wiper_Init(NULL);
         return WiperPartition_Open();
      } else {
         fprintf(stderr,SHRINK_DISABLED_ERR);
      }
   } else {
      fprintf(stderr, SHRINK_FEATURE_ERR);
   }
   return NULL;
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
   signal(SIGINT, ShrinkWiperDestroy);
   part = ShrinkGetPartition(mountPoint, quiet_flag);
   if (part == NULL) {
      fprintf(stderr, "Unable to find partition\n");
      return EX_OSFILE;
   }
   /*
    * Verify that shrinking is still possible before going through with the
    * wiping. This obviously isn't atomic, but it should take care of
    * the case where the user takes a snapshot with the toolbox open.
    */
   if (!GuestApp_IsDiskShrinkEnabled()) {
      fprintf(stderr, SHRINK_CONFLICT_ERR);
      free(part);
      return EX_TEMPFAIL;
   }

   wiper = Wiper_Start (part, MAX_WIPER_FILE_SIZE);
   while (progress < 100 && wiper != NULL) {
      err = Wiper_Next(&wiper, &progress);
      if (strlen(err) > 0) {
         if (strcmp(err, "error.create") == 0) {
            fprintf(stderr, "Error, Unable to create wiper file\n");
            free(part);
            return EX_TEMPFAIL;
         }
         else {
            fprintf(stderr, "Error, %s", err);
            free(part);
            return EX_TEMPFAIL;
         }
         free(wiper);
         wiper = NULL;
      }
      if (!quiet_flag) {
	 printf("\rProgress: %d [", progress);
	 for (i = 0; i <= progress / 10; i++) {
	    printf("=");
	 }
	 printf(">");
	 for (; i <= 100 / 10; i++) {
	    printf(" ");
	 }
	 printf("]");
      }
   }
   if (progress >= 100) {
      if (!quiet_flag) {
         printf("\nDisk shrinking complete\n");
      }
      wiper = NULL;
      free(part);
      return EXIT_SUCCESS;
   } else {
      fprintf(stderr, "Shrinking not completed\n");
      return EX_TEMPFAIL;
   }
}


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
