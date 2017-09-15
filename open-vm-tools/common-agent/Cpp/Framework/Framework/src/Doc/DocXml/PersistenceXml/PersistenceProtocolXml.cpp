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
#include "Doc/PersistenceDoc/CPersistenceProtocolDoc.h"
#include "Xml/XmlUtils/CXmlElement.h"
#include "Doc/DocXml/PersistenceXml/PersistenceProtocolXml.h"

using namespace Caf;

void PersistenceProtocolXml::add(
	const SmartPtrCPersistenceProtocolDoc persistenceProtocolDoc,
	const SmartPtrCXmlElement thisXml) {
	CAF_CM_STATIC_FUNC_VALIDATE("PersistenceProtocolXml", "add");
	CAF_CM_VALIDATE_SMARTPTR(persistenceProtocolDoc);
	CAF_CM_VALIDATE_SMARTPTR(thisXml);

	const std::string protocolNameVal = persistenceProtocolDoc->getProtocolName();
	if (! protocolNameVal.empty()) {
		thisXml->addAttribute("protocolName", protocolNameVal);
	}

	const std::string uriVal = persistenceProtocolDoc->getUri();
	if (! uriVal.empty()) {
		thisXml->addAttribute("uri", uriVal);
	}

	const std::string uriAmqpVal = persistenceProtocolDoc->getUriAmqp();
	if (! uriAmqpVal.empty()) {
		thisXml->addAttribute("uriAmqp", uriAmqpVal);
	}

	const std::string uriTunnelVal = persistenceProtocolDoc->getUriTunnel();
	if (! uriTunnelVal.empty()) {
		thisXml->addAttribute("uriTunnel", uriTunnelVal);
	}

	const std::string tlsCertVal = persistenceProtocolDoc->getTlsCert();
	if (! tlsCertVal.empty()) {
		const SmartPtrCXmlElement tlsCertXml = thisXml->createAndAddElement("tlsCert");
		tlsCertXml->setValue(tlsCertVal);
	}

	const std::string tlsProtocolVal = persistenceProtocolDoc->getTlsProtocol();
	if (! tlsProtocolVal.empty()) {
		thisXml->addAttribute("tlsProtocol", tlsProtocolVal);
	}

	const Cdeqstr tlsCipherCollectionVal = persistenceProtocolDoc->getTlsCipherCollection();
	if (! tlsCipherCollectionVal.empty()) {
		const SmartPtrCXmlElement tlsCipherCollectionXml =
				thisXml->createAndAddElement("tlsCipherCollection");
		for (TConstIterator<std::deque<std::string> > valueIter(tlsCipherCollectionVal);
			valueIter; valueIter++) {
			const SmartPtrCXmlElement valueXml =
					tlsCipherCollectionXml->createAndAddElement("cipher");
			valueXml->setValue(*valueIter);
		}
	}

	const SmartPtrCCertCollectionDoc tlsCertCollectionVal =
			persistenceProtocolDoc->getTlsCertCollection();
	if (! tlsCertCollectionVal.IsNull()) {
		const SmartPtrCXmlElement tlsCertCollectionXml =
			thisXml->createAndAddElement("tlsCertCollection");
		CertCollectionXml::add(tlsCertCollectionVal, tlsCertCollectionXml);
	}

	const std::string uriAmqpPathVal = persistenceProtocolDoc->getUriAmqpPath();
	if (! uriAmqpPathVal.empty()) {
		thisXml->addAttribute("uriAmqpPath", uriAmqpPathVal);
	}

	const std::string uriTunnelPathVal = persistenceProtocolDoc->getUriTunnelPath();
	if (! uriTunnelPathVal.empty()) {
		thisXml->addAttribute("uriTunnelPath", uriTunnelPathVal);
	}

	const std::string tlsCertPathVal = persistenceProtocolDoc->getTlsCertPath();
	if (! tlsCertPathVal.empty()) {
		thisXml->addAttribute("tlsCertPath", tlsCertPathVal);
	}

	const SmartPtrCCertPathCollectionDoc tlsCertPathCollectionVal =
			persistenceProtocolDoc->getTlsCertPathCollection();
	if (! tlsCertPathCollectionVal.IsNull()) {
		const SmartPtrCXmlElement tlsCertPathCollectionXml =
			thisXml->createAndAddElement("tlsCertPathCollection");
		CertPathCollectionXml::add(tlsCertPathCollectionVal, tlsCertPathCollectionXml);
	}
}

