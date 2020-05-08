/*********************************************************
 * Copyright (C) 2019 VMware, Inc. All rights reserved.
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
 * appInfoUtil.c --
 *
 *      Utility functions for the application list information.
 */


#include <stdio.h>
#include <stdlib.h>
#include "appInfoInt.h"


/*
 ******************************************************************************
 * AppInfoDestroyAppData --
 *
 * Free function for application data. This function is called by the glib
 * for each element in the application list while freeing.
 *
 * @param[in] data      Pointer to the application data.
 *
 * @retval NONE
 *
 ******************************************************************************
 */

void
AppInfoDestroyAppData(gpointer data) {
   AppInfo *appInfo = (AppInfo *) data;

   if (appInfo == NULL) {
      return;
   }

   free(appInfo->appName);
   free(appInfo->version);
   free(appInfo);
}


/*
 ******************************************************************************
 * AppInfoDestroyAppList --
 *
 * Frees the entire memory allocated for the application list.
 *
 * @param[in] appList      Pointer to the application list.
 *
 * @retval NONE
 *
 ******************************************************************************
 */

void
AppInfo_DestroyAppList(GSList *appList) {
   if (appList == NULL) {
      return;
   }

   g_slist_free_full(appList, AppInfoDestroyAppData);
}


/*
 ******************************************************************************
 * AppInfoCompareApps --
 *
 * Compare function used by glib while sorting the application list.
 * For windows guests, the memory used by the application is used for comparing.
 * For linux guests, no comparison is done and hence 0 is returned.
 *
 * @param[in] a      Pointer to the first application that should be compared.
 * @param[in] b      Pointer to the second application that should be compared.
 *
 * @retval -1 if a should be kept before b in the sorted list.
 *          0 if both the elements are same.
 *          1 if b should be kept before a in the sorted list.
 *
 ******************************************************************************
 */

static gint
AppInfoCompareApps(gconstpointer a,
                   gconstpointer b) {
#if defined(_WIN32)
   if (a != NULL && b != NULL) {
      size_t aMemory = ((AppInfo *) a)->memoryUsed;
      size_t bMemory = ((AppInfo *) b)->memoryUsed;
      if (aMemory < bMemory) {
         return 1;
      } else if (aMemory == bMemory) {
         return 0;
      } else {
         return -1;
      }
   }
   return 0;
#else
   return 0;
#endif
}


/*
 ******************************************************************************
 * AppInfo_SortAppList --
 *
 * Sorts the provided list of applications.
 *
 * @param[in] appList List of applications that need to be sorted.
 *
 * @retval The sorted list of applications.
 *
 ******************************************************************************
 */

GSList *
AppInfo_SortAppList(GSList *appList) // IN/OUT
{
   if (appList == NULL) {
      return appList;
   }

   return g_slist_sort(appList, AppInfoCompareApps);
}