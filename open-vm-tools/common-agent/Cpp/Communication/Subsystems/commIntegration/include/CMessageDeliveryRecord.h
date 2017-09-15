/*
 *  Created on: Nov 26, 2014
 *      Author: bwilliams
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CMessageDeliveryRecord_h
#define CMessageDeliveryRecord_h


#include "CMessagePartDescriptorSourceRecord.h"
#include "Integration/IIntMessage.h"

namespace Caf {

class CMessageDeliveryRecord {
public:
	CMessageDeliveryRecord();
	virtual ~CMessageDeliveryRecord();

public:
	void initialize(
	   const UUID& correlationId,
	   const uint32 numberOfParts,
	   const uint32 startingPartNumber,
	   const std::deque<SmartPtrCMessagePartDescriptorSourceRecord>& messagePartSources,
	   const IIntMessage::SmartPtrCHeaders& messageHeaders);

public:
	UUID getCorrelationId() const;

	std::string getCorrelationIdStr() const;

	uint32 getNumberOfParts() const;

	uint32 getStartingPartNumber() const;

	std::deque<SmartPtrCMessagePartDescriptorSourceRecord> getMessagePartSources() const;

	IIntMessage::SmartPtrCHeaders getMessageHeaders() const;

private:
	bool _isInitialized;
   UUID _correlationId;
   uint32 _numberOfParts;
   uint32 _startingPartNumber;
   std::deque<SmartPtrCMessagePartDescriptorSourceRecord> _messagePartSources;
   IIntMessage::SmartPtrCHeaders _messageHeaders;

private:
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(CMessageDeliveryRecord);
};

CAF_DECLARE_SMART_POINTER(CMessageDeliveryRecord);

}

#endif /* CMessageDeliveryRecord_h */
