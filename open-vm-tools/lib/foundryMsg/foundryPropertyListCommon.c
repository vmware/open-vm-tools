/*********************************************************
 * Copyright (C) 2004-2016, 2021 VMware, Inc. All rights reserved.
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
 * foundryPropertyListCommon.c --
 *
 * Some utility functions for manipulating property lists. Property
 * lists are now used in both the client and the VMX. The VMX uses them
 * as part of the socket protocol with the client. As a result, these
 * functions have been factored out into the stand-alone message library 
 * so it can be used by the VMX tree without also linking in the 
 * entire foundry client-side library.
 */

#include "vmware.h"
#include "util.h"
#include "str.h"
#include "unicode.h"
#include "vixCommands.h"

#include "vixOpenSource.h"

/*
 * The length of the 'size' field is 4 bytes -- avoid the confusion
 * of size_t on 32 vs 64 bit platforms.
 */
#define  PROPERTY_LENGTH_SIZE 4

/*
 * Lets not trust sizeof()
 */
#define  PROPERTY_SIZE_INT32     4
#define  PROPERTY_SIZE_INT64     8
#define  PROPERTY_SIZE_BOOL      1
// The size may be different on different machines.
// To be safe, we always use 8 bytes.
#define  PROPERTY_SIZE_POINTER   8

static VixError
VixPropertyListDeserializeImpl(VixPropertyListImpl *propList,
                               const char *buffer,
                               size_t bufferSize,
                               Bool clobber,
                               VixPropertyListBadEncodingAction action);


/*
 *-----------------------------------------------------------------------------
 *
 * VixPropertyList_Initialize --
 *
 *       Initialize a list to be empty. This is an internal function
 *       that is used both when we allocate a property list that wil be passed
 *       to the client as a handle, and when we allocate an internal property
 *       list that was not allocated as a handle.
 *
 * Results:
 *       None.
 *
 * Side effects:
 *
 *-----------------------------------------------------------------------------
 */

void 
VixPropertyList_Initialize(VixPropertyListImpl *propList)  // IN
{
   ASSERT(propList);
   propList->properties = NULL;
} // VixPropertyList_Initialize


/*
 *-----------------------------------------------------------------------------
 *
 * VixPropertyList_RemoveAllWithoutHandles --
 *
 *       Delete all properties in a list. This is an internal procedure
 *       that takes a VixPropertyListImpl as a parameter.
 *
 * Results:
 *       None
 *
 * Side effects:
 *       The property list is empty.
 *
 *-----------------------------------------------------------------------------
 */

void 
VixPropertyList_RemoveAllWithoutHandles(VixPropertyListImpl *propList)   // IN
{
   VixPropertyValue *property;

   if (NULL == propList) {
      return;
   }

   while (NULL != propList->properties) {
      property = propList->properties;
      propList->properties = property->next;

      if (VIX_PROPERTYTYPE_STRING == property->type) {
         if (property->isSensitive) {
            Util_ZeroString(property->value.strValue);
         }
         free(property->value.strValue);
      } else if (VIX_PROPERTYTYPE_BLOB == property->type) {
         if (property->isSensitive) {
            Util_Zero(property->value.blobValue.blobContents,
                      property->value.blobValue.blobSize);
         }
         free(property->value.blobValue.blobContents);
      }
    
      free(property);
   }
} // VixPropertyList_RemoveAllWithoutHandles


/*
 *-----------------------------------------------------------------------------
 *
 * VixPropertyList_MarkAllSensitive --
 *
 *       Mark all properties in a list sensitive.
 *
 * Results:
 *       As above
 *
 * Side effects:
 *       None
 *
 *-----------------------------------------------------------------------------
 */

void
VixPropertyList_MarkAllSensitive(VixPropertyListImpl *propList)  // IN/OUT:
{
   if (NULL != propList) {
      VixPropertyValue *property = propList->properties;

      while (NULL != property) {
         property->isSensitive = TRUE;

         property = property->next;
      }
   }
} // VixPropertyList_MarkAllSensitive


