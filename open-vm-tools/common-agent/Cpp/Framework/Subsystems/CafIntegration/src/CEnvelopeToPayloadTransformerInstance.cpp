/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Common/IAppContext.h"
#include "Doc/CafCoreTypesDoc/CAttachmentDoc.h"
#include "Doc/PayloadEnvelopeDoc/CPayloadEnvelopeDoc.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "CEnvelopeToPayloadTransformerInstance.h"
#include "Exception/CCafException.h"
#include "Integration/Caf/CCafMessagePayloadParser.h"
#include "Integration/Caf/CCafMessageCreator.h"

using namespace Caf;

CEnvelopeToPayloadTransformerInstance::CEnvelopeToPayloadTransformerInstance() :
	_isInitialized(false),
	CAF_CM_INIT("CEnvelopeToPayloadTransformerInstance") {
}

CEnvelopeToPayloadTransformerInstance::~CEnvelopeToPayloadTransformerInstance() {
}

void CEnvelopeToPayloadTransformerInstance::initialize(
	const IBean::Cargs& ctorArgs,
	const IBean::Cprops& properties,
	const SmartPtrIDocument& configSection) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(configSection);

	_id = configSection->findRequiredAttribute("id");
	_isInitialized = true;
}

std::string CEnvelopeToPayloadTransformerInstance::getId() const {
	CAF_CM_FUNCNAME_VALIDATE("getId");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _id;
}

void CEnvelopeToPayloadTransformerInstance::wire(
	const SmartPtrIAppContext& appContext,
	const SmartPtrIChannelResolver& channelResolver) {
	CAF_CM_FUNCNAME_VALIDATE("wire");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(appContext);
	CAF_CM_VALIDATE_INTERFACE(channelResolver);
}

SmartPtrIIntMessage CEnvelopeToPayloadTransformerInstance::transformMessage(
	const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME_VALIDATE("transformMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	const SmartPtrCPayloadEnvelopeDoc payloadEnvelope =
			CCafMessagePayloadParser::getPayloadEnvelope(message->getPayload());

	const std::deque<SmartPtrCAttachmentDoc> attachmentCollection =
			payloadEnvelope->getAttachmentCollection()->getAttachment();

	const SmartPtrCDynamicByteArray payload= findPayload(attachmentCollection);
	const std::deque<SmartPtrCAttachmentDoc> attachmentCollectionRm =
			removePayload(attachmentCollection);

	return CCafMessageCreator::create(
			payload, attachmentCollectionRm, message->getHeaders());
}

SmartPtrIIntMessage CEnvelopeToPayloadTransformerInstance::processErrorMessage(
	const SmartPtrIIntMessage& message) {
	return transformMessage(message);
}

SmartPtrCDynamicByteArray CEnvelopeToPayloadTransformerInstance::findPayload(
		const std::deque<SmartPtrCAttachmentDoc>& attachmentCollection) const {
	CAF_CM_FUNCNAME("findPayload");
	CAF_CM_VALIDATE_STL(attachmentCollection);

	std::string payloadPath;
	for (TConstIterator<std::deque<SmartPtrCAttachmentDoc> > attachmentIter(attachmentCollection);
		attachmentIter; attachmentIter++) {
		const SmartPtrCAttachmentDoc attachment = *attachmentIter;

		if (attachment->getName().compare("_EnvelopePayload_") == 0) {
			UriUtils::SUriRecord sourceUriRecord;
			UriUtils::parseUriString(attachment->getUri(), sourceUriRecord);

			if (sourceUriRecord.protocol.compare("file") != 0) {
				CAF_CM_EXCEPTION_VA1(ERROR_INVALID_STATE,
					"Payload attachment must be a file - uri: %s",
					attachment->getUri().c_str());
			}

			UriUtils::SFileUriRecord sourceFileUriRecord;
			UriUtils::parseFileAddress(sourceUriRecord.address, sourceFileUriRecord);
			payloadPath = sourceFileUriRecord.path;

			if (!FileSystemUtils::doesFileExist(payloadPath)) {
				CAF_CM_EXCEPTION_VA1(ERROR_FILE_NOT_FOUND,
					"File not found - %s", payloadPath.c_str());
			}
		}
	}

	if (payloadPath.empty()) {
		CAF_CM_EXCEPTION_VA0(ERROR_NOT_FOUND, "Payload attachment not found");
	}

	return FileSystemUtils::loadByteFile(payloadPath);
}

std::deque<SmartPtrCAttachmentDoc> CEnvelopeToPayloadTransformerInstance::removePayload(
	const std::deque<SmartPtrCAttachmentDoc>& attachmentCollection) const {
	CAF_CM_FUNCNAME_VALIDATE("removePayload");
	CAF_CM_VALIDATE_STL(attachmentCollection);

	std::deque<SmartPtrCAttachmentDoc> rc;
	for (TConstIterator<std::deque<SmartPtrCAttachmentDoc> > attachmentIter(attachmentCollection);
		attachmentIter; attachmentIter++) {
		const SmartPtrCAttachmentDoc attachment = *attachmentIter;

		if (attachment->getName().compare("_EnvelopePayload_") != 0) {
			rc.push_back(attachment);
		}
	}

	return rc;
}
