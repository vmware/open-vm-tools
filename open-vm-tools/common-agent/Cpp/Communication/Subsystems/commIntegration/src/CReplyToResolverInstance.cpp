/*
 *  Created on: Aug 16, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
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

	// Read cache map into memory
	loadCache();

	_isInitialized = true;
}

void CReplyToResolverInstance::terminateBean() {
	persistCache();
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
	CAF_CM_FUNCNAME("invokeExpression");
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

inline std::string CReplyToResolverInstance::getResolverCacheFilePath()
{
	return AppConfigUtils::getRequiredString("communication_amqp", "resolver_cache_file");
}

/*
 * private methods
 *
 */
void CReplyToResolverInstance::loadCache() {
	CAF_CM_FUNCNAME_VALIDATE("loadCache");
	CAF_CM_LOCK_UNLOCK;

	//const std::string cacheFilePath = AppConfigUtils::getRequiredString("communication_amqp", "resolver_cache_file");
	const std::string cacheFilePath = getResolverCacheFilePath();
	const std::string cacheDirPath = FileSystemUtils::getDirname(cacheFilePath);
	if (! FileSystemUtils::doesDirectoryExist(cacheDirPath)) {
		FileSystemUtils::createDirectory(cacheDirPath);
	}
	if (FileSystemUtils::doesFileExist(cacheFilePath)) {

                const std::deque<std::string> fileContents = FileSystemUtils::loadTextFileIntoColl(cacheFilePath);
		for(TConstIterator<std::deque<std::string> > fileLineIter(fileContents); fileLineIter; fileLineIter++) {
			//const std::string fileLine = *fileLineIter;
			const Cdeqstr fileLineTokens = CStringUtils::split(*fileLineIter, ' ');

			CAF_CM_LOG_DEBUG_VA2("cache entry - reqId: %s, addr: %s",
				fileLineTokens[0].c_str(), fileLineTokens[1].c_str());
			if (fileLineTokens.size() == 2) {
				UUID reqId;
				BasePlatform::UuidFromString(fileLineTokens[0].c_str(), reqId);
				_replyToAddresses.insert(std::make_pair(reqId, fileLineTokens[1]));
			}
                }

	} else {
		CAF_CM_LOG_DEBUG_VA1("resolver cache is not available - resolverCache: %s", cacheFilePath.c_str());
	}

}

void CReplyToResolverInstance::persistCache() {
	CAF_CM_FUNCNAME_VALIDATE("persistCache");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	//const std::string cacheFilePath = AppConfigUtils::getRequiredString("communication_amqp", "resolver_cache_file");
	const std::string cacheFilePath = getResolverCacheFilePath();
	std::stringstream contents;
	//contents.str("");
	for (AddressMap::const_iterator replyToIter = _replyToAddresses.begin();
		replyToIter != _replyToAddresses.end(); ++replyToIter) {

		std::string reqIdStr = BasePlatform::UuidToString(replyToIter->first);
		contents << reqIdStr << " " << replyToIter->second << std::endl;
		CAF_CM_LOG_DEBUG_VA2("caching entry - reqId: %s, addr: %s",
				reqIdStr.c_str(), replyToIter->second.c_str());
	}
	if (contents.str().length() > 0) {
		CAF_CM_LOG_DEBUG_VA0("Caching resolver map.");
		FileSystemUtils::saveTextFile(cacheFilePath, contents.str());
	}
	

}

