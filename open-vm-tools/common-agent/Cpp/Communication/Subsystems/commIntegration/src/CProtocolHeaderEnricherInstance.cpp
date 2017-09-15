/*
 *  Created on: Aug 16, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Integration/Caf/CCafMessageHeadersWriter.h"
#include "Common/IAppContext.h"
#include "Doc/CafCoreTypesDoc/CProtocolCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CProtocolDoc.h"
#include "Doc/PayloadEnvelopeDoc/CPayloadEnvelopeDoc.h"
#include "Integration/Core/CIntMessage.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Exception/CCafException.h"
#include "CProtocolHeaderEnricherInstance.h"
#include "Integration/Caf/CCafMessagePayloadParser.h"

using namespace Caf;

CProtocolHeaderEnricherInstance::CProtocolHeaderEnricherInstance() :
	_isInitialized(false),
	CAF_CM_INIT_LOG("CProtocolHeaderEnricherInstance") {
}

CProtocolHeaderEnricherInstance::~CProtocolHeaderEnricherInstance() {
}

void CProtocolHeaderEnricherInstance::initialize(
	const IBean::Cargs& ctorArgs,
	const IBean::Cprops& properties,
	const SmartPtrIDocument& configSection) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(configSection);
	_id = configSection->findRequiredAttribute("id");
	_isInitialized = true;
}

std::string CProtocolHeaderEnricherInstance::getId() const {
	CAF_CM_FUNCNAME_VALIDATE("getId");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _id;
}

void CProtocolHeaderEnricherInstance::wire(
	const SmartPtrIAppContext& appContext,
	const SmartPtrIChannelResolver& channelResolver) {
	CAF_CM_FUNCNAME_VALIDATE("wire");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(appContext);
}

SmartPtrIIntMessage CProtocolHeaderEnricherInstance::transformMessage(
		const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME_VALIDATE("transformMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	const SmartPtrCPayloadEnvelopeDoc payloadEnvelope =
			CCafMessagePayloadParser::getPayloadEnvelope(message->getPayload());

	const SmartPtrCProtocolDoc protocol = findProtocol(payloadEnvelope);

	UriUtils::SUriRecord uriRecord;
	UriUtils::parseUriString(protocol->getUri(), uriRecord);

	SmartPtrCCafMessageHeadersWriter messageHeadersWriter =
			CCafMessageHeadersWriter::create();
	messageHeadersWriter->setProtocol(uriRecord.protocol);
	messageHeadersWriter->setProtocolAddress(uriRecord.address);

	CAF_CM_LOG_DEBUG_VA2(
			"Enhanced the headers - protocol: \"%s\", connStr: \"%s\"",
			uriRecord.protocol.c_str(), uriRecord.address.c_str());

	SmartPtrCIntMessage messageImpl;
	messageImpl.CreateInstance();
	messageImpl->initialize(message->getPayload(),
			messageHeadersWriter->getHeaders(), message->getHeaders());

	return messageImpl;
}

SmartPtrCProtocolDoc CProtocolHeaderEnricherInstance::findProtocol(
		const SmartPtrCPayloadEnvelopeDoc& payloadEnvelope) const {
	CAF_CM_FUNCNAME("findProtocol");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(payloadEnvelope);

	const SmartPtrCProtocolCollectionDoc protocolCollectionDoc =
			payloadEnvelope->getProtocolCollection();
	CAF_CM_VALIDATE_SMARTPTR(protocolCollectionDoc);

	const std::deque<SmartPtrCProtocolDoc> protocolCollection =
			protocolCollectionDoc->getProtocol();

	if (protocolCollection.size() != 1) {
		CAF_CM_EXCEPTION_VA1(E_NOTIMPL,
				"Multiple protocols are not yet supported - %d",
				protocolCollection.size());
	}

	return *(protocolCollection.begin());
}
