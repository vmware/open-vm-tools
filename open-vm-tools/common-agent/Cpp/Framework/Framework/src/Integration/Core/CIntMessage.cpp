/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "ICafObject.h"
#include "IVariant.h"
#include "Integration/IIntMessage.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "Integration/Core/CIntMessage.h"
#include "Exception/CCafException.h"

using namespace Caf;

CIntMessage::CIntMessage() :
	_isInitialized(false),
	_messageId(CAFCOMMON_GUID_NULL),
	CAF_CM_INIT("CIntMessage") {
}

CIntMessage::~CIntMessage() {
}

CIntMessage::SmartPtrCHeaders CIntMessage::mergeHeaders(
	const SmartPtrCHeaders& newHeaders,
	const SmartPtrCHeaders& origHeaders) {
	CIntMessage::SmartPtrCHeaders headers;
	headers.CreateInstance();

	if (newHeaders) {
		std::copy(
				newHeaders->begin(),
				newHeaders->end(),
				std::inserter(
						*headers,
						headers->end()));
	}
	if (origHeaders) {
		std::copy(
				origHeaders->begin(),
				origHeaders->end(),
				std::inserter(
						*headers,
						headers->end()));
	}

	return headers;
}

void CIntMessage::initializeStr(
	const std::string& payloadStr,
	const SmartPtrCHeaders& newHeaders,
	const SmartPtrCHeaders& origHeaders) {
	CAF_CM_FUNCNAME_VALIDATE("initializeStr");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);

	std::string payloadStrTmp = payloadStr;
	if (payloadStr.empty()) {
		payloadStrTmp = "";
	}

	SmartPtrCDynamicByteArray payload;
	payload.CreateInstance();
	payload->allocateBytes(payloadStrTmp.length());
	payload->memCpy(payloadStrTmp.c_str(), payloadStrTmp.length());

	initialize(payload, newHeaders, origHeaders);
}

void CIntMessage::initialize(
	const SmartPtrCDynamicByteArray& payload,
	const SmartPtrCHeaders& newHeaders,
	const SmartPtrCHeaders& origHeaders) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
		// payload is optional
		// newHeaders are optional
		// origHeaders are optional

		_payload = payload;
		::UuidCreate(&_messageId);

		_headers = mergeHeaders(newHeaders, origHeaders);

		IIntMessage::CHeaders::iterator header = _headers->find(MessageHeaders::_sID);
		if (_headers->end() == header) {
			_headers->insert(std::make_pair(MessageHeaders::_sID,
				std::make_pair(CVariant::createString(CStringUtils::createRandomUuid()), SmartPtrICafObject())));
		}
		header = _headers->find(MessageHeaders::_sTIMESTAMP);
		if (_headers->end() == header) {
			_headers->insert(std::make_pair(MessageHeaders::_sTIMESTAMP,
				std::make_pair(CVariant::createUint64(CDateTimeUtils::getTimeMs()), SmartPtrICafObject())));
		}
		_isInitialized = true;
	}
	CAF_CM_EXIT;
}

SmartPtrCDynamicByteArray CIntMessage::getPayload() const {
	CAF_CM_FUNCNAME_VALIDATE("getPayload");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _payload;
}

std::string CIntMessage::getPayloadStr() const {
	CAF_CM_FUNCNAME_VALIDATE("getPayloadStr");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return reinterpret_cast<const char*>(_payload->getPtr());
}

UUID CIntMessage::getMessageId() const {
	CAF_CM_FUNCNAME_VALIDATE("getMessageId");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _messageId;
}

std::string CIntMessage::getMessageIdStr() const {
	CAF_CM_FUNCNAME_VALIDATE("getMessageIdStr");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return BasePlatform::UuidToString(_messageId);
}

IIntMessage::SmartPtrCHeaders CIntMessage::getHeaders() const {
	CAF_CM_FUNCNAME_VALIDATE("getHeaders");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _headers;
}

SmartPtrIVariant CIntMessage::findOptionalHeader(
	const std::string& key) const {
	CAF_CM_FUNCNAME_VALIDATE("findOptionalHeader");

	SmartPtrIVariant value;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(key);

		CHeaders::const_iterator headerIter = _headers->find(key);
		if (headerIter != _headers->end()) {
			value = headerIter->second.first;
		}
	}
	CAF_CM_EXIT;

	return value;
}

SmartPtrIVariant CIntMessage::findRequiredHeader(
	const std::string& key) const {
	CAF_CM_FUNCNAME("findRequiredHeader");

	SmartPtrIVariant value;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(key);

		CHeaders::const_iterator headerIter = _headers->find(key);
		if (headerIter == _headers->end()) {
			CAF_CM_EXCEPTIONEX_VA1(NoSuchElementException, ERROR_NOT_FOUND,
				"Key not found in headers - %s", key.c_str());
		}

		value = headerIter->second.first;
	}
	CAF_CM_EXIT;

	return value;
}

std::string CIntMessage::findOptionalHeaderAsString(
	const std::string& key) const {
	std::string valueStr;
	SmartPtrIVariant value = findOptionalHeader(key);
	if (value) {
		valueStr = value->toString();
	}
	return valueStr;
}

std::string CIntMessage::findRequiredHeaderAsString(
	const std::string& key) const {
	std::string valueStr;
	SmartPtrIVariant value = findRequiredHeader(key);
	if (value) {
		valueStr = value->toString();
	}
	return valueStr;
}

SmartPtrICafObject CIntMessage::findOptionalObjectHeader(
	const std::string& key) const {
	CAF_CM_FUNCNAME_VALIDATE("findOptionalObjectHeader");

	SmartPtrICafObject value;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(key);

		CHeaders::const_iterator headerIter = _headers->find(key);
		if (headerIter != _headers->end()) {
			value = headerIter->second.second;
		}
	}
	CAF_CM_EXIT;

	return value;
}

SmartPtrICafObject CIntMessage::findRequiredObjectHeader(
	const std::string& key) const {
	CAF_CM_FUNCNAME("findRequiredObjectHeader");

	SmartPtrICafObject value;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(key);

		CHeaders::const_iterator headerIter = _headers->find(key);
		if (headerIter == _headers->end()) {
			CAF_CM_EXCEPTIONEX_VA1(NoSuchElementException, ERROR_NOT_FOUND,
				"Key not found in headers - %s", key.c_str());
		}

		value = headerIter->second.second;
	}
	CAF_CM_EXIT;

	return value;
}
