/*********************************************************
 * Copyright (C) 2004 VMware, Inc. All rights reserved.
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
#include "vm_version.h"
#include "util.h"
#include "str.h"

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
         free(property->value.strValue);
      } else if (VIX_PROPERTYTYPE_BLOB == property->type) {
         free(property->value.blobValue.blobContents);
      }
      
      free(property);
   }
} // VixPropertyList_RemoveAllWithoutHandles


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
VixPropertyList_Serialize(VixPropertyListImpl    *propList,       // IN
                          Bool dirtyOnly,                         // IN
                          size_t *resultSize,                     // OUT
                          char **resultBuffer)                    // OUT
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
 
   if ((NULL == propList) ||
       (NULL == resultSize) ||
       (NULL == resultBuffer)) {
      err = VIX_E_INVALID_ARG;
      goto abort;
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
      
      switch(property->type) {
         ////////////////////////////////////////////////////////
         case VIX_PROPERTYTYPE_INTEGER:
            bufferSize += PROPERTY_SIZE_INT32;
            break;

         ////////////////////////////////////////////////////////
         case VIX_PROPERTYTYPE_STRING:
            if (property->value.strValue) {
               bufferSize += (strlen(property->value.strValue) + 1);
            } else {
               err = VIX_E_INVALID_ARG;
               goto abort;
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
            bufferSize += PROPERTY_SIZE_POINTER;
            break;

         ////////////////////////////////////////////////////////
         default:
            err = VIX_E_UNRECOGNIZED_PROPERTY;
            goto abort;     
      }

      property = property->next;
   }

   *resultBuffer = (char*) Util_SafeCalloc(1, bufferSize);
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

      switch(property->type) {
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
               goto abort;
            }
            break;

         ////////////////////////////////////////////////////////
         case VIX_PROPERTYTYPE_POINTER:
            if (property->value.ptrValue) {
               valueLength = PROPERTY_SIZE_POINTER;
               memcpy(&(serializeBuffer[pos]), &valueLength, propertyValueLengthSize);
               pos += propertyValueLengthSize;
               memcpy(&(serializeBuffer[pos]), &(property->value.ptrValue), sizeof(property->value.ptrValue));
            } else {
               err = VIX_E_INVALID_ARG;
               goto abort;
            }
            break;

         ////////////////////////////////////////////////////////
         default:
             err = VIX_E_UNRECOGNIZED_PROPERTY;
             goto abort;     
      }
      
      pos += valueLength;
      property = property->next;
   }

   ASSERT(pos == bufferSize);
   *resultSize = bufferSize;
   
abort:
   if (VIX_OK != err) {
      free(serializeBuffer);
      *resultBuffer = NULL;
      *resultSize = 0;
   }

   return err;
} // FoundryPropertList_Serialize


/*
 *-----------------------------------------------------------------------------
 *
 * VixPropertyList_Deserialize --
 *
 *       Deserialize a property list from a buffer. 
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
VixPropertyList_Deserialize(VixPropertyListImpl *propList,     // IN
                            const char *buffer,                // IN
                            size_t bufferSize)                 // IN
{
   VixError err = VIX_OK;
   VixPropertyValue *property = NULL;
   size_t pos = 0;
   char *strPtr;
   int *intPtr;
   Bool *boolPtr;
   int64 *int64Ptr;
   void **ptrPtr;
   unsigned char* blobPtr;
   int *propertyIDPtr;
   int *lengthPtr;
   size_t propertyIDSize;
   size_t propertyTypeSize;
   size_t propertyValueLengthSize;
   size_t headerSize;
   VixPropertyType *propertyTypePtr;

   if ((NULL == propList)
       || (NULL == buffer)) {
      err = VIX_E_INVALID_ARG;
      goto abort;
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
       * Create the property if missing
       */
      err = VixPropertyList_FindProperty(propList,
                                         *propertyIDPtr,
                                         *propertyTypePtr,
                                         0, // index
                                         TRUE, //createIfMissing
                                         &property);
      
      if (VIX_OK != err) {
         goto abort;
      }
      
      /*
       * Initialize the property to the received value
       */
      switch(*propertyTypePtr) {
         ////////////////////////////////////////////////////////
         case VIX_PROPERTYTYPE_INTEGER:
            intPtr = (int*) &(buffer[pos]);
            property->value.intValue = *intPtr;
            break;

         ////////////////////////////////////////////////////////
         case VIX_PROPERTYTYPE_STRING:
            strPtr = (char*) &(buffer[pos]);
            free(property->value.strValue);
            property->value.strValue = Util_SafeStrdup(strPtr);
            break;

         ////////////////////////////////////////////////////////
         case VIX_PROPERTYTYPE_BOOL:
            boolPtr = (Bool*) &(buffer[pos]);
            property->value.boolValue = *boolPtr;
            break;
         
         ////////////////////////////////////////////////////////
         case VIX_PROPERTYTYPE_INT64:
            int64Ptr = (int64*) &(buffer[pos]);
            property->value.int64Value = *int64Ptr;
            break;

         ////////////////////////////////////////////////////////
         case VIX_PROPERTYTYPE_BLOB:
            blobPtr = (unsigned char*) &(buffer[pos]);
            property->value.blobValue.blobSize = *lengthPtr;
            memcpy(property->value.blobValue.blobContents, blobPtr, *lengthPtr);
            break;

         ////////////////////////////////////////////////////////
         case VIX_PROPERTYTYPE_POINTER:
            // The size may be different on different machines.
            // To be safe, we always use 8 bytes.
            ptrPtr = (void**) &(buffer[pos]);
            property->value.ptrValue = *ptrPtr;
            break;

         ////////////////////////////////////////////////////////
         default:
            err = VIX_E_UNRECOGNIZED_PROPERTY;
            goto abort;     
      }

      pos += *lengthPtr;
   }

