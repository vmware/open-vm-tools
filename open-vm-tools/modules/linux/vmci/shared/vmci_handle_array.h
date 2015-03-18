/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 *********************************************************/

/*
 * vmci_handle_array.h --
 *
 *	Simple dynamic array.
 */

#ifndef _VMCI_HANDLE_ARRAY_H_
#define _VMCI_HANDLE_ARRAY_H_

#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMKERNEL
#include "includeCheck.h"

#include "vmci_kernel_if.h"
#include "vmware.h"

#include "vmci_defs.h"
#include "vm_assert.h"
#ifdef VMKERNEL
#include "vm_libc.h"
#endif // VMKERNEL

#define VMCI_HANDLE_ARRAY_DEFAULT_SIZE 4

typedef struct VMCIHandleArray {
   uint32          capacity;
   uint32          size;
   VMCIHandle      entries[1];
} VMCIHandleArray;


/*
 *-----------------------------------------------------------------------------------
 *
 * VMCIHandleArray_Create --
 *
 * Results:
 *      Array if successful, NULL if not.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------------
 */

static INLINE VMCIHandleArray *
VMCIHandleArray_Create(uint32 capacity)
{
   VMCIHandleArray *array;

   if (capacity == 0) {
      capacity = VMCI_HANDLE_ARRAY_DEFAULT_SIZE;
   }

   array = (VMCIHandleArray *)VMCI_AllocKernelMem(sizeof array->capacity +
                                                  sizeof array->size +
                                                  capacity * sizeof(VMCIHandle),
                                                  VMCI_MEMORY_NONPAGED |
                                                     VMCI_MEMORY_ATOMIC);
   if (array == NULL) {
      return NULL;
   }
   array->capacity = capacity;
   array->size = 0;

   return array;
}


/*
 *-----------------------------------------------------------------------------------
 *
 * VMCIHandleArray_Destroy --
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------------
 */

static INLINE void
VMCIHandleArray_Destroy(VMCIHandleArray *array)
{
   VMCI_FreeKernelMem(array,
                      sizeof array->capacity + sizeof array->size +
                      array->capacity * sizeof(VMCIHandle));
}


/*
 *-----------------------------------------------------------------------------------
 *
 * VMCIHandleArray_AppendEntry --
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Array may be reallocated.
 *
 *-----------------------------------------------------------------------------------
 */

static INLINE void
VMCIHandleArray_AppendEntry(VMCIHandleArray **arrayPtr,
                            VMCIHandle handle)
{
   VMCIHandleArray *array;

   ASSERT(arrayPtr && *arrayPtr);
   array = *arrayPtr;

   if (UNLIKELY(array->size >= array->capacity)) {
      /* reallocate. */
      uint32 arraySize = sizeof array->capacity + sizeof array->size +
         array->capacity * sizeof(VMCIHandle);
      VMCIHandleArray *newArray = (VMCIHandleArray *)
         VMCI_AllocKernelMem(arraySize + array->capacity * sizeof(VMCIHandle),
                             VMCI_MEMORY_NONPAGED | VMCI_MEMORY_ATOMIC);
      if (newArray == NULL) {
         return;
      }
      memcpy(newArray, array, arraySize);
      newArray->capacity *= 2;
      VMCI_FreeKernelMem(array, arraySize);
      *arrayPtr = newArray;
      array = newArray;
   }
   array->entries[array->size] = handle;
   array->size++;
}


/*
 *-----------------------------------------------------------------------------------
 *
 * VMCIHandleArray_RemoveEntry --
 *
 * Results:
 *      Handle that was removed, VMCI_INVALID_HANDLE if entry not found.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------------
 */

static INLINE VMCIHandle
VMCIHandleArray_RemoveEntry(VMCIHandleArray *array,
                            VMCIHandle entryHandle)
{
   uint32 i;
   VMCIHandle handle = VMCI_INVALID_HANDLE;

   ASSERT(array);
   for (i = 0; i < array->size; i++) {
      if (VMCI_HANDLE_EQUAL(array->entries[i], entryHandle)) {
	 handle = array->entries[i];
	 array->entries[i] = array->entries[array->size-1];
	 array->entries[array->size-1] = VMCI_INVALID_HANDLE;
	 array->size--;
	 break;
      }
   }

   return handle;
}


