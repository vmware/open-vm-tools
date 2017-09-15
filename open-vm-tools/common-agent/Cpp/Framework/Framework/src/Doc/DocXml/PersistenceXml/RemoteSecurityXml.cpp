/*
 *  Author: bwilliams
 *  Created: Nov 16, 2015
 *
 *  Copyright (C) 2015-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#include "stdafx.h"

#include "Doc/DocXml/PersistenceXml/CertCollectionXml.h"
#include "Doc/DocXml/PersistenceXml/CertPathCollectionXml.h"

#include "Doc/PersistenceDoc/CCertCollectionDoc.h"
#include "Doc/PersistenceDoc/CCertPathCollectionDoc.h"
#include "Doc/PersistenceDoc/CRemoteSecurityDoc.h"
#include "Xml/XmlUtils/CXmlElement.h"
#include "Doc/DocXml/PersistenceXml/RemoteSecurityXml.h"

using namespace Caf;

void RemoteSecurityXml::add(
		const SmartPtrCRemoteSecurityDoc remoteSecurityDoc,
		const SmartPtrCXmlElement thisXml) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("RemoteSecurityXml", "add");
	CAF_CM_VALIDATE_SMARTPTR(remoteSecurityDoc);
	CAF_CM_VALIDATE_SMARTPTR(thisXml);

	const std::string remoteIdVal = remoteSecurityDoc->getRemoteId();
	if (! remoteIdVal.empty()) {
		thisXml->addAttribute("remoteId", remoteIdVal);
	}

	const std::string protocolNameVal = remoteSecurityDoc->getProtocolName();
	if (! protocolNameVal.empty()) {
		thisXml->addAttribute("protocolName", protocolNameVal);
	}

	const std::string cmsCertVal = remoteSecurityDoc->getCmsCert();
	if (! cmsCertVal.empty()) {
		const SmartPtrCXmlElement cmsCertXml = thisXml->createAndAddElement("cmsCert");
		cmsCertXml->setValue(cmsCertVal);
	}

	const std::string cmsCipherNameVal = remoteSecurityDoc->getCmsCipherName();
	if (! cmsCipherNameVal.empty()) {
		thisXml->addAttribute("cmsCipherName", cmsCipherNameVal);
	}

	const SmartPtrCCertCollectionDoc cmsCertCollectionVal =
			remoteSecurityDoc->getCmsCertCollection();
	if (! cmsCertCollectionVal.IsNull()) {
		const SmartPtrCXmlElement cmsCertCollectionXml =
			thisXml->createAndAddElement("cmsCertCollection");
		CertCollectionXml::add(cmsCertCollectionVal, cmsCertCollectionXml);
	}

	const std::string cmsCertPathVal = remoteSecurityDoc->getCmsCertPath();
	if (! cmsCertPathVal.empty()) {
		thisXml->addAttribute("cmsCertPath", cmsCertPathVal);
	}

	const SmartPtrCCertPathCollectionDoc cmsCertPathCollectionVal =
			remoteSecurityDoc->getCmsCertPathCollection();
	if (! cmsCertPathCollectionVal.IsNull()) {
		const SmartPtrCXmlElement cmsCertPathCollectionXml =
			thisXml->createAndAddElement("cmsCertPathCollection");
		CertPathCollectionXml::add(cmsCertPathCollectionVal, cmsCertPathCollectionXml);
	}
}

SmartPtrCRemoteSecurityDoc RemoteSecurityXml::parse(
		const SmartPtrCXmlElement thisXml) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("RemoteSecurityXml", "parse");
	CAF_CM_VALIDATE_SMARTPTR(thisXml);

	const std::string remoteIdVal =
		thisXml->findOptionalAttribute("remoteId");

	const std::string protocolNameVal =
		thisXml->findOptionalAttribute("protocolName");

	std::string cmsCertVal;
	const SmartPtrCXmlElement cmsCertXml = thisXml->findOptionalChild("cmsCert");
	if (cmsCertXml) {
		cmsCertVal = cmsCertXml->getValue();
	}

	const std::string cmsCipherNameVal =
		thisXml->findOptionalAttribute("cmsCipherName");

	const SmartPtrCXmlElement cmsCertCollectionXml =
		thisXml->findOptionalChild("cmsCertCollection");
	SmartPtrCCertCollectionDoc cmsCertCollectionVal;
	if (! cmsCertCollectionXml.IsNull()) {
		cmsCertCollectionVal = CertCollectionXml::parse(cmsCertCollectionXml);
	}

	const std::string cmsCertPathVal =
		thisXml->findOptionalAttribute("cmsCertPath");

	const SmartPtrCXmlElement cmsCertPathCollectionXml =
		thisXml->findOptionalChild("cmsCertPathCollection");
	SmartPtrCCertPathCollectionDoc cmsCertPathCollectionVal;
	if (! cmsCertPathCollectionXml.IsNull()) {
		cmsCertPathCollectionVal = CertPathCollectionXml::parse(cmsCertPathCollectionXml);
	}

	SmartPtrCRemoteSecurityDoc remoteSecurityDoc;
	remoteSecurityDoc.CreateInstance();
	remoteSecurityDoc->initialize(
			remoteIdVal,
			protocolNameVal,
			cmsCertVal,
			cmsCipherNameVal,
			cmsCertCollectionVal,
			cmsCertPathVal,
			cmsCertPathCollectionVal);

	return remoteSecurityDoc;
}

