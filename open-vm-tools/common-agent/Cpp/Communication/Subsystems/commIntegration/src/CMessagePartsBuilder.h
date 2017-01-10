/*
 *  Created on: Nov 19, 2014
 *      Author: bwilliams
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CMessagePartsBuilder_h
#define CMessagePartsBuilder_h


#include "Memory/DynamicArray/DynamicArrayInc.h"

namespace Caf {

/**
 * Class that emits and parses message parts header blocks.<br>
 */
class CMessagePartsBuilder {
public:
	static void put(const byte value, SmartPtrCDynamicByteArray& buffer);
	static void put(const uint16 value, SmartPtrCDynamicByteArray& buffer);
	static void put(const uint32 value, SmartPtrCDynamicByteArray& buffer);
	static void put(const uint64 value, SmartPtrCDynamicByteArray& buffer);
	static void put(const GUID value, SmartPtrCDynamicByteArray& buffer);

private:
	static void putBytes(const byte* buf, const uint32 bufLen,
		SmartPtrCDynamicByteArray& buffer);

private:
	CAF_CM_DECLARE_NOCREATE(CMessagePartsBuilder);
};

}

#endif /* CMessagePartsBuilder_h */
