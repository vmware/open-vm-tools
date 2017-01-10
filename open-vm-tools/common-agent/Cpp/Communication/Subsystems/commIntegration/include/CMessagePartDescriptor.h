/*
 *  Created on: Nov 19, 2014
 *      Author: bwilliams
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CMessagePartDescriptor_h
#define CMessagePartDescriptor_h


#include "Memory/DynamicArray/DynamicArrayInc.h"

namespace Caf {

class CMessagePartDescriptor;
CAF_DECLARE_SMART_POINTER(CMessagePartDescriptor);

/**
 * Class that emits and parses message parts header blocks.<br>
 */
class CMessagePartDescriptor {
public:
	/**
	 * Converts the BLOCK_SIZE data in a ByteBuffer into a MessagePartsHeader
	 * <p>
	 * The incoming ByteBuffer position will be modified.
	 * @param buffer ByteBuffer to convert
	 * @return a MessagePartsHeader
	*/
	static SmartPtrCMessagePartDescriptor fromByteBuffer(SmartPtrCDynamicByteArray& buffer);

	/**
	 * Converts a byte array into a MessagePartsHeader
	 * @param blockData byte array to convert
	 * @return a MessagePartsHeader
	 */
	static SmartPtrCMessagePartDescriptor fromArray(SmartPtrCDynamicByteArray& blockData);

	static SmartPtrCDynamicByteArray toArray(const uint16 attachmentNumber,
			const uint32 partNumber, const uint32 dataSize, const uint32 dataOffset);

public:
	CMessagePartDescriptor();
	virtual ~CMessagePartDescriptor();

public:
	/**
	 * @param correlationId the correlation id
	 * @param numberOfParts the total number of parts
	 */
	void initialize(
		const uint16 attachmentNumber,
		const uint32 partNumber,
		const uint32 dataSize,
		const uint32 dataOffset);

	/**
	 * @return the attachmentNumber
	 */
	uint16 getAttachmentNumber() const;
	std::string getAttachmentNumberStr() const;

	/**
	 * @return the partNumber
	 */
	uint32 getPartNumber() const;

	/**
	 * @return the dataSize
	 */
	uint32 getDataSize() const;

	/**
	 * @return the dataOffset
	 */
	uint32 getDataOffset() const;

public:
	/**
    * The <code>BLOCK_SIZE</code> field stores the size of a <code>MessagePartsHeader</code> in byte array form.<br>
    */
   static const uint32 BLOCK_SIZE = 20;
   static const byte CAF_MSG_VERSION = 1;

private:
   static const byte RESERVED = (byte)0xcd;

private:
	bool _isInitialized;
	uint16 _attachmentNumber;
	uint32 _partNumber;
	uint32 _dataSize;
	uint32 _dataOffset;

private:
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(CMessagePartDescriptor);
};

}

#endif /* CMessagePartDescriptor_h */
