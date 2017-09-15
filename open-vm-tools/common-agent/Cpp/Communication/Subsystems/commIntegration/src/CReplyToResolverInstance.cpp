/*
 *  Created on: Aug 16, 2012
 *      Author: mdonahue
 *
 *  Copyright (c) 2012 VMware, Inc.  All rights reserved.
 *  -- VMware Confidential
 */

#include "stdafx.h"

#include "Integration/Caf/CCafMessageHeaders.h"
#include "Doc/PayloadEnvelopeDoc/CPayloadEnvelopeDoc.h"
#include "IVariant.h"
#include "Integration/IIntMessage.h"
#include "Exception/CCafException.h"
#include "CReplyToResolverInstance.h"
#include "amqpCore/DefaultAmqpHeaderMapper.h"
#include "Integration/Caf/CCafMessagePayloadParser.h"

using namespace Caf;

CReplyToResolverInstance::CReplyToResolverInstance() :
	_isInitialized(false),
	CAF_CM_INIT_LOG("CReplyToResolverInstance") {
	CAF_CM_INIT_THREADSAFE;
}

CReplyToResolverInstance::~CReplyToResolverInstance() {
}

void CReplyToResolverInstance::initializeBean(
	const IBean::Cargs& ctorArgs,
	const IBean::Cprops& properties) {
	CAF_CM_FUNCNAME_VALIDATE("initializeBean");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	_isInitialized = true;
}

void CReplyToResolverInstance::terminateBean() {
}

std::string CReplyToResolverInstance::cacheReplyTo(
	const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME("cacheReplyTo");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	const SmartPtrCPayloadEnvelopeDoc payloadEnvelope =
			CCafMessagePayloadParser::getPayloadEnvelope(message->getPayload());

	const SmartPtrCCafMessageHeaders cafMessageHeaders =
			CCafMessageHeaders::create(message->getHeaders());
	const std::string replyTo = cafMessageHeaders->getOptionalStr(
			AmqpIntegration::DefaultAmqpHeaderMapper::REPLY_TO);

	if (! replyTo.empty()) {
		const UUID requestId = payloadEnvelope->getRequestId();
		const std::string requestIdStr = BasePlatform::UuidToString(requestId);
		CAF_CM_LOG_DEBUG_VA2(
				"Caching replyTo: [reqId=%s][replyTo=%s]",
				requestIdStr.c_str(),
				replyTo.c_str());
		_replyToAddresses.insert(std::make_pair(requestId, replyTo));
	} else {
		CAF_CM_EXCEPTIONEX_VA1(
				NoSuchElementException,
				0,
				"Message does not have a '%s' header.",
				AmqpIntegration::DefaultAmqpHeaderMapper::REPLY_TO.c_str());
	}
	return replyTo;
}

std::string CReplyToResolverInstance::lookupReplyTo(
	const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME("lookupReplyTo");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	std::string replyTo;

	const SmartPtrCPayloadEnvelopeDoc payloadEnvelope =
			CCafMessagePayloadParser::getPayloadEnvelope(message->getPayload());

	const SmartPtrCCafMessageHeaders cafMessageHeaders =
			CCafMessageHeaders::create(message->getHeaders());

	const UUID requestId = payloadEnvelope->getRequestId();
	AddressMap::iterator replyToIter = _replyToAddresses.find(requestId);
	if (replyToIter != _replyToAddresses.end()) {
		replyTo = replyToIter->second;
		_replyToAddresses.erase(replyToIter);
	} else {
		const std::string requestIdStr = BasePlatform::UuidToString(requestId);
		CAF_CM_EXCEPTIONEX_VA1(
				NoSuchElementException,
				0,
				"Request id %s was not found in the address collection.",
				requestIdStr.c_str());
	}
	return replyTo;
}

SmartPtrIVariant CReplyToResolverInstance::invokeExpression(
		const std::string& methodName,
		const Cdeqstr& methodParams,
		const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME("lookupReplyTo");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_ASSERT(!methodParams.size());
	SmartPtrIVariant result;

	if (methodName == "lookupReplyTo") {
		result = CVariant::createString(lookupReplyTo(message));
	} else {
		CAF_CM_EXCEPTIONEX_VA1(
				NoSuchMethodException,
				0,
				"Method '%s' is not supported by this invoker",
				methodName.c_str());
	}
	return result;
}
