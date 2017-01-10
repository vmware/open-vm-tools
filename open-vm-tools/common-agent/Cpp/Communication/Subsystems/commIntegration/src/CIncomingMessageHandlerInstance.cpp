/*
 *  Created on: Aug 16, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Integration/Caf/CCafMessageHeadersWriter.h"
#include "CMessagePartDescriptor.h"
#include "Common/IAppContext.h"
#include "Doc/CafCoreTypesDoc/CAttachmentDoc.h"
#include "Doc/PayloadEnvelopeDoc/CPayloadEnvelopeDoc.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "CIncomingMessageHandlerInstance.h"
#include "CMessagePartsHeader.h"
#include "Integration/Core/MessageHeaders.h"
#include "Integration/Caf/CCafMessageCreator.h"
#include "Integration/Caf/CCafMessagePayloadParser.h"
#include "Integration/Caf/CCafMessagePayload.h"
#include "Integration/Core/CMessageHeaderUtils.h"

using namespace Caf;

CIncomingMessageHandlerInstance::CIncomingMessageHandlerInstance() :
	_isInitialized(false),
	CAF_CM_INIT_LOG("CIncomingMessageHandlerInstance") {
}

CIncomingMessageHandlerInstance::~CIncomingMessageHandlerInstance() {
}

void CIncomingMessageHandlerInstance::initialize(
	const IBean::Cargs& ctorArgs,
	const IBean::Cprops& properties,
	const SmartPtrIDocument& configSection) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(configSection);
	_id = configSection->findRequiredAttribute("id");
	_isInitialized = true;
}

std::string CIncomingMessageHandlerInstance::getId() const {
	CAF_CM_FUNCNAME_VALIDATE("getId");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _id;
}

void CIncomingMessageHandlerInstance::wire(
	const SmartPtrIAppContext& appContext,
	const SmartPtrIChannelResolver& channelResolver) {
	CAF_CM_FUNCNAME_VALIDATE("wire");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(appContext);
}

SmartPtrIIntMessage CIncomingMessageHandlerInstance::transformMessage(
	const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME_VALIDATE("transformMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(message);

	SmartPtrIIntMessage newMessage = handleMessage(message);

	return newMessage;
}
/**
 * Processes an incoming message
 * <p>
 * Pre-processes an incoming message and:<br>
 * (1) determines if it is a duplicate message<br>
 * (2) determines if it is a stand-alone message or part of a larger message
 * (chunking)<br>
 * (3) enhances the message headers with internal information<br>
 * (4) caches the message's replyTo information<br>
 * <p>
 * This method will return <i>null<i> if the message cannot be processed as
 * is. This is the case if the message is a chunk of a larger message. When
 * all of the chunks are assembled then the assembled message will be
 * returned.
 * <p>
 * This component interacts with the MessageDeliveryStateRecorder in the
 * case of message receipts. If a message receipt needs to be generated, a
 * record will be added to the MessageDeliveryStateRecorder which will
 * produce and send the receipt.
 * <p>
 * If the message is a duplicate it will be marked as such and returned so
 * that logic in the context file can do something with it if desired.
 *
 * @param message incoming message
 * @return the incoming message if it can be processed on its own or null if
 *         not
 */
SmartPtrIIntMessage CIncomingMessageHandlerInstance::handleMessage(
	const SmartPtrIIntMessage& message) {
	CAF_CM_STATIC_FUNC_VALIDATE("CIncomingMessageHandlerInstance", "handleMessage");
	CAF_CM_VALIDATE_INTERFACE(message);

	const std::string workingDir =
		AppConfigUtils::getRequiredString("communication_amqp", _sConfigWorkingDir);

	CMessageHeaderUtils::log(message->getHeaders());

	SmartPtrIIntMessage rc;
	if (CMessageHeaderUtils::getBoolOpt(
		message->getHeaders(), MessageHeaders::_sMULTIPART)) {
		rc = getAssembledMessage(message, workingDir);
	} else {
		rc = message;
	}
	return rc;
}

