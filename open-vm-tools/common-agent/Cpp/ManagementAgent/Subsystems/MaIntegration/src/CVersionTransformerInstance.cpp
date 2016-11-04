/*
G *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Common/IAppContext.h"
#include "Doc/PayloadEnvelopeDoc/CPayloadEnvelopeDoc.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Exception/CCafException.h"
#include "CVersionTransformerInstance.h"
#include "Integration/Caf/CCafMessagePayloadParser.h"

using namespace Caf;

CVersionTransformerInstance::CVersionTransformerInstance() :
	_isInitialized(false),
	CAF_CM_INIT_LOG("CVersionTransformerInstance") {
}

CVersionTransformerInstance::~CVersionTransformerInstance() {
}

void CVersionTransformerInstance::initialize(
		const IBean::Cargs& ctorArgs,
		const IBean::Cprops& properties,
		const SmartPtrIDocument& configSection) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(configSection);

	_id = configSection->findRequiredAttribute("id");

	_isInitialized = true;
}

std::string CVersionTransformerInstance::getId() const {
	CAF_CM_FUNCNAME_VALIDATE("getId");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _id;
}

void CVersionTransformerInstance::wire(
		const SmartPtrIAppContext& appContext,
		const SmartPtrIChannelResolver& channelResolver) {
	CAF_CM_FUNCNAME_VALIDATE("wire");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(appContext);
	CAF_CM_VALIDATE_INTERFACE(channelResolver);
}

SmartPtrIIntMessage CVersionTransformerInstance::transformMessage(
		const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME_VALIDATE("transformMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(message);

	const SmartPtrCPayloadEnvelopeDoc payloadEnvelope =
			CCafMessagePayloadParser::getPayloadEnvelope(message->getPayload());

	SmartPtrIIntMessage rc = message;
	rc = transformEnvelope(payloadEnvelope, rc);
	rc = transformPayload(payloadEnvelope, rc);

	return rc;
}

SmartPtrIIntMessage CVersionTransformerInstance::transformEnvelope(
		const SmartPtrCPayloadEnvelopeDoc& payloadEnvelope,
		const SmartPtrIIntMessage& message) const {
	CAF_CM_FUNCNAME("transformEnvelope");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(payloadEnvelope);
	CAF_CM_VALIDATE_SMARTPTR(message);

	const std::string payloadType = payloadEnvelope->getPayloadType();
	const std::string envelopeVersion = payloadEnvelope->getVersion();

	std::string receivedMajorVersion;
	std::string receivedMinorVersion;
	parseVersion(payloadType, envelopeVersion, receivedMajorVersion, receivedMinorVersion);

	// Throwing an unsupported version exception is the last resort.
	// If at all possible, transform the old version document into the
	// new version. For example, if this is a v1.0 envelope, transform
	// it into v1.1 and return it, otherwise throw an unsupported version
	// exception.
	const std::string expectedMajorVersion = "1";
	const std::string expectedMinorVersion = "0";
	if (receivedMajorVersion.compare(expectedMajorVersion) != 0) {
		// Major version incompatibilities are not supported
		CAF_CM_EXCEPTIONEX_VA5(UnsupportedVersionException, ERROR_NOT_SUPPORTED,
				"Unsupported envelope major version - payloadType: %s, received: %s.%s, expected: %s.%s",
				payloadType.c_str(),
				receivedMajorVersion.c_str(), receivedMinorVersion.c_str(),
				expectedMajorVersion.c_str(), expectedMinorVersion.c_str());
	}
	if (receivedMinorVersion.compare(expectedMinorVersion) != 0) {
		// Minor version incompatibilities are not supported
		CAF_CM_EXCEPTIONEX_VA5(UnsupportedVersionException, ERROR_NOT_SUPPORTED,
				"Unsupported envelope minor version - payloadType: %s, received: %s.%s, expected: %s.%s",
				payloadType.c_str(),
				receivedMajorVersion.c_str(), receivedMinorVersion.c_str(),
				expectedMajorVersion.c_str(), expectedMinorVersion.c_str());
	}

	return message;
}

SmartPtrIIntMessage CVersionTransformerInstance::transformPayload(
		const SmartPtrCPayloadEnvelopeDoc& payloadEnvelope,
		const SmartPtrIIntMessage& message) const {
	CAF_CM_FUNCNAME("transformPayload");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(payloadEnvelope);
	CAF_CM_VALIDATE_SMARTPTR(message);

	const std::string payloadType = payloadEnvelope->getPayloadType();
	const std::string payloadVersion = payloadEnvelope->getPayloadVersion();

	std::string receivedMajorVersion;
	std::string receivedMinorVersion;
	parseVersion(payloadType, payloadVersion, receivedMajorVersion, receivedMinorVersion);

	// Throwing an unsupported version exception is the last resort.
	// If at all possible, transform the old version document into the
	// new version. For example, if this is a v1.0 mgmtRequest, transform
	// it into v1.1 and return it, otherwise throw an unsupported version
	// exception. The problem is that the payload is probably in an attachment
	// that's been signed and encrypted, so it isn't available to be transformed.
	// To handle this case, we'll probably do the enforcement here and the
	// actual transformation later in the process when the payload has been
	// verified and decrypted.
	const std::string expectedMajorVersion = "1";
	const std::string expectedMinorVersion = "0";
	if (receivedMajorVersion.compare(expectedMajorVersion) != 0) {
		// Major version incompatibilities are not supported
		CAF_CM_EXCEPTIONEX_VA5(UnsupportedVersionException, ERROR_NOT_SUPPORTED,
				"Unsupported payload major version - payloadType: %s, received: %s.%s, expected: %s.%s",
				payloadType.c_str(),
				receivedMajorVersion.c_str(), receivedMinorVersion.c_str(),
				expectedMajorVersion.c_str(), expectedMinorVersion.c_str());
	}
	if (receivedMinorVersion.compare(expectedMinorVersion) != 0) {
		// Minor version incompatibilities are not supported
		CAF_CM_EXCEPTIONEX_VA5(UnsupportedVersionException, ERROR_NOT_SUPPORTED,
				"Unsupported payload minor version - payloadType: %s, received: %s.%s, expected: %s.%s",
				payloadType.c_str(),
				receivedMajorVersion.c_str(), receivedMinorVersion.c_str(),
				expectedMajorVersion.c_str(), expectedMinorVersion.c_str());
	}

	return message;
}

void CVersionTransformerInstance::parseVersion(
		const std::string& messageType,
		const std::string& version,
		std::string& majorVersion,
		std::string& minorVersion) const {
	CAF_CM_FUNCNAME("parseVersion");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(messageType);
	CAF_CM_VALIDATE_STRING(version);

	const Cdeqstr versionDeq = CStringUtils::split(version, '.');
	if (versionDeq.size() < 2) {
		CAF_CM_EXCEPTION_VA2(ERROR_INVALID_DATA,
				"Invalid version format - messageType: %s, version: %s",
				messageType.c_str(), version.c_str());
	}

	majorVersion = versionDeq[0];
	minorVersion = versionDeq[1];
}
