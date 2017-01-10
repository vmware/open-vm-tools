//////////////////////////////////////////////////////////////////////////////
//  
//  Author:		Greg Burk
//
//  Purpose:	This template provides a way to create an array that has
//				certain safety features that help solve some of the problems
//				often created by arrays in C++.  You can create an array of
//				any type.  The safety features of this class include array
//				bounds checking (indexes out of range), automatic
//				initialization, and automatic destruction.  It also provides
//				some features to check the integrity of the array.  This is
//				especially useful when you get and pass the raw pointer to
//				the array into a third party function (i.e. Windows API).
//				
//				The ${TDynamicArray::verify()} function is used for this
//				purpose.  It does two primary things:
//
//				1) When the array is allocated, some extra space is allocated
//				for some sentinel bytes.  These bytes are checked to make
//				sure they are still intact when verify() is called.
//
//				2) When the array is allocated, the address of the memory
//				that was allocated is stored and is XORed with a known
//				bit pattern and the result is also stored.  When verify()
//				is called, this pattern is XORed again with the address
//				and the result should be the original pattern.
//
//				If either of the above checks fails, an exception is thrown.
//				The primary weakness of this strategy is that the call to
//				verify() is left up to the user.  If the raw pointer is passed
//				and something gets messed up, if verify() was not called, the
//				problem won't be discovered until the next time a function
//				that calls verify() (most do) is called.  This will cause us
//				to loose the context in which the problem occurred.
//
//  Created: Wednesday, August 07, 2002 2:27:39 PM
//  
//	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
//	-- VMware Confidential
//  
//////////////////////////////////////////////////////////////////////////////
#ifndef _TDynamicArray_H_
#define _TDynamicArray_H_

#include <string.h>
#include "Exception/CCafException.h"

namespace Caf {

//////////////////////////////////////////////////////////////////////////////
//	Define sentinel bit pattern.
//////////////////////////////////////////////////////////////////////////////
#ifdef __x86_64__
static const uint64 gs_ulDynamicArraySentinelBitPattern = (0xAAAAAAAAAAAAAAAA);
#else
static const uint32 gs_ulDynamicArraySentinelBitPattern = (0xAAAAAAAA);
#endif
static const uint32 gs_ulDynamicArraySentinelElementCount = (3);

template<typename T, typename Allocator>
class TDynamicArray {
public:
	typedef T Type;

	//////////////////////////////////////////////////////////////////////////
	// Default Constructor
	//////////////////////////////////////////////////////////////////////////
	TDynamicArray() :
		CAF_CM_INIT("TDynamicArray"),
		_sentinelBits(gs_ulDynamicArraySentinelBitPattern),
		_isSentinelSet(false),
		_elementCount(0),
		_elementIndex(0),
		_byteCount(0),
		_data(NULL) {
		// Initialize the sentinel buffers.
		::memset(_sentinelBytes, 0, sizeof(_sentinelBytes));
	}

	//////////////////////////////////////////////////////////////////////////
	// Destructor
	//////////////////////////////////////////////////////////////////////////
	virtual ~TDynamicArray() {
		CAF_CM_FUNCNAME("~TDynamicArray");
		try {
			freeArray();
		}
		CAF_CM_CATCH_ALL;
		CAF_CM_CLEAREXCEPTION;
	}

	//////////////////////////////////////////////////////////////////////////
	// PutDescription
	//
	// Assign description to object instance.
	//////////////////////////////////////////////////////////////////////////
	void putDescription(const char * pszDesc) {
		CAF_CM_FUNCNAME_VALIDATE("putDescription");
		CAF_CM_VALIDATE_STRINGPTRA(pszDesc);
		_description = pszDesc;
	}

	//////////////////////////////////////////////////////////////////////////
	// GetPtr
	//
	// Get const pointer to internal data
	//////////////////////////////////////////////////////////////////////////
	const T * getPtr() const {
		// Pre-validation.
		verifySentinel();

		// Get pointer.
		const T * rc = _data;

		return rc;
	}

