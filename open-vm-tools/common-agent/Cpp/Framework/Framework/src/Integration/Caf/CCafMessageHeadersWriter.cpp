/*
 *	 Author: bwilliams
 *  Created: Jul 2009
 *
 *	Copyright (C) 2009-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Integration/IIntMessage.h"
#include "Integration/Caf/CCafMessageHeadersWriter.h"
#include "Integration/Core/FileHeaders.h"
#include "Integration/Core/MessageHeaders.h"

using namespace Caf;

SmartPtrCCafMessageHeadersWriter CCafMessageHeadersWriter::create() {
	SmartPtrCCafMessageHeadersWriter rc;
	rc.CreateInstance();
	rc->initialize();

	return rc;
}

CCafMessageHeadersWriter::CCafMessageHeadersWriter(void) :
	_isInitialized(false),
	CAF_CM_INIT("CCafMessageHeadersWriter") {
}

CCafMessageHeadersWriter::~CCafMessageHeadersWriter(void) {
}

void CCafMessageHeadersWriter::initialize() {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);

	_headers.CreateInstance();
	_isInitialized = true;
}

IIntMessage::SmartPtrCHeaders CCafMessageHeadersWriter::getHeaders() const {
	CAF_CM_FUNCNAME_VALIDATE("getHeaders");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _headers;
}

void CCafMessageHeadersWriter::setPayloadType(
		const std::string& payloadType) {
	CAF_CM_FUNCNAME_VALIDATE("setPayloadType");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(payloadType);

	insertString("payloadType", payloadType);
}

void CCafMessageHeadersWriter::setVersion(
		const std::string& version) {
	CAF_CM_FUNCNAME_VALIDATE("setVersion");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(version);

	insertString("version", version);
}

void CCafMessageHeadersWriter::setPayloadVersion(
		const std::string& payloadVersion) {
	CAF_CM_FUNCNAME_VALIDATE("setPayloadVersion");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(payloadVersion);

	insertString("payloadVersion", payloadVersion);
}

void CCafMessageHeadersWriter::setClientId(
		const UUID& clientId) {
	CAF_CM_FUNCNAME_VALIDATE("setClientId");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	std::string clientIdStr = BasePlatform::UuidToString(clientId);
	insertString("clientId", clientIdStr);
}

void CCafMessageHeadersWriter::setClientId(
		const std::string& clientIdStr) {
	CAF_CM_FUNCNAME_VALIDATE("setClientId");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(clientIdStr);

	insertString("clientId", clientIdStr);
}

void CCafMessageHeadersWriter::setRequestId(
		const UUID& requestId) {
	CAF_CM_FUNCNAME_VALIDATE("setRequestId");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	std::string requestIdStr = BasePlatform::UuidToString(requestId);
	insertString("requestId", requestIdStr);
}

void CCafMessageHeadersWriter::setRequestId(
		const std::string& requestIdStr) {
	CAF_CM_FUNCNAME_VALIDATE("setRequestId");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(requestIdStr);

	insertString("requestId", requestIdStr);
}

void CCafMessageHeadersWriter::setPmeId(
		const std::string& pmeId) {
	CAF_CM_FUNCNAME_VALIDATE("setPmeId");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(pmeId);

	insertString("pmeId", pmeId);
}

void CCafMessageHeadersWriter::setSessionId(
		const UUID& sessionId) {
	CAF_CM_FUNCNAME_VALIDATE("setSessionId");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	std::string sessionIdStr = BasePlatform::UuidToString(sessionId);
	insertString("sessionId", sessionIdStr);
}

void CCafMessageHeadersWriter::setSessionId(
		const std::string& sessionIdStr) {
	CAF_CM_FUNCNAME_VALIDATE("setSessionId");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(sessionIdStr);

	insertString("sessionId", sessionIdStr);
}

void CCafMessageHeadersWriter::setRelDirectory(
		const std::string& relDirectory) {
	CAF_CM_FUNCNAME_VALIDATE("setRelDirectory");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(relDirectory);

	insertString("relDirectory", relDirectory);
}

void CCafMessageHeadersWriter::setRelFilename(
		const std::string& relFilename) {
	CAF_CM_FUNCNAME_VALIDATE("setRelFilename");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(relFilename);

	insertString(FileHeaders::_sFILENAME, relFilename);
}

void CCafMessageHeadersWriter::setProviderUri(
		const std::string& providerUri) {
	CAF_CM_FUNCNAME_VALIDATE("setProviderUri");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(providerUri);

	insertString("providerUri", providerUri);
}

void CCafMessageHeadersWriter::setIsThrowable(
		const bool& isThrowable) {
	CAF_CM_FUNCNAME_VALIDATE("setIsThrowable");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	insertBool(MessageHeaders::_sIS_THROWABLE, isThrowable);
}

void CCafMessageHeadersWriter::setIsMultiPart(
		const bool& isMultiPart) {
	CAF_CM_FUNCNAME_VALIDATE("setIsMultiPart");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	insertBool(MessageHeaders::_sMULTIPART, isMultiPart);
}

void CCafMessageHeadersWriter::setProtocol(
		const std::string& protocol) {
	CAF_CM_FUNCNAME_VALIDATE("setProtocol");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(protocol);

	insertString(MessageHeaders::_sPROTOCOL_TYPE, protocol);
}

void CCafMessageHeadersWriter::setProtocolAddress(
		const std::string& protocolAddress) {
	CAF_CM_FUNCNAME_VALIDATE("setProtocolAddress");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(protocolAddress);

	insertString(MessageHeaders::_sPROTOCOL_CONNSTR, protocolAddress);
}

void CCafMessageHeadersWriter::setFlowDirection(
		const std::string& flowDirection) {
	CAF_CM_FUNCNAME_VALIDATE("setFlowDirection");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(flowDirection);

	insertString("cafcomm.internal.msgflow", flowDirection);
}

void CCafMessageHeadersWriter::insertString(
	const std::string& key,
	const std::string& value) {
	CAF_CM_FUNCNAME_VALIDATE("insertString");
	CAF_CM_VALIDATE_STRING(key);
	CAF_CM_VALIDATE_STRING(value);
	_headers->insert(std::make_pair(key, std::make_pair(CVariant::createString(value), SmartPtrICafObject())));
}

void CCafMessageHeadersWriter::insertBool(
	const std::string& key,
	const bool& value) {
	CAF_CM_FUNCNAME_VALIDATE("insertBool");
	CAF_CM_VALIDATE_STRING(key);
	_headers->insert(std::make_pair(key, std::make_pair(CVariant::createBool(value), SmartPtrICafObject())));
}