/*
 *-----------------------------------------------------------------------------
 *
 * VixPropertyList_Serialize --
 *
 *       Serialize a property list to a buffer. The buffer is allocated by
 *       this routine to be of the required size and should be freed by caller.
 *
 *       This function should be modified to deal with the case of 
 *       properties of type VIX_PROPERTYTYPE_HANDLE.
 *     
 *
 * Results:
 *      VixError.
 *
 * Side effects:
 *       None.
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixPropertyList_Serialize(VixPropertyListImpl *propList,  // IN:
                          Bool dirtyOnly,                 // IN:
                          size_t *resultSize,             // OUT:
                          char **resultBuffer)            // OUT:
{
   VixError err = VIX_OK;
   VixPropertyValue *property = NULL;
   char *serializeBuffer = NULL;
   int valueLength;
   size_t headerSize;
   size_t propertyIDSize;
   size_t propertyTypeSize;
   size_t propertyValueLengthSize;
   size_t bufferSize = 0;
   size_t pos = 0;
 
   ASSERT_ON_COMPILE(PROPERTY_LENGTH_SIZE == sizeof valueLength);

   if ((NULL == propList) ||
       (NULL == resultSize) ||
       (NULL == resultBuffer)) {
      err = VIX_E_INVALID_ARG;
      goto quit;
   }
 
   propertyIDSize = sizeof(property->propertyID);
   propertyTypeSize = sizeof(property->type);
   propertyValueLengthSize = PROPERTY_LENGTH_SIZE;

   headerSize = propertyIDSize + propertyTypeSize + propertyValueLengthSize;

   /*
    * Walk the property list to determine size of the needed buffer
    */
   property = propList->properties;
   while (NULL != property) {
      /*
       * If only the dirty properties need to be serialized
       * then skip the unchanged ones.
       */
      if (dirtyOnly && (!property->isDirty)) {
         property = property->next;
         continue;
      }

      bufferSize += headerSize;
    
      switch (property->type) {
         ////////////////////////////////////////////////////////
         case VIX_PROPERTYTYPE_INTEGER:
            bufferSize += PROPERTY_SIZE_INT32;
            break;

         ////////////////////////////////////////////////////////
         case VIX_PROPERTYTYPE_STRING:
            if (property->value.strValue) {
               valueLength = strlen(property->value.strValue) + 1;
               /*
                * The deserialization code rejects all non-UTF-8 strings.
                * There should not be any non-UTF-8 strings passing
                * through our code since we should have either converted
                * non-UTF-8 strings from system APIs to UTF-8, or validated
                * that any client-provided strings were UTF-8. But this
                * if we've missed something, this should hopefully catch the
                * offending code close to the act.
                */
               if (!Unicode_IsBufferValid(property->value.strValue,
                                          valueLength,
                                          STRING_ENCODING_UTF8)) {
                  Log("%s: attempted to send a non-UTF-8 string for "
                      "property %d.\n",
                      __FUNCTION__, property->propertyID);
                  ASSERT(0);
                  err = VIX_E_INVALID_UTF8_STRING;
               }
               bufferSize += valueLength;
            } else {
               err = VIX_E_INVALID_ARG;
               goto quit;
            }
            break;

         ////////////////////////////////////////////////////////
         case VIX_PROPERTYTYPE_BOOL:
            bufferSize += PROPERTY_SIZE_BOOL;
            break;

         ////////////////////////////////////////////////////////
         case VIX_PROPERTYTYPE_INT64:
            bufferSize += PROPERTY_SIZE_INT64;
            break;

         ////////////////////////////////////////////////////////
         case VIX_PROPERTYTYPE_BLOB:
            bufferSize += property->value.blobValue.blobSize;
            break;

         ////////////////////////////////////////////////////////
         case VIX_PROPERTYTYPE_POINTER:
            /*
             * We should not serialize any pointer.
             * Catch such programming errors.
             */
            err = VIX_E_INVALID_ARG;
            Log("%s:%d, pointer properties cannot be serialized.\n",
                __FUNCTION__, __LINE__);
            goto quit;

         ////////////////////////////////////////////////////////
         default:
            err = VIX_E_UNRECOGNIZED_PROPERTY;
            goto quit;   
      }

      property = property->next;
   }

   *resultBuffer = (char*) VixMsg_MallocClientData(bufferSize);
   if (NULL == *resultBuffer) {
      err = VIX_E_OUT_OF_MEMORY;
      goto quit;
   }
   serializeBuffer = *resultBuffer;

   pos = 0;
   property = propList->properties;

   /*
    * Write out the properties to the buffer in the following format:
    * PropertyID | PropertyType | DataLength | Data
    */

   while (NULL != property) { 
      /*
       * If only the dirty properties need to be serialized
       * then skip the unchanged ones.
       */
      if (dirtyOnly && (!property->isDirty)) {
         property = property->next;
         continue;
      }

      memcpy(&(serializeBuffer[pos]), &(property->propertyID), propertyIDSize);
      pos += propertyIDSize;
      memcpy(&(serializeBuffer[pos]), &(property->type), propertyTypeSize);
      pos += propertyTypeSize;

      switch (property->type) {
         ////////////////////////////////////////////////////////
         case VIX_PROPERTYTYPE_INTEGER:
             valueLength = PROPERTY_SIZE_INT32;
             memcpy(&(serializeBuffer[pos]), &valueLength, propertyValueLengthSize);
             pos += propertyValueLengthSize;
             memcpy(&(serializeBuffer[pos]), &(property->value.intValue), valueLength);
             break;

         ////////////////////////////////////////////////////////
         case VIX_PROPERTYTYPE_STRING:
             valueLength = (int) strlen(property->value.strValue) + 1;
             memcpy(&(serializeBuffer[pos]), &valueLength, propertyValueLengthSize);
             pos += propertyValueLengthSize;
             Str_Strcpy(&(serializeBuffer[pos]), property->value.strValue, valueLength);
             break;

         ////////////////////////////////////////////////////////
         case VIX_PROPERTYTYPE_BOOL:
             valueLength = PROPERTY_SIZE_BOOL;
             memcpy(&(serializeBuffer[pos]), &valueLength, propertyValueLengthSize);
             pos += propertyValueLengthSize;
             memcpy(&(serializeBuffer[pos]), &(property->value.boolValue), valueLength);
             break;

         ////////////////////////////////////////////////////////
         case VIX_PROPERTYTYPE_INT64:
             valueLength = PROPERTY_SIZE_INT64;
             memcpy(&(serializeBuffer[pos]), &valueLength, propertyValueLengthSize);
             pos += propertyValueLengthSize;
             memcpy(&(serializeBuffer[pos]), &(property->value.int64Value), valueLength);
             break;

         ////////////////////////////////////////////////////////
         case VIX_PROPERTYTYPE_BLOB:
            if (property->value.blobValue.blobContents) {
               valueLength = property->value.blobValue.blobSize;
               memcpy(&(serializeBuffer[pos]), &valueLength, propertyValueLengthSize);
               pos += propertyValueLengthSize;
               memcpy(&(serializeBuffer[pos]), 
                      property->value.blobValue.blobContents, 
                      valueLength);
            } else {
               err = VIX_E_INVALID_ARG;
               goto quit;
            }
            break;

         ////////////////////////////////////////////////////////
         case VIX_PROPERTYTYPE_POINTER:
            NOT_IMPLEMENTED();

         ////////////////////////////////////////////////////////
         default:
             err = VIX_E_UNRECOGNIZED_PROPERTY;
             goto quit;   
      }
    
      pos += valueLength;
      property = property->next;
   }

   ASSERT(pos == bufferSize);
   *resultSize = bufferSize;
 
quit:
   if (VIX_OK != err) {
      free(serializeBuffer);
      if (NULL != resultBuffer) {
         *resultBuffer = NULL;
      }
      if (NULL != resultSize) {
         *resultSize = 0;
      }
   }

   return err;
} // FoundryPropertList_Serialize


