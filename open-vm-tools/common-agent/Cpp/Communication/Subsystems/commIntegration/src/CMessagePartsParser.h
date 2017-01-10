/*
 *  Created on: Nov 19, 2014
 *      Author: bwilliams
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CMessagePartsParser_h
#define CMessagePartsParser_h


#include "Memory/DynamicArray/DynamicArrayInc.h"

namespace Caf {

/**
 * Class that emits and parses message parts header blocks.<br>
 */
class CMessagePartsParser {
public:
	static byte getByte(SmartPtrCDynamicByteArray& buffer);
   static uint16 getUint16(SmartPtrCDynamicByteArray& buffer);
   static uint32 getUint32(SmartPtrCDynamicByteArray& buffer);
   static uint64 getUint64(SmartPtrCDynamicByteArray& buffer);
   static UUID getGuid(SmartPtrCDynamicByteArray& buffer);
   static void get8Bytes(SmartPtrCDynamicByteArray& buffer, byte* data);

private:
	CAF_CM_DECLARE_NOCREATE(CMessagePartsParser);
};

}

#endif /* CMessagePartsParser_h */
