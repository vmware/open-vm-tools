/*
 *  Author: bwilliams
 *  Created: Nov 16, 2015
 *
 *  Copyright (C) 2015-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#include "stdafx.h"

#include "Doc/DocXml/PersistenceXml/LocalSecurityXml.h"
#include "Doc/DocXml/PersistenceXml/PersistenceProtocolCollectionXml.h"
#include "Doc/DocXml/PersistenceXml/RemoteSecurityCollectionXml.h"

#include "Doc/PersistenceDoc/CLocalSecurityDoc.h"
#include "Doc/PersistenceDoc/CPersistenceDoc.h"
#include "Doc/PersistenceDoc/CPersistenceProtocolCollectionDoc.h"
#include "Doc/PersistenceDoc/CRemoteSecurityCollectionDoc.h"
#include "Xml/XmlUtils/CXmlElement.h"
#include "Doc/DocXml/PersistenceXml/PersistenceXml.h"

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

	const SmartPtrCPersistenceProtocolCollectionDoc persistenceProtocolCollectionVal =
			persistenceDoc->getPersistenceProtocolCollection();
	if (! persistenceProtocolCollectionVal.IsNull()) {
		const SmartPtrCXmlElement persistenceProtocolCollectionXml =
			thisXml->createAndAddElement("persistenceProtocolCollection");
		PersistenceProtocolCollectionXml::add(persistenceProtocolCollectionVal, persistenceProtocolCollectionXml);
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

	const SmartPtrCXmlElement persistenceProtocolCollectionXml =
		thisXml->findOptionalChild("persistenceProtocolCollection");
	SmartPtrCPersistenceProtocolCollectionDoc persistenceProtocolCollectionVal;
	if (! persistenceProtocolCollectionXml.IsNull()) {
		persistenceProtocolCollectionVal = PersistenceProtocolCollectionXml::parse(persistenceProtocolCollectionXml);
	}

	const std::string versionVal =
		thisXml->findOptionalAttribute("version");

	SmartPtrCPersistenceDoc persistenceDoc;
	persistenceDoc.CreateInstance();
	persistenceDoc->initialize(
		localSecurityVal,
		remoteSecurityCollectionVal,
		persistenceProtocolCollectionVal,
		versionVal);

	return persistenceDoc;
}