/*
 *-----------------------------------------------------------------------------
 *
 * VixPropertyList_Deserialize --
 *
 *       Deserialize a property list from a buffer. Repeated properties
 *       are clobbered.
 *
 * Results:
 *      VixError.
 *
 * Side effects:
 *       None.
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixPropertyList_Deserialize(VixPropertyListImpl *propList,            // IN
                            const char *buffer,                       // IN
                            size_t bufferSize,                        // IN
                            VixPropertyListBadEncodingAction action)  // IN
{
   return VixPropertyListDeserializeImpl(propList,
                                         buffer,
                                         bufferSize,
                                         TRUE, // clobber
                                         action);
} // VixPropertyList_Deserialize


/*
 *-----------------------------------------------------------------------------
 *
 * VixPropertyList_DeserializeNoClobber --
 *
 *       Deserialize a property list from a buffer. Repeated properties
 *       are preserved.
 *
 * Results:
 *      VixError.
 *
 * Side effects:
 *       None.
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixPropertyList_DeserializeNoClobber(VixPropertyListImpl *propList,     // IN
                                     const char *buffer,                // IN
                                     size_t bufferSize,                 // IN
                                     VixPropertyListBadEncodingAction action) // IN
{
   return VixPropertyListDeserializeImpl(propList,
                                         buffer,
                                         bufferSize,
                                         FALSE, // clobber
                                         action);
} // VixPropertyList_DeserializeNoClobber


/*
 *-----------------------------------------------------------------------------
 *
 * VixPropertyListDeserializeImpl --
 *
 *       Deserialize a property list from a buffer. 
 *
 *       This function should be modified to deal with the case of 
 *       properties of type VIX_PROPERTYTYPE_HANDLE.
 *
 * Results:
 *      VixError.
 *
 * Side effects:
 *       None.
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixPropertyListDeserializeImpl(VixPropertyListImpl *propList,            // IN
                               const char *buffer,                       // IN
                               size_t bufferSize,                        // IN
                               Bool clobber,                             // IN
                               VixPropertyListBadEncodingAction action)  // IN
{
   VixError err = VIX_OK;
   VixPropertyValue *property = NULL;
   size_t pos = 0;
   char *strPtr;
   int *intPtr;
   Bool *boolPtr;
   int64 *int64Ptr;
   unsigned char* blobPtr;
   int *propertyIDPtr;
   int *lengthPtr;
   size_t propertyIDSize;
   size_t propertyTypeSize;
   size_t propertyValueLengthSize;
   size_t headerSize;
   VixPropertyType *propertyTypePtr;
   Bool allocateFailed;
   Bool needToEscape;

   if ((NULL == propList)
       || (NULL == buffer)) {
      err = VIX_E_INVALID_ARG;
      goto quit;
   }

   propertyIDSize = sizeof(*propertyIDPtr);
   propertyTypeSize = sizeof(*propertyTypePtr);
   propertyValueLengthSize = PROPERTY_LENGTH_SIZE;
   headerSize = propertyIDSize + propertyTypeSize + propertyValueLengthSize;

   /*
    * Read properties from the buffer and add them to the property list.
    */
   while ((pos+headerSize) < bufferSize) {
      propertyIDPtr = (int*) &(buffer[pos]);
      pos += propertyIDSize;
      propertyTypePtr = (VixPropertyType*) &(buffer[pos]);
      pos += propertyTypeSize;
      lengthPtr = (int*) &(buffer[pos]);
      pos += propertyValueLengthSize;

      /*
       * Do not allow lengths of 0 or fewer bytes. Those do not make sense,
       * unless you can pass a NULL blob, which Serialize() does not allow.
       * Also, make sure the value is contained within the bounds of the buffer.
       */
      if ((*lengthPtr < 1) || ((*lengthPtr + pos) > bufferSize)) {
         err = VIX_E_INVALID_SERIALIZED_DATA;
         goto quit;
      }

      /*
       * Create the property if missing
       */
      if (clobber) {
         err = VixPropertyList_FindProperty(propList,
                                            *propertyIDPtr,
                                            *propertyTypePtr,
                                            0, // index
                                            TRUE, //createIfMissing
                                            &property);
      } else {
         err = VixPropertyListAppendProperty(propList,
                                             *propertyIDPtr,
                                             *propertyTypePtr,
                                             &property);
      }

      if (VIX_OK != err) {
         goto quit;
      }

      /*
       * Initialize the property to the received value
       */
      switch (*propertyTypePtr) {
         ////////////////////////////////////////////////////////
         case VIX_PROPERTYTYPE_INTEGER:
            if (PROPERTY_SIZE_INT32 != *lengthPtr) {
               err = VIX_E_INVALID_SERIALIZED_DATA;
               goto quit;
            }
            intPtr = (int*) &(buffer[pos]);
            property->value.intValue = *intPtr;
            break;

         ////////////////////////////////////////////////////////
         case VIX_PROPERTYTYPE_STRING:
            strPtr = (char*) &(buffer[pos]);
            /*
             * The length that Serialize() generates includes the terminating
             * NUL character.
             */
            if (strPtr[*lengthPtr - 1] != '\0') {
               err = VIX_E_INVALID_SERIALIZED_DATA;
               goto quit;
            }

            needToEscape = FALSE;

            /*
             * Make sure the string is valid UTF-8 before copying it. We
             * expect all strings stored in the process to be UTF-8.
             */
            if (!Unicode_IsBufferValid(strPtr, *lengthPtr,
                                       STRING_ENCODING_UTF8)) {
               Log("%s: non-UTF-8 string received for property %d.\n",
                   __FUNCTION__, *propertyIDPtr);
               switch (action) {
               case VIX_PROPERTY_LIST_BAD_ENCODING_ERROR:
                  err = VIX_E_INVALID_UTF8_STRING;
                  goto quit;
               case VIX_PROPERTY_LIST_BAD_ENCODING_ESCAPE:
                  needToEscape = TRUE;
               }

            }
            free(property->value.strValue);

            if (needToEscape) {
               property->value.strValue =
                  Unicode_EscapeBuffer(strPtr, *lengthPtr,
                                       STRING_ENCODING_UTF8);
               if (NULL == property->value.strValue) {
                  err = VIX_E_OUT_OF_MEMORY;
                  goto quit;
               }
            } else {
               property->value.strValue =
                  VixMsg_StrdupClientData(strPtr, &allocateFailed);
               if (allocateFailed) {
                  err = VIX_E_OUT_OF_MEMORY;
                  goto quit;
               }
            }
            break;

         ////////////////////////////////////////////////////////
         case VIX_PROPERTYTYPE_BOOL:
            if (PROPERTY_SIZE_BOOL != *lengthPtr) {
               err = VIX_E_INVALID_SERIALIZED_DATA;
               goto quit;
            }
            boolPtr = (Bool*) &(buffer[pos]);
            property->value.boolValue = *boolPtr;
            break;

         ////////////////////////////////////////////////////////
         case VIX_PROPERTYTYPE_INT64:
            if (PROPERTY_SIZE_INT64 != *lengthPtr) {
               err = VIX_E_INVALID_SERIALIZED_DATA;
               goto quit;
            }
            int64Ptr = (int64*) &(buffer[pos]);
            property->value.int64Value = *int64Ptr;
            break;

         ////////////////////////////////////////////////////////
         case VIX_PROPERTYTYPE_BLOB:
            blobPtr = (unsigned char*) &(buffer[pos]);
            property->value.blobValue.blobSize = *lengthPtr;
            /*
             * Use regular malloc() when allocating amounts specified by another
             * process. Admittedly we've already bounds checked it, but this is
             * pretty easy to handle.
             */
            free(property->value.blobValue.blobContents);
            property->value.blobValue.blobContents =
               VixMsg_MallocClientData(*lengthPtr);
            if (NULL == property->value.blobValue.blobContents) {
               err = VIX_E_OUT_OF_MEMORY;
               goto quit;
            }
            memcpy(property->value.blobValue.blobContents, blobPtr, *lengthPtr);
            break;

         ////////////////////////////////////////////////////////
         case VIX_PROPERTYTYPE_POINTER:
            /*
             * Deserialize an pointer property should not be allowed.
             * An evil peer could send us such data.
             */
            err = VIX_E_INVALID_SERIALIZED_DATA;
            Log("%s:%d, pointer properties cannot be serialized.\n",
                __FUNCTION__, __LINE__);
            goto quit;

         ////////////////////////////////////////////////////////
         default:
            err = VIX_E_UNRECOGNIZED_PROPERTY;
            goto quit;
      }

      pos += *lengthPtr;
   }

