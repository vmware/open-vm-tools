/*********************************************************
 * Copyright (C) 2013-2019 VMware, Inc. All rights reserved.
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
 * cache.c --
 *
 * Module-specific components of the vmhgfs driver.
 */
#include "module.h"
#if !defined(__FreeBSD__) && !defined(__SOLARIS__)
#include <glib.h>
#endif

/*
 * We make the default attribute cache timeout 1 second which is the same
 * as the FUSE driver.
 * This can be overridden with the mount option attr_timeout=T
 */
#define CACHE_TIMEOUT HGFS_DEFAULT_TTL
#define CACHE_PURGE_TIME 10
#define CACHE_PURGE_SLEEP_TIME 30
#define HASH_THRESHOLD_SIZE (2046 * 4)
#define HASH_PURGE_SIZE (HASH_THRESHOLD_SIZE / 2)
#include "cache.h"

/*
 * HgfsAttrCache, holds an entry for each path
 */

typedef struct HgfsAttrCache {
   HgfsAttrInfo attr; /* Attribute of a file or directory */
   uint64 changeTime; /* time the attribute was last updated */
   struct list_head list; /* used in linked list implementation */
   char path[0];      /* path of the file corresponding the the attr */
} HgfsAttrCache;


/* Head of the list */
struct HgfsAttrCache attrList;

/*Lock for accessing the attribute cache*/
static pthread_mutex_t HgfsAttrCacheLock = PTHREAD_MUTEX_INITIALIZER;

#if !defined(__FreeBSD__) && !defined(__SOLARIS__)
static void HgfsInvalidateParentsChildren(const char* parent);
#endif

/*
 * Lists are used to manage attribute cache in Solaris and FreeBSD,
 * HashTables are used in Linux. HashTables perform better and hence
 * once newer version of glib with hash tables are packaged for Solaris
 * and FreeBSD, this section will go away.
 */

#if defined(__FreeBSD__) || defined(__SOLARIS__)

/*
 *----------------------------------------------------------------------
 *
 * HgfsInitCache
 *
 *    Initializes the list attrList to cache attributes.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 *
 */

