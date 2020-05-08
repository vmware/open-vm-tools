/*********************************************************
 * Copyright (C) 2012-2017,2019 VMware, Inc. All rights reserved.
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

/*********************************************************
 * The contents of this file are subject to the terms of the Common
 * Development and Distribution License (the "License") version 1.0
 * and no later version.  You may not use this file except in
 * compliance with the License.
 *
 * You can obtain a copy of the License at
 *         http://www.opensource.org/licenses/cddl1.php
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 *********************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include "vm_basic_types.h"
#include "str.h"
#include "dataMap.h"
#include "vm_ctype.h"

/*
 * A union for all kinds of data fields types.
 */
typedef union {
   struct {
      int64  val;
   } number;

   struct {
      int32 length;
      char *str;
   } string;

   struct {
      int32 length;
      int64 *numbers;
   } numList;

   struct {
      char **strings; /* a list of string pointers, the last elment is NULL */
      int32 *lengths; /* a list of lengths for strings in strings. */
   } strList;
} DMFieldValue;

typedef struct {
   DMFieldType type;
   DMFieldValue value;
} DataMapEntry;

/* structure used in hashMap iteration callback */
typedef struct {
   DataMap *map;
   ErrorCode result;   /* store the previous callback function result value */

   /* the followings are used during serialization, deserialization and
    * pretty print.
    */
   char *buffer;
   uint32 buffLen;        /* available buffer size */
   uint32 maxNumElems;    /* this limits the number of elements for list print. */
   uint32 maxStrLen;      /* max number of bytes to print for each string */
   FieldIdNameEntry *fieldIdList;   /* array for field ID to name mapping */
   uint32 fieldIdListLen; /* fieldIdList size */
} ClientData;

static const uint64 magic_cookie = 0x4d41474943ULL;   /* 'MAGIC' */

/*
 *-----------------------------------------------------------------------------
 *
 * AddEntry_Int64 --
 *
 *      - low level helper function to add a numeric type entry to the map
 *
 * Result:
 *      0 on success
 *      error code otherwise
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static ErrorCode
AddEntry_Int64(DataMap *that,      // IN/OUT
               DMKeyType key,      // IN
               int64  value)       // IN
{
   DataMapEntry *entry = (DataMapEntry *)malloc(sizeof(DataMapEntry));

   if (entry == NULL) {
      return DMERR_INSUFFICIENT_MEM;
   }
   entry->type = DMFIELDTYPE_INT64;
   entry->value.number.val = value;
   if (!HashMap_Put(that->map, &key, &entry)) {
      return DMERR_INSUFFICIENT_MEM;
   }
   return DMERR_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * AddEntry_String --
 *
 *      Low level helper function to add a string type entry to the map.
 *      - 'str': ownership of the str pointer is passed to the map on success.
 *
 * Result:
 *      0 on success
 *      error code otherwise
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static ErrorCode
AddEntry_String(DataMap *that,      // IN/OUT
                DMKeyType  key,     // IN
                char *str,          // IN
                int32 strLen)       // IN
{
   DataMapEntry *entry = (DataMapEntry *)malloc(sizeof(DataMapEntry));

   if (entry == NULL) {
      return DMERR_INSUFFICIENT_MEM;
   }
   entry->type = DMFIELDTYPE_STRING;
   entry->value.string.str = str;
   entry->value.string.length = strLen;

   if (!HashMap_Put(that->map, &key, &entry)) {
      return DMERR_INSUFFICIENT_MEM;
   }
   return DMERR_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * AddEntry_Int64List --
 *
 *      Low level helper function to add a list of numbers to the map
 *      - 'numbers': ownership of this pointer is passed to the map on success.
 *      - 'listLen': the number of integers in the list of 'numbers'.
 *
 * Result:
 *      0 on success
 *      error code otherwise
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static ErrorCode
AddEntry_Int64List(DataMap *that,            // IN/OUT
                   DMKeyType key,            // IN
                   int64 *numbers,           // IN
                   int32 listLen)            // IN
{
   DataMapEntry *entry = (DataMapEntry *)malloc(sizeof(DataMapEntry));

   if (entry == NULL) {
      return DMERR_INSUFFICIENT_MEM;
   }
   entry->type = DMFIELDTYPE_INT64LIST;
   entry->value.numList.numbers = numbers;
   entry->value.numList.length = listLen;

   if (!HashMap_Put(that->map, &key, &entry)) {
      return DMERR_INSUFFICIENT_MEM;
   }
   return DMERR_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * AddEntry_StringList --
 *
 *      Low level helper function to add a list of trings to the map
 *      - 'strList': this pointer points to an array of pointers which points to
 *        strings. Upon success, the ownership of this array is passed to the
 *        map, no copy of strings is made. The last element in this array
 *        pointer must be NULL.
 *      - 'strLens': this is an array of integers which indicating the length of
 *        cooresponding string in strList. the ownership is passed to the map
 *        as well on success.
 *
 * Result:
 *      0 on success
 *      error code otherwise
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static ErrorCode
AddEntry_StringList(DataMap *that,            // IN/OUT
                    DMKeyType key,            // IN
                    char **strList,           // IN
                    int32 *strLens)           // IN
{
   DataMapEntry *entry = (DataMapEntry *)malloc(sizeof(DataMapEntry));

   if (entry == NULL) {
      return DMERR_INSUFFICIENT_MEM;
   }
   entry->type = DMFIELDTYPE_STRINGLIST;
   entry->value.strList.strings = strList;
   entry->value.strList.lengths = strLens;

   if (!HashMap_Put(that->map, &key, &entry)) {
      return DMERR_INSUFFICIENT_MEM;
   }
   return DMERR_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FreeStringList --
 *
 *      Low level helper function to free a list of strings.
 *
 * Result:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
FreeStringList(char **strList,    // IN
               int32 *strLens)    // IN
{
   if (*strList != NULL) {
      char **ptr;
      for (ptr = strList; *ptr != NULL; ptr++) {
         free(*ptr);
      }
   }

   free(strLens);
   free(strList);
}


/*
 *-----------------------------------------------------------------------------
 *
 * FreeEntryPayload --
 *
 *      - low level helper function to free entry payload only
 *
 * Result:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
FreeEntryPayload(DataMapEntry *entry)    // IN
{
   if (entry == NULL) {
      return;
   }

   switch(entry->type) {
      case DMFIELDTYPE_INT64:
         break;
      case DMFIELDTYPE_STRING:
         free(entry->value.string.str);
         break;
      case DMFIELDTYPE_INT64LIST:
         free(entry->value.numList.numbers);
         break;
      case DMFIELDTYPE_STRINGLIST:
         FreeStringList(entry->value.strList.strings,
                        entry->value.strList.lengths);
         break;
      default:
         ASSERT(0);    /*  we do not expect this to happen */
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * EncodeInt32 --
 *
 *      Low level helper function to encode an int32 into a byte buffer.
 *      - 'buf': *buf points to the output buffer, *buf is advanced properly.
 *
 * Result:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