quit:
   if ((VIX_OK != err) && (NULL != propList)) {
      VixPropertyList_RemoveAllWithoutHandles(propList);
   }

   return err;
} // VixPropertyList_Deserialize


/*
 *-----------------------------------------------------------------------------
 *
 * VixPropertyList_FindProperty --
 *
 *       This is an internal routine that finds a property in the list.
 *
 *       If the property is found, then this also checks that the property
 *       has an expected type; if the types mismatch thenit returns an error.
 *
 *       It optionally creates a property if it is missing.
 *
 * Results:
 *       VixError
 *
 * Side effects:
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixPropertyList_FindProperty(VixPropertyListImpl *propList,    // IN
                             int propertyID,                   // IN
                             VixPropertyType type,             // IN
                             int index,                        // IN
                             Bool createIfMissing,             // IN
                             VixPropertyValue **resultEntry)   // OUT
{
   VixError err = VIX_OK;
   VixPropertyValue *property = NULL;

   if (NULL == resultEntry) {
      err = VIX_E_INVALID_ARG;
      goto quit;
   }
   *resultEntry = NULL;


   property = propList->properties;
   while (NULL != property) {
      if (propertyID == property->propertyID) {
         if (index > 0) {
            index--;
         } else {
            if ((VIX_PROPERTYTYPE_ANY != type) 
                  && (type != property->type)) {
               err = VIX_E_TYPE_MISMATCH;
            }
            *resultEntry = property;
            goto quit;
         }
      } // (propertyID == property->propertyID)

      property = property->next;
   } // while (NULL != property)

   /*
    * If we get to here, then the property doesn't exist.
    * Either create it or return an error.
    */
   if (!createIfMissing) {
      err = VIX_E_UNRECOGNIZED_PROPERTY;
      goto quit;
   }

   err = VixPropertyListAppendProperty(propList,
                                       propertyID,
                                       type,
                                       resultEntry);

quit:
   return err;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VixPropertyListAppendProperty --
 *
 *       This is an internal routine that creates a property for the
 *       append routines.
 *
 * Results:
 *       VixError
 *
 * Side effects:
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixPropertyListAppendProperty(VixPropertyListImpl *propList,   // IN
                              int propertyID,                  // IN
                              VixPropertyType type,            // IN
                              VixPropertyValue **resultEntry)  // OUT
{
   VixError err = VIX_OK;
   VixPropertyValue *lastProperty;
   VixPropertyValue *property = NULL;

   if (NULL == resultEntry) {
      err = VIX_E_INVALID_ARG;
      goto quit;
   }
   *resultEntry = NULL;

   property = (VixPropertyValue *) 
      Util_SafeCalloc(1, sizeof(VixPropertyValue));

   property->type = type;
   property->propertyID = propertyID;
   property->isDirty = TRUE;
   property->isSensitive = FALSE;

   /*
    * We only have to initialize the values that we release, so
    * we don't try to release an invalid reference.
    */
   if (VIX_PROPERTYTYPE_STRING == property->type) {
      property->value.strValue = NULL;
   } else if (VIX_PROPERTYTYPE_BLOB == property->type) {
      property->value.blobValue.blobContents = NULL;
   } else if (VIX_PROPERTYTYPE_HANDLE == property->type) {
      property->value.handleValue = VIX_INVALID_HANDLE;
   }

   /*
    * Put the new property on the end of the list. Some property lists,
    * like a list of VMs or snapshots, assume the order is meaningful and 
    * so it should be preserved.
    */
   lastProperty = propList->properties;
   while ((NULL != lastProperty) && (NULL != lastProperty->next)) {
      lastProperty = lastProperty->next;
   }

 
   if (NULL == lastProperty) {
      propList->properties = property;
   } else {
      lastProperty->next = property;
   }
   property->next = NULL;


   *resultEntry = property;

quit:
   return err;
} // VixPropertyListAppendProperty