void
HgfsInitCache()
{
   INIT_LIST_HEAD(&attrList.list);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsGetAttrCache
 *
 *    Retrieves the attr from the list for a given path.
 *
 * Results:
 *    0 on success else -1 on error
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

int
HgfsGetAttrCache(const char* path,   //IN: Path of file or directory
                 HgfsAttrInfo *attr) //IN: Attribute for a given path
{
   HgfsAttrCache *tmp;
   int res = -1;
   int diff;

   pthread_mutex_lock(&HgfsAttrCacheLock);

   attrList.changeTime = HGFS_GET_CURRENT_TIME();
   list_for_each_entry(tmp, &attrList.list, list) {
      LOG(4, ("tmp->path = %d\n", tmp->path));
      if (strcmp(path, tmp->path) == 0) {
         LOG(4, ("cache hit. path = %s\n", tmp->path));

         diff = (HGFS_GET_CURRENT_TIME() - tmp->changeTime) / 10000000;

         LOG(4, ("time since last updated is %d seconds\n", diff));
         if (diff <= CACHE_TIMEOUT) {
            *attr = tmp->attr;
            res = 0;
         }
         break;
      }
   }

   pthread_mutex_unlock(&HgfsAttrCacheLock);
   return res;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsSetAttrCache
 *
 *    Updates the list with the given (key, attr)pair.
 *
 * Results:
 *    0 on success else negative value on error
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

int
HgfsSetAttrCache(const char* path,   //IN: Path of file or directory
                 HgfsAttrInfo *attr) //IN: Attribute for a given path
{
   HgfsAttrCache *tmp;
   int res = 0;

   pthread_mutex_lock(&HgfsAttrCacheLock);

   attrList.changeTime = HGFS_GET_CURRENT_TIME();
   list_for_each_entry (tmp, &attrList.list, list) {
      if (strcmp(path, tmp->path) == 0) {
         tmp->attr = *attr;
         tmp->changeTime = HGFS_GET_CURRENT_TIME();
         LOG(4, ("cache entry updated. path = %s\n", tmp->path));
         goto out;
      }
   }

   tmp = malloc(sizeof(HgfsAttrCache) + strlen(path) + 1);
   if (tmp == NULL) {
      res = -ENOMEM;
      goto out;
   }
   INIT_LIST_HEAD(&tmp->list);
   Str_Strcpy(tmp->path, path, strlen(path) + 1);
   tmp->attr = *attr;
   tmp->changeTime = HGFS_GET_CURRENT_TIME();
   list_add(&tmp->list, &attrList.list);
   LOG(4, ("cache entry added. path = %s\n", tmp->path));

out:
   pthread_mutex_unlock(&HgfsAttrCacheLock);
   return res;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsInvalidateAttrCache
 *
 *    Invalidate the attribute list entry for a given path.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

void
HgfsInvalidateAttrCache(const char* path)      //IN: Path to file
{
   HgfsAttrCache *tmp;

   pthread_mutex_lock(&HgfsAttrCacheLock);

   list_for_each_entry (tmp, &attrList.list, list) {
      if (strcmp(path, tmp->path) == 0) {
         list_del(&tmp->list);
         free(tmp);
         break;
      }
   }

   pthread_mutex_unlock(&HgfsAttrCacheLock);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsPurgeCache:
 *
 *    This routine is called by an independent thread to purge the cache,
 *    deletion is based on time of last update.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

void*
HgfsPurgeCache(void* unused) //IN: Thread argument
{
   HgfsAttrCache *tmp;
   HgfsAttrCache *prev;
   int diff;

   while (1)
   {
      sleep(CACHE_PURGE_SLEEP_TIME);

      pthread_mutex_lock(&HgfsAttrCacheLock);

      list_for_each_entry_safe (tmp, prev, &attrList.list, list) {
         diff = (HGFS_GET_CURRENT_TIME() - tmp->changeTime) / 10000000;
         if (diff > CACHE_PURGE_TIME) {
            list_del (&tmp->list);
            free (tmp);
         }
      }

      pthread_mutex_unlock(&HgfsAttrCacheLock);
   }
   return 0;
}

#else


GHashTable *g_hash_table;

/*
 *----------------------------------------------------------------------
 *
 * HgfsInitCache
 *
 *    Creates a hash table with string as a key.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *----------------------------------------------------------------------
 *
 */

void
HgfsInitCache()
{
   g_hash_table = g_hash_table_new(g_str_hash, g_str_equal);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsGetAttrCache
 *
 *    Retrieves the attr from the HashTable for a given path.
 *
 * Results:
 *    0 on success else -1 on error
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

int
HgfsGetAttrCache(const char* path,   //IN: Path of file or directory
                 HgfsAttrInfo *attr) //IN: Attribute for a given path
{
   HgfsAttrCache *tmp;
   int res = -1;

   pthread_mutex_lock(&HgfsAttrCacheLock);

   tmp = (HgfsAttrCache *)g_hash_table_lookup(g_hash_table, path);
   if (tmp != NULL) {
      int diff;

      LOG(4, ("cache hit. path = %s\n", tmp->path));

      diff = (HGFS_GET_CURRENT_TIME() - tmp->changeTime) / 10000000;
      LOG(4, ("time since last updated is %d seconds\n", diff));
      if ( diff <= CACHE_TIMEOUT ) {
         *attr = tmp->attr;
         res = 0;
      }
   }

   pthread_mutex_unlock(&HgfsAttrCacheLock);
   return res;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsSetAttrCache
 *
 *    Updates the HashTable with the given (key, attr) pair.
 *
 * Results:
 *    0 on success else negative value on error
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

int
HgfsSetAttrCache(const char* path,         //IN: Path of file or directory
                 HgfsAttrInfo *attr)       //IN: Attribute for a given path
{
   HgfsAttrCache *tmp;
   int res = 0;

   pthread_mutex_lock(&HgfsAttrCacheLock);

   tmp = (HgfsAttrCache *)g_hash_table_lookup(g_hash_table, path);
   if (tmp != NULL) {
      tmp->attr = *attr;
      tmp->changeTime = HGFS_GET_CURRENT_TIME();
      goto out;
   }

   tmp = malloc(sizeof(HgfsAttrCache) + strlen(path) + 1);
   if (tmp == NULL) {
      res = -ENOMEM;
      goto out;
   }

   Str_Strcpy(tmp->path, path, strlen(path) + 1);
   tmp->attr = *attr;
   tmp->changeTime = HGFS_GET_CURRENT_TIME();

   g_hash_table_insert(g_hash_table, (gpointer)tmp->path, (gpointer)tmp);

out:
   pthread_mutex_unlock(&HgfsAttrCacheLock);
   return res;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsInvalidateAttrCache
 *
 *    Invalidate the HashTable entry for a path.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

void
HgfsInvalidateAttrCache(const char* path)      //IN: Path to file
{
   HgfsAttrCache *tmp;

   pthread_mutex_lock(&HgfsAttrCacheLock);
   tmp = (HgfsAttrCache *)g_hash_table_lookup(g_hash_table, path);
   if (tmp != NULL) {
      tmp->changeTime = 0;
      if (tmp->attr.type == HGFS_FILE_TYPE_DIRECTORY) {
         HgfsInvalidateParentsChildren(tmp->path);
      }
   }
   pthread_mutex_unlock(&HgfsAttrCacheLock);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsInvalidateParentsChildren
 *
 *    This routine is called by the general function to invalidate a cache
 *    entry. If the entry is a directory this function is called to invalidate
 *    any cached children.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static void
HgfsInvalidateParentsChildren(const char* parent)      //IN: parent
{
   gpointer key, value;
   GHashTableIter iter;
   size_t parentLen = Str_Strlen(parent, PATH_MAX);

   LOG(4, ("Invalidating cache children for parent = %s\n",
           parent));

   g_hash_table_iter_init(&iter, g_hash_table);

   while (g_hash_table_iter_next(&iter, &key, &value)) {
      HgfsAttrCache *child = (HgfsAttrCache *)value;

      if (Str_Strncasecmp(parent, child->path, parentLen) == 0) {
         LOG(10, ("Invalidating cache child = %s\n", child->path));
         child->changeTime = 0;
      }
   }
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsPurgeCache
 *
 *    This routine is called by an independent thread to purge the cache,
 *    for performance reasons, the deletion is done in random based
 *    on the order of iteration.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

void*
HgfsPurgeCache(void* unused)      //IN: Thread argument
{
   gpointer key, value;
   GHashTableIter iter;

   while (1) {
      sleep(CACHE_PURGE_SLEEP_TIME);

      pthread_mutex_lock(&HgfsAttrCacheLock);

      if (g_hash_table_size(g_hash_table) >= HASH_THRESHOLD_SIZE) {
         g_hash_table_iter_init(&iter, g_hash_table);
         while (g_hash_table_iter_next(&iter, &key, &value) &&
               (g_hash_table_size(g_hash_table) >= HASH_PURGE_SIZE)) {
            g_hash_table_iter_remove(&iter);
            free(value);
         }
      }

      pthread_mutex_unlock(&HgfsAttrCacheLock);
   }
   return 0;
}
#endif
