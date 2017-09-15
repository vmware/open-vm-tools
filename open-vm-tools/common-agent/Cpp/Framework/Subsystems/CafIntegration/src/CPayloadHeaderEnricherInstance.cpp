/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Integration/Caf/CBeanPropertiesHelper.h"
#include "Integration/Caf/CCafMessageHeadersWriter.h"
#include "Common/IAppContext.h"
#include "Doc/PayloadEnvelopeDoc/CPayloadEnvelopeDoc.h"
#include "Integration/Core/CIntMessage.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "CPayloadHeaderEnricherInstance.h"
#include "Integration/Caf/CCafMessagePayloadParser.h"

using namespace Caf;

CPayloadHeaderEnricherInstance::CPayloadHeaderEnricherInstance() :
	_isInitialized(false),
	_includeFilename(false),
	CAF_CM_INIT_LOG("CPayloadHeaderEnricherInstance") {
}

CPayloadHeaderEnricherInstance::~CPayloadHeaderEnricherInstance() {
}

void CPayloadHeaderEnricherInstance::initialize(
	const IBean::Cargs& ctorArgs,
	const IBean::Cprops& properties,
	const SmartPtrIDocument& configSection) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(configSection);

	_id = configSection->findRequiredAttribute("id");

	const SmartPtrCBeanPropertiesHelper beanProperties =
			CBeanPropertiesHelper::create(properties);
	_includeFilename = beanProperties->getOptionalBool("includeFilename", true);

	_isInitialized = true;
}

std::string CPayloadHeaderEnricherInstance::getId() const {
	CAF_CM_FUNCNAME_VALIDATE("getId");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _id;
}

void CPayloadHeaderEnricherInstance::wire(
	const SmartPtrIAppContext& appContext,
	const SmartPtrIChannelResolver& channelResolver) {
	CAF_CM_FUNCNAME_VALIDATE("wire");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(appContext);
	CAF_CM_VALIDATE_INTERFACE(channelResolver);
}

SmartPtrIIntMessage CPayloadHeaderEnricherInstance::transformMessage(
	const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME_VALIDATE("transformMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	const SmartPtrCPayloadEnvelopeDoc payloadEnvelope =
			CCafMessagePayloadParser::getPayloadEnvelope(message->getPayload());

	const std::string payloadType = payloadEnvelope->getPayloadType();
	const std::string clientIdStr =
			BasePlatform::UuidToString(payloadEnvelope->getClientId());
	const std::string requestIdStr =
			BasePlatform::UuidToString(payloadEnvelope->getRequestId());
	const std::string pmeIdStr = payloadEnvelope->getPmeId();
	const std::string version = payloadEnvelope->getVersion();
	const std::string payloadVersion = payloadEnvelope->getPayloadVersion();

	SmartPtrCCafMessageHeadersWriter messageHeadersWriter =
			CCafMessageHeadersWriter::create();
	messageHeadersWriter->setPayloadType(payloadType);
	messageHeadersWriter->setClientId(clientIdStr);
	messageHeadersWriter->setRequestId(requestIdStr);
	messageHeadersWriter->setPmeId(pmeIdStr);
	messageHeadersWriter->setVersion(version);
	messageHeadersWriter->setPayloadVersion(payloadVersion);

	if (_includeFilename) {
		std::string relFilename = FileSystemUtils::buildPath(
				clientIdStr, requestIdStr, pmeIdStr);
		relFilename = FileSystemUtils::buildPath(relFilename, _sPayloadRequestFilename);

		messageHeadersWriter->setRelFilename(relFilename);

		CAF_CM_LOG_DEBUG_VA2(
			"Enhanced the headers - payloadType: \"%s\", filename: \"%s\"",
			payloadType.c_str(), relFilename.c_str());
	}

	SmartPtrCIntMessage messageImpl;
	messageImpl.CreateInstance();
	messageImpl->initialize(message->getPayload(),
			messageHeadersWriter->getHeaders(), message->getHeaders());

	return messageImpl;
}
