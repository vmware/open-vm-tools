/*********************************************************
 * Copyright (C) 1998-2016,2020 VMware, Inc. All rights reserved.
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
 * err.c --
 *
 *      General error handling library
 *
 */

#include "vmware.h"
#include "errInt.h"
#include "str.h"
#include "vm_atomic.h"
#include "hashTable.h"
#include "util.h"
#include "codeset.h"


/*
 * Constants
 */

#define HASHTABLE_SIZE 2048


/*
 * Types
 */

typedef struct ErrInfo {
   Err_Number number;
   char *string;
} ErrInfo;


/*
 * Variables
 */

/*
 * We statically link lib/err in several libraries. This means that a
 * single binary may have several copies of lib/err. These pointers are
 * not static so that we have one copy across the entire binary.
 */
Atomic_Ptr errNumTable;
Atomic_Ptr errPtrTable;
#if defined VMX86_DEBUG && defined __linux__
Atomic_Ptr errStrTable;
#endif

#define NUMTABLE() HashTable_AllocOnce(&errNumTable, HASHTABLE_SIZE, \
                                       HASH_INT_KEY | HASH_FLAG_ATOMIC, \
                                       ErrFreeErrInfo)
#define PTRTABLE() HashTable_AllocOnce(&errPtrTable, HASHTABLE_SIZE, \
                                       HASH_INT_KEY | HASH_FLAG_ATOMIC, NULL)
#if defined VMX86_DEBUG && defined __linux__
#define STRTABLE() HashTable_AllocOnce(&errStrTable, HASHTABLE_SIZE, \
                                       HASH_STRING_KEY | HASH_FLAG_ATOMIC, \
                                       NULL)
#endif


/*
 *----------------------------------------------------------------------
 *
 * ErrFreeErrInfo --
 *
 *      HashTableFreeEntryFn helper function to free ErrInfo struct.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
ErrFreeErrInfo(void *pErrInfo) // IN
{
   ErrInfo *errInfo = pErrInfo;
   if (errInfo) {
      free(errInfo->string);
      free(errInfo);
   }
}


/*
 *----------------------------------------------------------------------
 *
 * Err_ErrString --
 *
 *      Returns a string that corresponds to the last error message.
 *      The error number used is that which is native to the platform,
 *      errno on POSIXen and GetLastError on Windows.
 *
 * Results:
 *      Error message string.
 *
 * Side effects:
 *      None.
 *	Current error number is preserved.
 *
 *----------------------------------------------------------------------
 */

const char *
Err_ErrString(void)
{
   return Err_Errno2String(Err_Errno());
}


/*
 *----------------------------------------------------------------------
 *
 * Err_Errno2String --
 *
 *      Return a string that corresponds to the passed error number.
 *      The error number used is that which is native to the platform,
 *      errno on POSIXen and GetLastError on Windows.
 *
 *	The string is in English in UTF-8, has indefinite lifetime,
 *	and need not be freed.
 *
 * Results:
 *      Error message string in UTF-8.
 *
 * Side effects:
 *      None.
 *	Current error number is preserved.
 *
 *----------------------------------------------------------------------
 */

