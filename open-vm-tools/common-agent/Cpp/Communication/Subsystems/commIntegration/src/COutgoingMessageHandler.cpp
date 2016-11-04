/*
 *  Created on: Nov 25, 2014
 *      Author: bwilliams
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "CMessageDeliveryRecord.h"
#include "CMessagePartDescriptorSourceRecord.h"
#include "Doc/PayloadEnvelopeDoc/CPayloadEnvelopeDoc.h"
#include "Integration/Core/CIntMessage.h"
#include "Integration/IIntMessage.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "Integration/Core/CIntMessageHeaders.h"
#include "Exception/CCafException.h"
#include "COutgoingMessageHandler.h"
#include "CMessagePartsHeader.h"
#include "CMessagePartDescriptor.h"
#include "amqpCore/DefaultAmqpHeaderMapper.h"
#include "Integration/Caf/CCafMessagePayloadParser.h"
#include "Integration/Core/CMessageHeaderUtils.h"
#include "Integration/Core/MessageHeaders.h"
#include "Integration/Core/CMessageHeaderUtils.h"

#include <fstream>

using namespace Caf;

COutgoingMessageHandler::COutgoingMessageHandler() :
	_isInitialized(false),
	CAF_CM_INIT_LOG("COutgoingMessageHandler") {
}

COutgoingMessageHandler::~COutgoingMessageHandler() {
}

void COutgoingMessageHandler::initializeBean(
	const IBean::Cargs& ctorArgs,
	const IBean::Cprops& properties) {
	CAF_CM_FUNCNAME_VALIDATE("initializeBean");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STL_EMPTY(ctorArgs);
	CAF_CM_VALIDATE_STL_EMPTY(properties);

	_isInitialized = true;
}

void COutgoingMessageHandler::terminateBean() {
}

/**
 * Handles the incoming management request message
 * <p>
 * Incoming messages are checked for local attachments that need to be transmitted.
 * If the resulting message would be too large to transmit then multiple message
 * records are created and stored for the {@link OutgoingMessageProducer} to handle.  If the
 * message is small enough to fit in a single transmission then it will be returned
 * from this handler if a message receipt is not requested.
 *
 * @param message the incoming management request message
 * @return null if the message must be transmitted as multiple parts or the message itself if it can be transmitted as is
 */
