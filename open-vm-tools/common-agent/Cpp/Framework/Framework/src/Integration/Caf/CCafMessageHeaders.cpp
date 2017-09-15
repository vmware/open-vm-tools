/*
 *	 Author: bwilliams
 *  Created: Jul 2009
 *
 *	Copyright (C) 2009-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "IVariant.h"
#include "Integration/IIntMessage.h"
#include "Exception/CCafException.h"
#include "Integration/Caf/CCafMessageHeaders.h"
#include "Integration/Core/FileHeaders.h"

using namespace Caf;

SmartPtrCCafMessageHeaders CCafMessageHeaders::create(
		const IIntMessage::SmartPtrCHeaders& headers) {
	CAF_CM_STATIC_FUNC_VALIDATE("CCafMessageHeaders", "create");
	CAF_CM_VALIDATE_SMARTPTR(headers);

	SmartPtrCCafMessageHeaders rc;
	rc.CreateInstance();
	rc->initialize(headers);

	return rc;
}

CCafMessageHeaders::CCafMessageHeaders(void) :
	_isInitialized(false),
	CAF_CM_INIT_LOG("CCafMessageHeaders") {
}

CCafMessageHeaders::~CCafMessageHeaders(void) {
}

void CCafMessageHeaders::initialize(
		const IIntMessage::SmartPtrCHeaders& headers) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(headers);

	_headers = headers;
	_isInitialized = true;
}

IIntMessage::SmartPtrCHeaders CCafMessageHeaders::getHeaders() const {
	CAF_CM_FUNCNAME_VALIDATE("getHeaders");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _headers;
}

std::string CCafMessageHeaders::getPayloadType() const {
	CAF_CM_FUNCNAME_VALIDATE("getPayloadType");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return getRequiredStr("payloadType");
}

std::string CCafMessageHeaders::getPayloadTypeOpt(
		const std::string& defaultVal) const {
	CAF_CM_FUNCNAME_VALIDATE("getPayloadTypeOpt");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	std::string rcStr = getOptionalStr("payloadType");
	if (rcStr.empty()) {
		rcStr = defaultVal;
	}

	return rcStr;
}

std::string CCafMessageHeaders::getVersion() const {
	CAF_CM_FUNCNAME_VALIDATE("getVersion");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return getRequiredStr("version");
}

std::string CCafMessageHeaders::getVersionOpt(
		const std::string& defaultVal) const {
	CAF_CM_FUNCNAME_VALIDATE("getVersionOpt");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	std::string rcStr = getOptionalStr("version");
	if (rcStr.empty()) {
		rcStr = defaultVal;
	}

	return rcStr;
}

UUID CCafMessageHeaders::getClientId() const {
	CAF_CM_FUNCNAME_VALIDATE("getClientId");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	UUID rc;
	const std::string rcStr = getClientIdStr();
	BasePlatform::UuidFromString(rcStr.c_str(), rc);
	return rc;
}

std::string CCafMessageHeaders::getClientIdStr() const {
	CAF_CM_FUNCNAME_VALIDATE("getClientIdStr");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return getRequiredStr("clientId");
}

UUID CCafMessageHeaders::getClientIdOpt(
		const UUID defaultVal) const {
	CAF_CM_FUNCNAME_VALIDATE("getClientIdOpt");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	UUID rc = defaultVal;
	const std::string rcStr = getClientIdStrOpt();
	if (! rcStr.empty()) {
		BasePlatform::UuidFromString(rcStr.c_str(), rc);
	}

	return rc;
}

std::string CCafMessageHeaders::getClientIdStrOpt(
		const std::string& defaultVal) const {
	CAF_CM_FUNCNAME_VALIDATE("getClientIdStrOpt");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	std::string rcStr = getOptionalStr("clientId");
	if (rcStr.empty()) {
		rcStr = defaultVal;
	}

	return rcStr;
}

UUID CCafMessageHeaders::getRequestId() const {
	CAF_CM_FUNCNAME_VALIDATE("getRequestId");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	UUID rc;
	const std::string rcStr = getRequestIdStr();
	BasePlatform::UuidFromString(rcStr.c_str(), rc);
	return rc;
}

std::string CCafMessageHeaders::getRequestIdStr() const {
	CAF_CM_FUNCNAME_VALIDATE("getRequestIdStr");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return getRequiredStr("requestId");
}

UUID CCafMessageHeaders::getRequestIdOpt(
		const UUID defaultVal) const {
	CAF_CM_FUNCNAME_VALIDATE("getRequestIdOpt");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	UUID rc = defaultVal;
	const std::string rcStr = getRequestIdStrOpt();
	if (! rcStr.empty()) {
		BasePlatform::UuidFromString(rcStr.c_str(), rc);
	}

	return rc;
}

std::string CCafMessageHeaders::getRequestIdStrOpt(
		const std::string& defaultVal) const {
	CAF_CM_FUNCNAME_VALIDATE("getRequestIdStrOpt");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	std::string rcStr = getOptionalStr("requestId");
	if (rcStr.empty()) {
		rcStr = defaultVal;
	}

	return rcStr;
}

std::string CCafMessageHeaders::getPmeId() const {
	CAF_CM_FUNCNAME_VALIDATE("getPmeId");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return getRequiredStr("pmeId");
}

std::string CCafMessageHeaders::getPmeIdOpt(
		const std::string& defaultVal) const {
	CAF_CM_FUNCNAME_VALIDATE("getPmeIdOpt");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	std::string rcStr = getOptionalStr("pmeId");
	if (rcStr.empty()) {
		rcStr = defaultVal;
	}

	return rcStr;
}

UUID CCafMessageHeaders::getSessionId() const {
	CAF_CM_FUNCNAME_VALIDATE("getSessionId");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	UUID rc;
	const std::string rcStr = getSessionIdStr();
	BasePlatform::UuidFromString(rcStr.c_str(), rc);
	return rc;
}

std::string CCafMessageHeaders::getSessionIdStr() const {
	CAF_CM_FUNCNAME_VALIDATE("getSessionIdStr");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return getRequiredStr("sessionId");
}

UUID CCafMessageHeaders::getSessionIdOpt(
		const UUID defaultVal) const {
	CAF_CM_FUNCNAME_VALIDATE("getSessionIdOpt");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	UUID rc = defaultVal;
	const std::string rcStr = getSessionIdStrOpt();
	if (! rcStr.empty()) {
		BasePlatform::UuidFromString(rcStr.c_str(), rc);
	}

	return rc;
}

std::string CCafMessageHeaders::getSessionIdStrOpt(
		const std::string& defaultVal) const {
	CAF_CM_FUNCNAME_VALIDATE("getSessionIdStrOpt");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	std::string rcStr = getOptionalStr("sessionId");
	if (rcStr.empty()) {
		rcStr = defaultVal;
	}

	return rcStr;
}

std::string CCafMessageHeaders::getRelDirectory() const {
	CAF_CM_FUNCNAME_VALIDATE("getRelDirectory");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return getRequiredStr("relDirectory");
}

std::string CCafMessageHeaders::getRelDirectoryOpt(
		const std::string& defaultVal) const {
	CAF_CM_FUNCNAME_VALIDATE("getRelDirectoryOpt");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	std::string rcStr = getOptionalStr("relDirectory");
	if (rcStr.empty()) {
		rcStr = defaultVal;
	}

	return rcStr;
}

std::string CCafMessageHeaders::getRelFilename() const {
	CAF_CM_FUNCNAME_VALIDATE("getRelFilename");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return getRequiredStr(FileHeaders::_sFILENAME);
}

std::string CCafMessageHeaders::getRelFilenameOpt(
		const std::string& defaultVal) const {
	CAF_CM_FUNCNAME_VALIDATE("getRelFilenameOpt");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	std::string rcStr = getOptionalStr(FileHeaders::_sFILENAME);
	if (rcStr.empty()) {
		rcStr = defaultVal;
	}

	return rcStr;
}

std::string CCafMessageHeaders::getProviderUri() const {
	CAF_CM_FUNCNAME_VALIDATE("getProviderUri");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return getRequiredStr("providerUri");
}

std::string CCafMessageHeaders::getProviderUriOpt(
		const std::string& defaultVal) const {
	CAF_CM_FUNCNAME_VALIDATE("getProviderUriOpt");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	std::string rcStr = getOptionalStr("providerUri");
	if (rcStr.empty()) {
		rcStr = defaultVal;
	}

	return rcStr;
}

std::string CCafMessageHeaders::getFlowDirection() const {
	CAF_CM_FUNCNAME_VALIDATE("getFlowDirection");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return getRequiredStr("cafcomm.internal.msgflow");
}

std::string CCafMessageHeaders::getFlowDirectionOpt(
		const std::string& defaultVal) const {
	CAF_CM_FUNCNAME_VALIDATE("getFlowDirectionOpt");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	std::string rcStr = getOptionalStr("cafcomm.internal.msgflow");
	if (rcStr.empty()) {
		rcStr = defaultVal;
	}

	return rcStr;
}

std::string CCafMessageHeaders::getRequiredStr(
		const std::string& key) const {
	CAF_CM_FUNCNAME_VALIDATE("getRequiredStr");
	CAF_CM_VALIDATE_STRING(key);
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return findRequiredHeader(key)->toString();
}

std::string CCafMessageHeaders::getOptionalStr(
		const std::string& key,
		const std::string& defaultVal) const {
	CAF_CM_FUNCNAME_VALIDATE("getOptionalStr");
	CAF_CM_VALIDATE_STRING(key);
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	std::string rcStr = defaultVal;
	const SmartPtrIVariant valueVar = findOptionalHeader(key);
	if (! valueVar.IsNull()) {
		rcStr = valueVar->toString();
	}

	return rcStr;
}

bool CCafMessageHeaders::getRequiredBool(
		const std::string& key) const {
	CAF_CM_FUNCNAME_VALIDATE("getRequiredBool");
	CAF_CM_VALIDATE_STRING(key);
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	const SmartPtrIVariant valueVar = findRequiredHeader(key);
	CAF_CM_VALIDATE_BOOL(valueVar->isBool());

	const std::string valueStr = valueVar->toString();
	CAF_CM_LOG_DEBUG_VA2("key: %s, value: %s", key.c_str(), valueStr.c_str());

	return (valueStr.compare("true") == 0);
}

bool CCafMessageHeaders::getOptionalBool(
		const std::string& key,
		const bool defaultVal) const {
	CAF_CM_FUNCNAME_VALIDATE("getOptionalBool");
	CAF_CM_VALIDATE_STRING(key);
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	const SmartPtrIVariant valueVar = findOptionalHeader(key);

	bool rc = defaultVal;
	if (! valueVar.IsNull()) {
		CAF_CM_VALIDATE_BOOL(valueVar->isBool());

		const std::string valueStr = valueVar->toString();
		CAF_CM_LOG_DEBUG_VA2("key: %s, value: %s", key.c_str(), valueStr.c_str());

		rc = (valueStr.compare("true") == 0);
	}

	return rc;
}

SmartPtrIVariant CCafMessageHeaders::findOptionalHeader(
	const std::string& key) const {
	CAF_CM_FUNCNAME_VALIDATE("findOptionalHeader");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(key);

	SmartPtrIVariant value;

	IIntMessage::CHeaders::const_iterator headerIter = _headers->find(key);
	if (headerIter != _headers->end()) {
		value = headerIter->second.first;
	}

	return value;
}

SmartPtrIVariant CCafMessageHeaders::findRequiredHeader(
	const std::string& key) const {
	CAF_CM_FUNCNAME("findRequiredHeader");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(key);

	IIntMessage::CHeaders::const_iterator headerIter = _headers->find(key);
	if (headerIter == _headers->end()) {
		CAF_CM_EXCEPTIONEX_VA1(NoSuchElementException, ERROR_NOT_FOUND,
			"Key not found in headers - %s", key.c_str());
	}

	return headerIter->second.first;
}