/*
 *-----------------------------------------------------------------------------
 *
 * VixPropertyList_GetString --
 *
 *       Return a copy of a string property value. The value is identified
 *       by the integer property ID.
 *
 *       This fails if the value is not present, or if it is a different
 *       type, or if the caller did not pass a valid out parameter to
 *       receive the value.
 *
 * Results:
 *       VixError. VIX_OK if the property was found.
 *                 VIX_E_UNRECOGNIZED_PROPERTY if the property was not found.
 *
 * Side effects:
 *       None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixPropertyList_GetString(VixPropertyListImpl *propList,       // IN
                          int propertyID,                      // IN
                          int index,                           // IN
                          char **resultValue)                  // OUT
{
   VixError err = VIX_OK;
   VixPropertyValue *property = NULL;

   if ((NULL == propList) || (NULL == resultValue)) {
      err = VIX_E_INVALID_ARG;
      goto quit;
   }
   *resultValue = NULL;

   err = VixPropertyList_FindProperty(propList, 
                                      propertyID, 
                                      VIX_PROPERTYTYPE_STRING, 
                                      index,
                                      FALSE,
                                      &property);
   if (VIX_OK != err) {
      goto quit;
   }

   if (NULL != property->value.strValue) {
      *resultValue = Util_SafeStrdup(property->value.strValue);
   }

quit:
   return err;
} // VixPropertyList_GetString


/*
 *-----------------------------------------------------------------------------
 *
 * VixPropertyListSetStringImpl --
 *
 *       Saves a copy of a string property value. Sets sensitivity.
 *
 * Results:
 *       As above
 *
 * Side effects:
 *       None
 *
 *-----------------------------------------------------------------------------
 */

static void
VixPropertyListSetStringImpl(VixPropertyValue *property,  // IN:
                             const char *value,           // IN:
                             Bool isSensitive)            // IN:
{
   if (NULL != property->value.strValue) {
      if (property->isSensitive) {
         Util_ZeroString(property->value.strValue);
      }
      free(property->value.strValue);
      property->value.strValue = NULL;
   }
   if (NULL != value) {
      property->value.strValue = Util_SafeStrdup(value);
   }
   property->isDirty = TRUE;
   property->isSensitive = isSensitive;
} // VixPropertyListSetStringImpl


/*
 *-----------------------------------------------------------------------------
 *
 * VixPropertyList_SetString --
 *
 *       Saves a copy of a string property value. The value is identified
 *       by the integer property ID.
 *
 *       Value names are unique within a single property list.
 *       If a previous value with the same propertyID value already
 *       existed in this property list, then it is replaced with the new
 *       value. Otherwise, a new value is added.
 *
 *       This fails if the value is present but has a different type.
 *
 * Results:
 *       VixError
 *
 * Side effects:
 *       None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixPropertyList_SetString(VixPropertyListImpl *propList,  // IN:
                          int propertyID,                 // IN:
                          const char *value)              // IN:
{
   VixError err = VIX_OK;
   VixPropertyValue *property = NULL;

   if (NULL == propList) {
      err = VIX_E_INVALID_ARG;
      goto quit;
   }

   /*
    * Find or create an entry for this property.
    */
   err = VixPropertyList_FindProperty(propList,
                                      propertyID,
                                      VIX_PROPERTYTYPE_STRING,
                                      0,
                                      TRUE,
                                      &property);
   if (VIX_OK == err) {
      VixPropertyListSetStringImpl(property, value, property->isSensitive);
   }

quit:

   return err;
} // VixPropertyList_SetString


/*
 *-----------------------------------------------------------------------------
 *
 * VixPropertyList_SetStringSensitive --
 *
 *       Saves a copy of a string property value. The value is identified
 *       by the integer property ID. Mark sensitive.
 *
 *       Value names are unique within a single property list.
 *       If a previous value with the same propertyID value already
 *       existed in this property list, then it is replaced with the new
 *       value. Otherwise, a new value is added.
 *
 *       This fails if the value is present but has a different type.
 *
 * Results:
 *       VixError
 *
 * Side effects:
 *       None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixPropertyList_SetStringSensitive(VixPropertyListImpl *propList,  // IN:
                                   int propertyID,                 // IN:
                                   const char *value)              // IN:
{
   VixError err = VIX_OK;
   VixPropertyValue *property = NULL;

   if (NULL == propList) {
      err = VIX_E_INVALID_ARG;
      goto quit;
   }

   /*
    * Find or create an entry for this property.
    */
   err = VixPropertyList_FindProperty(propList,
                                      propertyID,
                                      VIX_PROPERTYTYPE_STRING,
                                      0,
                                      TRUE,
                                      &property);

   if (VIX_OK == err) {
      VixPropertyListSetStringImpl(property, value, TRUE);
   }

quit:

   return err;
} // VixPropertyList_SetString