	//////////////////////////////////////////////////////////////////////////
	// GetNonConstPtr
	//
	// Get non-const pointer to internal data.  This function should be used
	// only when you must get a pointer that is to be written to, and you
	// should always call the ${TDynamicArray::verify()} function after modifying the data
	// pointed to by this pointer or passing the pointer to a function that
	// modifies the data pointed to by this pointer.
	//////////////////////////////////////////////////////////////////////////
	T * getNonConstPtr() {
		// Pre-validation.
		verifySentinel();

		// Get pointer.
		T * rc = _data;

		return rc;
	}

	//////////////////////////////////////////////////////////////////////////
	// const Conversion Operator
	//////////////////////////////////////////////////////////////////////////
	operator const T *() const {
		// Pre-validation.
		verifySentinel();

		// Get pointer.
		const T * rc = _data;

		return rc;
	}

	//////////////////////////////////////////////////////////////////////////
	// GetPtrAt
	//
	// Returns a const pointer to the internal array at a given index
	//////////////////////////////////////////////////////////////////////////
	const T * getPtrAt(const uint32 elementIndex) const {
		// Pre-validation.
		verifyNotNull();
		verifySentinel();
		verifyElementCount(elementIndex);

		// Get the pointer at the index specified.
		const T * rc = &_data[elementIndex];

		return rc;
	}

	//////////////////////////////////////////////////////////////////////////
	// GetNonConstPtrAt
	//
	// Returns a non-const pointer to the internal array at a given index.
	// This function should be used only when you must get a pointer that is
	// to be written to, and you should always call the ${TDynamicArray::verify()}
	// function after modifying the data pointed to by this pointer or passing
	// the pointer to a function that modifies the data pointed to by this
	// pointer.
	//////////////////////////////////////////////////////////////////////////
	T * getNonConstPtrAt(const uint32 elementIndex) {
		// Pre-validation.
		verifyNotNull();
		verifySentinel();
		verifyElementCount(elementIndex);

		// Get the pointer at the index specified.
		T * rc = &_data[elementIndex];

		return rc;
	}

	//////////////////////////////////////////////////////////////////////////
	// GetAt
	//
	// Returns the array element at a given index
	//////////////////////////////////////////////////////////////////////////
	T getAt(const uint32 elementIndex) const {
		// Pre-validation.
		verifyNotNull();
		verifySentinel();
		verifyElementCount(elementIndex);

		// Get the pointer at the index specified.
		T tRetVal = _data[elementIndex];

		return tRetVal;
	}

	//////////////////////////////////////////////////////////////////////////
	// getPtrAtCurrentPos
	//
	// Returns a const pointer to the internal array at the current position.
	//////////////////////////////////////////////////////////////////////////
	const T * getPtrAtCurrentPos() const {
		return getPtrAt(_elementIndex);
	}

	//////////////////////////////////////////////////////////////////////////
	// getNonConstPtrAtCurrentPos
	//
	// Returns a non-const pointer to the internal array at the current position.
	// This function should be used only when you must get a pointer that is
	// to be written to, and you should always call the ${TDynamicArray::verify()}
	// function after modifying the data pointed to by this pointer or passing
	// the pointer to a function that modifies the data pointed to by this
	// pointer.
	//////////////////////////////////////////////////////////////////////////
	T * getNonConstPtrAtCurrentPos() {
		return getNonConstPtrAt(_elementIndex);
	}

	//////////////////////////////////////////////////////////////////////////
	// GetAtCurrentPos
	//
	// Returns the array element at the current position
	//////////////////////////////////////////////////////////////////////////
	T getAtCurrentPos() const {
		return getAt(_elementIndex);
	}

	//////////////////////////////////////////////////////////////////////////
	// SetAt
	//
	// Sets the array element at a given index
	//////////////////////////////////////////////////////////////////////////
	void setAt(
			const uint32 elementIndex,
			const T value) {
		// Pre-validation.
		verifyNotNull();
		verifySentinel();
		verifyElementCount(elementIndex);

		// Set the pointer at the index specified.
		_data[elementIndex] = value;
	}

	//////////////////////////////////////////////////////////////////////////
	// getElementCount
	//
	// Returns the number of array elements.
	//////////////////////////////////////////////////////////////////////////
	uint32 getElementCount() const {
		return _elementCount;
	}

	//////////////////////////////////////////////////////////////////////////
	// getByteCount
	//
	// Returns the size of the array in bytes.
	//////////////////////////////////////////////////////////////////////////
	uint32 getByteCount() const {
		return _byteCount;
	}

