/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Integration/Caf/CBeanPropertiesHelper.h"
#include "Integration/Caf/CCafMessageHeaders.h"
#include "CCmsMessage.h"
#include "CCmsMessageAttachments.h"
#include "Common/IAppContext.h"
#include "Doc/CafCoreTypesDoc/CAttachmentDoc.h"
#include "Doc/PayloadEnvelopeDoc/CPayloadEnvelopeDoc.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Exception/CCafException.h"
#include "CCmsMessageTransformerInstance.h"
#include "Integration/Caf/CCafMessageCreator.h"
#include "Integration/Caf/CCafMessagePayloadParser.h"

using namespace Caf;

CCmsMessageTransformerInstance::CCmsMessageTransformerInstance() :
		_isInitialized(false),
		_isSigningEnforced(true),
		_isEncryptionEnforced(true),
		CAF_CM_INIT("CCmsMessageTransformerInstance") {
}

CCmsMessageTransformerInstance::~CCmsMessageTransformerInstance() {
}

void CCmsMessageTransformerInstance::initialize(
		const IBean::Cargs& ctorArgs,
		const IBean::Cprops& properties,
		const SmartPtrIDocument& configSection) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(configSection);

	const SmartPtrCBeanPropertiesHelper beanProperties =
			CBeanPropertiesHelper::create(properties);

	_id = configSection->findRequiredAttribute("id");
	const std::string cmsPolicyStr = beanProperties->getRequiredString("cmsPolicy");

	_isSigningEnforced = beanProperties->getRequiredBool("isSigningEnforced");
	_isEncryptionEnforced = beanProperties->getRequiredBool("isEncryptionEnforced");

	_workingDirectory = AppConfigUtils::getRequiredString("communication_amqp",
		_sConfigWorkingDir);

	_isInitialized = true;
}

std::string CCmsMessageTransformerInstance::getId() const {
	CAF_CM_FUNCNAME_VALIDATE("getId");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _id;
}

void CCmsMessageTransformerInstance::wire(
		const SmartPtrIAppContext& appContext,
		const SmartPtrIChannelResolver& channelResolver) {
	CAF_CM_FUNCNAME_VALIDATE("wire");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(appContext);
	CAF_CM_VALIDATE_INTERFACE(channelResolver);
}

SmartPtrIIntMessage CCmsMessageTransformerInstance::transformMessage(
		const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME("transformMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(message);

	SmartPtrCCafMessageHeaders cafMessageHeaders =
			CCafMessageHeaders::create(message->getHeaders());
	const std::string msgFlow = cafMessageHeaders->getFlowDirection();

	const SmartPtrCPayloadEnvelopeDoc payloadEnvelope =
			CCafMessagePayloadParser::getPayloadEnvelope(message->getPayload());

	SmartPtrCCmsMessage cmsMessage;
	cmsMessage.CreateInstance();
	cmsMessage->initialize(
			BasePlatform::UuidToString(payloadEnvelope->getClientId()),
			payloadEnvelope->getPmeId());

	SmartPtrCCmsMessageAttachments cmsMessageAttachments;
	cmsMessageAttachments.CreateInstance();
	cmsMessageAttachments->initialize(cmsMessage);

	SmartPtrIIntMessage rc;
	if (msgFlow.compare("OUTGOING") == 0) {
		rc = createOutgoingPayload(message->getHeaders(), payloadEnvelope, cmsMessageAttachments);
	} else if (msgFlow.compare("INCOMING") == 0) {
		rc = createIncomingPayload(message->getHeaders(), payloadEnvelope, cmsMessageAttachments);
	} else {
		CAF_CM_EXCEPTION_VA1(E_FAIL,
				"Invalid msgflow header value: %s", msgFlow.c_str());
	}

	return rc;
}

SmartPtrIIntMessage CCmsMessageTransformerInstance::createOutgoingPayload(
		const IIntMessage::SmartPtrCHeaders& headers,
		const SmartPtrCPayloadEnvelopeDoc& payloadEnvelope,
		const SmartPtrCCmsMessageAttachments& cmsMessageAttachments) const {
	CAF_CM_FUNCNAME_VALIDATE("createOutgoingPayload");
	CAF_CM_VALIDATE_SMARTPTR(headers);
	CAF_CM_VALIDATE_SMARTPTR(payloadEnvelope);
	CAF_CM_VALIDATE_SMARTPTR(cmsMessageAttachments);

	const std::deque<SmartPtrCAttachmentDoc> attachmentCollection =
			payloadEnvelope->getAttachmentCollection()->getAttachment();
	const std::deque<SmartPtrCAttachmentDoc> attachmentCollectionCms =
			cmsMessageAttachments->encryptAndSignAttachments(attachmentCollection);

	return CCafMessageCreator::createPayloadEnvelope(
			payloadEnvelope, attachmentCollectionCms, headers);
}

SmartPtrIIntMessage CCmsMessageTransformerInstance::createIncomingPayload(
		const IIntMessage::SmartPtrCHeaders& headers,
		const SmartPtrCPayloadEnvelopeDoc& payloadEnvelope,
		const SmartPtrCCmsMessageAttachments& cmsMessageAttachments) const {
	CAF_CM_FUNCNAME_VALIDATE("createIncomingPayload");
	CAF_CM_VALIDATE_SMARTPTR(headers);
	CAF_CM_VALIDATE_SMARTPTR(payloadEnvelope);
	CAF_CM_VALIDATE_SMARTPTR(cmsMessageAttachments);

	// Get the attachment collection out of the payload.
	const std::deque<SmartPtrCAttachmentDoc> attachmentCollectionCms =
			payloadEnvelope->getAttachmentCollection()->getAttachment();

	// Make sure the attachments meet the minimum security bar.
	cmsMessageAttachments->enforceSecurityOnAttachments(attachmentCollectionCms,
			_isSigningEnforced, _isEncryptionEnforced);

	// Decrypt/verify the attachments.
	const std::deque<SmartPtrCAttachmentDoc> attachmentCollection =
			cmsMessageAttachments->decryptAndVerifyAttachments(attachmentCollectionCms);

	return CCafMessageCreator::createPayloadEnvelope(
			payloadEnvelope, attachmentCollection, headers);
}
