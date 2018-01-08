/*********************************************************
 * Copyright (C) 2008-2017 VMware, Inc. All rights reserved.
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
 * posixInt.h --
 *
 *	Internal definitions for the Posix wrapper module.
 */

#ifndef _POSIXINT_H_
#define _POSIXINT_H_

#define INCLUDE_ALLOW_USERLEVEL
#include "includeCheck.h"

#include "posix.h"
#include "unicode.h"
#include "hashTable.h"
#include "vm_atomic.h"
#include "util.h"
#include "str.h"


#ifndef _WIN32 // {

/*
 *----------------------------------------------------------------------
 *
 * PosixConvertToCurrent --
 *
 *      Utility function to convert a UTF8 string to the current encoding.
 *
 * Results:
 *      TRUE on success.
 *      Conversion result in *out or NULL on failure.
 *      errno is untouched on success, set to UNICODE_CONVERSION_ERRNO
 *      on failure.
 *
 * Side effects:
 *      As described.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
PosixConvertToCurrent(const char *in,   // IN: string to convert
                      char **out)       // OUT: conversion result
{
   int e = errno;
   char *p = Unicode_GetAllocBytes(in, STRING_ENCODING_DEFAULT);
   Bool success = p != NULL || in == NULL;

   if (success) {
      errno = e;
      *out = p;
   } else {
      errno = UNICODE_CONVERSION_ERRNO;
      *out = NULL;
   }
   return success;
}


/*
 *----------------------------------------------------------------------
 *
 * PosixConvertToCurrentList --
 *
 *      Utility function to convert a list of UTF8 strings to the current
 *      encoding. Return NULL if list is NULL.
 *
 * Results:
 *      TRUE on success.
 *      Conversion result in *out or NULL on failure.
 *      errno is untouched on success, set to UNICODE_CONVERSION_ERRNO
 *      on failure.
 *
 * Side effects:
 *      As described.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
PosixConvertToCurrentList(char *const *in,  // IN: list to convert
                          char ***out)      // OUT: conversion result
{
   int e = errno;
   char **p;
   Bool success;

   if (in == NULL) {
      p = NULL;
      success = TRUE;
   } else {
      p = Unicode_GetAllocList(in, -1, STRING_ENCODING_DEFAULT);
      success = p != NULL;
   }

   if (!success) {
      e = UNICODE_CONVERSION_ERRNO;
   }
   *out = p;
   errno = e;
   return success;
}

#endif // }


/*
 * Hash table for Posix_Getenv
 */

typedef struct PosixEnvEntry {
   Atomic_Ptr value;
   Atomic_Ptr lastValue;
} PosixEnvEntry;

static INLINE void
PosixEnvFree(void *v)
{
   // never called
   NOT_REACHED();
}


/*
 *-----------------------------------------------------------------------------
 *
 * PosixGetenvHash --
 *
 *      Save away UTF8 string for Posix_Getenv() to make it persistent.
 *
 * Results:
 *      The value.
 *
 * Side effects:
 *      The passed-in value string may be saved in a hash table,
 *      or it may be freed.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE_SINGLE_CALLER char *
PosixGetenvHash(const char *name,  // IN
                char *value)       // IN/OUT: may be freed
{
   static Atomic_Ptr htPtr;
   HashTable *ht;
   char *oldValue;
   PosixEnvEntry *e;

   /*
    * Don't need to save NULL.
    */

   if (value == NULL) {
      return value;
   }

   ht = HashTable_AllocOnce(&htPtr, 128,
                            HASH_FLAG_ATOMIC | HASH_FLAG_COPYKEY |
                               HASH_STRING_KEY,
                            PosixEnvFree);

   /*
    * We have to allow any number of concurrent getenv()'s.
    * So if the saved value is the same, don't change it,
    * and if it must change, all the threads together must
    * establish a single new value.
    *
    * On the other hand, we don't have to support concurrent
    * getenv() and setenv().
    */

   for (;;) {
      /*
       * If not in hash table, then insert and return.
       */

      if (!HashTable_Lookup(ht, name, (void **) &e)) {
         e = Util_SafeMalloc(sizeof *e);
         Atomic_WritePtr(&e->value, value);
         Atomic_WritePtr(&e->lastValue, NULL);
         if (!HashTable_Insert(ht, name, e)) {
            Posix_Free(e);
            continue;
         }
         break;
      }

      /*
       * If old value is the same, then use it.
       */

      oldValue = Atomic_ReadPtr(&e->value);
      if (Str_Strcmp(oldValue, value) == 0) {
         Posix_Free(value);
         value = oldValue;
         break;
      }

      /*
       * If value has changed, use new value, but don't free old
       * value yet because somebody else may be going through this
       * same code and still using it.  Because we don't need
       * to care about concurrent setenv(), a single lastValue
       * slot is sufficient.
       */

      if (Atomic_ReadIfEqualWritePtr(&e->value, oldValue, value) == oldValue) {
         oldValue = Atomic_ReadWritePtr(&e->lastValue, oldValue);
         Posix_Free(oldValue);
         break;
      }
   }

   return value;
}

#endif // ifndef _POSIXINT_H_
