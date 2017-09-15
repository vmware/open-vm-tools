/*
 *  Author: bwilliams
 *  Created: Nov 16, 2015
 *
 *  Copyright (C) 2015-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#include "stdafx.h"

#include "Doc/DocXml/PersistenceXml/PersistenceProtocolXml.h"

#include "Doc/PersistenceDoc/CPersistenceProtocolCollectionDoc.h"
#include "Doc/PersistenceDoc/CPersistenceProtocolDoc.h"
#include "Xml/XmlUtils/CXmlElement.h"
#include "Doc/DocXml/PersistenceXml/PersistenceProtocolCollectionXml.h"

using namespace Caf;

void PersistenceProtocolCollectionXml::add(
	const SmartPtrCPersistenceProtocolCollectionDoc persistenceProtocolCollectionDoc,
	const SmartPtrCXmlElement thisXml) {
	CAF_CM_STATIC_FUNC_VALIDATE("PersistenceProtocolCollectionXml", "add");
	CAF_CM_VALIDATE_SMARTPTR(persistenceProtocolCollectionDoc);
	CAF_CM_VALIDATE_SMARTPTR(thisXml);

	const std::deque<SmartPtrCPersistenceProtocolDoc> persistenceProtocolVal =
		persistenceProtocolCollectionDoc->getPersistenceProtocol();
	if (! persistenceProtocolVal.empty()) {
		for (TConstIterator<std::deque<SmartPtrCPersistenceProtocolDoc> > persistenceProtocolIter(persistenceProtocolVal);
			persistenceProtocolIter; persistenceProtocolIter++) {
			const SmartPtrCXmlElement persistenceProtocolXml =
				thisXml->createAndAddElement("persistenceProtocol");
			PersistenceProtocolXml::add(*persistenceProtocolIter, persistenceProtocolXml);
		}
	}
}

SmartPtrCPersistenceProtocolCollectionDoc PersistenceProtocolCollectionXml::parse(
	const SmartPtrCXmlElement thisXml) {
	CAF_CM_STATIC_FUNC_VALIDATE("PersistenceProtocolCollectionXml", "parse");
	CAF_CM_VALIDATE_SMARTPTR(thisXml);

	const CXmlElement::SmartPtrCElementCollection persistenceProtocolChildrenXml =
		thisXml->findOptionalChildren("persistenceProtocol");

	std::deque<SmartPtrCPersistenceProtocolDoc> persistenceProtocolVal;
	if (! persistenceProtocolChildrenXml.IsNull() && ! persistenceProtocolChildrenXml->empty()) {
		for (TConstIterator<CXmlElement::CElementCollection> persistenceProtocolXmlIter(*persistenceProtocolChildrenXml);
			persistenceProtocolXmlIter; persistenceProtocolXmlIter++) {
			const SmartPtrCXmlElement persistenceProtocolXml = persistenceProtocolXmlIter->second;
			const SmartPtrCPersistenceProtocolDoc persistenceProtocolDoc =
				PersistenceProtocolXml::parse(persistenceProtocolXml);
			persistenceProtocolVal.push_back(persistenceProtocolDoc);
		}
	}

	SmartPtrCPersistenceProtocolCollectionDoc persistenceProtocolCollectionDoc;
	persistenceProtocolCollectionDoc.CreateInstance();
	persistenceProtocolCollectionDoc->initialize(
		persistenceProtocolVal);

	return persistenceProtocolCollectionDoc;
}