/*
 *-----------------------------------------------------------------------------
 *
 * VixPropertyList_GetInteger --
 *
 *       Return a copy of a integer property value. The value is identified
 *       by the integer property ID.
 *
 *       This fails if the value is not present, or if it is a different
 *       type, or if the caller did not pass a valid out parameter to
 *       receive the value.
 *
 * Results:
 *       VixError. VIX_OK if the property was found.
 *                 VIX_E_UNRECOGNIZED_PROPERTY if the property was not found.
 *
 * Side effects:
 *       None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixPropertyList_GetInteger(VixPropertyListImpl *propList,      // IN
                           int propertyID,                     // IN
                           int index,                          // IN
                           int *resultValue)                   // OUT
{
   VixError err = VIX_OK;
   VixPropertyValue *property = NULL;

   if ((NULL == resultValue) || (NULL == propList)) {
      err = VIX_E_INVALID_ARG;
      goto quit;
   }
 
   err = VixPropertyList_FindProperty(propList,
                                      propertyID,
                                      VIX_PROPERTYTYPE_INTEGER, 
                                      index,
                                      FALSE, 
                                      &property);
   if (VIX_OK != err) {
      goto quit;
   }

   *resultValue = property->value.intValue;

quit: 
   return err;
} // VixPropertyList_GetInteger


/*
 *-----------------------------------------------------------------------------
 *
 * VixPropertyList_SetInteger --
 *
 *       Saves a copy of a integer property value. The value is identified
 *       by the integer property ID.
 *
 *       Value names are unique within a single property list.
 *       If a previous value with the same propertyID value already
 *       existed in this property list, then it is replaced with the new
 *       value. Otherwise, a new value is added.
 *
 *       This fails if the value is present but has a different type.
 *
 * Results:
 *       VixError.
 *
 * Side effects:
 *       None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixPropertyList_SetInteger(VixPropertyListImpl *propList,      // IN
                           int propertyID,                     // IN
                           int value)                          // IN
{
   VixError err = VIX_OK;
   VixPropertyValue *property = NULL;

   if (NULL == propList) {
      err = VIX_E_INVALID_ARG;
      goto quit;
   }
 
   /*
    * Find or create an entry for this property.
    */
   err = VixPropertyList_FindProperty(propList,
                                      propertyID,
                                      VIX_PROPERTYTYPE_INTEGER, 
                                      0,
                                      TRUE, 
                                      &property);
   if (VIX_OK != err) {
      goto quit;
   }

   property->value.intValue = value;
   property->isDirty = TRUE;

quit:
   return err;
} // VixPropertyList_SetInteger


/*
 *-----------------------------------------------------------------------------
 *
 * VixPropertyList_GetBool --
 *
 *       Return a copy of a boolean property value. The value is identified
 *       by the integer property ID.
 *
 *       This fails if the value is not present, or if it is a different
 *       type, or if the caller did not pass a valid out parameter to
 *       receive the value.
 *
 * Results:
 *       VixError. VIX_OK if the property was found.
 *                 VIX_E_UNRECOGNIZED_PROPERTY if the property was not found.
 *
 * Side effects:
 *       None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixPropertyList_GetBool(VixPropertyListImpl *propList,      // IN
                        int propertyID,                     // IN
                        int index,                          // IN
                        Bool *resultValue)                  // OUT
{
   VixError err = VIX_OK;
   VixPropertyValue *property = NULL;

   if ((NULL == resultValue) || (NULL == propList)) {
      err = VIX_E_INVALID_ARG;
      goto quit;
   }

   err = VixPropertyList_FindProperty(propList,
                                      propertyID,
                                      VIX_PROPERTYTYPE_BOOL, 
                                      index,
                                      FALSE, 
                                      &property);
   if (VIX_OK != err) {
      goto quit;
   }

   if (NULL == property) {
      goto quit;
   }

   *resultValue = property->value.boolValue;

quit: 
   return err;
} // VixPropertyList_GetBool


/*
 *-----------------------------------------------------------------------------
 *
 * VixPropertyList_SetBool --
 *
 *       Saves a copy of a Bool property value. The value is identified
 *       by the integer property ID.
 *
 *       Value names are unique within a single property list.
 *       If a previous value with the same propertyID value already
 *       existed in this property list, then it is replaced with the new
 *       value. Otherwise, a new value is added.
 *
 *       This fails if the value is present but has a different type.
 *
 * Results:
 *       VixError.
 *
 * Side effects:
 *       None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixPropertyList_SetBool(VixPropertyListImpl *propList,      // IN
                        int propertyID,                     // IN
                        Bool value)                         // IN
{
   VixError err = VIX_OK;
   VixPropertyValue *property = NULL;

   if (NULL == propList) {
      err = VIX_E_INVALID_ARG;
      goto quit;
   }

   /*
    * Find or create an entry for this property.
    */
   err = VixPropertyList_FindProperty(propList,
                                      propertyID, 
                                      VIX_PROPERTYTYPE_BOOL, 
                                      0,
                                      TRUE, 
                                      &property);
   if (VIX_OK != err) {
      goto quit;
   }

   property->value.boolValue = value;
   property->isDirty = TRUE;

quit:
   return err;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VixPropertyList_GetInt64 --
 *
 *       Return a copy of a Int64 property value. The value is identified
 *       by the integer property ID.
 *
 *       This fails if the value is not present, or if it is a different
 *       type, or if the caller did not pass a valid out parameter to
 *       receive the value.
 *
 * Results:
 *       VixError. VIX_OK if the property was found.
 *                 VIX_E_UNRECOGNIZED_PROPERTY if the property was not found.
 *
 * Side effects:
 *       None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixPropertyList_GetInt64(VixPropertyListImpl *propList,     // IN
                         int propertyID,                    // IN
                         int index,                         // IN
                         int64 *resultValue)                // OUT
{
   VixError err = VIX_OK;
   VixPropertyValue *property = NULL;

   if ((NULL == resultValue) || (NULL == propList)) {
      err = VIX_E_INVALID_ARG;
      goto quit;
   }

   err = VixPropertyList_FindProperty(propList,
                                      propertyID,
                                      VIX_PROPERTYTYPE_INT64, 
                                      index,
                                      FALSE, 
                                      &property);
   if (VIX_OK != err) {
      goto quit;
   }

   *resultValue = property->value.int64Value;

quit: 
   return err;
} // VixPropertyList_GetInt64