SmartPtrIIntMessage CIncomingMessageHandlerInstance::getAssembledMessage(
	const SmartPtrIIntMessage& message,
	const std::string& workingDir) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CIncomingMessageHandlerInstance", "getAssembledMessage");
	CAF_CM_VALIDATE_INTERFACE(message);
	CAF_CM_VALIDATE_STRING(workingDir);

	const std::string correlationId = processMessage(message, workingDir);

	const std::string messageDir = FileSystemUtils::buildPath(
		workingDir, correlationId);
	const std::string manifestFile = FileSystemUtils::buildPath(messageDir, "0.part");
	CAF_CM_LOG_DEBUG_VA1("Reconstructing manifest - %s", manifestFile.c_str());

	const SmartPtrCDynamicByteArray payload =
			CCafMessagePayload::createBufferFromFile(manifestFile);
	const SmartPtrCPayloadEnvelopeDoc payloadEnvelope =
			CCafMessagePayloadParser::getPayloadEnvelope(payload);

	const std::deque<SmartPtrCAttachmentDoc> attachmentDocs =
			payloadEnvelope->getAttachmentCollection()->getAttachment();

	// refactor the local attachments to point to the transferred
	// attachments
	std::deque<SmartPtrCAttachmentDoc> refactoredAttachments;
	if (!attachmentDocs.empty()) {
		if (!attachmentDocs.empty()) {
			CAF_CM_LOG_DEBUG_VA2(
				"Refactoring attachments - correlationId: %s, numAttachments: %d",
				correlationId.c_str(), attachmentDocs.size());
			for (TConstIterator<std::deque<SmartPtrCAttachmentDoc> > attachmentIter(attachmentDocs);
				attachmentIter; attachmentIter++) {
				const SmartPtrCAttachmentDoc attachment = *attachmentIter;
				const std::string attachmentName = attachment->getName();
				const std::string attachmentUri = attachment->getUri();

				UriUtils::SUriRecord uriRecord;
				UriUtils::parseUriString(attachmentUri, uriRecord);

				CAF_CM_LOG_DEBUG_VA3("Parsed URI - Uri: %s, protocol: %s, address: %s",
					attachmentUri.c_str(), uriRecord.protocol.c_str(),
					uriRecord.address.c_str());

				if(uriRecord.protocol.compare("attachment") == 0) {
					const std::string uriPath = uriRecord.address;
					const std::string::size_type slashPos = uriPath.find('/');
					const std::string attNumStr = uriPath.substr(slashPos + 1);

					const std::string attFile = FileSystemUtils::buildPath(
						messageDir, attNumStr + ".part");
					const std::string attUri = UriUtils::appendParameters(
						attFile, uriRecord.parameters);

					SmartPtrCAttachmentDoc refactoredAttachment;
					refactoredAttachment.CreateInstance();
					refactoredAttachment->initialize(attachment->getName(),
						attachment->getType(), "file:///" + attUri, false,
						attachment->getCmsPolicy());
					refactoredAttachments.push_back(refactoredAttachment);

					CAF_CM_LOG_DEBUG_VA3(
						"Adding refactored attachment - name: %s, type: %s, uri: %s",
						refactoredAttachment->getName().c_str(),
						refactoredAttachment->getType().c_str(),
						refactoredAttachment->getUri().c_str());
				} else {
					refactoredAttachments.push_back(attachment);
				}
			}
		}
	}

	SmartPtrIIntMessage newMessage;
	if (refactoredAttachments.empty()) {
		newMessage = CCafMessageCreator::create(
				payload, refactoredAttachments, message->getHeaders());
	} else {
		const SmartPtrCCafMessageHeadersWriter cafMessageHeadersWriter =
				CCafMessageHeadersWriter::create();
		cafMessageHeadersWriter->insertString(
				MessageHeaders::_sMULTIPART_WORKING_DIR, messageDir);

		newMessage = CCafMessageCreator::createPayloadEnvelope(
				payloadEnvelope, refactoredAttachments,
				cafMessageHeadersWriter->getHeaders(), message->getHeaders());
	}

	return newMessage;
}

std::string CIncomingMessageHandlerInstance::processMessage(
	const SmartPtrIIntMessage& message,
	const std::string& workingDir) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CIncomingMessageHandlerInstance", "processMessage");
	CAF_CM_VALIDATE_INTERFACE(message);
	CAF_CM_VALIDATE_STRING(workingDir);

	SmartPtrCDynamicByteArray payload = message->getPayload();
	payload->resetCurrentPos();

	CAF_CM_LOG_DEBUG_VA1("Processing payload - byteCount: %d",
		payload->getByteCount());

	const std::string payloadPath = FileSystemUtils::buildPath(FileSystemUtils::getTmpDir(), "payload.out");
	FileSystemUtils::saveByteFile(payloadPath, payload->getPtr(), payload->getByteCount());

	SmartPtrCMessagePartsHeader header =
		CMessagePartsHeader::fromByteBuffer(payload);
	CAF_CM_LOG_DEBUG_VA3(
		"Processing message parts - version: %d, correlationId: %s, numberOfParts: %d",
		CMessagePartsHeader::CAF_MSG_VERSION, header->getCorrelationIdStr().c_str(),
		header->getNumberOfParts());

	const std::string messageDir = FileSystemUtils::buildPath(
		workingDir, header->getCorrelationIdStr());
	if (!FileSystemUtils::doesFileExist(messageDir)) {
		CAF_CM_LOG_DEBUG_VA1("Creating directory - %s", messageDir.c_str());
		FileSystemUtils::createDirectory(messageDir);
	}

	while (payload->getByteCountFromCurrentPos() > 0) {
		SmartPtrCMessagePartDescriptor partDescriptor = CMessagePartDescriptor::fromByteBuffer(payload);
		CAF_CM_LOG_DEBUG_VA5(
			"Processing message parts descriptor - version: %d, attachmentNumber: %d, partNumber: %d, dataSize: %d, dataOffset: %d",
			CMessagePartDescriptor::CAF_MSG_VERSION, partDescriptor->getAttachmentNumber(),
			partDescriptor->getPartNumber(), partDescriptor->getDataSize(), partDescriptor->getDataOffset());

		const std::string attachmentFile = FileSystemUtils::buildPath(
			messageDir, partDescriptor->getAttachmentNumberStr() + ".part");

		SmartPtrCDynamicByteArray partBuffer;
		partBuffer.CreateInstance();
		partBuffer->allocateBytes(partDescriptor->getDataSize());
		partBuffer->memCpy(payload->getPtrAtCurrentPos(), partDescriptor->getDataSize());
		partBuffer->incrementCurrentPos(static_cast<uint32>(partDescriptor->getDataOffset()));

		payload->incrementCurrentPos(partDescriptor->getDataSize());

		FileSystemUtils::saveByteFile(attachmentFile, partBuffer->getPtrAtCurrentPos(), partBuffer->getByteCountFromCurrentPos());
	}

	return header->getCorrelationIdStr();
}
