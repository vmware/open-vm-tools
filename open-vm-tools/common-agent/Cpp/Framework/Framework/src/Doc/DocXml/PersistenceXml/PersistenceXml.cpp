/*
 *  Author: bwilliams
 *  Created: Nov 16, 2015
 *
 *  Copyright (c) 2015 Vmware, Inc.  All rights reserved.
 *  -- VMware Confidential
 *
 */

#include "stdafx.h"
#include "PersistenceXml.h"

using namespace Caf;

void PersistenceXml::add(
	const SmartPtrCPersistenceDoc persistenceDoc,
	const SmartPtrCXmlElement thisXml) {
	CAF_CM_STATIC_FUNC_VALIDATE("PersistenceXml", "add");
	CAF_CM_VALIDATE_SMARTPTR(persistenceDoc);
	CAF_CM_VALIDATE_SMARTPTR(thisXml);

	const SmartPtrCLocalSecurityDoc localSecurityVal =
			persistenceDoc->getLocalSecurity();
	if (! localSecurityVal.IsNull()) {
		const SmartPtrCXmlElement localSecurityXml =
			thisXml->createAndAddElement("localSecurity");
		LocalSecurityXml::add(localSecurityVal, localSecurityXml);
	}

	const SmartPtrCRemoteSecurityCollectionDoc remoteSecurityCollectionVal =
			persistenceDoc->getRemoteSecurityCollection();
	if (! remoteSecurityCollectionVal.IsNull()) {
		const SmartPtrCXmlElement remoteSecurityCollectionXml =
			thisXml->createAndAddElement("remoteSecurityCollection");
		RemoteSecurityCollectionXml::add(remoteSecurityCollectionVal, remoteSecurityCollectionXml);
	}

	const SmartPtrCPersistenceProtocolDoc persistenceProtocolVal =
		persistenceDoc->getPersistenceProtocol();
	if (! persistenceProtocolVal.IsNull()) {
		const SmartPtrCXmlElement persistenceProtocolXml =
			thisXml->createAndAddElement("persistenceProtocol");
		PersistenceProtocolXml::add(persistenceProtocolVal, persistenceProtocolXml);
	}

	const std::string versionVal = persistenceDoc->getVersion().empty() ? "1.0" : persistenceDoc->getVersion();
	thisXml->addAttribute("version", versionVal);
}

SmartPtrCPersistenceDoc PersistenceXml::parse(
	const SmartPtrCXmlElement thisXml) {
	CAF_CM_STATIC_FUNC_VALIDATE("PersistenceXml", "parse");
	CAF_CM_VALIDATE_SMARTPTR(thisXml);

	const SmartPtrCXmlElement localSecurityXml =
		thisXml->findOptionalChild("localSecurity");
	SmartPtrCLocalSecurityDoc localSecurityVal;
	if (! localSecurityXml.IsNull()) {
		localSecurityVal = LocalSecurityXml::parse(localSecurityXml);
	}

	const SmartPtrCXmlElement remoteSecurityCollectionXml =
		thisXml->findOptionalChild("remoteSecurityCollection");
	SmartPtrCRemoteSecurityCollectionDoc remoteSecurityCollectionVal;
	if (! remoteSecurityCollectionXml.IsNull()) {
		remoteSecurityCollectionVal = RemoteSecurityCollectionXml::parse(remoteSecurityCollectionXml);
	}

	const SmartPtrCXmlElement persistenceProtocolXml =
		thisXml->findOptionalChild("persistenceProtocol");
	SmartPtrCPersistenceProtocolDoc persistenceProtocolVal;
	if (! persistenceProtocolXml.IsNull()) {
		persistenceProtocolVal = PersistenceProtocolXml::parse(persistenceProtocolXml);
	}

	const std::string versionVal =
		thisXml->findOptionalAttribute("version");

	SmartPtrCPersistenceDoc persistenceDoc;
	persistenceDoc.CreateInstance();
	persistenceDoc->initialize(
		localSecurityVal,
		remoteSecurityCollectionVal,
		persistenceProtocolVal,
		versionVal);

	return persistenceDoc;
}