SmartPtrCPersistenceProtocolDoc PersistenceProtocolXml::parse(
	const SmartPtrCXmlElement thisXml) {
	CAF_CM_STATIC_FUNC_VALIDATE("PersistenceProtocolXml", "parse");
	CAF_CM_VALIDATE_SMARTPTR(thisXml);

	const std::string protocolNameVal =
		thisXml->findOptionalAttribute("protocolName");

	const std::string uriVal =
		thisXml->findOptionalAttribute("uri");

	const std::string uriAmqpVal =
		thisXml->findOptionalAttribute("uriAmqp");

	const std::string uriTunnelVal =
		thisXml->findOptionalAttribute("uriTunnel");

	std::string tlsCertVal;
	const SmartPtrCXmlElement tlsCertXml = thisXml->findOptionalChild("tlsCert");
	if (tlsCertXml) {
		tlsCertVal = tlsCertXml->getValue();
	}

	const std::string tlsProtocolVal =
		thisXml->findOptionalAttribute("tlsProtocol");

	const SmartPtrCXmlElement tlsCipherCollectionXml =
		thisXml->findOptionalChild("tlsCipherCollection");
	std::deque<std::string> tlsCipherCollectionVal;
	if (! tlsCipherCollectionXml.IsNull()) {
		const CXmlElement::SmartPtrCElementCollection valueCollectionXml =
				tlsCipherCollectionXml->findOptionalChildren("cipher");
		if (! valueCollectionXml.IsNull() && ! valueCollectionXml->empty()) {
			for (TConstIterator<CXmlElement::CElementCollection> valueXmlIter(*valueCollectionXml);
				valueXmlIter; valueXmlIter++) {
				const SmartPtrCXmlElement valueXml = valueXmlIter->second;
				const std::string valueDoc = valueXml->getValue();
				tlsCipherCollectionVal.push_back(valueDoc);
			}
		}
	}

	const SmartPtrCXmlElement tlsCertCollectionXml =
		thisXml->findOptionalChild("tlsCertCollection");
	SmartPtrCCertCollectionDoc tlsCertCollectionVal;
	if (! tlsCertCollectionXml.IsNull()) {
		tlsCertCollectionVal = CertCollectionXml::parse(tlsCertCollectionXml);
	}

	const std::string uriAmqpPathVal =
		thisXml->findOptionalAttribute("uriAmqpPath");

	const std::string uriTunnelPathVal =
		thisXml->findOptionalAttribute("uriTunnelPath");

	const std::string tlsCertPathVal =
		thisXml->findOptionalAttribute("tlsCertPath");

	const SmartPtrCXmlElement tlsCertPathCollectionXml =
		thisXml->findOptionalChild("tlsCertPathCollection");
	SmartPtrCCertPathCollectionDoc tlsCertPathCollectionVal;
	if (! tlsCertPathCollectionXml.IsNull()) {
		tlsCertPathCollectionVal = CertPathCollectionXml::parse(tlsCertPathCollectionXml);
	}

	SmartPtrCPersistenceProtocolDoc persistenceProtocolDoc;
	persistenceProtocolDoc.CreateInstance();
	persistenceProtocolDoc->initialize(
			protocolNameVal,
			uriVal,
			uriAmqpVal,
			uriTunnelVal,
			tlsCertVal,
			tlsProtocolVal,
			tlsCipherCollectionVal,
			tlsCertCollectionVal,
			uriAmqpPathVal,
			uriTunnelPathVal,
			tlsCertPathVal,
			tlsCertPathCollectionVal);

	return persistenceProtocolDoc;
}

