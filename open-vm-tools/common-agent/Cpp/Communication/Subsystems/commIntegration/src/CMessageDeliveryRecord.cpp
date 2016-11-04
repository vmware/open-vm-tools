/*
 *  Created on: Nov 26, 2014
 *      Author: bwilliams
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "CMessagePartDescriptorSourceRecord.h"
#include "Integration/IIntMessage.h"
#include "CMessageDeliveryRecord.h"

using namespace Caf;

CMessageDeliveryRecord::CMessageDeliveryRecord() :
	_isInitialized(false),
	_correlationId(CAFCOMMON_GUID_NULL),
	_numberOfParts(0),
	_startingPartNumber(0),
	CAF_CM_INIT("CMessageDeliveryRecord") {
}

CMessageDeliveryRecord::~CMessageDeliveryRecord() {
}

void CMessageDeliveryRecord::initialize(
   const UUID& correlationId,
   const uint32 numberOfParts,
   const uint32 startingPartNumber,
   const std::deque<SmartPtrCMessagePartDescriptorSourceRecord>& messagePartSources,
   const IIntMessage::SmartPtrCHeaders& messageHeaders) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);

	_correlationId = correlationId;
   _numberOfParts = numberOfParts;
   _startingPartNumber = startingPartNumber;
   _messagePartSources = messagePartSources;
   _messageHeaders = messageHeaders;

   _isInitialized = true;
}

UUID CMessageDeliveryRecord::getCorrelationId() const {
	CAF_CM_FUNCNAME_VALIDATE("getCorrelationId");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _correlationId;
}

std::string CMessageDeliveryRecord::getCorrelationIdStr() const {
	CAF_CM_FUNCNAME_VALIDATE("getCorrelationId");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return BasePlatform::UuidToString(_correlationId);
}

uint32 CMessageDeliveryRecord::getNumberOfParts() const {
	CAF_CM_FUNCNAME_VALIDATE("getNumberOfParts");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _numberOfParts;
}

uint32 CMessageDeliveryRecord::getStartingPartNumber() const {
	CAF_CM_FUNCNAME_VALIDATE("getStartingPartNumber");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _startingPartNumber;
}

std::deque<SmartPtrCMessagePartDescriptorSourceRecord> CMessageDeliveryRecord::getMessagePartSources() const {
	CAF_CM_FUNCNAME_VALIDATE("getMessagePartSources");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _messagePartSources;
}

IIntMessage::SmartPtrCHeaders CMessageDeliveryRecord::getMessageHeaders() const {
	CAF_CM_FUNCNAME_VALIDATE("getMessageHeaders");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _messageHeaders;
}
