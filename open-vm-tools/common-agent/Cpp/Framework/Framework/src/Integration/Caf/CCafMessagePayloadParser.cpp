/*
 *	 Author: bwilliams
 *  Created: Jul 2009
 *
 *	Copyright (c) 2009-2011 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#include "stdafx.h"
#include "CCafMessagePayloadParser.h"

using namespace Caf;

SmartPtrCPayloadEnvelopeDoc CCafMessagePayloadParser::getPayloadEnvelope(
		const SmartPtrCDynamicByteArray& payload) {
	CAF_CM_STATIC_FUNC_VALIDATE("CCafMessagePayloadParser", "getPayloadEnvelope");
	CAF_CM_VALIDATE_SMARTPTR(payload);

	return PayloadEnvelopeXml::parse(bufferToXml(payload, "caf:payloadEnvelope"));
}

SmartPtrCInstallProviderJobDoc CCafMessagePayloadParser::getInstallProviderJob(
		const SmartPtrCDynamicByteArray& payload) {
	CAF_CM_STATIC_FUNC_VALIDATE("CCafMessagePayloadParser", "getInstallProviderJob");
	CAF_CM_VALIDATE_SMARTPTR(payload);

	return InstallProviderJobXml::parse(bufferToXml(payload, "caf:cafInstallProviderJob"));
}

SmartPtrCUninstallProviderJobDoc CCafMessagePayloadParser::getUninstallProviderJob(
		const SmartPtrCDynamicByteArray& payload) {
	CAF_CM_STATIC_FUNC_VALIDATE("CCafMessagePayloadParser", "getUninstallProviderJob");
	CAF_CM_VALIDATE_SMARTPTR(payload);

	return UninstallProviderJobXml::parse(bufferToXml(payload, "caf:cafUninstallProviderJob"));
}

SmartPtrCProviderRequestDoc CCafMessagePayloadParser::getProviderRequest(
		const SmartPtrCDynamicByteArray& payload) {
	CAF_CM_STATIC_FUNC_VALIDATE("CCafMessagePayloadParser", "getProviderRequest");
	CAF_CM_VALIDATE_SMARTPTR(payload);

	return XmlRoots::parseProviderRequestFromString(bufferToStr(payload));
}

SmartPtrCProviderRegDoc CCafMessagePayloadParser::getProviderReg(
		const SmartPtrCDynamicByteArray& payload) {
	CAF_CM_STATIC_FUNC_VALIDATE("CCafMessagePayloadParser", "getProviderReg");
	CAF_CM_VALIDATE_SMARTPTR(payload);

	return XmlRoots::parseProviderRegFromString(bufferToStr(payload));
}

SmartPtrCInstallRequestDoc CCafMessagePayloadParser::getInstallRequest(
		const SmartPtrCDynamicByteArray& payload) {
	CAF_CM_STATIC_FUNC_VALIDATE("CCafMessagePayloadParser", "getInstallRequest");
	CAF_CM_VALIDATE_SMARTPTR(payload);

	return XmlRoots::parseInstallRequestFromString(bufferToStr(payload));
}

SmartPtrCMgmtRequestDoc CCafMessagePayloadParser::getMgmtRequest(
		const SmartPtrCDynamicByteArray& payload) {
	CAF_CM_STATIC_FUNC_VALIDATE("CCafMessagePayloadParser", "getMgmtRequest");
	CAF_CM_VALIDATE_SMARTPTR(payload);

	return XmlRoots::parseMgmtRequestFromString(bufferToStr(payload));
}

SmartPtrCXmlElement CCafMessagePayloadParser::bufferToXml(
		const SmartPtrCDynamicByteArray& payload,
		const std::string& payloadType) {
	CAF_CM_STATIC_FUNC_VALIDATE("CCafMessagePayloadParser", "bufferToXml");
	CAF_CM_VALIDATE_SMARTPTR(payload);

	return CXmlUtils::parseString(bufferToStr(payload), payloadType);
}

std::string CCafMessagePayloadParser::bufferToStr(
		const SmartPtrCDynamicByteArray& payload) {
	CAF_CM_STATIC_FUNC_VALIDATE("CCafMessagePayloadParser", "bufferToStr");
	CAF_CM_VALIDATE_SMARTPTR(payload);

	return reinterpret_cast<const char*>(payload->getPtr());
}