abort:   
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
      goto abort;
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
            goto abort;
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
      goto abort;
   }

   err = VixPropertyListAppendProperty(propList,
                                       propertyID,
                                       type,
                                       resultEntry);

abort:
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
      goto abort;
   }
   *resultEntry = NULL;

   property = (VixPropertyValue *) 
      Util_SafeCalloc(1, sizeof(VixPropertyValue));

   property->type = type;
   property->propertyID = propertyID;
   property->isDirty = TRUE;

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

abort:
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
 *                     VIX_E_UNRECOGNIZED_PROPERTY if the property was not found.
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
      goto abort;
   }
   *resultValue = NULL;

   err = VixPropertyList_FindProperty(propList, 
                                      propertyID, 
                                      VIX_PROPERTYTYPE_STRING, 
                                      index,
                                      FALSE,
                                      &property);
   if (VIX_OK != err) {
      goto abort;
   }

   if (NULL != property->value.strValue) {
      *resultValue = Util_SafeStrdup(property->value.strValue);
   }

abort:
   return err;
} // VixPropertyList_GetString


/*
 *-----------------------------------------------------------------------------
 *
 * VixPropertyList_SetString --
 *
 *       Saves a copy of a string property value. The value is identified
 *       by the integer property ID.
 *
 *       Value names are unique within a single proeprty list.
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
VixPropertyList_SetString(VixPropertyListImpl *propList,    // IN
                          int propertyID,                   // IN
                          const char *value)                // IN
{
   VixError err = VIX_OK;
   VixPropertyValue *property = NULL;

   if (NULL == propList) {
      err = VIX_E_INVALID_ARG;
      goto abort;
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
   if (VIX_OK != err) {
      goto abort;
   }

   if (NULL != property->value.strValue) {
      free(property->value.strValue);
      property->value.strValue = NULL;
   }
   if (NULL != value) {
      property->value.strValue = Util_SafeStrdup(value);
   }
   property->isDirty = TRUE;

abort:
   return(err);
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
 *                     VIX_E_UNRECOGNIZED_PROPERTY if the property was not found.
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
      goto abort;
   }
   
   err = VixPropertyList_FindProperty(propList,
                                      propertyID,
                                      VIX_PROPERTYTYPE_INTEGER, 
                                      index,
                                      FALSE, 
                                      &property);
   if (VIX_OK != err) {
      goto abort;
   }

   *resultValue = property->value.intValue;

abort:   
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
 *       Value names are unique within a single proeprty list.
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
      goto abort;
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
      goto abort;
   }

   property->value.intValue = value;
   property->isDirty = TRUE;

abort:
   return(err);
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
 *                     VIX_E_UNRECOGNIZED_PROPERTY if the property was not found.
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
      goto abort;
   }

   err = VixPropertyList_FindProperty(propList,
                                      propertyID,
                                      VIX_PROPERTYTYPE_BOOL, 
                                      index,
                                      FALSE, 
                                      &property);
   if (VIX_OK != err) {
      goto abort;
   }

   if (NULL == property) {
      goto abort;
   }

   *resultValue = property->value.boolValue;

abort:   
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
 *       Value names are unique within a single proeprty list.
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
      goto abort;
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
      goto abort;
   }

   property->value.boolValue = value;
   property->isDirty = TRUE;

abort:
   return(err);
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
 *                     VIX_E_UNRECOGNIZED_PROPERTY if the property was not found.
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
      goto abort;
   }

   err = VixPropertyList_FindProperty(propList,
                                      propertyID,
                                      VIX_PROPERTYTYPE_INT64, 
                                      index,
                                      FALSE, 
                                      &property);
   if (VIX_OK != err) {
      goto abort;
   }

   *resultValue = property->value.int64Value;

abort:   
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
 *       Value names are unique within a single proeprty list.
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
      goto abort;
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
      goto abort;
   }

   property->value.int64Value = value;
   property->isDirty = TRUE;

abort:
   return(err);
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
 *                     VIX_E_UNRECOGNIZED_PROPERTY if the property was not found.
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
      goto abort;
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
      goto abort;
   }

   if ((property->value.blobValue.blobSize > 0) 
         && (NULL != property->value.blobValue.blobContents)) {
      *resultSize = property->value.blobValue.blobSize;
      
      *resultValue = Util_SafeMalloc(property->value.blobValue.blobSize);
      memcpy(*resultValue, 
             property->value.blobValue.blobContents, 
             property->value.blobValue.blobSize);
   }

abort:
   return err;
} // VixPropertyList_GetBlob


/*
 *-----------------------------------------------------------------------------
 *
 * VixPropertyList_SetBlob --
 *
 *       Saves a copy of a blob property value. The value is identified
 *       by the integer property ID.
 *
 *       Value names are unique within a single proeprty list.
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
VixPropertyList_SetBlob(VixPropertyListImpl *propList,      // IN
                        int propertyID,                     // IN
                        int blobSize,                       // IN
                        const unsigned char *value)         // IN
{
   VixError err = VIX_OK;
   VixPropertyValue *property = NULL;

   if (NULL == propList) {
      err = VIX_E_INVALID_ARG;
      goto abort;
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
   if (VIX_OK != err) {
      goto abort;
   }

   if (NULL != property->value.blobValue.blobContents) {
      free(property->value.blobValue.blobContents);
      property->value.blobValue.blobContents = NULL;
   }

   property->value.blobValue.blobSize = blobSize;
   if ((NULL != value) && (blobSize > 0)) {
      property->value.blobValue.blobContents = Util_SafeMalloc(blobSize);
      memcpy(property->value.blobValue.blobContents, value, blobSize);
   }

   property->isDirty = TRUE;

abort:
   return(err);
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
      goto abort;
   }

   err = VixPropertyList_FindProperty(propList,
                                      propertyID,
                                      VIX_PROPERTYTYPE_POINTER, 
                                      index,
                                      FALSE, 
                                      &property);
   if (VIX_OK != err) {
      goto abort;
   }

   *resultValue = property->value.ptrValue;

abort:   
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
 *       Value names are unique within a single proeprty list.
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
      goto abort;
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
      goto abort;
   }

   property->value.ptrValue = value;
   property->isDirty = TRUE;

abort:
   return(err);
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

   return(foundIt);
} // VixPropertyList_PropertyExists


