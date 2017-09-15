/*
 *  Author: bwilliams
 *  Created: Nov 16, 2015
 *
 *  Copyright (C) 2015-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#include "stdafx.h"

#include "Doc/PersistenceDoc/CCertPathCollectionDoc.h"
#include "Xml/XmlUtils/CXmlElement.h"
#include "Doc/DocXml/PersistenceXml/CertPathCollectionXml.h"

using namespace Caf;

void CertPathCollectionXml::add(
	const SmartPtrCCertPathCollectionDoc certPathCollectionDoc,
	const SmartPtrCXmlElement thisXml) {
	CAF_CM_STATIC_FUNC_VALIDATE("CertPathCollectionXml", "add");
	CAF_CM_VALIDATE_SMARTPTR(certPathCollectionDoc);
	CAF_CM_VALIDATE_SMARTPTR(thisXml);

	const std::deque<std::string> certPathVal =
		certPathCollectionDoc->getCertPath();

	if (! certPathVal.empty()) {
		for (TConstIterator<std::deque<std::string> > certPathIter(certPathVal);
			certPathIter; certPathIter++) {
			const SmartPtrCXmlElement certPathXml =
				thisXml->createAndAddElement("certPath");
			certPathXml->setValue(*certPathIter);
		}
	}
}

SmartPtrCCertPathCollectionDoc CertPathCollectionXml::parse(
	const SmartPtrCXmlElement thisXml) {
	CAF_CM_STATIC_FUNC_VALIDATE("CertPathCollectionXml", "parse");
	CAF_CM_VALIDATE_SMARTPTR(thisXml);

	const CXmlElement::SmartPtrCElementCollection certPathChildrenXml =
		thisXml->findOptionalChildren("certPath");

	std::deque<std::string> certPathVal;
	if (! certPathChildrenXml.IsNull() && ! certPathChildrenXml->empty()) {
		for (TConstIterator<CXmlElement::CElementCollection> certPathXmlIter(*certPathChildrenXml);
			certPathXmlIter; certPathXmlIter++) {
			const SmartPtrCXmlElement certPathXml = certPathXmlIter->second;
			const std::string certPathDoc = certPathXml->getValue();
			certPathVal.push_back(certPathDoc);
		}
	}

	SmartPtrCCertPathCollectionDoc certPathCollectionDoc;
	certPathCollectionDoc.CreateInstance();
	certPathCollectionDoc->initialize(certPathVal);

	return certPathCollectionDoc;
}