EncodeInt32(char **buf,   // IN/OUT
            int32 num)    // IN
{
   uint32 netVal = htonl((uint32)num);
   *((uint32 *)(*buf)) = netVal;
   (*buf) += sizeof(int32);
}


/*
 *-----------------------------------------------------------------------------
 *
 * DecodeInt32 --
 *
 *      Low level helper function to decode an int32 from a byte buffer
 *      - 'buf': *buf points to the input buffer. *buf is advanced accordingly
 *               on success.
 *      - 'left': indicates number of bytes left in the input buffer, *left is
 *                updated accordingly on success.
 *
 * Result:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static ErrorCode
DecodeInt32(char **buf,    // IN/OUT
            int32 *left,   // IN/OUT
            int32 *num)    // OUT
{
   uint32 val;

   if (*left < sizeof(int32)) {
      return DMERR_TRUNCATED_DATA;
   }

   val = ntohl(*((uint32 *)(*buf)));
   *num = (int32) val;

   *buf += sizeof(int32);
   *left -= sizeof(int32);

   return DMERR_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * EncodeInt64 --
 *
 *      Low level helper function to encode an int64 into a byte buffer
 *      - 'buf': *buf points to the output buffer, *buf is advanced properly.
 *
 * Result:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
EncodeInt64(char **buf,   // IN/OUT
            int64 num)    // IN
{
   EncodeInt32(buf, (uint32)num);
   EncodeInt32(buf, (uint32)(num >> 32));
}


/*
 *-----------------------------------------------------------------------------
 *
 * DecodeInt64 --
 *
 *      Low level helper function to decode an int64 from a byte buffer
 *      - 'buf': *buf points to the input buffer. *buf is advanced accordingly
 *               on success.
 *      - 'left': indicates number of bytes left in the input buffer, *left is
 *                updated accordingly on success.
 *
 * Result:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static ErrorCode
DecodeInt64(char **buf,    // IN/OUT
            int32 *left,   // IN/OUT
            int64 *num)    // OUT
{
   ErrorCode res;
   uint32 low;
   uint32 high;

   res = DecodeInt32(buf, left, &low);
   if (res == DMERR_SUCCESS) {
      res = DecodeInt32(buf, left, &high);
      if (res == DMERR_SUCCESS) {
         *num = (int64)(((((uint64)high)<< 32) | low));
      }
   }

   return res;
}


/*
 *-----------------------------------------------------------------------------
 *
 * EncodeString --
 *
 *      Low level helper function to encode a string into a byte buffer
 *      - 'buf': *buf points to the output buffer, *buf is advanced properly.
 *
 * Result:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
EncodeString(char **buf,         // IN/OUT
             const char *str,    // IN
             int32 strLen)       // IN
{
   EncodeInt32(buf, strLen);
   memcpy(*buf, str, strLen);
   (*buf) += strLen;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DecodeString --
 *
 *      - low level helper function to decode a string from  a byte buffer
 *
 * Result:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static ErrorCode
DecodeString(char **buf,         // IN/OUT
             int32 *left,        // IN/OUT
             char **str,         // OUT
             int32 *strLen)      // OUT
{
   ErrorCode res;

   res = DecodeInt32(buf, left, strLen);

   if (res != DMERR_SUCCESS) {
      return res;
   }

   if (*strLen <= 0) {
      return DMERR_BAD_DATA;
   }

   if (*left < *strLen) {
      return DMERR_TRUNCATED_DATA;
   }

   *str = (char *)malloc(*strLen);
   if (*str == NULL) {
      return DMERR_INSUFFICIENT_MEM;
   }

   memcpy(*str, *buf, *strLen);
   *buf += *strLen;
   *left -= *strLen;

   return res;
}


/*
 *-----------------------------------------------------------------------------
 *
 * EncodeInt64List --
 *
 *      Low level helper function to encode an int64 list into a byte buffer
 *      - 'buf': *buf points to the output buffer, *buf is advanced properly.
 *
 * Result:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
EncodeInt64List(char **buf,        // IN/OUT
                int64 *numList,    // IN
                int32 listLen)     // IN
{
   int32 i;

   EncodeInt32(buf, (uint32)listLen);

   for(i = 0; i< listLen; i++) {
      EncodeInt64(buf, numList[i]);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * DecodeInt64List --
 *
 *      Low level helper function to decode an int64 list from a byte buffer
 *      - 'buf': *buf points to the input buffer. *buf is advanced accordingly
 *               on success.
 *      - 'left': indicates number of bytes left in the input buffer, *left is
 *                updated accordingly on success.
 *
 * Result:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static ErrorCode
DecodeInt64List(char **buf,        // IN/OUT
                int32 *left,       // IN/OUT
                DMKeyType fieldId, // IN
                DataMap *that)     // OUT
{
   int32 listLen;
   int64 *numList = NULL;
   ErrorCode res;
   int32 i;

   res = DecodeInt32(buf, left, &listLen);
   if (res != DMERR_SUCCESS) {
      return res;
   }

   if (listLen < 0 || listLen > *left / sizeof(int64)) {
      /* listLen can be zero to support an empty list */
      return DMERR_BAD_DATA;
   }

   if (listLen) {
      numList = (int64 *)malloc(sizeof(int64) * listLen);
      if (numList == NULL) {
         return DMERR_INSUFFICIENT_MEM;
      }

      for(i = 0; i< listLen; i++) {
         res = DecodeInt64(buf, left, numList + i);
         if (res != DMERR_SUCCESS) {
            break;
         }
      }
   }

   if (res == DMERR_SUCCESS) {
      res = AddEntry_Int64List(that, fieldId, numList, listLen);
   }

   if (res != DMERR_SUCCESS) {
      /* clean up memory */
      free(numList);
   }

   return res;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FreeEntry --
 *
 *      - low level helper function to free an entry.
 *
 * Result:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
FreeEntry(DataMapEntry *entry)    // IN
{
   FreeEntryPayload(entry);
   free(entry);
}


