/*
 *  Created on: Nov 19, 2014
 *     Author: bwilliams
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CMessagePartDescriptorCalculator_h
#define CMessagePartDescriptorCalculator_h


#include "CMessagePartDescriptorSourceRecord.h"
#include "CMessagePartRecord.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"

namespace Caf {

class CMessagePartDescriptorCalculator {
public:
	static uint32 getMaxPartSize();

	static std::deque<SmartPtrCMessagePartDescriptorSourceRecord> calculateSourcePartRecords(
			const SmartPtrCDynamicByteArray& payload);

private:
	static std::deque<SmartPtrCMessagePartDescriptorSourceRecord> refactorMessageIntoPartRecords(
			const std::string& workingDirectory,
			const SmartPtrCDynamicByteArray& payload);

	static std::deque<SmartPtrCMessagePartDescriptorSourceRecord> splitMessagePartRecords(
			std::deque<SmartPtrCMessagePartRecord>& messageParts);

private:
	CAF_CM_DECLARE_NOCREATE(CMessagePartDescriptorCalculator);
};

}

#endif /* CMessagePartDescriptorCalculator_h */
