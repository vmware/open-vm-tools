/*
 *  Created on: Nov 19, 2014
 *     Author: bwilliams
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "CMessagePartDescriptorSourceRecord.h"
#include "CMessagePartRecord.h"
#include "Doc/CafCoreTypesDoc/CAttachmentDoc.h"
#include "Doc/PayloadEnvelopeDoc/CPayloadEnvelopeDoc.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "Exception/CCafException.h"
#include "CMessagePartDescriptorCalculator.h"
#include "Integration/Caf/CCafMessagePayloadParser.h"
#include "Integration/Caf/CCafMessagePayload.h"
#include "Integration/Caf/CCafMessageCreator.h"

using namespace Caf;

uint32 CMessagePartDescriptorCalculator::getMaxPartSize() {
	return AppConfigUtils::getRequiredUint32("communication_amqp", "max_part_size");
}

std::deque<SmartPtrCMessagePartDescriptorSourceRecord> CMessagePartDescriptorCalculator::calculateSourcePartRecords(
		const SmartPtrCDynamicByteArray& payload) {
	CAF_CM_STATIC_FUNC_VALIDATE("CMessagePartDescriptorCalculator", "calculateSourcePartRecords");
	CAF_CM_VALIDATE_SMARTPTR(payload);

	const std::string workingDirectory = AppConfigUtils::getRequiredString("communication_amqp",
			_sConfigWorkingDir);

	return refactorMessageIntoPartRecords(workingDirectory, payload);
}

/*
 * Parse the local file attachments and message body into records
 * reflecting the full size of each part.
 */
std::deque<SmartPtrCMessagePartDescriptorSourceRecord> CMessagePartDescriptorCalculator::refactorMessageIntoPartRecords(
	const std::string& workingDirectory,
	const SmartPtrCDynamicByteArray& payload) {
	CAF_CM_STATIC_FUNC_LOG("CMessagePartDescriptorCalculator", "refactorMessageIntoPartRecords");
	CAF_CM_VALIDATE_STRING(workingDirectory);
	CAF_CM_VALIDATE_SMARTPTR(payload);
	// attachmentCollectionDoc is optional

	const SmartPtrCPayloadEnvelopeDoc payloadEnvelope =
			CCafMessagePayloadParser::getPayloadEnvelope(payload);

	const std::deque<SmartPtrCAttachmentDoc> sourceAttachments =
			payloadEnvelope->getAttachmentCollection()->getAttachment();

	// Scan the attachment collection and create part records for
	// any local file attachments that need to be transmitted.
	std::deque<SmartPtrCMessagePartRecord> messageParts;
	std::deque<SmartPtrCAttachmentDoc> refactoredAttachments;
	if (!sourceAttachments.empty()) {
		int16 attachmentNumber = 1;
		for (TConstIterator<std::deque<SmartPtrCAttachmentDoc> > sourceAttachmentIter(sourceAttachments);
			sourceAttachmentIter; sourceAttachmentIter++) {
			const SmartPtrCAttachmentDoc attachment = *sourceAttachmentIter;

			UriUtils::SUriRecord uriRecord;
			UriUtils::parseUriString(attachment->getUri(), uriRecord);

			if ((uriRecord.protocol.compare("file") == 0) && !attachment->getIsReference()) {
				CAF_CM_LOG_DEBUG_VA1("Processing local file attachment - uri: %s",
					attachment->getUri().c_str());

				UriUtils::SFileUriRecord fileUriRecord;
				UriUtils::parseFileAddress(uriRecord.address, fileUriRecord);
				const std::string attachmentPath = fileUriRecord.path;

				if (!FileSystemUtils::doesFileExist(attachmentPath)) {
					CAF_CM_EXCEPTION_VA1(ERROR_FILE_NOT_FOUND,
						"File not found - %s", attachmentPath.c_str());
				}

				const int64 fileSize = FileSystemUtils::getFileSize(attachmentPath);
				CAF_CM_LOG_DEBUG_VA2("Processing local file attachment - file: %s, size: %d",
					attachmentPath.c_str(), fileSize);

				SmartPtrCMessagePartRecord messagePart;
				messagePart.CreateInstance();
				messagePart->initialize(attachmentNumber, attachmentPath, 0, fileSize);
				messageParts.push_back(messagePart);

				const std::string newAttachmentUri = UriUtils::appendParameters(
					"attachment:/" + CStringConv::toString<int16>(attachmentNumber),
					uriRecord.parameters);

				CAF_CM_LOG_DEBUG_VA1("New attachment URI - %s", newAttachmentUri.c_str());

				SmartPtrCAttachmentDoc attachmentNew;
				attachmentNew.CreateInstance();
				attachmentNew->initialize(attachment->getName(), attachment->getType(),
					newAttachmentUri, attachment->getIsReference(),
					attachment->getCmsPolicy());

				refactoredAttachments.push_back(attachmentNew);
				++attachmentNumber;
			} else {
				refactoredAttachments.push_back(attachment);
			}
		}
	}

	// If there are local file attachments to send then rebuild the message
	// with the refactored attribute collection.
	SmartPtrCDynamicByteArray payloadNew = payload;
	if (!messageParts.empty()) {
		payloadNew = CCafMessageCreator::createPayloadEnvelope(
				payloadEnvelope, refactoredAttachments)->getPayload();
	}

	// Does the payload itself need to be split?
	// If it is larger than max_part_size OR if there are transmitted attachments then 'yes'
	const std::string newPayload =
			CStringUtils::trim(CCafMessagePayload::saveToStr(payloadNew));
	if ((newPayload.length() > getMaxPartSize()) || ! messageParts.empty()) {
		// Save the new payload to a file
		const std::string requestIdStr =
				BasePlatform::UuidToString(payloadEnvelope->getRequestId());
		const std::string payloadFileName = requestIdStr + "-payload.xml";
		const std::string payloadFile = FileSystemUtils::buildPath(
			workingDirectory, payloadFileName);
		FileSystemUtils::saveTextFile(payloadFile, newPayload);

		SmartPtrCMessagePartRecord messagePart;
		messagePart.CreateInstance();
		messagePart->initialize(0, payloadFile, 0, newPayload.length());

		messageParts.push_back(messagePart);
	}

	// If there are MessagePartRecords then split them apart into
	// MessagePartSourceRecords and return them else there is no
	// splitting required.
	if (!messageParts.empty()) {
		return splitMessagePartRecords(messageParts);
	} else {
		return std::deque<SmartPtrCMessagePartDescriptorSourceRecord>();
	}
}