	//////////////////////////////////////////////////////////////////////////
	// getByteCountSize
	//
	// Returns the size of the array in bytes.
	//////////////////////////////////////////////////////////////////////////
	size_t getByteCountSize() const {
		return static_cast<size_t>(_byteCount);
	}

	//////////////////////////////////////////////////////////////////////////
	// IsNull
	//
	// Returns true if the pointer to the internal array is null or false if
	// the array is not null
	//////////////////////////////////////////////////////////////////////////
	bool isNull() const {
		return ((NULL == _data) ? true : false);
	}

	//////////////////////////////////////////////////////////////////////////
	// verify
	//
	// Verifies that the array is still properly bound and in good shape.
	//////////////////////////////////////////////////////////////////////////
	void verify() const {
		// verify.
		verifySentinel();
	}

	//////////////////////////////////////////////////////////////////////////
	// Allocate
	//
	// Allocates a new array on the heap (elementCount = number of elements)
	//////////////////////////////////////////////////////////////////////////
	TDynamicArray & allocateElements(const uint32 elementCount) {
		// Free the previous array if it exists.
		freeArray();

		// Call internal allocation function to actually allocate the
		// array buffer.
		internalAllocate(elementCount);

		return (*this);
	}

	//////////////////////////////////////////////////////////////////////////
	// AllocateBytes
	//
	// Allocates a new array on the heap.
	//////////////////////////////////////////////////////////////////////////
	TDynamicArray & allocateBytes(const size_t byteCount) {
		return allocateBytes(static_cast<const uint32>(byteCount));
	}

	//////////////////////////////////////////////////////////////////////////
	// AllocateBytes
	//
	// Allocates a new array on the heap.
	//////////////////////////////////////////////////////////////////////////
	TDynamicArray & allocateBytes(const uint32 byteCount) {
		// Calculate actual number of array elements to allocate based on
		// the byte length.
		const uint32 elementCount = byteCountToElementCount(byteCount);

		// Delegate to Allocate().
		allocateElements(elementCount);

		return (*this);
	}

	//////////////////////////////////////////////////////////////////////////
	// ReAllocate
	//
	// Reallocates a array on the heap, keeping original contents.
	// culLength = number of elements
	//////////////////////////////////////////////////////////////////////////
	TDynamicArray & reallocateElements(const uint32 elementCount) {
		CAF_CM_FUNCNAME("reallocateElements");

		// Declare variables to hold copy of original information.
		uint32 origElementIndex = 0;
		uint32 origByteCount = 0;
		T * origData = NULL;

		try {
			// Pre-validation.
			verifySentinel();

			// Create a temporary copy of the original buffer and length.
			origElementIndex = _elementIndex;
			origByteCount = _byteCount;
			origData = _data;

			// Reset the data pointer, length, and byte length.
			_data = NULL;
			_elementCount = 0;
			_elementIndex = 0;
			_byteCount = 0;

			// Reset the sentinel set flag.
			_isSentinelSet = false;

			// Call internal allocation function to actually allocate the
			// array buffer.
			internalAllocate(elementCount);

			// Copy the original into the new if it exists.
			if((origData != NULL) && (origByteCount > 0)) {
				// Calculate the number of bytes to copy.
				const uint32 bytesToCopy =
						(_byteCount < origByteCount) ? _byteCount : origByteCount;

				// Copy the bytes.
				::memcpy(_data, origData, bytesToCopy);

				const uint32 elementsToCopy = byteCountToElementCount(bytesToCopy);
				_elementIndex =
						(elementsToCopy < origElementIndex) ? elementsToCopy : origElementIndex;
			}

			// verify the sentinel bytes.
			verifySentinel();
		}
		CAF_CM_CATCH_ALL;

		// Delete the original buffer if non-null.
		if (origData) {
			Allocator::freeMemory(origData);
		}

		CAF_CM_THROWEXCEPTION;

		return (*this);
	}

	//////////////////////////////////////////////////////////////////////////
	// Grow
	//
	// Increases the size of the array by the length
	// (number of elements) supplied
	//////////////////////////////////////////////////////////////////////////
	TDynamicArray & grow(const uint32 elementCount) {
		reallocateElements(_elementCount + elementCount);

		return (*this);
	}

