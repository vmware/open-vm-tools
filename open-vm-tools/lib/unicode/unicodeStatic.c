/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/

/*
 * unicodeStatic.c --
 *
 *      Manages memory for static ConstUnicode literal strings
 *      created like:
 *
 *         ConstUnicode foo = U("foo");
 *
 *      and
 *
 *         ConstUnicode c = U_UNESCAPE("Copyright \\u00A9 VMware, Inc.");
 *
 *      Uses two HashTables to hold static ConstUnicode strings,
 *      protected by a single mutex.  Static ConstUnicode strings are
 *      keyed off the ASCII bytes passed to the static macros.
 *
 *      Unescaped strings are kept separate from escaped strings so
 *      users can expect a literal "\\" to stay as-is by default.
 *
 *      This implementation exists to works around gcc's lack of an
 *      intrinsic 16-bit wide character type; wchar_t on gcc is 32
 *      bits wide, which is not useful for most Unicode algorithms.
 *
 *      Note that the Win32 compiler does have an intrinsic 16-bit
 *      wide character type (L"foo"), so we can optimize for that case
 *      in a later implementation.
 *
 *      If GCC later offers an intrinsic 16-bit wide character type,
 *      we can completely get rid of this implementation.
 */

#include "vmware.h"
#include "hashTable.h"
#include "syncMutex.h"
#include "unicodeBase.h"
#include "unicodeInt.h"
#include "util.h"

static HashTable *UnicodeStaticStringTable = NULL;
static HashTable *UnicodeStaticUnescapedStringTable = NULL;

static void UnicodeStaticCreateTableIfNeeded(HashTable **table);


/*
 *-----------------------------------------------------------------------------
 *
 * UnicodeStaticCreateTableIfNeeded --
 *
 *      Helper function to create a static 7-bit ASCII -> Unicode
 *      string table if needed.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      If *table is NULL, allocates a HashTable to use ASCII string keys
 *      and stores the result in *table.
 *
 *-----------------------------------------------------------------------------
 */

void
UnicodeStaticCreateTableIfNeeded(HashTable **table) // OUT
{
   if (!*table) {
      /*
       * We don't supply a free function, since these strings are
       * never freed.
       *
       * We use HASH_STRING_KEY to use the 7-bit ASCII strings passed
       * to Unicode_GetStatic() as the lookup key into the hash, to
       * avoid creating the same string multiple times.
       */
      *table = HashTable_Alloc(4096, HASH_STRING_KEY, NULL);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_GetStatic --
 *
 *      Helper function for U() and U_UNESCAPE() macros.
 *
 *      Given a NUL-terminated ASCII string, returns a ConstUnicode
 *      string containing the string's contents.
 *
 *      If unescape is TRUE, then \\uABCD becomes the Unicode code
 *      point U+ABCD and \\U001FABCD becomes the Unicode code point
 *      U+1FABCD in the resulting string.
 *
 * Results:
 *      A ConstUnicode string.  Memory is managed inside this module;
 *      caller does not need to free.
 *
 * Side effects:
 *      Creates UnicodeStaticStringTable and UnicodeStaticUnescapedStringTable
 *      if they don't yet exist.
 *      Creates and inserts a Unicode string into the appropriate static
 *      string table if the key 'asciiBytes' is not found in the table.
 *
 *-----------------------------------------------------------------------------
 */

ConstUnicode
Unicode_GetStatic(const char *asciiBytes, // IN
                  Bool unescape)          // IN
{
   static Atomic_Ptr lckStorage;
   SyncMutex *lck;
   Unicode result = NULL;
   HashTable *stringTable;

   lck = SyncMutex_CreateSingleton(&lckStorage);
   SyncMutex_Lock(lck);

   UnicodeStaticCreateTableIfNeeded(&UnicodeStaticUnescapedStringTable);
   UnicodeStaticCreateTableIfNeeded(&UnicodeStaticStringTable);

   if (unescape) {
      stringTable = UnicodeStaticUnescapedStringTable;
   } else {
      stringTable = UnicodeStaticStringTable;
   }

   if (!HashTable_Lookup(stringTable, asciiBytes, (void **)&result)) {
      result = UnicodeAllocStatic(asciiBytes, unescape);

      if (result) {
         HashTable_Insert(stringTable, asciiBytes, result);
      }
   }

   SyncMutex_Unlock(lck);

   return result;
}
