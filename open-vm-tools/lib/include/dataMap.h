/*********************************************************
 * Copyright (C) 2013-2017 VMware, Inc. All rights reserved.
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

#ifndef _DATA_MAP_H_
#define _DATA_MAP_H_

#include "hashMap.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int32 DMKeyType;

/*
 * Error codes for DataMap APIs
 */
typedef enum {
   DMERR_SUCCESS,                  /* success code */
   DMERR_NOT_FOUND,                /* data does not exist */
   DMERR_ALREADY_EXIST,            /* field ID already exist */
   DMERR_DUPLICATED_FIELD_IDS,     /* duplicated IDs in deserilization */
   DMERR_INSUFFICIENT_MEM,         /* insufficient memory */
   DMERR_TYPE_MISMATCH,            /* type does not match */
   DMERR_INVALID_ARGS,             /* invalid arguments */
   DMERR_UNKNOWN_TYPE,             /* type unknow in decoding */
   DMERR_TRUNCATED_DATA,           /* more data expected during decoding */
   DMERR_BUFFER_TOO_SMALL,         /* a user buffer is too small */
   DMERR_INTEGER_OVERFLOW,         /* an integer overflow happened */
   DMERR_BAD_DATA                  /* bad data during decoding */
} ErrorCode;

/*
 * Data field types
 */
typedef enum {
   DMFIELDTYPE_EMPTY,
   DMFIELDTYPE_INT64,
   DMFIELDTYPE_STRING,
   DMFIELDTYPE_INT64LIST,
   DMFIELDTYPE_STRINGLIST,
   DMFIELDTYPE_MAX
} DMFieldType;

typedef struct {
   HashMap *map;
   uint64 cookie;   /* so we know the datamap is not some garbage data */
} DataMap;

typedef struct {
   DMKeyType fieldId;
   const char *fieldName;
} FieldIdNameEntry;

/*
 * Initializer
 */
ErrorCode
DataMap_Create(DataMap *that);   // IN/OUT
ErrorCode
DataMap_Destroy(DataMap *that);   // IN/OUT
ErrorCode
DataMap_Copy(const DataMap *src,  // IN
             DataMap *dst);       // OUT
ErrorCode
DataMap_Serialize(const DataMap *that,   //IN
                  char **buf,            // OUT
                  uint32 *bufLen);          // OUT
ErrorCode
DataMap_Deserialize(const char *bufIn,     // IN
                    const int32 bufLen,    // IN
                    DataMap *that);        // OUT

ErrorCode
DataMap_DeserializeContent(const char *bufIn,     // IN
                           const int32 bufLen,    // IN
                           DataMap *that);        // OUT
/*
 * Setters
 */

ErrorCode
DataMap_SetInt64(DataMap *that,      // IN/OUT
                 DMKeyType fieldId,  // IN
                 int64 value,        // IN
                 Bool replace);      // IN
ErrorCode
DataMap_SetString(DataMap *that,       // IN/OUT
                  DMKeyType fieldId,   // IN
                  char *str,           // IN
                  int32 strLen,        // IN
                  Bool replace);       // IN
ErrorCode
DataMap_SetInt64List(DataMap *that,        // IN/OUT
                     DMKeyType fieldId,    // IN
                     int64 *numList,       // IN
                     int32 listLen,        // IN
                     Bool replace);        // IN
ErrorCode
DataMap_SetStringList(DataMap *that,          // IN/OUT
                      DMKeyType fieldId,      // IN
                      char **strList,         // IN
                      int32 *strLens,         // IN
                      Bool replace);          // IN

/*
 *  Getters
 */

DMFieldType
DataMap_GetType(const DataMap *that,    // IN
                DMKeyType fieldId);     // IN

ErrorCode
DataMap_GetInt64(const DataMap *that,      // IN
                 DMKeyType fieldId,        // IN
                 int64 *value);            // OUT

ErrorCode
DataMap_GetString(const DataMap *that,      // IN
                  DMKeyType fieldId,        // IN
                  char **str,               // OUT
                  int32 *strLen);           // OUT

ErrorCode
DataMap_GetInt64List(const DataMap *that,      // IN
                     DMKeyType fieldId,        // IN
                     int64 **numList,          // OUT
                     int32 *listLen);          // OUT
ErrorCode
DataMap_GetStringList(const DataMap *that,      // IN
                      DMKeyType fieldId,        // IN
                      char ***strList,          // OUT
                      int32 **strLens);         // OUT

ErrorCode
DataMap_ToString(const DataMap *that,               // IN
                 FieldIdNameEntry *fieldIdList,     // IN
                 int32 fieldIdListLen,              // IN
                 int32 maxNumElements,              // IN
                 int32 maxStrLen,                   // IN
                 char **buf);                       // OUT

#ifdef __cplusplus
}    /* end of extern "C"  */
#endif


#endif // #ifdef _DATA_MAP_H_