	//////////////////////////////////////////////////////////////////////////
	// ReAllocateBytes
	//
	// Reallocates a array on the heap, keeping original contents.
	//////////////////////////////////////////////////////////////////////////
	TDynamicArray & reallocateBytes(const size_t byteCount) {
		return reallocateBytes(static_cast<const uint32>(byteCount));
	}

	//////////////////////////////////////////////////////////////////////////
	// ReAllocateBytes
	//
	// Reallocates a array on the heap, keeping original contents.
	//////////////////////////////////////////////////////////////////////////
	TDynamicArray & reallocateBytes(const uint32 byteCount) {
		// Calculate actual number of array elements to allocate based
		// on the byte length.
		const uint32 elementCount = byteCountToElementCount(byteCount);

		// Delegate to ReAllocate() function.
		reallocateElements(elementCount);

		return (*this);
	}

	//////////////////////////////////////////////////////////////////////////
	// Free
	//
	// Frees the memory for the array.
	//////////////////////////////////////////////////////////////////////////
	void freeArray() {
		// Free the buffer if it exists.
		if (_data) {
			verifySentinel();

			// Zeroize the buffer for cryptographic purposes
			::memset(_data, 0, _byteCount);

			Allocator::freeMemory(_data);
			_data = NULL;

			// Reset to initial value.
			_sentinelBits = gs_ulDynamicArraySentinelBitPattern;
		}

		// Reset the length and byte length.
		_elementCount = 0;
		_elementIndex = 0;
		_byteCount = 0;

		// Reset the sentinel set flag.
		_isSentinelSet = false;
	}

