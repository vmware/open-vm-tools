/*
 *  Author: bwilliams
 *  Created: July 3, 2015
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#include "stdafx.h"

#include "Doc/DocXml/CafCoreTypesXml/AttachmentCollectionXml.h"
#include "Doc/DocXml/CafCoreTypesXml/PropertyCollectionXml.h"
#include "Doc/DocXml/CafCoreTypesXml/ProtocolCollectionXml.h"

#include "Doc/CafCoreTypesDoc/CAttachmentCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CPropertyCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CProtocolCollectionDoc.h"
#include "Doc/PayloadEnvelopeDoc/CPayloadEnvelopeDoc.h"
#include "Xml/XmlUtils/CXmlElement.h"
#include "Doc/DocXml/PayloadEnvelopeXml/PayloadEnvelopeXml.h"

using namespace Caf;

void PayloadEnvelopeXml::add(
	const SmartPtrCPayloadEnvelopeDoc payloadEnvelopeDoc,
	const SmartPtrCXmlElement thisXml) {
	CAF_CM_STATIC_FUNC_VALIDATE("PayloadEnvelopeXml", "add");
	CAF_CM_VALIDATE_SMARTPTR(payloadEnvelopeDoc);
	CAF_CM_VALIDATE_SMARTPTR(thisXml);

	const std::string clientIdVal =
		BasePlatform::UuidToString(payloadEnvelopeDoc->getClientId());
	if (! clientIdVal.empty()) {
		thisXml->addAttribute("clientId", clientIdVal);
	}

	const std::string requestIdVal =
		BasePlatform::UuidToString(payloadEnvelopeDoc->getRequestId());
	if (! requestIdVal.empty()) {
		thisXml->addAttribute("requestId", requestIdVal);
	}

	const std::string pmeIdVal = payloadEnvelopeDoc->getPmeId();
	if (! pmeIdVal.empty()) {
		thisXml->addAttribute("pmeId", pmeIdVal);
	}

	const std::string payloadVersionVal = payloadEnvelopeDoc->getPayloadVersion();
	if (! payloadVersionVal.empty()) {
		thisXml->addAttribute("payloadVersion", payloadVersionVal);
	}

	const std::string payloadTypeVal = payloadEnvelopeDoc->getPayloadType();
	if (! payloadTypeVal.empty()) {
		thisXml->addAttribute("payloadType", payloadTypeVal);
	}

	const SmartPtrCAttachmentCollectionDoc attachmentCollectionVal =
			payloadEnvelopeDoc->getAttachmentCollection();
	if (! attachmentCollectionVal.IsNull()) {
		const SmartPtrCXmlElement attachmentCollectionXml =
			thisXml->createAndAddElement("attachmentCollection");
		AttachmentCollectionXml::add(attachmentCollectionVal, attachmentCollectionXml);
	}

	const SmartPtrCProtocolCollectionDoc protocolCollectionVal =
			payloadEnvelopeDoc->getProtocolCollection();
	if (! protocolCollectionVal.IsNull()) {
		const SmartPtrCXmlElement protocolCollectionXml =
			thisXml->createAndAddElement("protocolCollection");
		ProtocolCollectionXml::add(protocolCollectionVal, protocolCollectionXml);
	}

	const SmartPtrCPropertyCollectionDoc headerCollectionVal =
		payloadEnvelopeDoc->getHeaderCollection();
	if (! headerCollectionVal.IsNull()) {
		const SmartPtrCXmlElement headerCollectionXml =
			thisXml->createAndAddElement("headerCollection");
		PropertyCollectionXml::add(headerCollectionVal, headerCollectionXml);
	}

	const std::string versionVal = payloadEnvelopeDoc->getVersion().empty() ? "1.0" : payloadEnvelopeDoc->getVersion();
	thisXml->addAttribute("version", versionVal);
}

SmartPtrCPayloadEnvelopeDoc PayloadEnvelopeXml::parse(
	const SmartPtrCXmlElement thisXml) {
	CAF_CM_STATIC_FUNC_VALIDATE("PayloadEnvelopeXml", "parse");
	CAF_CM_VALIDATE_SMARTPTR(thisXml);

	const std::string clientIdStrVal =
		thisXml->findOptionalAttribute("clientId");
	UUID clientIdVal = CAFCOMMON_GUID_NULL;
	if (! clientIdStrVal.empty()) {
		BasePlatform::UuidFromString(clientIdStrVal.c_str(), clientIdVal);
	}

	const std::string requestIdStrVal =
		thisXml->findOptionalAttribute("requestId");
	UUID requestIdVal = CAFCOMMON_GUID_NULL;
	if (! requestIdStrVal.empty()) {
		BasePlatform::UuidFromString(requestIdStrVal.c_str(), requestIdVal);
	}

	const std::string pmeIdVal =
		thisXml->findOptionalAttribute("pmeId");

	const std::string payloadTypeVal =
		thisXml->findOptionalAttribute("payloadType");

	const std::string payloadVersionVal =
		thisXml->findOptionalAttribute("payloadVersion");

	const SmartPtrCXmlElement attachmentCollectionXml =
		thisXml->findOptionalChild("attachmentCollection");
	SmartPtrCAttachmentCollectionDoc attachmentCollectionVal;
	if (! attachmentCollectionXml.IsNull()) {
		attachmentCollectionVal = AttachmentCollectionXml::parse(attachmentCollectionXml);
	}

	const SmartPtrCXmlElement protocolCollectionXml =
		thisXml->findOptionalChild("protocolCollection");

	SmartPtrCProtocolCollectionDoc protocolCollectionVal;
	if (! protocolCollectionXml.IsNull()) {
		protocolCollectionVal = ProtocolCollectionXml::parse(protocolCollectionXml);
	}

	const SmartPtrCXmlElement headerCollectionXml =
		thisXml->findOptionalChild("headerCollection");
	SmartPtrCPropertyCollectionDoc headerCollectionVal;
	if (! headerCollectionXml.IsNull()) {
		headerCollectionVal = PropertyCollectionXml::parse(headerCollectionXml);
	}

	const std::string versionVal =
		thisXml->findOptionalAttribute("version");

	SmartPtrCPayloadEnvelopeDoc payloadEnvelopeDoc;
	payloadEnvelopeDoc.CreateInstance();
	payloadEnvelopeDoc->initialize(
		clientIdVal,
		requestIdVal,
		pmeIdVal,
		payloadTypeVal,
		payloadVersionVal,
		attachmentCollectionVal,
		protocolCollectionVal,
		headerCollectionVal,
		versionVal);

	return payloadEnvelopeDoc;
}