const char *
Err_Errno2String(Err_Number errorNumber) // IN
{
   HashTable *numTable;
   HashTable *ptrTable;
   ErrInfo *info;
   ErrInfo *oldInfo;
   Err_Number oldErrno = Err_Errno();

   ASSERT(errorNumber != ERR_INVALID);

   /*
    * Look up the error in numTable.
    * Or insert it if it's not there.
    */

   numTable = NUMTABLE();
   if (!HashTable_Lookup(numTable, (void *) (uintptr_t) errorNumber,
			 (void **) &info)) {
      char buf[2048];
      const char *p;
      size_t n;

      /*
       * Convert number to string and build the info structure.
       */

      p = ErrErrno2String(errorNumber, buf, sizeof buf);

      info = Util_SafeMalloc(sizeof *info);
      info->number = errorNumber;
      info->string = Util_SafeStrdup(p);

      /*
       * To be safe, make sure the end of the string is at
       * a UTF-8 boundary, but we can only do this when the
       * string is in our buffer (it may not be).
       */

      n = strlen(info->string);
      n = CodeSet_Utf8FindCodePointBoundary(info->string, n);
      info->string[n] = '\0';

      /*
       * Try to insert new info into numTable.
       * If that fails, then we must have lost out to someone else.
       * Use theirs in that case.
       */

      oldInfo = HashTable_LookupOrInsert(numTable,
				         (void *) (uintptr_t) errorNumber,
				         info);
      if (oldInfo != info) {
	 ASSERT(oldInfo->number == info->number);
	 ASSERT(Str_Strcmp(oldInfo->string, info->string) == 0);
	 free(info->string);
	 free(info);
	 info = oldInfo;
      }
   }

   /*
    * Try to insert info into ptrTable.
    * We need to do it even if we didn't create this entry,
    * because we may get here before the other guy (who created
    * the entry and inserted it into numTable).
    */

   ptrTable = PTRTABLE();
   oldInfo = HashTable_LookupOrInsert(ptrTable, info->string, info);
   ASSERT(oldInfo == info);

#if defined VMX86_DEBUG && defined __linux__
   {
      HashTable *strTable = STRTABLE();
      ErrInfo *i = HashTable_LookupOrInsert(strTable, info->string, info);
      ASSERT(i == info);
   }
#endif

   Err_SetErrno(oldErrno);
   return info->string;
}


/*
 *----------------------------------------------------------------------
 *
 * Err_String2Errno --
 *
 *      Return an error number that corresponds to the passed string.
 *      The error number used is that which is native to the platform,
 *      errno on POSIXen and GetLastError on Windows.
 *
 *	To be recognized, the string must be one previously returned
 *	by Err_Errno2String.  Any other string (even a copy of
 *	a valid error string) returns ERR_INVALID.
 *
 * Results:
 *      Error number or ERR_INVALID.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Err_Number
Err_String2Errno(const char *string) // IN
{
   HashTable *ptrTable = PTRTABLE();
   ErrInfo *info;

   if (!HashTable_Lookup(ptrTable, string, (void **) &info)) {
      return ERR_INVALID;
   }

   ASSERT(info->string == string);
   ASSERT(info->number != ERR_INVALID);
   return info->number;
}


/*
 *----------------------------------------------------------------------
 *
 * Err_Exit --
 *
 *      Reclaim memory.  Useful for avoiding leaks at exit being
 *      reported by valgrind / Memory Validator.
 *
 *      Assumes that no other threads are calling into bora/lib/err.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
Err_Exit(void) // IN
{
   HashTable *numTable = NUMTABLE();
   HashTable *ptrTable = PTRTABLE();
#if defined VMX86_DEBUG && defined __linux__
   HashTable *strTable = STRTABLE();

   HashTable_FreeUnsafe(strTable);
#endif
   HashTable_FreeUnsafe(ptrTable);
   HashTable_FreeUnsafe(numTable);
}


#ifdef VMX86_DEBUG
/*
 *----------------------------------------------------------------------
 *
 * Err_String2ErrnoDebug --
 *
 *      Return an error number that corresponds to the passed string.
 *
 *	This is the debug version that uses the whole string as key,
 *	instead of just the address.
 *
 * Results:
 *      Error number or ERR_INVALID.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Err_Number
Err_String2ErrnoDebug(const char *string) // IN
{
#ifdef __linux__
   HashTable *strTable = STRTABLE();
   ErrInfo *info;

   if (!HashTable_Lookup(strTable, string, (void **) &info)) {
      return ERR_INVALID;
   }

   ASSERT(Str_Strcmp(info->string, string) == 0);
   ASSERT(info->number != ERR_INVALID);
   if (info->string != string) {
      Log("%s: errno %d, string \"%s\" at %p, originally at %p.\n",
	  __FUNCTION__, info->number, string, string, info->string);
   }
   return info->number;
#else
   return ERR_INVALID;
#endif
}
#endif