	//////////////////////////////////////////////////////////////////////////
	// ArrayCpy
	//
	// Copies the array entries from crArray into this array.
	//////////////////////////////////////////////////////////////////////////
	void arrayCpy(const TDynamicArray & crArray) {
		// Make sure reference is not to this.
		if (this != &crArray) {
			// Pre-validation.
			verifyNotNull();
			verifyByteCount(crArray.getByteCount());
			verifySentinel();
			crArray.verifySentinel();

			// Reinitialize this array.
			memSet();

			// Copy the array.
			memcpy(_data, crArray.getPtr(), crArray.getByteCount());

			// Post-validation.
			verifySentinel();
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// ArrayCmp
	//
	// Compare this array with crArray.  Compares the entire array, so arrays
	// must be equal size to evaluate to equal.
	//////////////////////////////////////////////////////////////////////////
	int32 arrayCmp(const TDynamicArray & crArray) const {
		int32 iRetVal = 0;

		// Make sure reference is not to this.
		if (this != &crArray) {
			// Pre-validation.
			verifySentinel();
			crArray.verifySentinel();

			// Make sure the arrays are the same length.
			if ((_data == NULL) && (crArray._data == NULL)) {
				iRetVal = 0;
			} else if (getByteCount() == crArray.getByteCount()) {
				// Compare the array.
				iRetVal = ::memcmp(_data, crArray.getPtr(), getByteCount());
			} else {
				iRetVal = (getByteCount() > crArray.getByteCount()) ? 1 : -1;
			}

			// Post-validation.
			verifySentinel();
		}

		return iRetVal;
	}

	//////////////////////////////////////////////////////////////////////////
	// ArrayPrepend
	//
	// Prepend this array with the provided array.  This call will grow the
	// array by the size of the provided array.
	//////////////////////////////////////////////////////////////////////////
	void arrayPrepend(const TDynamicArray & crArray) {
		CAF_CM_FUNCNAME("arrayPrepend");

		// Declare variables to hold copy of original information.
		uint32 origByteCount = 0;
		uint32 origElementCount = 0;
		T * origData = NULL;

		try {
			// Pre-validation.
			verifySentinel();

			// Create a temporary copy of the original buffer and length.
			origData = _data;
			origByteCount = _byteCount;
			origElementCount = _elementCount;

			// Reset the data pointer, length, and byte length.
			_data = NULL;
			_elementCount = 0;
			_elementIndex = 0;
			_byteCount = 0;

			// Reset the sentinel set flag.
			_isSentinelSet = false;

			// Call internal allocation function to actually allocate the
			// array buffer.
			internalAllocate(origElementCount + crArray.getElementCount());

			// Get an intermediate pointer to the internal buffer.
			T * tempData = _data;

			// Copy the provided data into the new buffer if it exists.
			if((crArray.getPtr() != NULL) && (crArray.getByteCount() > 0)) {
				// Copy the bytes.
				::memcpy(tempData, crArray.getPtr(), crArray.getByteCount());

				// Advance the pointer to the end of the data just copied.
				tempData += crArray.getElementCount();
			}

			// Copy the original into the new if it exists.
			if((origData != NULL) && (origByteCount > 0)) {
				// Copy the bytes.
				::memcpy(tempData, origData, origByteCount);
			}

			// verify the sentinel bytes.
			verifySentinel();
		}
		CAF_CM_CATCH_ALL;

		// Delete the original buffer if non-null.
		if (origData) {
			Allocator::freeMemory(origData);
		}

		CAF_CM_THROWEXCEPTION;
	}

	//////////////////////////////////////////////////////////////////////////
	// ArrayAppend
	//
	// Append this array with the provided array.  This call will grow the
	// array by the size of the provided array.
	//////////////////////////////////////////////////////////////////////////
	void arrayAppend(const TDynamicArray & crArray) {
		CAF_CM_FUNCNAME("arrayAppend");

		// Declare variables to hold copy of original information.
		uint32 origByteCount = 0;
		uint32 origElementCount = 0;
		uint32 origElementIndex = 0;
		T * origData = NULL;

		try {
			// Pre-validation.
			verifySentinel();

			// Create a temporary copy of the original buffer and length.
			origData = _data;
			origByteCount = _byteCount;
			origElementCount = _elementCount;
			origElementIndex = _elementIndex;

			// Reset the data pointer, length, and byte length.
			_data = NULL;
			_elementCount = 0;
			_elementIndex = 0;
			_byteCount = 0;

			// Reset the sentinel set flag.
			_isSentinelSet = false;

			// Call internal allocation function to actually allocate the
			// array buffer.
			internalAllocate(origElementCount + crArray.getElementCount());

			// Get an intermediate pointer to the internal buffer.
			T * tempData = _data;

			// Copy the original into the new if it exists.
			if((origData != NULL) && (origByteCount > 0))
			{
				// Copy the bytes.
				::memcpy(tempData, origData, origByteCount);

				// Advance the pointer to the end of the data just copied.
				tempData += origElementCount;
			}

			// Copy the provided data into the new buffer if it exists.
			if((crArray.getPtr() != NULL) && (crArray.getByteCount() > 0)) {
				// Copy the bytes.
				::memcpy(tempData, crArray.getPtr(), crArray.getByteCount());
			}

			_elementIndex = origElementIndex;

			// verify the sentinel bytes.
			verifySentinel();
		}
		CAF_CM_CATCH_ALL;

		// Delete the original buffer if non-null.
		if (origData) {
			Allocator::freeMemory(origData);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// MemSet
	//
	// Initializes the array by filling it with the fillValue.
	//////////////////////////////////////////////////////////////////////////
	void memSet(const byte fillValue = 0) {
		// Pre-validation.
		verifyNotNull();
		verifySentinel();

		// Initialize the buffer.
		::memset(_data, fillValue, _byteCount);
		_elementIndex = 0;

		// Post-validation.
		verifySentinel();
	}

	//////////////////////////////////////////////////////////////////////////
	// MemCpy
	//
	// Copy the memory from sourceData into this array up to byteCount bytes.
	//////////////////////////////////////////////////////////////////////////
	void memCpy(
			const void * sourceData,
			const size_t byteCount) {
		memCpy(sourceData, static_cast<const uint32>(byteCount));
	}

	//////////////////////////////////////////////////////////////////////////
	// MemCpy
	//
	// Copy the memory from sourceData into this array up to byteCount bytes.
	//////////////////////////////////////////////////////////////////////////
	void memCpy(
			const void * sourceData,
			const uint32 byteCount) {
		// Pre-validation.
		verifyNotNull();
		verifySentinel();

		// Make sure the data will fit if we copy it.
		verifyByteCount(byteCount);

		// Copy the data.
		::memcpy(_data, sourceData, byteCount);
		_elementIndex = 0;

		// Post-validation.
		verifySentinel();
	}

	//////////////////////////////////////////////////////////////////////////
	// MemAppend
	//
	// Append the memory from sourceData into this array up to byteCount bytes.
	//////////////////////////////////////////////////////////////////////////
	void memAppend(
			const void * sourceData,
			const size_t byteCount) {
		memAppend(sourceData, static_cast<const uint32>(byteCount));
	}

	//////////////////////////////////////////////////////////////////////////
	// MemAppend
	//
	// Append the memory from sourceData into this array up to byteCount bytes.
	//////////////////////////////////////////////////////////////////////////
	void memAppend(
			const void * sourceData,
			const uint32 byteCount) {
		// Pre-validation.
		verifyNotNull();
		verifySentinel();

		// Make sure the data will fit if we copy it.
		const uint32 startingByteCount = elementCountToByteCount(_elementIndex);
		verifyByteCount(startingByteCount + byteCount);

		// Copy the data.
		::memcpy(_data + _elementIndex, sourceData, byteCount);
		_elementIndex += byteCountToElementCount(byteCount);

		// Post-validation.
		verifySentinel();
	}

	//////////////////////////////////////////////////////////////////////////
	// MemCmp
	//
	// Compare the memory from sourceData with this array up to byteCount bytes.
	//////////////////////////////////////////////////////////////////////////
	int32 memCmp(
			const void * sourceData,
			const size_t byteCount) {
		return memCmp(sourceData, static_cast<const uint32>(byteCount));
	}

	//////////////////////////////////////////////////////////////////////////
	// MemCmp
	//
	// Compare the memory from sourceData with this array up to byteCount bytes.
	//////////////////////////////////////////////////////////////////////////
	int32 memCmp(
			const void * sourceData,
			const uint32 byteCount) const {
		CAF_CM_FUNCNAME_VALIDATE("memCmp");
		CAF_CM_VALIDATE_PTR(sourceData);
		CAF_CM_VALIDATE_POSITIVE(byteCount);

		// Pre-validation.
		verifySentinel();

		// Make sure the comparison can take place within our array bounds.
		verifyByteCount(byteCount);

		// Do comparison.
		int32 iRetVal = 0;
		if ((_data == NULL) && (sourceData == NULL)) {
			iRetVal = 0;
		} else {
			iRetVal = ::memcmp(_data, sourceData, byteCount);
		}

		// Post-validation.
		verifySentinel();

		return iRetVal;
	}

	//////////////////////////////////////////////////////////////////////////
	// MemiCmp
	//
	// Compare the memory from sourceData with this array up to byteCount bytes
	// (case insensitive).
	//////////////////////////////////////////////////////////////////////////
	int32 memiCmp(
			const void * sourceData,
			const size_t byteCount) {
		return memiCmp(sourceData, static_cast<const uint32>(byteCount));
	}

	//////////////////////////////////////////////////////////////////////////
	// MemiCmp
	//
	// Compare the memory from sourceData with this array up to byteCount bytes
	// (case insensitive).
	//////////////////////////////////////////////////////////////////////////
	int32 memiCmp(
			const void * sourceData,
			const uint32 byteCount) const {
		CAF_CM_FUNCNAME_VALIDATE("memiCmp");
		CAF_CM_VALIDATE_PTR(sourceData);
		CAF_CM_VALIDATE_POSITIVE(byteCount);

		// Pre-validation.
		verifySentinel();

		// Make sure the comparison can take place within our array bounds.
		verifyByteCount(byteCount);

		// Do comparison.
		int32 iRetVal = 0;
		if ((_data == NULL) && (sourceData == NULL)) {
			iRetVal = 0;
		} else {
			iRetVal = memicmp(_data, sourceData, byteCount);
		}

		// Post-validation.
		verifySentinel();

		return iRetVal;
	}

	//////////////////////////////////////////////////////////////////////////
	// incrementElementIndex
	//
	// Increments the current position of the index into the buffer.
	//////////////////////////////////////////////////////////////////////////
	void incrementCurrentPos(const uint32 elementCount) {
		CAF_CM_FUNCNAME("incrementCurrentPos");

		if ((_elementIndex + elementCount) > _elementCount) {
			CAF_CM_EXCEPTION_VA3(ERROR_INSUFFICIENT_BUFFER,
				"Current position is longer than the total size for '%s' - currentPos: %d, totalLen: %d",
				_description.c_str(), _elementIndex + elementCount, _elementCount);
		}

		_elementIndex += elementCount;
	}

	//////////////////////////////////////////////////////////////////////////
	// resetElementIndex
	//
	// Resets the current position of the index into the buffer.
	//////////////////////////////////////////////////////////////////////////
	void resetCurrentPos() {
		_elementIndex = 0;
	}

	//////////////////////////////////////////////////////////////////////////
	// getByteCountFromCurrentPos
	//
	// Returns the size of the array in bytes from the current position.
	//////////////////////////////////////////////////////////////////////////
	uint32 getByteCountFromCurrentPos() const {
		const uint32 byteIndex = elementCountToByteCount(_elementIndex);
		return (_byteCount - byteIndex);
	}

private:
	//////////////////////////////////////////////////////////////////////////
	// internalAllocate (Private)
	//
	// Set the sentinel bytes at the end of the array.
	//////////////////////////////////////////////////////////////////////////
	void internalAllocate(const uint32 elementCount) {
		CAF_CM_FUNCNAME("internalAllocate");

		// This function assumes the calling function has freed any pre-
		// existing array.
		CAF_CM_VALIDATE_NULLPTR(_data);
		CAF_CM_VALIDATE_ZERO(_elementCount);
		CAF_CM_VALIDATE_ZERO(_byteCount);

		// Allocate the new buffer.  This buffer is
		_data = static_cast<T*>(Allocator::allocMemory(
			sizeof(T) * (elementCount + gs_ulDynamicArraySentinelElementCount)));

		// verify that the allocation succeeded.
		if (!_data) {
			CAF_CM_EXCEPTION_VA1(ERROR_OUTOFMEMORY, "Array allocation failed for '%s'",
				_description.c_str());
		}

		// Set sentinel bits
#ifdef __x86_64__
		_sentinelBits = reinterpret_cast<const uint64>(_data)
			^ gs_ulDynamicArraySentinelBitPattern;
#else
		_sentinelBits = reinterpret_cast<const uint32>(_data) ^ gs_ulDynamicArraySentinelBitPattern;
#endif

		// Initialize the new buffer.
		::memset(_data, 0,
			((elementCount + gs_ulDynamicArraySentinelElementCount) * sizeof(T)));

		// Set the length and byte length.
		_elementCount = elementCount;
		_elementIndex = 0;
		_byteCount = elementCountToByteCount(elementCount);

		// Set the sentinel bytes.
		setSentinel();

		// verify the sentinel bytes.
		verifySentinel();
	}

	//////////////////////////////////////////////////////////////////////////
	// SetSentinel (Private)
	//
	// Set the sentinel bytes at the end of the array.
	//////////////////////////////////////////////////////////////////////////
	void setSentinel() {
		// Set the sentinel characters.
		for (uint32 ulIndex = 0; ulIndex < (2 * sizeof(T)); ++ulIndex) {
			// Set the sentinel bytes at the end of the array.
			reinterpret_cast<byte*>(_data)[(_byteCount + ulIndex + sizeof(T))] =
				((ulIndex % 2) == 0) ? 0xFF : 0xDD;

			// Make a copy of the sentinel bytes for later comparison.
			_sentinelBytes[ulIndex + sizeof(T)] = ((ulIndex % 2) == 0) ? 0xFF : 0xDD;
		}

		// Set the flag indicating the sentinel is set.
		_isSentinelSet = true;
	}

	//////////////////////////////////////////////////////////////////////////
	// verifyByteCount (Private)
	//
	// Verifies that the byte length supplied is not longer than the array
	// byte length.
	//////////////////////////////////////////////////////////////////////////
	void verifyByteCount(const uint32 byteCount) const {
		CAF_CM_FUNCNAME("verifyByteCount");

		if (byteCount > _byteCount) {
			CAF_CM_EXCEPTION_VA3(ERROR_INVALID_INDEX, "The byte length specified [%d] "
				"exceeds the array length [%d] for '%s'", byteCount, _byteCount,
				_description.c_str());
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// verifyElementCount (Private)
	//
	// Verifies that the index supplied does not go past end of the array.
	//////////////////////////////////////////////////////////////////////////
	void verifyElementCount(const uint32 elementCount) const {
		CAF_CM_FUNCNAME("verifyElementCount");

		if (elementCount >= _elementCount) {
			CAF_CM_EXCEPTION_VA3(ERROR_INVALID_INDEX, "The index specified [%d] is "
				"beyond the array bounds [%d] for '%s'", elementCount, (_elementCount - 1),
				_description.c_str());
		}
	}

	uint32 byteCountToElementCount(const uint32 byteCount) const {
		uint32 rc = 0;
		if (byteCount > 0) {
			rc = (byteCount / sizeof(T)) + (byteCount % sizeof(T));
		}

		return rc;
	}

	uint32 elementCountToByteCount(const uint32 elementCount) const {
		return (elementCount * sizeof(T));
	}

protected:
	//////////////////////////////////////////////////////////////////////////
	// verifySentinel (Private)
	//
	// Verifies that the sentinel bytes are still intact.
	//////////////////////////////////////////////////////////////////////////
	void verifySentinel() const {
		CAF_CM_FUNCNAME("verifySentinel");

		if (_isSentinelSet) {
#ifdef __x86_64__
			if ((_sentinelBits ^ reinterpret_cast<const uint64>(_data))
				!= gs_ulDynamicArraySentinelBitPattern)
#else
				if ((_sentinelBits ^ reinterpret_cast<const uint32>(_data)) !=
					gs_ulDynamicArraySentinelBitPattern)
#endif
				{
				CAF_CM_EXCEPTION_VA1(ERROR_INVALID_DATA,
					"The sentinel BITS for array '%s' are no longer valid.",
					_description.c_str());
			} else if (::memcmp(_sentinelBytes,
				(reinterpret_cast<const byte *>(_data) + _byteCount),
				sizeof(_sentinelBytes)) != 0) {
				CAF_CM_EXCEPTION_VA1(ERROR_INVALID_DATA,
					"The sential BYTES for array '%s' are no longer valid.",
					_description.c_str());
			}
		} else if (_sentinelBits != gs_ulDynamicArraySentinelBitPattern) {
			CAF_CM_EXCEPTION_VA1(ERROR_INVALID_DATA,
				"The sential BITS for array '%s' are no longer valid.", _description.c_str());
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// verifyNotNull (Private)
	//
	// Verifies that the data pointer is not null.
	//////////////////////////////////////////////////////////////////////////
	void verifyNotNull() const {
		CAF_CM_FUNCNAME("verifyNotNull");

		if (NULL == _data) {
			CAF_CM_EXCEPTION_VA1(E_POINTER, "The pointer to the array is null for '%s'",
				_description.c_str());
		}
	}

private:
	TDynamicArray(const TDynamicArray & crRhs);
	TDynamicArray & operator=(const TDynamicArray & crRhs);

#ifdef WIN32
	//////////////////////////////////////////////////////////////////////////
	// Operator[] (Private)
	//
	// We need to provide an implementation to satisfy the VC8 compiler
	// even though we never intend this method to be called.
	//////////////////////////////////////////////////////////////////////////
	T operator[](size_t)
	{
		// Yes, this code looks very wrong, however, DO NOT REMOVE it!
		// It is here to supply an implementation that doesn't generate
		// compiler warnings. This method will never be called.
		T* tpRetVal = NULL;
		return *tpRetVal;
	}
#endif

private:
	CAF_CM_CREATE;
	byte _sentinelBytes[gs_ulDynamicArraySentinelElementCount * sizeof(T)];
#ifdef __x86_64__
	uint64 _sentinelBits;
#else
	uint32 _sentinelBits;
#endif
	bool _isSentinelSet;

	// NOTE: ...Count are not zero-relative, ...Index are zero-relative.

	// An element is the template type (e.g. wchar_t), so the element count
	// is the number of these template types in the array.
	uint32 _elementCount;
	uint32 _elementIndex;

	// The number of bytes consumed by the array of template types.
	uint32 _byteCount;

protected:
	std::string _description;
	T * _data;
};

}

#endif // _TDynamicArray_H_
