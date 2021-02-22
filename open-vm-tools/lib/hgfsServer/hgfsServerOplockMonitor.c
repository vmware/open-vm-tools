/*********************************************************
 * Copyright (C) 2020-2021 VMware, Inc. All rights reserved.
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
 * hgfsServerOplockMonitor.c --
 *
 *      Implements functions for HGFS server opportunistic lock monitoring
 *      subfeature.
 */


#include "vmware.h"
#include "hashTable.h"
#include "hgfsServerOplockMonitor.h"
#include "mutexRankLib.h"
#include "util.h"

 /*
  * Local data
  */
#define AS_KEY(_x)  ((const void *)(uintptr_t)(_x))

/*
 * Define the max count for hash table gOplockMonitorMap.
 */
#define OPLOCK_MONITOR_MAP_MAX_COUNT HGFS_OPLOCK_MAX_COUNT

/*
 * Define the max count for hash table gOplockMonitorHandleMap.
 * Different monitor requests may target the same file, for each file there is
 * one item in hash table gOplockMonitorMap, and for each monitor request there
 * is one item in hash table gOplockMonitorHandleMap.
 * We support 4 monitor requests for each file.
 */
#define OPLOCK_MONITOR_HANDLE_MAP_MAX_COUNT (4 * OPLOCK_MONITOR_MAP_MAX_COUNT)


/*
 * This structure is the type of oplockMonitorData.callbackList.
 */
typedef struct {
   DblLnkLst_Links links;
   uint64 handle;
   HgfsOplockCallback callback;
   void *data;
} oplockMonitorCallbackList;

/*
 * This structure is the value field of hash table gOplockMonitorMap.
 */
typedef struct {
   fileDesc fileDesc;
   char *utf8Name;
   MXUserExclLock *lock;
   DblLnkLst_Links callbackList;
} oplockMonitorData;

/*
 * Caller can use oplock module to monitor the file change event by providing
 * the file path instead of file descriptor.
 * This hash table maps the file path to structure oplockMonitorData
 * which stores the information for the file, for example the file descriptor.
 * This hash table is mainly used to check if a file has already been opened,
 * which means when many callers monitor the same file, we only need to open
 * that file once.
 */
static HashTable *gOplockMonitorMap = NULL;

/*
 * This hash table is used to map the monitor handle to structure
 * oplockMonitorData.
 * This hash table is used when un-monitor the file change.
 */
static HashTable *gOplockMonitorHandleMap = NULL;

/* Lock for gOplockMonitorMap and gOplockMonitorHandleMap. */
static MXUserExclLock *oplockMonitorLock;

/* Indicates if the oplock monitor module is initialized. */
static Bool gOplockMonitorInit = FALSE;