/*
 *-----------------------------------------------------------------------------------
 *
 * VMCIHandleArray_RemoveTail --
 *
 * Results:
 *      Handle that was removed, VMCI_INVALID_HANDLE if array was empty.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------------
 */

static INLINE VMCIHandle
VMCIHandleArray_RemoveTail(VMCIHandleArray *array)
{
   VMCIHandle handle;

   if (array->size == 0) {
      return VMCI_INVALID_HANDLE;
   }
   handle = array->entries[array->size-1];
   array->entries[array->size-1] = VMCI_INVALID_HANDLE;
   array->size--;

   return handle;
}


/*
 *-----------------------------------------------------------------------------------
 *
 * VMCIHandleArray_GetEntry --
 *
 * Results:
 *      Handle at given index, VMCI_INVALID_HANDLE if invalid index.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------------
 */

static INLINE VMCIHandle
VMCIHandleArray_GetEntry(const VMCIHandleArray *array,
                         uint32 index)
{
   ASSERT(array);
   if (UNLIKELY(index >= array->size)) {
      return VMCI_INVALID_HANDLE;
   }

   return array->entries[index];
}


/*
 *-----------------------------------------------------------------------------------
 *
 * VMCIHandleArray_GetSize --
 *
 * Results:
 *      Number of entries in array.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------------
 */

static INLINE uint32
VMCIHandleArray_GetSize(const VMCIHandleArray *array)
{
   ASSERT(array);
   return array->size;
}


/*
 *-----------------------------------------------------------------------------------
 *
 * VMCIHandleArray_HasEntry --
 *
 * Results:
 *      TRUE is entry exists in array, FALSE if not.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------------
 */

static INLINE Bool
VMCIHandleArray_HasEntry(const VMCIHandleArray *array,
                         VMCIHandle entryHandle)
{
   uint32 i;

   ASSERT(array);
   for (i = 0; i < array->size; i++) {
      if (VMCI_HANDLE_EQUAL(array->entries[i], entryHandle)) {
	 return TRUE;
      }
   }

   return FALSE;
}


/*
 *-----------------------------------------------------------------------------------
 *
 * VMCIHandleArray_GetCopy --
 *
 * Results:
 *      Returns pointer to copy of array on success or NULL, if memory allocation
 *      fails.
 *
 * Side effects:
 *      Allocates nonpaged memory.
 *
 *-----------------------------------------------------------------------------------
 */

static INLINE VMCIHandleArray *
VMCIHandleArray_GetCopy(const VMCIHandleArray *array)
{
   VMCIHandleArray *arrayCopy;

   ASSERT(array);

   arrayCopy = (VMCIHandleArray *)VMCI_AllocKernelMem(sizeof array->capacity +
                                                      sizeof array->size +
                                                      array->size * sizeof(VMCIHandle),
                                                      VMCI_MEMORY_NONPAGED |
                                                         VMCI_MEMORY_ATOMIC);
   if (arrayCopy != NULL) {
      memcpy(&arrayCopy->size, &array->size,
             sizeof array->size + array->size * sizeof(VMCIHandle));
      arrayCopy->capacity = array->size;
   }

   return arrayCopy;
}


/*
 *-----------------------------------------------------------------------------------
 *
 * VMCIHandleArray_GetHandles --
 *
 * Results:
 *      NULL if the array is empty. Otherwise, a pointer to the array
 *      of VMCI handles in the handle array.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------------
 */

static INLINE VMCIHandle *
VMCIHandleArray_GetHandles(VMCIHandleArray *array) // IN
{
   ASSERT(array);

   if (array->size) {
      return array->entries;
   } else {
      return NULL;
   }
}

#endif // _VMCI_HANDLE_ARRAY_H_