/*
 *-----------------------------------------------------------------------------
 *
 * LookupEntry --
 *
 *      - helper function to lookup an entry in the data map.
 *
 * Result:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static DataMapEntry *
LookupEntry(const DataMap *that,    // IN
            DMKeyType fieldId)      // IN
{
   if ((that != NULL) && (that->map != NULL)) {
      void *rv = HashMap_Get(that->map, &fieldId);
      if (rv == NULL) {
          return NULL;   /* key not found */
      }
      return *((DataMapEntry **)rv);
   } else {
      return NULL;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * DataMap_GetType --
 *
 *      Get the value type for a given fieldID.
 *
 * Result:
 *      - DMFIELDTYPE_EMPTY is returned if entry does not exist.
 *      - One of other values in DMFieldType otherwise.
 *
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

DMFieldType
DataMap_GetType(const DataMap *that,    // IN
                DMKeyType fieldId)      // IN
{
   DataMapEntry *entry;

   ASSERT(that->cookie == magic_cookie);

   entry = LookupEntry(that, fieldId);
   if (entry == NULL) {
      return DMFIELDTYPE_EMPTY;
   } else {
      return entry->type;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * DataMap_Create --
 *
 *      - Initialize an Empty DataMap using the DataMap storage pointed
 *        by that.
 *      - The memory pointed by that may be allocated from heap or stack.
 *      - This function may allocate additional memory from the heap to
 *        initialize the DataMap object.
 *
 * Result:
 *      0(DMERR_SUCCESS) on success.
 *      error code on failures.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

ErrorCode
DataMap_Create(DataMap *that)    // IN/OUT
{
   if (that == NULL) {
      return DMERR_INVALID_ARGS;
   }

   that->map = HashMap_AllocMap(16, sizeof(DMKeyType), sizeof(DataMapEntry *));

   if (that->map != NULL) {
      that->cookie = magic_cookie;
      return DMERR_SUCCESS;
   }

   return DMERR_INSUFFICIENT_MEM;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToBufferUpdate --
 *
 *      Update result, advance buffer pointer, and adjust buffer left size etc
 *
 * Result:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
ToBufferUpdate(ClientData *clientData,   // IN/OUT
               uint32 len)
{
   if (len >= clientData->buffLen) {
      clientData->result = DMERR_BUFFER_TOO_SMALL;
      clientData->buffer += clientData->buffLen;
      clientData->buffLen = 0;
   } else {
      clientData->buffer += len;
      clientData->buffLen -= len;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToBufferString --
 *
 *      Copy a string to a buffer.
 *
 * Result:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
ToBufferString(ClientData *clientData,   // IN/OUT
               const char *str)          // IN
{
   int32 len;

   if (clientData->result != DMERR_SUCCESS) {
      /* an error occurred already, so stop. */
      return;
   }

   ASSERT(clientData->buffLen > 0);

   len = snprintf(clientData->buffer, clientData->buffLen, "%s", str);

   ToBufferUpdate(clientData, len);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToBufferStringN --
 *
 *      Copy N bytes from a string to a buffer.
 *
 * Result:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
ToBufferStringN(ClientData *clientData,   // IN/OUT
                const char *str,          // IN
                uint32 len)                // IN
{
   uint32 copyLen = len;

   if (clientData->result != DMERR_SUCCESS) {
      /* an error occurred already, so stop. */
      return;
   }

   ASSERT(clientData->buffLen > 0);

   if (copyLen >= clientData->buffLen) {
      copyLen = clientData->buffLen - 1;
   }
   memcpy(clientData->buffer, str, copyLen);
   clientData->buffer[copyLen] = '\0';

   ToBufferUpdate(clientData, len);
}


/*
 *-----------------------------------------------------------------------------
 *
 * GetLimit --
 *
 *      Get the limit from the given max and actual length.
 *
 * Result:
 *      Returns the limit
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static int32
GetLimit(int32 max,        // IN:  -1 means no limit
         int32 length)     // IN
{
   if (max < 0) {
      return length;
   }

   return  max < length ? max : length;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToBufferInt64 --
 *
 *      Copy an int64 to a buffer.
 *
 * Result:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
ToBufferInt64(ClientData *clientData,   // IN/OUT
              int64 num)                // IN
{
   uint32 len;

   if (clientData->result != DMERR_SUCCESS) {
      /* an error occurred already, so stop. */
      return;
   }

   ASSERT(clientData->buffLen > 0);

   len = snprintf(clientData->buffer, clientData->buffLen,
                  "%"FMT64"d", num);

   ToBufferUpdate(clientData, len);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToBufferIdType --
 *
 *      Convert the ID, name and type to a string.
 *
 * Result:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
ToBufferIdType(ClientData *clientData,   // IN/OUT
               const char *idName,       // IN
               DMKeyType fieldId,        // IN
               const char *type)         // IN
{
   uint32 len;

   if (clientData->result != DMERR_SUCCESS) {
      /* an error occurred already, so stop. */
      return;
   }

   ASSERT(clientData->buffLen > 0);

   len = snprintf(clientData->buffer, clientData->buffLen,
                  "--> FIELD_%s(%d, %s): [",
                  idName, fieldId, type);

   ToBufferUpdate(clientData, len);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToBufferEndLine --
 *
 *      Print to end the current line
 *
 * Result:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
ToBufferEndLine(ClientData *clientData)   // IN/OUT
{
   ToBufferString(clientData, "]\n");
}


/*
 *-----------------------------------------------------------------------------
 *
 * IsPrintable --
 *
 *      Check is a string is printable or not.
 *      - 'len': if printable, *len is the printable length, '\0' is excluded.
 *
 * Result:
 *      TRUE if yes, FALSE otherwise
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
IsPrintable(const char *str,    // IN
            int32 strLen,       // IN
            int32 *len)         // OUT
{
   Bool printable = TRUE;
   int32 cc;

   for (cc = 0; cc < strLen; cc ++) {
      /* isprint crashes with negative value in windows debug mode */
      if ((!CType_IsPrint(str[cc])) && (!CType_IsSpace(str[cc]))) {
         printable = FALSE;
         break;
      }
   }

   if (printable) {
      *len = strLen;
   } else {
      /* if only the last char is not printable and is '\0',
       * make it printable
       */
      if ((cc == strLen - 1) && (str[cc] == '\0')) {
         printable = TRUE;
         *len = strLen - 1;
      }
   }

   return printable;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToBufferHexString --
 *
 *      Print a string into a buffer, the string may be in binary format.
 *      - 'strLen': is the length of str.
 *
 * Result:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
ToBufferHexString(ClientData *clientData,    // IN/OUT
                  const char *str,           // IN
                  uint32 strLen)              // IN
{
   Bool printable;
   int32 len; /* the number of printable chars */
   int32 maxLen = clientData->maxStrLen;  /* -1 means no limit */

   if (clientData->result != DMERR_SUCCESS) {
      /* an error occurred already, so stop. */
      return;
   }

   ASSERT(clientData->buffLen > 0);

   maxLen = GetLimit(maxLen, strLen);

   printable = IsPrintable(str, maxLen, &len);

   if (printable) {
      ToBufferString(clientData, "\"");
      ToBufferStringN(clientData, str, len);
      if (maxLen < strLen) {
         ToBufferString(clientData, "...");  /* to indicate partial print */
      }
      ToBufferString(clientData, "\"");
   } else {
      int i;
      /* print the string in hex */

      ToBufferString(clientData, "(");

      for (i = 0; i < maxLen; i++) {
         char hexStr[3];

         if (i) {
            ToBufferString(clientData, ",");  /* separator */
         }
         snprintf(hexStr, sizeof(hexStr), "%02x", (unsigned char)(str[i]));
         ToBufferString(clientData, hexStr);
         if (clientData->result != DMERR_SUCCESS) {
            break;
         }
      }

      if (maxLen < strLen) {
         /* "..." to indicate partial print*/
         ToBufferString(clientData, ",...");
      }

      ToBufferString(clientData, ")");
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * HashMapToStringEntryCb --
 *
 *      Convert to an entry to a string.
 *
 * Result:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
HashMapToStringEntryCb(void *key,            // IN
                       void *data,           // IN
                       void *userData)       // IN/OUT
{
   DataMapEntry *entry = *((DataMapEntry **)data);
   ClientData *clientData = (ClientData *)userData;
   DMKeyType fieldId = *((DMKeyType *)key);

   const char *idName = NULL; /* field ID name */

   if (clientData->result != DMERR_SUCCESS) {
      /* A previous error has occurred, so stop. */
      return;
   }

   if (clientData->fieldIdList!= NULL) {
      int32 cc;
      for (cc = 0; cc < clientData->fieldIdListLen; cc ++) {
         if (fieldId == clientData->fieldIdList[cc].fieldId) {
            idName = clientData->fieldIdList[cc].fieldName;
            break;
         }
      }
   }

   if (idName == NULL) {
      idName = "";
   }

   switch(entry->type) {
      case DMFIELDTYPE_INT64:
         {
            ToBufferIdType(clientData, idName, fieldId, "int64");
            ToBufferInt64(clientData, entry->value.number.val);
            ToBufferEndLine(clientData);
            break;
         }
      case DMFIELDTYPE_STRING:
         {
            ToBufferIdType(clientData, idName, fieldId, "string");
            ToBufferHexString(clientData, entry->value.string.str,
                              entry->value.string.length);
            ToBufferEndLine(clientData);
            break;
         }
      case DMFIELDTYPE_INT64LIST:
         {
            int32 cc;
            int32 max = GetLimit(clientData->maxNumElems,
                                 entry->value.numList.length);

            ToBufferIdType(clientData, idName, fieldId, "int64List");
            for (cc = 0; cc < max; cc++) {
               if (cc != 0) {
                  ToBufferString(clientData, ",");  /* add a separator */
               }
               ToBufferInt64(clientData, entry->value.numList.numbers[cc]);
            }

            if (max < entry->value.numList.length) {
               /* to indicate partial print*/
               ToBufferString(clientData, ",...");
            }

            ToBufferEndLine(clientData);
            break;
         }
      case DMFIELDTYPE_STRINGLIST:
         {
            char **strPtr = entry->value.strList.strings;
            int32 *lenPtr = entry->value.strList.lengths;
            int32 cc = 0;
            int32 max = clientData->maxNumElems;

            ToBufferIdType(clientData, idName, fieldId, "stringList");

            for (; *strPtr != NULL; strPtr++, lenPtr++, cc++) {
               if ((max >= 0) && (cc >= max)) {
                  break;
               }
               if (cc > 0) {
                  ToBufferString(clientData, ","); /* add a separator */
               }
               ToBufferHexString(clientData, *strPtr, *lenPtr);

               if (clientData->result != DMERR_SUCCESS) {
                  return;
               }
            }

            if (*strPtr != NULL) {
               /* to indicate partial print*/
               ToBufferString(clientData, ",...");
            }
            ToBufferEndLine(clientData);
            break;
         }
      default:
         {
            clientData->result = DMERR_UNKNOWN_TYPE;
            return;
         }
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * HashMapCalcEntrySizeCb --
 *
 *      - calculate how much space is needed to encode an entry
 *
 * Result:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
HashMapCalcEntrySizeCb(void *key,            // IN
                       void *data,           // IN
                       void *userData)       // IN/OUT
{
   DataMapEntry *entry = *((DataMapEntry **)data);
   ClientData *clientData = (ClientData *)userData;
   uint32 oldLen = clientData->buffLen;
   uint32 *buffLen = &(clientData->buffLen);

   if (clientData->result != DMERR_SUCCESS) {
      /* a previous error has occurred, so stop. */
      return;
   }

   switch(entry->type) {
      case DMFIELDTYPE_INT64:
         {
            *buffLen += sizeof(int32);        /* type */
            *buffLen += sizeof(DMKeyType);    /* fieldId */
            *buffLen += sizeof(int64);        /* int value */
            break;
         }
      case DMFIELDTYPE_STRING:
         {
            *buffLen += sizeof(int32);        /* type */
            *buffLen += sizeof(DMKeyType);    /* fieldId */
            *buffLen += sizeof(int32);        /* string length */
            *buffLen += entry->value.string.length; /* string payload */
            break;
         }
      case DMFIELDTYPE_INT64LIST:
         {
            *buffLen += sizeof(int32);        /* type */
            *buffLen += sizeof(DMKeyType);    /* fieldId */
            *buffLen += sizeof(int32);        /* list size */
            *buffLen += sizeof(int64) * entry->value.numList.length;
            break;
         }
      case DMFIELDTYPE_STRINGLIST:
         {
            char **strPtr = entry->value.strList.strings;
            int32 *lenPtr = entry->value.strList.lengths;

            *buffLen += sizeof(int32);        /* type */
            *buffLen += sizeof(DMKeyType);    /* fieldId */
            *buffLen += sizeof(int32);        /* list size */

            for (; *strPtr != NULL; strPtr++, lenPtr++) {
               if (*buffLen < oldLen) {
                  clientData->result = DMERR_INTEGER_OVERFLOW;
                  return;
               }

               *buffLen += sizeof(int32);   /* string length */
               *buffLen += *lenPtr;        /* string payload */
            }
            break;
         }
      default:
         {
            clientData->result = DMERR_UNKNOWN_TYPE;
            return;
         }
   }

   if (*buffLen < oldLen) {
      clientData->result = DMERR_INTEGER_OVERFLOW;
      return;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * HashMapSerializeEntryCb --
 *
 *      - serialize each entry into a byte buffer
 *
 * Result:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
HashMapSerializeEntryCb(void *key,            // IN
                        void *data,           // IN
                        void *userData)       // OUT
{
   DataMapEntry *entry = *((DataMapEntry **)data);
   ClientData *clientData = (ClientData *)userData;
   char **buffPtr = &(clientData->buffer);
   char *buffPtrOrig = clientData->buffer;

   EncodeInt32(buffPtr, entry->type);   /* encode type */
   EncodeInt32(buffPtr, *((DMKeyType *)key));   /* encode field id*/

   switch(entry->type) {
      case DMFIELDTYPE_INT64:
         EncodeInt64(buffPtr, entry->value.number.val);
         break;
      case DMFIELDTYPE_STRING:
         EncodeString(buffPtr, entry->value.string.str,
                      entry->value.string.length);
         break;
      case DMFIELDTYPE_INT64LIST:
         EncodeInt64List(buffPtr, entry->value.numList.numbers,
                         entry->value.numList.length);
         break;
      case DMFIELDTYPE_STRINGLIST:
         {
            char **strPtr = entry->value.strList.strings;
            int32 *lenPtr = entry->value.strList.lengths;
            int32 listSize = 0;
            char *listSizePtr = *buffPtr;

            /*reserve the space for list size, we will update later*/
            *buffPtr += sizeof(int32);

            for (; *strPtr != NULL; strPtr++, lenPtr++) {
               EncodeString(buffPtr, *strPtr, *lenPtr);
               listSize ++;
            }

            EncodeInt32(&listSizePtr, listSize);  /* now update the list size */

            break;
         }
      default:
         ASSERT(0);    /*  we do not expect this to happen */
   }

   /* Update left buffer size so we can do a sanity check at the end */
   clientData->buffLen -= (clientData->buffer - buffPtrOrig);
}


/*
 *-----------------------------------------------------------------------------
 *
 * DecodeStringList --
 *
 *      Decode a string list entry and add to the dataMap
 *      - 'buf': *buf points to the input buffer. *buf is advanced accordingly
 *               on success.
 *      - 'left': indicates number of bytes left in the input buffer, *left is
 *                updated accordingly on success.
 *
 * Result:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static ErrorCode
DecodeStringList(char **buf,           // IN
                 int32 *left,          // IN
                 DMKeyType fieldId,    // IN
                 DataMap *that)        // OUT
{
   ErrorCode res;
   int32 listSize;
   char **strList;
   int32 *strLens;
   int32 i;

   res = DecodeInt32(buf, left, &listSize);
   if (res != DMERR_SUCCESS) {
      return res;
   }

   if (listSize < 0 || listSize > *left / sizeof(int32)) {
      /* listSize can be zero to support an empty list */
      return DMERR_BAD_DATA;
   }

   strList = (char **)calloc(listSize + 1, sizeof(char *));
   if (strList == NULL) {
      return DMERR_INSUFFICIENT_MEM;
   }
   if (listSize) {
      strLens = (int32 *)malloc(sizeof(int32) * listSize);
      if (strLens == NULL) {
         FreeStringList(strList, strLens);
         return DMERR_INSUFFICIENT_MEM;
      }
   } else {
      strLens = NULL;
   }

   for (i = 0; i < listSize; i++) {
      res = DecodeString(buf, left, &strList[i], &strLens[i]);
      if (res != DMERR_SUCCESS) {
         break;
      }
   }

   if (res == DMERR_SUCCESS) {
      res = AddEntry_StringList(that, fieldId, strList, strLens);
   }

   if (res != DMERR_SUCCESS) {
      FreeStringList(strList, strLens);
   }

   return res;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyStringList --
 *
 *      - copy string list entry into another map
 *
 * Result:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static ErrorCode
CopyStringListEntry(DMKeyType fieldId,                  // IN
                    const DataMapEntry *entry,          // IN
                    DataMap *dst)                       // OUT
{
   char **oldList = entry->value.strList.strings;
   int32 *lenPtr = entry->value.strList.lengths;
   char **newList;
   int32 *newLens;
   int32 listSize = 0;
   char **ptr = oldList;
   ErrorCode res = DMERR_SUCCESS;
   int32 i;

   /* get the list Size */
   for (; *ptr != NULL; ptr++) {
      listSize ++;
   }

   newList = (char **)calloc(listSize + 1, sizeof(char *));
   if (newList == NULL) {
      return DMERR_INSUFFICIENT_MEM;
   }

   newLens = (int32 *)malloc(sizeof(int32) * listSize);

   if (newLens == NULL) {
      free(newList);
      return DMERR_INSUFFICIENT_MEM;
   }

   /* copy the length vector */
   memcpy(newLens, lenPtr, listSize * sizeof(int32));

   /* copy string one by one */
   for (i = 0; i < listSize; i++) {
      newList[i] = (char *)malloc(newLens[i]);
      if (newList[i] == NULL) {
         res = DMERR_INSUFFICIENT_MEM;
         break;
      }
      memcpy(newList[i], oldList[i], newLens[i]);
   }

   if (res == DMERR_SUCCESS) {
      res = AddEntry_StringList(dst, fieldId, newList, newLens);
   }

   if (res != DMERR_SUCCESS) {
      FreeStringList(newList, newLens);
   }

   return res;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HashMapCopyEntryCb --
 *
 *      - Call back function to copy a dataMap entry.
 *
 * Result:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
HashMapCopyEntryCb(void *key,              // IN
                   void *data,             // IN
                   void *userData)         // IN/OUT
{
   DMKeyType fieldId = *((DMKeyType *)key);
   DataMapEntry *entry = *((DataMapEntry **)data);
   ClientData *clientData = (ClientData *)userData;
   DataMap *dst = clientData->map;
   ErrorCode res = DMERR_SUCCESS;

   if (clientData->result != DMERR_SUCCESS) {
      /* previous  calls have encountered error, so stop */
      return;
   }

   switch(entry->type) {
      case DMFIELDTYPE_INT64:
         res = AddEntry_Int64(dst, fieldId, entry->value.number.val);
         break;
      case DMFIELDTYPE_STRING:
      {
         char *str = (char *)malloc(entry->value.string.length);
         if (str == NULL) {
            res = DMERR_INSUFFICIENT_MEM;
            break;
         }
         memcpy(str, entry->value.string.str, entry->value.string.length);
         res = AddEntry_String(dst, fieldId, str, entry->value.string.length);
         if (res != DMERR_SUCCESS) {
            free(str);
         }
         break;
      }
      case DMFIELDTYPE_INT64LIST:
      {
         int64 *numList;
            numList = (int64 *)malloc(sizeof(int64) *
                  (entry->value.numList.length));

         if (numList == NULL) {
            res = DMERR_INSUFFICIENT_MEM;
         } else {
            res = AddEntry_Int64List(dst, fieldId, numList,
                                     entry->value.numList.length);
            if (res != DMERR_SUCCESS) {
               free(numList);
            }
         }
         break;
      }
      case DMFIELDTYPE_STRINGLIST:
         res = CopyStringListEntry(fieldId, entry, dst);
         break;
      default:
         ASSERT(0);    /*  we do not expect this to happen */
         break;
   }

   clientData->result = res;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HashMapFreeEntryCb --
 *
 *      - call back function is called to free all entries in the hashMap.
 *
 * Result:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
HashMapFreeEntryCb(void *key, void *data, void *userData)
{
   DataMapEntry *entry = *((DataMapEntry **)data);
   FreeEntry(entry);
}


/*
 *-----------------------------------------------------------------------------
 *
 * DataMap_Destroy --
 *
 *     Destroy the DataMap object pointed by that. Frees all the internal
 *     pointers in the object. However the memory pointed by that is not
 *     freed.
 *
 * Result:
 *     0 on success.
 *     error code on failures.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

ErrorCode
DataMap_Destroy(DataMap *that)    // IN/OUT
{
   if (that == NULL) {
      return DMERR_INVALID_ARGS;
   }

   ASSERT(that->cookie == magic_cookie);

   HashMap_Iterate(that->map, HashMapFreeEntryCb, TRUE, NULL);

   HashMap_DestroyMap(that->map);

   that->map = NULL;
   that->cookie = 0;

   return DMERR_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DataMap_Copy --
 *
 *     Copy a DataMap, a deep copy.
 *     - 'dst': dst should *NOT* be initialized via DataMap_Create.
 *       the caller *MUST* call DataMap_Destroy to detroy dst upon success.
 *
 * Result:
 *     0 on success.
 *     error code on failures.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

ErrorCode
DataMap_Copy(const DataMap *src,  // IN
             DataMap *dst)        // OUT
{
   ClientData clientData;
   ErrorCode res;

   if (src == NULL || dst == NULL) {
      return  DMERR_INVALID_ARGS;
   }

   ASSERT(src->map != NULL);
   ASSERT(src->cookie == magic_cookie);

   /* init dst map */
   res = DataMap_Create(dst);
   if (res != DMERR_SUCCESS) {
      return res;
   }

   clientData.map = dst;
   clientData.result = DMERR_SUCCESS;

   HashMap_Iterate(src->map, HashMapCopyEntryCb, FALSE, &clientData);

   if (clientData.result != DMERR_SUCCESS) {
      DataMap_Destroy(dst);
   }

   return clientData.result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DataMap_Serialize --
 *
 *     Serialize a DataMap to a buffer.
 *     - 'buf': on success, this points to the allocated serialize buffer.
 *       The caller *MUST* free this buffer to avoid memory leak.
 *     - 'bufLen': on success, this indicates the length of the allocated
 *       buffer.
 *
 * Result:
 *     0 on success
 *     error code on failures.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

ErrorCode
DataMap_Serialize(const DataMap *that,     // IN
                  char **buf,              // OUT
                  uint32 *bufLen)          // OUT
{
   ClientData clientData;

   if (that == NULL || buf == NULL || bufLen == NULL) {
      return DMERR_INVALID_ARGS;
   }

   ASSERT(that->cookie == magic_cookie);

   /* get the buffer size first */
   memset(&clientData, 0, sizeof clientData);
   HashMap_Iterate(that->map, HashMapCalcEntrySizeCb, FALSE, &clientData);
   if (clientData.result != DMERR_SUCCESS) {
      return clientData.result;
   }

   /* 4 bytes is payload length */
   *bufLen = clientData.buffLen + sizeof(uint32);
   if (*bufLen < clientData.buffLen) {
      return DMERR_INTEGER_OVERFLOW;
   }

   *buf = (char *)malloc(*bufLen);

   if (*buf == NULL) {
      return DMERR_INSUFFICIENT_MEM;
   }

   /* now serialize the map into the buffer */
   clientData.map = (DataMap *)that;
   clientData.result = DMERR_SUCCESS;
   clientData.buffer = *buf;

   /* Encode the payload size */
   EncodeInt32(&(clientData.buffer), clientData.buffLen);

   HashMap_Iterate(that->map, HashMapSerializeEntryCb, FALSE, &clientData);

   /* sanity check, make sure the buffer size is just used up*/
   ASSERT(clientData.buffLen == 0);

   if (clientData.result != DMERR_SUCCESS) {
      free(*buf);
      *buf = NULL;
      *bufLen = 0;
   }
   return clientData.result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DataMap_Deserialize --
 *
 *      Initialize an empty DataMap from a buffer.
 *      - 'that': the given map should *NOT* be initialized by the caller.
 *        On success, the caller needs to call DataMap_Destropy on 'that' to
 *        avoid any memory leak.
 *
 * Result:
 *      - 0 on success
 *      - error code on failures.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

ErrorCode
DataMap_Deserialize(const char *bufIn ,    // IN
                    const int32 bufLen,    // IN
                    DataMap *that)         // OUT
{
   ErrorCode res;
   int32 left = bufLen;   /* number of bytes undecoded */
   int32 len;
   char *buf = (char *)bufIn;

   if (that == NULL || bufIn == NULL || bufLen < 0) {
      return DMERR_INVALID_ARGS;
   }

   /* decode the encoded buffer length */
   res = DecodeInt32(&buf, &left, &len);
   if (res != DMERR_SUCCESS) {
      return res;
   }

   if (len > bufLen - sizeof(int32)) {
      return DMERR_TRUNCATED_DATA;
   }

   left = len;

   return DataMap_DeserializeContent(buf, left, that);
}


/*
 *-----------------------------------------------------------------------------
 *
 * DataMap_DeserializeContent --
 *
 *      Initialize an empty DataMap from the content of the data map buffer
 *      - 'that': the given map should *NOT* be initialized by the caller.
 *        On success, the caller needs to call DataMap_Destropy on 'that' to
 *        avoid any memory leak.
 *
 * Result:
 *      - 0 on success
 *      - error code on failures.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

ErrorCode
DataMap_DeserializeContent(const char *content,    // IN
                           const int32 contentLen, // IN
                           DataMap *that)          // OUT
{
   ErrorCode res;
   int32 left = contentLen;   /* number of bytes undecoded */
   char *buf = (char *)content;

   res = DataMap_Create(that);   /* init the map */
   if (res != DMERR_SUCCESS) {
      return res;
   }

   while ((left> 0) && (res == DMERR_SUCCESS)) {
      DMFieldType type;
      DMKeyType fieldId;
      int32 val;

      res = DecodeInt32(&buf, &left, &val);     /* decode entry type */
      if (res != DMERR_SUCCESS) {
         goto out;
      }

      if (val >= DMFIELDTYPE_MAX) {
         res = DMERR_UNKNOWN_TYPE;
         goto out;
      }

      type = (DMFieldType)val;

      res = DecodeInt32(&buf, &left, &fieldId);   /* decode filedID */
      if (res != DMERR_SUCCESS) {
         goto out;
      }

      if (LookupEntry(that, fieldId) != NULL) {
         res = DMERR_DUPLICATED_FIELD_IDS;
         goto out;
      }

      /* decode individual entry */
      switch(type) {
         case DMFIELDTYPE_INT64:
         {
            int64 val;
            res = DecodeInt64(&buf, &left, &val);
            if (res != DMERR_SUCCESS) {
               goto out;
            }
            res = AddEntry_Int64(that, fieldId, val);
            break;
         }
         case DMFIELDTYPE_STRING:
         {
            char *str;
            int32 strLen;
            res = DecodeString(&buf, &left, &str, &strLen);
            if (res != DMERR_SUCCESS) {
               goto out;
            }
            res = AddEntry_String(that, fieldId, str, strLen);
            if (res != DMERR_SUCCESS) {
               /* clean up memory */
               free(str);
            }
            break;
         }
         case DMFIELDTYPE_INT64LIST:
         {
            res = DecodeInt64List(&buf, &left, fieldId, that);
            break;
         }
         case DMFIELDTYPE_STRINGLIST:
         {
            res = DecodeStringList(&buf, &left, fieldId, that);
            break;
         }
         default:
            res = DMERR_UNKNOWN_TYPE;
            break;
      }
   }

   if (res != DMERR_SUCCESS) {
      goto out;
   }
   return DMERR_SUCCESS;

out:
   DataMap_Destroy(that);
   return res;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DataMap_SetInt64 --
 *
 *      Set an integer value to the map by given field id.
 *      - 'replace': when an entry with the same fieldID exists, if replace is
 *        true, error will be returned, otherwise, the existing entry will be
 *        replaced.
 *
 * Result:
 *      0 on success
 *      Otherwise, corresponding error code is returned.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

ErrorCode
DataMap_SetInt64(DataMap *that,      // IN/OUT
                 DMKeyType fieldId,  // IN
                 int64 value,        // IN
                 Bool replace)       // IN
{
   DataMapEntry *entry;

   if (that == NULL) {
      return DMERR_INVALID_ARGS;
   }

   ASSERT(that->cookie == magic_cookie);

   entry = LookupEntry(that, fieldId);
   if (entry == NULL) {
      /* need to add a new entry */
      return AddEntry_Int64(that, fieldId, value);
   } else if (!replace){
      return DMERR_ALREADY_EXIST;
   } else {
      if ((entry->type != DMFIELDTYPE_INT64)) {
         FreeEntryPayload(entry);
         entry->type = DMFIELDTYPE_INT64;
      }

      /* simple update */
      entry->value.number.val = value;
      return DMERR_SUCCESS;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * DataMap_SetString --
 *
 *      Set a string field to a map.
 *      - 'str': the ownership of buffer pointed by str is passed to the map
 *        on success.
 *      - 'strLen': length of str, -1 means str is null terminated.
 *      - 'replace': when an entry with the same fieldID exists, if replace is
 *        true, error will be returned, otherwise, the existing entry will be
 *        replaced.
 *
 * Result:
 *      0 on success
 *      error code otherwise.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

ErrorCode
DataMap_SetString(DataMap *that,       // IN/OUT
                  DMKeyType fieldId,   // IN
                  char *str,           // IN
                  int32 strLen,        // IN
                  Bool replace)        // IN
{
   DataMapEntry *entry;

   if (that == NULL || str == NULL || (strLen < 0 && strLen != -1)) {
      return DMERR_INVALID_ARGS;
   }

   if (strLen == -1) {
      strLen = strlen(str);
   }

   ASSERT(that->cookie == magic_cookie);

   entry = LookupEntry(that, fieldId);
   if (entry == NULL) {
      return AddEntry_String(that, fieldId, str, strLen);
   } else if (!replace){
      return DMERR_ALREADY_EXIST;
   } else {
      FreeEntryPayload(entry);

      entry->type = DMFIELDTYPE_STRING;
      entry->value.string.str = str;
      entry->value.string.length = strLen;

      return DMERR_SUCCESS;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * DataMap_SetInt64List --
 *
 *      Set an integer value array to the map by given field id.
 *      - 'numList': the ownership of this list will be passed to the map on
 *        success, no copy of the list numbers is made.
 *      - 'replace': when an entry with the same fieldID exists, if replace is
 *        true, error will be returned, otherwise, the existing entry will be
 *        replaced.
 *
 * Result:
 *      0 on success
 *      Otherwise, corresponding error code is returned.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

ErrorCode
DataMap_SetInt64List(DataMap *that,        // IN/OUT
                     DMKeyType fieldId,    // IN
                     int64 *numList,       // IN
                     int32 listLen,        // IN
                     Bool replace)         // IN
{
   DataMapEntry *entry;

   if (that == NULL || numList == NULL || listLen < 0) {
      return DMERR_INVALID_ARGS;
   }

   ASSERT(that->cookie == magic_cookie);

   entry = LookupEntry(that, fieldId);
   if (entry == NULL) {
      /* need to add a new entry */
      return AddEntry_Int64List(that, fieldId, numList, listLen);
   } else if (!replace){
      return DMERR_ALREADY_EXIST;
   } else {
      FreeEntryPayload(entry);

      entry->type = DMFIELDTYPE_INT64LIST;
      entry->value.numList.numbers = numList;
      entry->value.numList.length = listLen;

      return DMERR_SUCCESS;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * DataMap_SetStringList --
 *
 *      Set a string list field to a map.
 *      - 'strList': this pointer points to an array of string pointers.
 *        Upon success, the ownership of this array is passed to the
 *        map, no copy of strings is made. The last element in this array
 *        pointer must be NULL.
 *      - 'strLens': this is an array of integers which indicating the length of
 *        cooresponding string in strList. the ownership is passed to the map
 *        as well on success.
 *      - 'replace': when an entry with the same fieldID exists, if replace is
 *        true, error will be returned, otherwise, the existing entry will be
 *        replaced.
 *
 * Result:
 *      0 on success
 *      error code otherwise.
 *
 * Side-effects:
 *     None
 *
 *-----------------------------------------------------------------------------
 */

ErrorCode
DataMap_SetStringList(DataMap *that,          // IN/OUT
                      DMKeyType fieldId,      // IN
                      char **strList,         // IN
                      int32 *strLens,         // IN
                      Bool replace)           // IN
{
   DataMapEntry *entry;

   if (that == NULL || strList == NULL || strLens == NULL) {
      return DMERR_INVALID_ARGS;
   }

   ASSERT(that->cookie == magic_cookie);

   entry = LookupEntry(that, fieldId);
   if (entry == NULL) {
      /* need to add a new entry */
      return AddEntry_StringList(that, fieldId, strList, strLens);
   } else if (!replace){
      return DMERR_ALREADY_EXIST;
   } else {
      FreeEntryPayload(entry);

      entry->type = DMFIELDTYPE_STRINGLIST;
      entry->value.strList.strings = strList;
      entry->value.strList.lengths = strLens;

      return DMERR_SUCCESS;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * DataMap_GetInt64 --
 *
 *      - Get an integer value from the map by given field id.
 *
 * Result:
 *      - DMERR_SUCCESS: *value has the value.
 *      - DMERR_NOT_FOUND:  no data is found
 *      - DMERR_TYPE_MISMATCH:  the entry in the map has other type.
 *      - Otherwise, corresponding error code is returned.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

ErrorCode
DataMap_GetInt64(const DataMap *that,      // IN
                 DMKeyType fieldId,        // IN
                 int64 *value)             // OUT
{
   DataMapEntry *entry;

   if (that == NULL || value == NULL) {
      return DMERR_INVALID_ARGS;
   }

   ASSERT(that->cookie == magic_cookie);

   entry = LookupEntry(that, fieldId);
   if (entry == NULL) {
      return DMERR_NOT_FOUND;
   }

   if (entry->type != DMFIELDTYPE_INT64) {
      return DMERR_TYPE_MISMATCH;
   }

   *value = entry->value.number.val;
   return DMERR_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DataMap_GetString --
 *
 *      Get a string value from the map by given fieldId.
 *      - 'str': upon success, *str is a string pointer which points to internal
 *        map structures, and should not be modified.
 *      - 'strLen': upon success, *strLen indicates the length of str.
 *
 * Result:
 *      - DMERR_SUCCESS: *str points to the string field in the map, *strLen has
 *        the string length.
 *      - DMERR_NOT_FOUND:  no data is found
 *      - DMERR_TYPE_MISMATCH:  the entry in the map has other type.
 *      - Otherwise, corresponding error code is returned.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

ErrorCode
DataMap_GetString(const DataMap *that,      // IN
                  DMKeyType fieldId,        // IN
                  char **str,               // OUT
                  int32 *strLen)            // OUT
{
   DataMapEntry *entry;

   if (that == NULL || str == NULL || strLen == NULL) {
      return DMERR_INVALID_ARGS;
   }

   ASSERT(that->cookie == magic_cookie);

   entry = LookupEntry(that, fieldId);
   if (entry == NULL) {
      return DMERR_NOT_FOUND;
   }

   if (entry->type != DMFIELDTYPE_STRING) {
      return DMERR_TYPE_MISMATCH;
   }

   *str = entry->value.string.str;
   *strLen = entry->value.string.length;
   return DMERR_SUCCESS;

}


/*
 *-----------------------------------------------------------------------------
 *
 * DataMap_GetInt64List --
 *
 *      Get an integer value list from the map by given field id.
 *      - 'numList': on success, *numList points data structures in the map,
 *        and should not be modified. *numList is an array of int64.
 *      - 'listLen': on success, *listLen indicates number of elements in
 *        numList.
 *
 * Result:
 *      - DMERR_SUCCESS: *numbers points to the list of numbers in the map,
 *        *listLen has the list length.
 *      - DMERR_NOT_FOUND:  no data is found
 *      - DMERR_TYPE_MISMATCH:  the entry in the map has other type.
 *      - Otherwise, corresponding error code is returned.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

ErrorCode
DataMap_GetInt64List(const DataMap *that,      // IN
                     DMKeyType fieldId,        // IN
                     int64 **numList,          // OUT
                     int32 *listLen)           // OUT
{
   DataMapEntry *entry;

   if (that == NULL || numList == NULL || listLen == NULL) {
      return DMERR_INVALID_ARGS;
   }

   ASSERT(that->cookie == magic_cookie);

   entry = LookupEntry(that, fieldId);
   if (entry == NULL) {
      return DMERR_NOT_FOUND;
   }

   if (entry->type != DMFIELDTYPE_INT64LIST) {
      return DMERR_TYPE_MISMATCH;
   }

   *numList = entry->value.numList.numbers;
   *listLen  = entry->value.numList.length;
   return DMERR_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DataMap_GetStringList --
 *
 *      Get a string list from the map by given fieldId.
 *      - 'strList': on success, *strList points to an array of strings which
 *        is owned by the map, and should not be modified.
 *        The last element in the array is NULL.
 *      - 'strLen': on success, *strLen is an array of numbers indicates the
 *        length of corresponding string in strList.
 *        This should not be modified either.
 *
 * Result:
 *      - DMERR_SUCCESS: *strList points to the list of strings in the map,
 *        *strLens has the length for each string.
 *      - DMERR_NOT_FOUND:  no data is found
 *      - DMERR_TYPE_MISMATCH:  the entry in the map has other type.
 *      - Otherwise, corresponding error code is returned.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

ErrorCode
DataMap_GetStringList(const DataMap *that,      // IN
                      DMKeyType fieldId,        // IN
                      char ***strList,          // OUT
                      int32 **strLens)          // OUT
{
   DataMapEntry *entry;

   if (that == NULL || strList == NULL || strLens == NULL) {
      return DMERR_INVALID_ARGS;
   }

   ASSERT(that->cookie == magic_cookie);

   entry = LookupEntry(that, fieldId);
   if (entry == NULL) {
      return DMERR_NOT_FOUND;
   }

   if (entry->type != DMFIELDTYPE_STRINGLIST) {
      return DMERR_TYPE_MISMATCH;
   }

   *strList = entry->value.strList.strings;
   *strLens = entry->value.strList.lengths;
   return DMERR_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DataMap_ToString --
 *
 *      Dump the content of the whole map into a user supplied buffer.
 *      - 'fieldIdList': this is an array of FieldIdNameEntry.
 *      - 'fieldIdListLen': the size of of feildIdList array.
 *      - 'maxNumElements': for list elements, this is the max number of
 *                          elemenents we print.  -1 means no limit.
 *      - 'maxStrLen': max number of bytes to print for a string, -1 no limit.
 *      - 'buf': *buf is a null terminated output string buffer, the caller
 *               *MUST* free this buffer later to avoid memory leak.
 *
 * Result:
 *      DMERR_SUCCESS on success.
 *      error code on failures.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

ErrorCode
DataMap_ToString(const DataMap *that,               // IN
                 FieldIdNameEntry *fieldIdList,     // IN
                 int32 fieldIdListLen,              // IN
                 int32 maxNumElements,              // IN
                 int32 maxStrLen,                   // IN
                 char **buf)                        // OUT
{
   ClientData clientData;
   char *buffPtr;

   /* This API is for debugging only, so we use hard coded buffer size */
   const int32 maxBuffSize = 10 * 1024;

   if (that == NULL || buf == NULL ||
      (maxNumElements < 0 && maxNumElements != -1) ||
      (maxStrLen < 0 && maxStrLen != -1)) {
      return DMERR_INVALID_ARGS;
   }

   *buf = NULL;
   ASSERT(that->cookie == magic_cookie);

   /* get the buffer size first */
   memset(&clientData, 0, sizeof clientData);
   clientData.map = (DataMap *)that;
   clientData.fieldIdList = fieldIdList;
   clientData.fieldIdListLen = fieldIdListLen;
   clientData.maxNumElems = maxNumElements;
   clientData.maxStrLen = maxStrLen;
   clientData.buffLen = maxBuffSize;

   buffPtr = (char *)malloc(maxBuffSize);
   if (buffPtr == NULL) {
      return DMERR_INSUFFICIENT_MEM;
   }
   *buf = buffPtr;

   clientData.buffer = buffPtr;

   ToBufferString(&clientData, "--> Begin\n");

   /* output each entry in the map */
   HashMap_Iterate(that->map, HashMapToStringEntryCb, FALSE, &clientData);

   ToBufferString(&clientData, "--> End.\n");

   /* sanity check, make sure the buffer is not overflown. */
   ASSERT(clientData.buffLen >= 0);
   ASSERT(buffPtr + maxBuffSize >= clientData.buffer);

   if (clientData.result == DMERR_BUFFER_TOO_SMALL) {
      const char truncStr[] = " DATA TRUNCATED!!!\n";

      ASSERT(maxBuffSize > strlen(truncStr));
      Str_Strcpy(buffPtr + maxBuffSize - strlen(truncStr) - 1, truncStr,
	         strlen(truncStr) + 1);
      return DMERR_SUCCESS;
   } else if (clientData.result != DMERR_SUCCESS) {
      *buf = NULL;
      free(buffPtr);
      return clientData.result;
   } else {
      clientData.buffer[0] = '\0';
      return DMERR_SUCCESS;
   }
}

