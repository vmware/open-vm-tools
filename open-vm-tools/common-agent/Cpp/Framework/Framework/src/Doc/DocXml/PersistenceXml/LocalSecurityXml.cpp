/*
 *  Author: bwilliams
 *  Created: Nov 16, 2015
 *
 *  Copyright (C) 2015-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#include "stdafx.h"

#include "Doc/PersistenceDoc/CLocalSecurityDoc.h"
#include "Xml/XmlUtils/CXmlElement.h"
#include "Doc/DocXml/PersistenceXml/LocalSecurityXml.h"

using namespace Caf;

void LocalSecurityXml::add(
	const SmartPtrCLocalSecurityDoc localSecurityDoc,
	const SmartPtrCXmlElement thisXml) {
	CAF_CM_STATIC_FUNC_VALIDATE("LocalSecurityXml", "add");
	CAF_CM_VALIDATE_SMARTPTR(localSecurityDoc);
	CAF_CM_VALIDATE_SMARTPTR(thisXml);

	const std::string localIdVal = localSecurityDoc->getLocalId();
	if (! localIdVal.empty()) {
		thisXml->addAttribute("localId", localIdVal);
	}

	const std::string privateKeyVal = localSecurityDoc->getPrivateKey();
	if (! privateKeyVal.empty()) {
		const SmartPtrCXmlElement privateKeyXml = thisXml->createAndAddElement("privateKey");
		privateKeyXml->setValue(privateKeyVal);
	}

	const std::string certVal = localSecurityDoc->getCert();
	if (! certVal.empty()) {
		const SmartPtrCXmlElement certXml = thisXml->createAndAddElement("cert");
		certXml->setValue(certVal);
	}

	const std::string privateKeyPathVal = localSecurityDoc->getPrivateKeyPath();
	if (! privateKeyPathVal.empty()) {
		thisXml->addAttribute("privateKeyPath", privateKeyPathVal);
	}

	const std::string certPathVal = localSecurityDoc->getCertPath();
	if (! certPathVal.empty()) {
		thisXml->addAttribute("certPath", certPathVal);
	}
}

SmartPtrCLocalSecurityDoc LocalSecurityXml::parse(
	const SmartPtrCXmlElement thisXml) {
	CAF_CM_STATIC_FUNC_VALIDATE("LocalSecurityXml", "parse");
	CAF_CM_VALIDATE_SMARTPTR(thisXml);

	const std::string localIdVal = thisXml->findOptionalAttribute("localId");

	std::string privateKeyVal;
	const SmartPtrCXmlElement privateKeyXml = thisXml->findOptionalChild("privateKey");
	if (privateKeyXml) {
		privateKeyVal = privateKeyXml->getValue();
	}

	std::string certVal;
	const SmartPtrCXmlElement certXml = thisXml->findOptionalChild("cert");
	if (certXml) {
		certVal = certXml->getValue();
	}

	const std::string privateKeyPathVal = thisXml->findOptionalAttribute("privateKeyPath");
	const std::string certPathVal = thisXml->findOptionalAttribute("certPath");

	SmartPtrCLocalSecurityDoc localSecurityDoc;
	localSecurityDoc.CreateInstance();
	localSecurityDoc->initialize(
			localIdVal,
			privateKeyVal,
			certVal,
			privateKeyPathVal,
			certPathVal);

	return localSecurityDoc;
}