void
HgfsOplockUnmonitorFileChangeInternal(HOM_HANDLE handle);


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsOplockMonitorInit --
 *
 *      Set up any related state for monitoring.
 *
 * Results:
 *      TRUE on success, FALSE on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsOplockMonitorInit(void)
{
   if (gOplockMonitorInit) {
      return TRUE;
   }

   // Oplock module must be initialized first.
   if (!HgfsServerOplockIsInited()) {
      Log("%s: Oplock module is not inited\n", __FUNCTION__);
      return FALSE;
   }

   gOplockMonitorMap = HashTable_Alloc(OPLOCK_MONITOR_MAP_MAX_COUNT,
                                      HASH_ISTRING_KEY | HASH_FLAG_COPYKEY,
                                      NULL);
   ASSERT(gOplockMonitorMap);

   gOplockMonitorHandleMap = HashTable_Alloc(OPLOCK_MONITOR_HANDLE_MAP_MAX_COUNT,
                                            HASH_INT_KEY,
                                            NULL);
   ASSERT(gOplockMonitorHandleMap);

   oplockMonitorLock = MXUser_CreateExclLock("HgfsoplockMonitorLock",
                                             RANK_hgfsSharedFolders);

   gOplockMonitorInit = TRUE;
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsOplockMonitorDestroy --
 *
 *      Tear down any related state for monitoring.
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
HgfsOplockMonitorDestroy(void)
{
   if (!gOplockMonitorInit) {
      return;
   }

   HashTable_Free(gOplockMonitorMap);
   HashTable_Free(gOplockMonitorHandleMap);
   MXUser_DestroyExclLock(oplockMonitorLock);
   gOplockMonitorInit = FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsOplockMonitorFileChangeCallback --
 *
 *    A callback function that called when the target file/directory is
 *    changed.
 *    Calls the caller provided callback.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

void
HgfsOplockMonitorFileChangeCallback(HgfsSessionInfo *session,   // IN:
                                    void *data)                 // IN:
{
   oplockMonitorData *monitorData = data;

   ASSERT(monitorData);
   MXUser_AcquireExclLock(oplockMonitorLock);
   if (HashTable_Lookup(gOplockMonitorMap, monitorData->utf8Name, NULL)) {
      DblLnkLst_Links *link, *nextLink;
      DblLnkLst_ForEachSafe(link, nextLink, &monitorData->callbackList) {
         oplockMonitorCallbackList *callbackItem = DblLnkLst_Container(link,
                                                   oplockMonitorCallbackList,
                                                   links);
         callbackItem->callback(session, callbackItem->data);
         /*
          * callbackItem->data has been freed in the user callback.
          */
         callbackItem->data = NULL;
         HgfsOplockUnmonitorFileChangeInternal(callbackItem->handle);
         /*
          * callbackItem has been freed in above function.
          */
         callbackItem = NULL;
      }
   }
   MXUser_ReleaseExclLock(oplockMonitorLock);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsOplockMonitorFileChange --
 *
 *    Monitor the file/directory change event by using oplock.
 *    The caller provided callback will be called if the file/directory is
 *    changed.
 *    This is one-shot action, after the event is fired, the oplock will be
 *    removed.
 *    The data that caller provides will be freed by:
 *       1. caller callback if the callback is called;
 *       2. caller callback if this function failed;
 *       3. this module if caller cancels the file change monitor.
 *
 * Results:
 *    HGFS_OPLOCK_INVALID_MONITOR_HANDLE on fail, handle on success.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

HOM_HANDLE
HgfsOplockMonitorFileChange(char *utf8Name,             // IN: Name in UTF8
                            HgfsSessionInfo *session,   // IN:
                            HgfsOplockCallback callback,// IN:
                            void *data)                 // IN:
{
   oplockMonitorData *monitorData = NULL;
   oplockMonitorCallbackList *callbackItem;
   HOM_HANDLE handle = HGFS_OPLOCK_INVALID_MONITOR_HANDLE;
   HgfsFileOpenInfo openInfo;
   HgfsLocalId localId;
   fileDesc newHandle;
   HgfsInternalStatus status;
   HgfsLockType serverLock = HGFS_LOCK_SHARED;

   MXUser_AcquireExclLock(oplockMonitorLock);
   if (!gOplockMonitorInit) {
      LOG(4, "%s: Oplock monitor is not inited\n", __FUNCTION__);
      goto error;
   }

   if (   HashTable_GetNumElements(gOplockMonitorMap)
       >= OPLOCK_MONITOR_MAP_MAX_COUNT) {
      LOG(4, "%s: Exceeds OPLOCK_MONITOR_MAP_MAX_COUNT\n", __FUNCTION__);
      goto error;
   }

   if (   HashTable_GetNumElements(gOplockMonitorHandleMap)
       >= OPLOCK_MONITOR_HANDLE_MAP_MAX_COUNT) {
      LOG(4, "%s: Exceeds OPLOCK_MONITOR_HANDLE_MAP_MAX_COUNT\n", __FUNCTION__);
      goto error;
   }

   /*
    * If there are multiple monitor request for the same file, we should open
    * the file only once, and add all the callback functions into one double
    * link list.
    */
   if (HashTable_Lookup(gOplockMonitorMap, utf8Name, (void **)&monitorData)) {
      callbackItem = Util_SafeMalloc(sizeof *callbackItem);
      handle = (HOM_HANDLE)callbackItem;
      DblLnkLst_Init(&callbackItem->links);
      callbackItem->handle = handle;
      callbackItem->callback = callback;
      callbackItem->data = data;
      DblLnkLst_LinkLast(&monitorData->callbackList,
                         &callbackItem->links);
      HashTable_Insert(gOplockMonitorHandleMap,
                       AS_KEY(handle),
                       (void *)monitorData);
      MXUser_ReleaseExclLock(oplockMonitorLock);
      return handle;
   }

   memset(&openInfo, 0, sizeof(openInfo));
   openInfo.mask = HGFS_OPEN_VALID_MODE | HGFS_OPEN_VALID_SHARE_ACCESS;
   openInfo.mode = HGFS_OPEN_MODE_READ_ONLY;
#ifdef _WIN32
   openInfo.shareAccess = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
#endif
   openInfo.flags = HGFS_OPEN;
   openInfo.utf8Name = utf8Name;
   openInfo.shareInfo.readPermissions = TRUE;
   status = HgfsPlatformValidateOpen(&openInfo, TRUE, session,
                                     &localId, &newHandle);

   if (status != HGFS_ERROR_SUCCESS) {
      LOG(4, "%s: Failed to open file: %s\n", __FUNCTION__, utf8Name);
      goto error;
   }

   monitorData = Util_SafeMalloc(sizeof *monitorData);
   monitorData->fileDesc = newHandle;
   monitorData->utf8Name = Util_SafeStrdup(utf8Name);
   DblLnkLst_Init(&monitorData->callbackList);

   if (!HgfsAcquireAIOServerLock(newHandle,
                                 session,
                                 &serverLock,
                                 HgfsOplockMonitorFileChangeCallback,
                                 monitorData)) {
      HgfsPlatformCloseFile(newHandle, NULL);
      LOG(4, "%s: Failed to acquire server lock for file: %s\n", __FUNCTION__, utf8Name);
      goto error;
   }

   callbackItem = Util_SafeMalloc(sizeof *callbackItem);
   handle = (HOM_HANDLE)callbackItem;
   DblLnkLst_Init(&callbackItem->links);
   callbackItem->handle = handle;
   callbackItem->callback = callback;
   callbackItem->data = data;
   DblLnkLst_LinkLast(&monitorData->callbackList,
                      &callbackItem->links);

   HashTable_Insert(gOplockMonitorMap, utf8Name, (void *)monitorData);
   HashTable_Insert(gOplockMonitorHandleMap, AS_KEY(handle), (void *)monitorData);
   MXUser_ReleaseExclLock(oplockMonitorLock);
   return handle;

error:
   if (monitorData) {
      free(monitorData->utf8Name);
      free(monitorData);
   }
   free(data);

   MXUser_ReleaseExclLock(oplockMonitorLock);
   return HGFS_OPLOCK_INVALID_MONITOR_HANDLE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsOplockUnmonitorFileChangeInternal --
 *
 *    Cancel the monitor action by closing the file descriptor.
 *    The lock for oplockMonitorLock should be acquired before calling this
 *    funcion.
 *    All the objects that related to handle will be released when returns
 *    from this function.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

void
HgfsOplockUnmonitorFileChangeInternal(HOM_HANDLE handle)             // IN:
{
   oplockMonitorData *monitorData = NULL;
   DblLnkLst_Links *link, *nextLink;
   oplockMonitorCallbackList *callbackItem;

   if (HashTable_Lookup(gOplockMonitorHandleMap,
                        AS_KEY(handle),
                        (void **)&monitorData)) {
      HashTable_Delete(gOplockMonitorHandleMap, AS_KEY(handle));

      DblLnkLst_ForEachSafe(link, nextLink, &monitorData->callbackList) {
         callbackItem = DblLnkLst_Container(link, oplockMonitorCallbackList, links);
         if (callbackItem->handle == handle) {
            DblLnkLst_Unlink1(&callbackItem->links);
            free(callbackItem->data);
            free(callbackItem);
            break;
         }
      }

      /*
       * Close the file if no one is monitoring it anymore.
       */
      if (DblLnkLst_IsLinked(&monitorData->callbackList) == FALSE) {
         HashTable_Delete(gOplockMonitorMap, monitorData->utf8Name);
         HgfsRemoveAIOServerLock(monitorData->fileDesc);
         free(monitorData->utf8Name);
         free(monitorData);
      }
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsOplockUnmonitorFileChange --
 *
 *    Cancel the monitor action.
 *    All the objects that related to handle will be released when returns
 *    from this function.
 *    the caller provided callback will not be called.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

void
HgfsOplockUnmonitorFileChange(HOM_HANDLE handle)             // IN:
{
   /*
    * This function is a callback function and may be called at any time, even
    * when the oplock monitor module is destroyed.
    * So, check if the oplock monitor module is initialized.
    */
   if (!gOplockMonitorInit) {
      Log("%s: OplockMonitor module is not inited\n", __FUNCTION__);
      return;
   }

   MXUser_AcquireExclLock(oplockMonitorLock);
   HgfsOplockUnmonitorFileChangeInternal(handle);
   MXUser_ReleaseExclLock(oplockMonitorLock);
}