/*
 *-----------------------------------------------------------------------------
 *
 * VixPropertyList_SetInt64 --
 *
 *       Saves a copy of a int64 property value. The value is identified
 *       by the integer property ID.
 *
 *       Value names are unique within a single property list.
 *       If a previous value with the same propertyID value already
 *       existed in this property list, then it is replaced with the new
 *       value. Otherwise, a new value is added.
 *
 *       This fails if the value is present but has a different type.
 *
 * Results:
 *       VixError.
 *
 * Side effects:
 *       None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixPropertyList_SetInt64(VixPropertyListImpl *propList,     // IN
                         int propertyID,                    // IN
                         int64 value)                       // IN
{
   VixError err = VIX_OK;
   VixPropertyValue *property = NULL;

   if (NULL == propList) {
      err = VIX_E_INVALID_ARG;
      goto quit;
   }
 
   /*
    * Find or create an entry for this property.
    */
   err = VixPropertyList_FindProperty(propList,
                                      propertyID,
                                      VIX_PROPERTYTYPE_INT64, 
                                      0,
                                      TRUE, 
                                      &property);
   if (VIX_OK != err) {
      goto quit;
   }

   property->value.int64Value = value;
   property->isDirty = TRUE;

quit:
   return err;
} // VixPropertyList_SetInt64


/*
 *-----------------------------------------------------------------------------
 *
 * VixPropertyList_GetBlob --
 *
 *       Return a copy of a Blob property value. The value is identified
 *       by the integer property ID.
 *
 *       This fails if the value is not present, or if it is a different
 *       type, or if the caller did not pass a valid out parameter to
 *       receive the value.
 *
 * Results:
 *       VixError. VIX_OK if the property was found.
 *                 VIX_E_UNRECOGNIZED_PROPERTY if the property was not found.
 *
 * Side effects:
 *       None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixPropertyList_GetBlob(VixPropertyListImpl *propList,      // IN
                        int propertyID,                     // IN
                        int index,                          // IN
                        int *resultSize,                    // OUT
                        unsigned char **resultValue)        // OUT
{
   VixError err = VIX_OK;
   VixPropertyValue *property = NULL;

   if ((NULL == propList) || (NULL == resultSize) || (NULL == resultValue)) {
      err = VIX_E_INVALID_ARG;
      goto quit;
   }
   *resultSize = 0;
   *resultValue = NULL;
 
   err = VixPropertyList_FindProperty(propList, 
                                      propertyID, 
                                      VIX_PROPERTYTYPE_BLOB, 
                                      index,
                                      FALSE,
                                      &property);
   if (VIX_OK != err) {
      goto quit;
   }

   if ((property->value.blobValue.blobSize > 0) 
         && (NULL != property->value.blobValue.blobContents)) {
      *resultSize = property->value.blobValue.blobSize;
    
      *resultValue = Util_SafeMalloc(property->value.blobValue.blobSize);
      memcpy(*resultValue, 
             property->value.blobValue.blobContents, 
             property->value.blobValue.blobSize);
   }

quit:
   return err;
} // VixPropertyList_GetBlob


/*
 *-----------------------------------------------------------------------------
 *
 * VixPropertyListSetBlobImpl --
 *
 *       Saves a copy of a blob property value. Set sensitivity.
 *
 * Results:
 *       As above.
 *
 * Side effects:
 *       None
 *
 *-----------------------------------------------------------------------------
 */

static void
VixPropertyListSetBlobImpl(VixPropertyValue *property,  // IN:
                           int blobSize,                // IN:
                           const unsigned char *value,  // IN:
                           Bool isSensitive)            // IN:
{
   if (NULL != property->value.blobValue.blobContents) {
      if (property->isSensitive) {
         Util_Zero(property->value.blobValue.blobContents,
                   property->value.blobValue.blobSize);
      }

      free(property->value.blobValue.blobContents);
      property->value.blobValue.blobContents = NULL;
   }

   property->value.blobValue.blobSize = blobSize;
   if ((NULL != value) && (blobSize > 0)) {
      property->value.blobValue.blobContents = Util_SafeMalloc(blobSize);
      memcpy(property->value.blobValue.blobContents, value, blobSize);
   }

   property->isDirty = TRUE;
   property->isSensitive = isSensitive;
} // VixPropertyListSetBlobImpl


/*
 *-----------------------------------------------------------------------------
 *
 * VixPropertyList_SetBlob --
 *
 *       Saves a copy of a blob property value. The value is identified
 *       by the integer property ID.
 *
 *       Value names are unique within a single property list.
 *       If a previous value with the same propertyID value already
 *       existed in this property list, then it is replaced with the new
 *       value. Otherwise, a new value is added.
 *
 *       This fails if the value is present but has a different type.
 *
 * Results:
 *       VixError.
 *
 * Side effects:
 *       None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixPropertyList_SetBlob(VixPropertyListImpl *propList,  // IN:
                        int propertyID,                 // IN:
                        int blobSize,                   // IN:
                        const unsigned char *value)     // IN:
{
   VixError err = VIX_OK;
   VixPropertyValue *property = NULL;

   if (NULL == propList) {
      err = VIX_E_INVALID_ARG;
      goto quit;
   }

   /*
    * Find or create an entry for this property.
    */
   err = VixPropertyList_FindProperty(propList,
                                      propertyID,
                                      VIX_PROPERTYTYPE_BLOB,
                                      0,
                                      TRUE,
                                      &property);

   if (VIX_OK == err) {
      VixPropertyListSetBlobImpl(property, blobSize, value,
                                 property->isSensitive);
   }

quit:
   return err;
} // VixPropertyList_SetBlob


