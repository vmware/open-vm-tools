/*
 *  Author: bwilliams
 *  Created: Nov 16, 2015
 *
 *  Copyright (C) 2015-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#include "stdafx.h"

#include "Doc/DocXml/PersistenceXml/PersistenceXml.h"

#include "Doc/PersistenceDoc/CPersistenceDoc.h"
#include "Xml/XmlUtils/CXmlElement.h"
#include "Doc/DocXml/PersistenceXml/PersistenceXmlRoots.h"

using namespace Caf;

std::string XmlRoots::savePersistenceToString(
	const SmartPtrCPersistenceDoc persistenceDoc) {
	CAF_CM_STATIC_FUNC_VALIDATE("XmlRoots", "savePersistenceToString");
	CAF_CM_VALIDATE_SMARTPTR(persistenceDoc);

	const std::string schemaNamespace = DocXmlUtils::getSchemaNamespace("fx");
	const std::string schemaLocation = DocXmlUtils::getSchemaLocation("fx/Persistence.xsd");

	const SmartPtrCXmlElement rootXml = CXmlUtils::createRootElement(
		"persistence", schemaNamespace, schemaLocation);
	PersistenceXml::add(persistenceDoc, rootXml);

	return rootXml->saveToString();
}

SmartPtrCPersistenceDoc XmlRoots::parsePersistenceFromString(
	const std::string xml) {
	CAF_CM_STATIC_FUNC_VALIDATE("XmlRoots", "parsePersistenceFromString");
	CAF_CM_VALIDATE_STRING(xml);

	const SmartPtrCXmlElement rootXml = CXmlUtils::parseString(xml, "caf:persistence");
	return PersistenceXml::parse(rootXml);
}

void XmlRoots::savePersistenceToFile(
	const SmartPtrCPersistenceDoc persistenceDoc,
	const std::string filePath) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("XmlRoots", "savePersistenceToFile");
	CAF_CM_VALIDATE_SMARTPTR(persistenceDoc);
	CAF_CM_VALIDATE_STRING(filePath);

	CAF_CM_LOG_DEBUG_VA1("Saving to file - %s", filePath.c_str());

	const std::string persistenceStr =
		savePersistenceToString(persistenceDoc);
	FileSystemUtils::saveTextFile(filePath, persistenceStr);
}

SmartPtrCPersistenceDoc XmlRoots::parsePersistenceFromFile(
	const std::string filePath) {
	CAF_CM_STATIC_FUNC_VALIDATE("XmlRoots", "parsePersistenceFromFile");
	CAF_CM_VALIDATE_STRING(filePath);

	const SmartPtrCXmlElement rootXml = CXmlUtils::parseFile(filePath, "caf:persistence");
	return PersistenceXml::parse(rootXml);
}