SmartPtrIIntMessage COutgoingMessageHandler::processMessage(
	const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME("processMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(message);

	SmartPtrIIntMessage rc;

	const SmartPtrCPayloadEnvelopeDoc payloadEnvelope =
			CCafMessagePayloadParser::getPayloadEnvelope(message->getPayload());

	// Touch up the outgoing message headers first so that they are
	// preserved through the rest of the system.
	IIntMessage::SmartPtrCHeaders headers = message->getHeaders();

	std::deque<SmartPtrCMessagePartDescriptorSourceRecord> messagePartSourceRecords =
		CMessagePartDescriptorCalculator::calculateSourcePartRecords(message->getPayload());
	if (messagePartSourceRecords.empty()) {
		rc = augmentHeaders(false, message);
	} else {
		// Message splitting required. Iterate the message parts and group them
		// such that each group of parts will fill maxPartSize bytes when transmitted.
		std::deque<SmartPtrCMessageDeliveryRecord> deliveryRecords;
		const uint32 maxPartSize = CMessagePartDescriptorCalculator::getMaxPartSize();
		const std::string correlationIdStr = CStringUtils::createRandomUuid();

		UUID correlationId;
		BasePlatform::UuidFromString(correlationIdStr.c_str(), correlationId);

		uint32 startPartNumber = 1;
		uint32 currentPartSize = 0;
		std::deque<SmartPtrCMessagePartDescriptorSourceRecord> deliveryParts;
		for (TConstIterator<std::deque<SmartPtrCMessagePartDescriptorSourceRecord> > sourceRecordIter(messagePartSourceRecords);
			sourceRecordIter; sourceRecordIter++) {
			const SmartPtrCMessagePartDescriptorSourceRecord sourceRecord = *sourceRecordIter;
			deliveryParts.push_back(sourceRecord);
			currentPartSize += sourceRecord->getDataLength();
			if (currentPartSize == maxPartSize) {
				if (CAF_CM_IS_LOG_DEBUG_ENABLED) {
					CAF_CM_LOG_DEBUG_VA5(
						"Adding message delivery record [size=%d][totalNumParts=%d][startPartNum=%d][parts=%d][correlationId=%s]",
						currentPartSize, messagePartSourceRecords.size(),
						startPartNumber, deliveryParts.size(), correlationIdStr.c_str());
				}

				SmartPtrCMessageDeliveryRecord deliveryRecord;
				deliveryRecord.CreateInstance();
				deliveryRecord->initialize(correlationId,
						static_cast<uint32>(messagePartSourceRecords.size()),
						startPartNumber, deliveryParts, message->getHeaders());

				deliveryRecords.push_back(deliveryRecord);

				startPartNumber += static_cast<uint32>(deliveryParts.size());
				deliveryParts.clear();
				currentPartSize = 0;
			} else if (currentPartSize > maxPartSize) {
				CAF_CM_EXCEPTION_VA2(ERROR_BUFFER_OVERFLOW,
					"Buffer overflow - currentPartSize: %d, maxPartSize: %d",
					currentPartSize, maxPartSize);
			}
		}

		if (currentPartSize > 0) {
			if (CAF_CM_IS_LOG_DEBUG_ENABLED) {
				CAF_CM_LOG_DEBUG_VA5(
					"Adding message delivery record [size=%d][totalNumParts=%d][startPartNum=%d][parts=%d][correlationId=%s]",
					currentPartSize, messagePartSourceRecords.size(),
					startPartNumber, deliveryParts.size(), correlationIdStr.c_str());
			}

			SmartPtrCMessageDeliveryRecord deliveryRecord;
			deliveryRecord.CreateInstance();
			deliveryRecord->initialize(correlationId,
					static_cast<uint32>(messagePartSourceRecords.size()),
					startPartNumber, deliveryParts, message->getHeaders());

			deliveryRecords.push_back(deliveryRecord);
		}

		if (deliveryRecords.size() != 1) {
			CAF_CM_EXCEPTION_VA1(ERROR_NOT_SUPPORTED,
				"Currently supports only one delivery record (i.e. no chunking) - size: %d",
				deliveryRecords.size());
		}

		const SmartPtrIIntMessage newMessage = rehydrateMultiPartMessage(
			deliveryRecords.back(), IIntMessage::SmartPtrCHeaders());
		rc = augmentHeaders(true, newMessage);
	}

	CMessageHeaderUtils::log(rc->getHeaders());

	return rc;
}

SmartPtrIIntMessage COutgoingMessageHandler::rehydrateMultiPartMessage(
	const SmartPtrCMessageDeliveryRecord& deliveryRecord,
	const IIntMessage::SmartPtrCHeaders& addlHeaders) {
	CAF_CM_STATIC_FUNC_LOG("COutgoingMessageHandler", "rehydrateMultiPartMessage");
	CAF_CM_VALIDATE_SMARTPTR(deliveryRecord);
	// addlHeaders are optional

	uint32 payloadSize = CMessagePartsHeader::BLOCK_SIZE;
	const std::deque<SmartPtrCMessagePartDescriptorSourceRecord> sourceRecords = deliveryRecord->getMessagePartSources();
	for (TConstIterator<std::deque<SmartPtrCMessagePartDescriptorSourceRecord> > sourceRecordIter(sourceRecords);
		sourceRecordIter; sourceRecordIter++) {
		const SmartPtrCMessagePartDescriptorSourceRecord sourceRecord = *sourceRecordIter;
		payloadSize += CMessagePartDescriptor::BLOCK_SIZE + sourceRecord->getDataLength();
	}

	SmartPtrCDynamicByteArray payload;
	payload.CreateInstance();
	payload->allocateBytes(payloadSize);

	const SmartPtrCDynamicByteArray partsHeader = CMessagePartsHeader::toArray(
		deliveryRecord->getCorrelationId(), deliveryRecord->getNumberOfParts());
	payload->memAppend(partsHeader->getPtr(), partsHeader->getByteCount());

	uint32 partNumber = deliveryRecord->getStartingPartNumber();
	if (CAF_CM_IS_LOG_DEBUG_ENABLED) {
		CAF_CM_LOG_DEBUG_VA3("[# sourceRecords=%d][payloadSize=%d][startingPartNumber=%d]",
			sourceRecords.size(), payloadSize, partNumber);
	}

	for (TConstIterator<std::deque<SmartPtrCMessagePartDescriptorSourceRecord> > sourceRecordIter(sourceRecords);
		sourceRecordIter; sourceRecordIter++) {
		const SmartPtrCMessagePartDescriptorSourceRecord sourceRecord = *sourceRecordIter;

		const SmartPtrCDynamicByteArray partDescriptor = CMessagePartDescriptor::toArray(
			sourceRecord->getAttachmentNumber(), partNumber++, sourceRecord->getDataLength(),
			sourceRecord->getDataOffset());
		payload->memAppend(partDescriptor->getPtr(), partDescriptor->getByteCount());

		CAF_CM_LOG_DEBUG_VA3("Reading from file - file: %s, len: %d, offset: %d",
			sourceRecord->getFilePath().c_str(), sourceRecord->getDataLength(),
			sourceRecord->getDataOffset());

		std::ifstream file(sourceRecord->getFilePath().c_str(), std::ios::binary);
		try {
			if (!file.is_open()) {
				CAF_CM_EXCEPTION_VA1(ERROR_FILE_NOT_FOUND,
					"Could not open binary file - %s", sourceRecord->getFilePath().c_str());
			}

			file.seekg(sourceRecord->getDataOffset(), std::ios::beg);
			file.read(reinterpret_cast<char*>(payload->getNonConstPtrAtCurrentPos()),
				sourceRecord->getDataLength());
			payload->verify();
			if (! file) {
				CAF_CM_EXCEPTION_VA3(ERROR_BUFFER_OVERFLOW,
					"Did not read full contents - file: %s, requested: %d, read: %d",
					sourceRecord->getFilePath().c_str(), sourceRecord->getDataLength(),
					file.gcount());
			}

			payload->incrementCurrentPos(sourceRecord->getDataLength());
		}
		CAF_CM_CATCH_ALL;
		file.close();
		CAF_CM_LOG_CRIT_CAFEXCEPTION;
		CAF_CM_THROWEXCEPTION;
	}

	SmartPtrCIntMessage rc;
	rc.CreateInstance();
	rc->initialize(payload, deliveryRecord->getMessageHeaders(), addlHeaders);

	return rc;
}

SmartPtrIIntMessage COutgoingMessageHandler::augmentHeaders(
	const bool isMultiPart,
	const SmartPtrIIntMessage& message) {
	CAF_CM_STATIC_FUNC_VALIDATE("COutgoingMessageHandler", "augmentHeaders");
	CAF_CM_VALIDATE_INTERFACE(message);

	std::string contentType;
	if (isMultiPart) {
		contentType = "application/octet-stream";
	} else {
		contentType = "text/plain";
	}

	CIntMessageHeaders messageHeaders;
	messageHeaders.insertBool(MessageHeaders::_sMULTIPART, isMultiPart);
	messageHeaders.insertString(AmqpIntegration::DefaultAmqpHeaderMapper::CONTENT_TYPE,
		contentType);

	SmartPtrCIntMessage messageImpl;
	messageImpl.CreateInstance();
	messageImpl->initialize(message->getPayload(), messageHeaders.getHeaders(),
		message->getHeaders());

	return messageImpl;
}