/*
 *-----------------------------------------------------------------------------
 *
 * VixPropertyList_SetBlobSensitive --
 *
 *       Saves a copy of a blob property value. The value is identified
 *       by the integer property ID. Set sentivity.
 *
 *       Value names are unique within a single property list.
 *       If a previous value with the same propertyID value already
 *       existed in this property list, then it is replaced with the new
 *       value. Otherwise, a new value is added.
 *
 *       This fails if the value is present but has a different type.
 *
 * Results:
 *       VixError.
 *
 * Side effects:
 *       None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixPropertyList_SetBlobSensitive(VixPropertyListImpl *propList,  // IN:
                                 int propertyID,                 // IN:
                                 int blobSize,                   // IN:
                                 const unsigned char *value)     // IN:
{
   VixError err = VIX_OK;
   VixPropertyValue *property = NULL;

   if (NULL == propList) {
      err = VIX_E_INVALID_ARG;
      goto quit;
   }

   /*
    * Find or create an entry for this property.
    */
   err = VixPropertyList_FindProperty(propList,
                                      propertyID,
                                      VIX_PROPERTYTYPE_BLOB,
                                      0,
                                      TRUE,
                                      &property);

   if (VIX_OK == err) {
      VixPropertyListSetBlobImpl(property, blobSize, value, TRUE);
   }

quit:
   return err;
} // VixPropertyList_SetBlob


/*
 *-----------------------------------------------------------------------------
 *
 * VixPropertyList_GetPtr --
 *
 *       Return a copy of a void* property value. The value is identified
 *       by the integer property ID.
 *
 *       This is a SHALLOW copy. It only copies the pointer, not what the
 *       pointer references.
 *
 *       This fails if the value is not present, or if it is a different
 *       type, or if the caller did not pass a valid out parameter to
 *       receive the value.
 *
 * Results:
 *       VixError. VIX_OK if the property was found.
 *                 VIX_E_UNRECOGNIZED_PROPERTY if the property was not found.
 *
 * Side effects:
 *       None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixPropertyList_GetPtr(VixPropertyListImpl *propList,     // IN
                       int propertyID,                    // IN
                       int index,                         // IN
                       void **resultValue)                // OUT
{
   VixError err = VIX_OK;
   VixPropertyValue *property = NULL;

   if ((NULL == resultValue) || (NULL == propList)) {
      err = VIX_E_INVALID_ARG;
      goto quit;
   }

   err = VixPropertyList_FindProperty(propList,
                                      propertyID,
                                      VIX_PROPERTYTYPE_POINTER, 
                                      index,
                                      FALSE, 
                                      &property);
   if (VIX_OK != err) {
      goto quit;
   }

   *resultValue = property->value.ptrValue;

quit: 
   return err;
} // VixPropertyList_GetPtr


/*
 *-----------------------------------------------------------------------------
 *
 * VixPropertyList_SetPtr --
 *
 *       Saves a copy of a ptr property value. The value is identified
 *       by the integer property ID.
 *
 *       This is a SHALLOW copy. It only copies the pointer, not what the
 *       pointer references.
 *
 *       Value names are unique within a single property list.
 *       If a previous value with the same propertyID value already
 *       existed in this property list, then it is replaced with the new
 *       value. Otherwise, a new value is added.
 *
 *       This fails if the value is present but has a different type.
 *
 * Results:
 *       VixError.
 *
 * Side effects:
 *       None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixPropertyList_SetPtr(VixPropertyListImpl *propList,     // IN
                       int propertyID,                    // IN
                       void *value)                       // IN
{
   VixError err = VIX_OK;
   VixPropertyValue *property = NULL;

   if (NULL == propList) {
      err = VIX_E_INVALID_ARG;
      goto quit;
   }
 
   /*
    * Find or create an entry for this property.
    */
   err = VixPropertyList_FindProperty(propList,
                                      propertyID,
                                      VIX_PROPERTYTYPE_POINTER, 
                                      0,
                                      TRUE, 
                                      &property);
   if (VIX_OK != err) {
      goto quit;
   }

   property->value.ptrValue = value;
   property->isDirty = TRUE;

quit:
   return err;
} // VixPropertyList_SetPtr


/*
 *-----------------------------------------------------------------------------
 *
 * VixPropertyList_PropertyExists --
 *
 *
 * Results:
 *       Bool
 *
 * Side effects:
 *
 *-----------------------------------------------------------------------------
 */

Bool
VixPropertyList_PropertyExists(VixPropertyListImpl *propList,     // IN
                               int propertyID,                    // IN
                               VixPropertyType type)              // IN
{
   VixError err = VIX_OK;
   VixPropertyValue *property = NULL;
   Bool foundIt = FALSE;

   err = VixPropertyList_FindProperty(propList,
                                      propertyID,
                                      type,
                                      0, // index
                                      FALSE, // createIfMissing
                                      &property);
   if ((VIX_OK == err) && (NULL != property)) {
      foundIt = TRUE;
   }

   return foundIt;
} // VixPropertyList_PropertyExists


/*
 *-----------------------------------------------------------------------------
 *
 * VixPropertyList_NumItems --
 *
 *       Returns a count of the properties in the list.
 *
 * Results:
 *       int - Number of properties in property list.
 *
 * Side effects:
 *       None.
 *
 *-----------------------------------------------------------------------------
 */

int
VixPropertyList_NumItems(VixPropertyListImpl *propList)     // IN
{
   VixPropertyValue *prop;
   int count = 0;

   if (propList == NULL) {
      return 0;
   }

   for (prop = propList->properties; prop != NULL; prop = prop->next) {
      ++count;
   }

   return count;
} // VixPropertyList_NumItems


/*
 *-----------------------------------------------------------------------------
 *
 * VixPropertyList_Empty --
 *
 *       Returns whether the property list has no properties.
 *
 * Results:
 *       Bool - True iff property list has no properties.
 *
 * Side effects:
 *       None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
VixPropertyList_Empty(VixPropertyListImpl *propList)     // IN
{
   return (propList == NULL || propList->properties == NULL);
} // VixPropertyList_Empty


