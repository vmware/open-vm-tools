/*
 *  Author: bwilliams
 *  Created: Nov 16, 2015
 *
 *  Copyright (C) 2015-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#include "stdafx.h"

#include "Doc/PersistenceDoc/CCertCollectionDoc.h"
#include "Xml/XmlUtils/CXmlElement.h"
#include "Doc/DocXml/PersistenceXml/CertCollectionXml.h"

using namespace Caf;

void CertCollectionXml::add(
	const SmartPtrCCertCollectionDoc certCollectionDoc,
	const SmartPtrCXmlElement thisXml) {
	CAF_CM_STATIC_FUNC_VALIDATE("CertCollectionXml", "add");
	CAF_CM_VALIDATE_SMARTPTR(certCollectionDoc);
	CAF_CM_VALIDATE_SMARTPTR(thisXml);

	const std::deque<std::string> certVal =
		certCollectionDoc->getCert();

	if (! certVal.empty()) {
		for (TConstIterator<std::deque<std::string> > certIter(certVal);
			certIter; certIter++) {
			const SmartPtrCXmlElement certXml =
				thisXml->createAndAddElement("cert");
			certXml->setValue(*certIter);
		}
	}
}

SmartPtrCCertCollectionDoc CertCollectionXml::parse(
	const SmartPtrCXmlElement thisXml) {
	CAF_CM_STATIC_FUNC_VALIDATE("CertCollectionXml", "parse");
	CAF_CM_VALIDATE_SMARTPTR(thisXml);

	const CXmlElement::SmartPtrCElementCollection certChildrenXml =
		thisXml->findOptionalChildren("cert");

	std::deque<std::string> certVal;
	if (! certChildrenXml.IsNull() && ! certChildrenXml->empty()) {
		for (TConstIterator<CXmlElement::CElementCollection> certXmlIter(*certChildrenXml);
			certXmlIter; certXmlIter++) {
			const SmartPtrCXmlElement certXml = certXmlIter->second;
			const std::string certDoc = certXml->getValue();
			certVal.push_back(certDoc);
		}
	}

	SmartPtrCCertCollectionDoc certCollectionDoc;
	certCollectionDoc.CreateInstance();
	certCollectionDoc->initialize(certVal);

	return certCollectionDoc;
}

