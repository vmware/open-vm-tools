/*********************************************************
 * Copyright (C) 2009-2016, 2019 VMware, Inc. All rights reserved.
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
 * wiperCommon.c --
 *
 *      OS-agnostic parts of the library for wiping a virtual disk.
 *
 */

#include <stdlib.h>
#include <string.h>
#include "dbllnklst.h"
#include "wiper.h"
#include "util.h"

/*
 *-----------------------------------------------------------------------------
 *
 * WiperSinglePartition_Allocate --
 *
 *      Allocates and initialized empty WiperPartition structure.
 *
 * Results:
 *      NULL if there is no memory, otherwise a pointer to newly allocated
 *      WiperPartition structure.
 *
 * Side Effects:
 *      Allocates memory.
 *
 *-----------------------------------------------------------------------------
 */

WiperPartition *
WiperSinglePartition_Allocate(void)
{
   WiperPartition *p = (WiperPartition *) malloc(sizeof *p);

   if (p != NULL) {
      memset(p->mountPoint, 0, sizeof p->mountPoint);
      p->type = PARTITION_UNSUPPORTED;
      p->fsType = NULL;
      p->fsName = NULL;
      p->comment = NULL;
      p->attemptUnmaps = TRUE;
      DblLnkLst_Init(&p->link);
   }

   return p;
}


/*
 *-----------------------------------------------------------------------------
 *
 * WiperSinglePartition_Close --
 *
 *      Destroy the information returned by a previous call to
 *      WiperSinglePartition_Allocate(). The partition should be removed
 *      from all lists prior to calling WiperSinglePartition_Close().
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      Frees memory occupied by the element.
 *
 *-----------------------------------------------------------------------------
 */

void
WiperSinglePartition_Close(WiperPartition *p)      // IN
{
   if (p) {
      free((char *)p->comment); /* Casting away constness */
      free((char *)p->fsType);  /* Casting away constness */
      free((char *)p->fsName);  /* Casting away constness */
      free(p);
   }
}


/*
 *---------------------------------------------------------------------------
 *
 * WiperPartition_Close --
 *
 *      Destroy the information collected by previous call to
 *      WiperPartition_Open().
 *
 * Results:
 *      None
 *
 * Side Effects:
 *      Frees memory occupied by elements of the list.
 *
 *---------------------------------------------------------------------------
 */

void
WiperPartition_Close(WiperPartition_List *pl)      // IN/OUT
{
   DblLnkLst_Links *curr, *next;

   DblLnkLst_ForEachSafe(curr, next, &pl->link) {
      WiperPartition *part = DblLnkLst_Container(curr, WiperPartition, link);

      DblLnkLst_Unlink1(curr);
      WiperSinglePartition_Close(part);
   }
}