/*
 * Split the message part records such that all parts will contain
 * at most max_part_size bytes.
 */
std::deque<SmartPtrCMessagePartDescriptorSourceRecord> CMessagePartDescriptorCalculator::splitMessagePartRecords(
	std::deque<SmartPtrCMessagePartRecord>& messageParts) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CMessagePartDescriptorCalculator", "splitMessagePartRecords");
	CAF_CM_VALIDATE_STL(messageParts);

	if (CAF_CM_IS_LOG_DEBUG_ENABLED) {
		CAF_CM_LOG_DEBUG_VA0("Message part records");
		for (TConstIterator<std::deque<SmartPtrCMessagePartRecord> > messagePartIter(messageParts);
			messagePartIter; messagePartIter++) {
			const SmartPtrCMessagePartRecord messagePart = *messagePartIter;
			CAF_CM_LOG_DEBUG_VA4(
				"Message part - attachmentNumber: %d, filePath: %s, dataLength: %d, dataOffset: %d",
				messagePart->getAttachmentNumber(), messagePart->getFilePath().c_str(),
				messagePart->getDataLength(), messagePart->getDataOffset());
		}
	}

	std::deque<SmartPtrCMessagePartDescriptorSourceRecord> packedSourceRecords;

	uint64 currentPartSize = 0;
	std::deque<SmartPtrCMessagePartRecord>::iterator messagePartIter = messageParts.begin();
	while (messagePartIter != messageParts.end()) {
		const SmartPtrCMessagePartRecord messagePart = *messagePartIter;

		const uint64 availablePartBytes =
				messagePart->getDataLength() > LONG_MAX
					? LONG_MAX : messagePart->getDataLength();
		const uint64 fillBytes = MIN(getMaxPartSize() - currentPartSize, availablePartBytes);

		SmartPtrCMessagePartDescriptorSourceRecord sourceRecord;
		sourceRecord.CreateInstance();
		sourceRecord->initialize(messagePart->getAttachmentNumber(),
			messagePart->getFilePath(), static_cast<uint32>(messagePart->getDataOffset()), static_cast<uint32>(fillBytes));

		packedSourceRecords.push_back(sourceRecord);

		currentPartSize += fillBytes;
		messagePart->setDataOffset(messagePart->getDataOffset() + fillBytes);
		messagePart->setDataLength(messagePart->getDataLength() - fillBytes);
		if (messagePart->getDataLength() == 0) {
			messagePartIter = messageParts.erase(messagePartIter);
		} else {
			++messagePartIter;
		}

		if (currentPartSize == getMaxPartSize()) {
			currentPartSize = 0;
		}
	}

	if (CAF_CM_IS_LOG_DEBUG_ENABLED) {
		CAF_CM_LOG_DEBUG_VA0("Split message part descriptor source records");
		for (TConstIterator<std::deque<SmartPtrCMessagePartDescriptorSourceRecord> > packedSourceRecordIter(packedSourceRecords);
			packedSourceRecordIter; packedSourceRecordIter++) {
			const SmartPtrCMessagePartDescriptorSourceRecord packedSourceRecord =
				*packedSourceRecordIter;
			CAF_CM_LOG_DEBUG_VA4(
				"Packed source - attachmentNumber: %d, filePath: %s, dataLength: %d, dataOffset: %d",
				packedSourceRecord->getAttachmentNumber(),
				packedSourceRecord->getFilePath().c_str(),
				packedSourceRecord->getDataLength(), packedSourceRecord->getDataOffset());
		}
	}

	return packedSourceRecords;
}
