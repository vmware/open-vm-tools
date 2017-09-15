/*********************************************************
 * Copyright (C) 2007-2016 VMware, Inc. All rights reserved.
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
 * unicodeStatic.c --
 *
 *      Manages memory for static const char *literal strings
 *      created like:
 *
 *         const char *c = U_UNESCAPE("Copyright \\u00A9 VMware, Inc.");
 *
 *      Uses two HashTables to hold static const char *strings. Static
 *      const char *strings are keyed off the ASCII bytes passed to the
 *      static macros.
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
#include "vm_atomic.h"
#include "hashTable.h"
#include "unicodeBase.h"
#include "unicodeInt.h"
#include "util.h"

/* These are Implicitly initialized to NULL */
static Atomic_Ptr UnicodeStringTable;
static Atomic_Ptr UnicodeUnescapedStringTable;


/*
 *-----------------------------------------------------------------------------
 *
 * UnicodeHashFree --
 *
 *      Called by the hash table functions when a value must be replaced.
 *
 * Results:
 *	The argument, a unicode, is freed.
 *
 * Side effects:
 *	None.
 *
 *-----------------------------------------------------------------------------
 */

static void
UnicodeHashFree(void *v)  // IN:
{
   free(v);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_GetStatic --
 *
 *      Helper function for the U_UNESCAPE() macro.
 *
 *      Given a NUL-terminated ASCII string, returns a const char *
 *      string containing the string's contents.
 *
 *      If unescape is TRUE, then \\uABCD becomes the Unicode code
 *      point U+ABCD and \\U001FABCD becomes the Unicode code point
 *      U+1FABCD in the resulting string.
 *
 * Results:
 *      A const char * string.  Memory is managed inside this module;
 *      caller does not need to free.
 *
 * Side effects:
 *      Creates the UnicodeStringTable and UnicodeUnescapedStringTable hash
 *      tables if it don't yet exist.
 *      Creates and inserts a Unicode string into the appropriate static
 *      string table if the key 'asciiBytes' is not found in the table.
 *
 *-----------------------------------------------------------------------------
 */

const char *
Unicode_GetStatic(const char *asciiBytes, // IN
                  Bool unescape)          // IN
{
   char *result = NULL;
   HashTable *stringTable;

   if (unescape) {
      stringTable = HashTable_AllocOnce(&UnicodeUnescapedStringTable, 4096, 
                                        HASH_FLAG_ATOMIC | HASH_STRING_KEY,
                                        UnicodeHashFree);
   } else {
      stringTable = HashTable_AllocOnce(&UnicodeStringTable, 4096, 
                                        HASH_FLAG_ATOMIC | HASH_STRING_KEY,
                                        UnicodeHashFree);
   }

   /*
    * Attempt a lookup for the key value; if it is found things are easy and
    * fine. Otherwise HashTable_LookupOrInsert is used to attempt to enter
    * the data in a racey manner. Should multiple threads attempt to enter
    * the same key concurrently one thread will get the entered data and the
    * other threads will detect that their entries were rejected; they
    * discard their copies of the data and use the entered data (values
    * will be stable).
    */

   if (!HashTable_Lookup(stringTable, asciiBytes, (void **) &result)) {
      char *newData = UnicodeAllocStatic(asciiBytes, unescape);

      if (newData) {
         result = HashTable_LookupOrInsert(stringTable, asciiBytes, newData);

         if (result != newData) {
            free(newData);
         }
      }
   }

   return result;
}
