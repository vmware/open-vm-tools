/*********************************************************
 * Copyright (c) 2011-2016, 2023 VMware, Inc. All rights reserved.
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

/**
 * @file nullDriver.c
 *
 * A "null" sync driver that just calls sync(2). This is used when both the
 * Linux kernel ioctls nor the vmsync driver are available in the system,
 * since at that point it's too late to tell the vmbackup code that there
 * is no sync driver.
 */

#include "debug.h"
#include "syncDriverInt.h"
#include "util.h"


/*
 *******************************************************************************
 * NullDriverClose --                                                     */ /**
 *
 * Frees the handle.
 *
 * @param[in] handle Handle to free.
 *
 *******************************************************************************
 */

static void
NullDriverClose(SyncDriverHandle handle)
{
   free(handle);
}


/*
 *******************************************************************************
 * NullDriver_Freeze --                                                   */ /**
 *
 * Calls sync().
 *
 * @param[in]  paths            Unused.
 * @param[out] handle           Where to store the operation handle.
 * @param[in]  ignoreFrozenFS   Unused.
 *
 * @return A SyncDriverErr.
 *
 *******************************************************************************
 */

SyncDriverErr
NullDriver_Freeze(const GSList *paths,
                  SyncDriverHandle *handle,
                  Bool ignoreFrozenFS)
{
   /*
    * This is more of a "let's at least do something" than something that
    * will actually ensure data integrity... we also need to return a dummy
    * handle.
    */
   SyncHandle *h = calloc(1, sizeof *h);
   if (h == NULL) {
      return SD_ERROR;
   }

   h->close = NullDriverClose;
   *handle = h;

   Debug(LGPFX "Using null driver...\n");
   sync();
   return SD_SUCCESS;
}

