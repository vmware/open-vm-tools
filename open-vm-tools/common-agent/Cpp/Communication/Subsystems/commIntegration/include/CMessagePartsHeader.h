/*
 *  Created on: Nov 19, 2014
 *      Author: bwilliams
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CMessagePartsHeader_h
#define CMessagePartsHeader_h


#include "Memory/DynamicArray/DynamicArrayInc.h"

namespace Caf {

class CMessagePartsHeader;
CAF_DECLARE_SMART_POINTER(CMessagePartsHeader);

/**
 * Class that emits and parses message parts header blocks.<br>
 */
class CMessagePartsHeader {
public:
   /**
    * Converts the BLOCK_SIZE data in a ByteBuffer into a MessagePartsHeader
    * <p>
    * The incoming ByteBuffer position will be modified.
    * @param buffer ByteBuffer to convert
    * @return a MessagePartsHeader
    */
   static SmartPtrCMessagePartsHeader fromByteBuffer(SmartPtrCDynamicByteArray& buffer);

   /**
    * Converts a byte array into a MessagePartsHeader
    * @param blockData byte array to convert
    * @return a MessagePartsHeader
    */
   static SmartPtrCMessagePartsHeader fromArray(SmartPtrCDynamicByteArray& blockData);

   static SmartPtrCDynamicByteArray toArray(const UUID correlationId,
   	const uint32 numberOfParts);

public:
	CMessagePartsHeader();
	virtual ~CMessagePartsHeader();

public:
   /**
    * @param correlationId the correlation id
    * @param numberOfParts the total number of parts
    */
   void initialize(
         const UUID correlationId,
         const uint32 numberOfParts);

   /**
    * @return the correlationId
    */
   UUID getCorrelationId() const;
   std::string getCorrelationIdStr() const;

   /**
    * @return the numberOfParts
    */
   uint32 getNumberOfParts() const;

public:
	/**
    * The <code>BLOCK_SIZE</code> field stores the size of a <code>MessagePartsHeader</code> in byte array form.<br>
    */
   static const uint32 BLOCK_SIZE = 24;
   static const byte CAF_MSG_VERSION = 1;

private:
   static const byte RESERVED1 = (byte)0xcd;
   static const byte RESERVED2 = (byte)0xcd;
   static const byte RESERVED3 = (byte)0xcd;

private:
	bool _isInitialized;
   UUID _correlationId;
   uint32 _numberOfParts;

private:
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(CMessagePartsHeader);
};

}

#endif /* CMessagePartsHeader_h */
