/*
 *	 Author: bwilliams
 *  Created: Jul 2009
 *
 *	Copyright (C) 2009-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Doc/CafCoreTypesDoc/CRequestHeaderDoc.h"
#include "Doc/ResponseDoc/CEventKeyCollectionDoc.h"
#include "Doc/ResponseDoc/CEventKeyDoc.h"
#include "Doc/ResponseDoc/CManifestDoc.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "Xml/XmlUtils/CXmlElement.h"
#include "Integration/Caf/CCafMessagePayload.h"
#include "Doc/DocXml/CafCoreTypesXml/RequestHeaderXml.h"
#include "Doc/DocXml/ResponseXml/ManifestXml.h"
#include "Doc/DocXml/ResponseXml/EventKeyCollectionXml.h"

using namespace Caf;

SmartPtrCCafMessagePayload CCafMessagePayload::create(
		const SmartPtrCDynamicByteArray& payload,
		const std::string& payloadType) {
	CAF_CM_STATIC_FUNC_VALIDATE("CCafMessagePayload", "create");
	CAF_CM_VALIDATE_SMARTPTR(payload);

	SmartPtrCCafMessagePayload rc;
	rc.CreateInstance();
	rc->initialize(payload, payloadType);

	return rc;
}

SmartPtrCCafMessagePayload CCafMessagePayload::createFromFile(
		const std::string& payloadFile,
		const std::string& payloadType) {
	CAF_CM_STATIC_FUNC_VALIDATE("CCafMessagePayload", "createFromFile");
	CAF_CM_VALIDATE_STRING(payloadFile);

	const SmartPtrCDynamicByteArray payload =
			FileSystemUtils::loadByteFile(payloadFile);

	return create(payload, payloadType);
}

SmartPtrCCafMessagePayload CCafMessagePayload::createFromStr(
		const std::string& payloadStr,
		const std::string& payloadType) {
	CAF_CM_STATIC_FUNC_VALIDATE("CCafMessagePayload", "createFromStr");
	CAF_CM_VALIDATE_STRING(payloadStr);

	SmartPtrCDynamicByteArray payload = createBufferFromStr(payloadStr);

	SmartPtrCCafMessagePayload rc;
	rc.CreateInstance();
	rc->initialize(payload, payloadType);

	return rc;
}

SmartPtrCDynamicByteArray CCafMessagePayload::createBufferFromStr(
		const std::string& payloadStr) {
	CAF_CM_STATIC_FUNC_VALIDATE("CCafMessagePayload", "createBufferFromStr");
	CAF_CM_VALIDATE_STRING(payloadStr);

	SmartPtrCDynamicByteArray rc;
	rc.CreateInstance();
	rc->allocateBytes(static_cast<uint32>(payloadStr.length()));
	rc->memCpy(payloadStr.c_str(), static_cast<uint32>(payloadStr.length()));

	return rc;
}

SmartPtrCDynamicByteArray CCafMessagePayload::createBufferFromFile(
		const std::string& payloadFile) {
	CAF_CM_STATIC_FUNC_VALIDATE("CCafMessagePayload", "createBufferFromFile");
	CAF_CM_VALIDATE_STRING(payloadFile);

	return FileSystemUtils::loadByteFile(payloadFile);
}

void CCafMessagePayload::saveToFile(
		const SmartPtrCDynamicByteArray& payload,
		const std::string& payloadPath) {
	CAF_CM_STATIC_FUNC_VALIDATE("CCafMessagePayload", "saveToFile");
	CAF_CM_VALIDATE_SMARTPTR(payload);
	CAF_CM_VALIDATE_STRING(payloadPath);

	FileSystemUtils::saveByteFile(payloadPath,
			payload->getPtr(), payload->getByteCount());
}

std::string CCafMessagePayload::saveToStr(
		const SmartPtrCDynamicByteArray& payload) {
	CAF_CM_STATIC_FUNC_VALIDATE("CCafMessagePayload", "saveToStr");
	CAF_CM_VALIDATE_SMARTPTR(payload);

	return reinterpret_cast<const char*>(payload->getPtr());
}

CCafMessagePayload::CCafMessagePayload(void) :
	_isInitialized(false),
	CAF_CM_INIT("CCafMessagePayload") {
}

CCafMessagePayload::~CCafMessagePayload(void) {
}

void CCafMessagePayload::initialize(
		const SmartPtrCDynamicByteArray& payload,
		const std::string& payloadType,
		const std::string& encoding) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(payload);
	CAF_CM_VALIDATE_STRING(encoding);
	CAF_CM_VALIDATE_BOOL(encoding.compare("xml") == 0);

	_payload = payload;
	_encoding = encoding;
	_payloadStr = saveToStr(payload);
	_payloadXml = CXmlUtils::parseString(_payloadStr, payloadType);

	_isInitialized = true;
}

std::string CCafMessagePayload::getPayloadStr() const {
	CAF_CM_FUNCNAME_VALIDATE("getPayloadStr");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _payloadStr;
}

SmartPtrCDynamicByteArray CCafMessagePayload::getPayload() const {
	CAF_CM_FUNCNAME_VALIDATE("getPayload");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _payload;
}

std::string CCafMessagePayload::getVersion() const {
	CAF_CM_FUNCNAME_VALIDATE("getVersion");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	SmartPtrCXmlElement headerXml = _payloadXml->findOptionalChild("requestHeader");
	if (headerXml.IsNull()) {
		headerXml = _payloadXml->findOptionalChild("responseHeader");
	}

	std::string rc;
	if (headerXml.IsNull()) {
		rc = _payloadXml->findRequiredAttribute("version");
	} else {
		rc = headerXml->findRequiredAttribute("version");
	}

	return rc;
}

SmartPtrCRequestHeaderDoc CCafMessagePayload::getRequestHeader() const {
	CAF_CM_FUNCNAME_VALIDATE("getRequestHeader");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	const SmartPtrCXmlElement requestHeaderXml =
			_payloadXml->findRequiredChild("requestHeader");
	return RequestHeaderXml::parse(requestHeaderXml);
}

SmartPtrCManifestDoc CCafMessagePayload::getManifest() const {
	CAF_CM_FUNCNAME_VALIDATE("getManifest");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	const SmartPtrCXmlElement manifestXml =
			_payloadXml->findRequiredChild("manifest");

	return ManifestXml::parse(manifestXml);
}

std::deque<SmartPtrCEventKeyDoc> CCafMessagePayload::getEventKeyCollection() const {
	CAF_CM_FUNCNAME_VALIDATE("getEventKeyCollection");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	std::deque<SmartPtrCEventKeyDoc> rc;

	SmartPtrCXmlElement eventKeyCollectionXml =
			_payloadXml->findOptionalChild("eventKeyCollection");
	if (eventKeyCollectionXml) {
		const SmartPtrCEventKeyCollectionDoc eventKeyCollection =
				EventKeyCollectionXml::parse(eventKeyCollectionXml);

		rc = eventKeyCollection->getEventKey();
	}

	return rc;
}
