/*
 *	Author: bwilliams
 *  Created: Jan 21, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Xml/MarkupParser/CMarkupParser.h"
#include "Xml/XmlUtils/CXmlElement.h"
#include "Exception/CCafException.h"
#include "CXmlUtils.h"

using namespace Caf;

SmartPtrCXmlElement CXmlUtils::parseFile(
	const std::string& path,
	const std::string& rootName) {
	CAF_CM_STATIC_FUNC("CXmlUtils", "parseFile");
	CAF_CM_VALIDATE_STRING(path);
	// rootName is optional

	if (!FileSystemUtils::doesFileExist(path)) {
		CAF_CM_EXCEPTION_VA1(ERROR_FILE_NOT_FOUND, "File not found: %s", path.c_str());
	}

	const MarkupParser::SmartPtrElement element = MarkupParser::parseFile(path);
	CAF_CM_VALIDATE_SMARTPTR(element);
	CAF_CM_VALIDATE_STRING(element->name);
	if (!rootName.empty()) {
		CAF_CM_VALIDATE_COND_VA3(element->name == rootName,
			"root not valid (\"%s\" != \"%s\") in %s", rootName.c_str(),
			element->name.c_str(), path.c_str());
	}

	SmartPtrCXmlElement xmlElement;
	xmlElement.CreateInstance();
	xmlElement->initialize(element, path);

	return xmlElement;
}

SmartPtrCXmlElement CXmlUtils::parseString(
	const std::string& xml,
	const std::string& rootName) {
	CAF_CM_STATIC_FUNC("CXmlUtils", "parseString");
	CAF_CM_VALIDATE_STRING(xml);
	// rootName is optional

	const std::string path = "fromString";

	const MarkupParser::SmartPtrElement element = MarkupParser::parseString(xml);
	CAF_CM_VALIDATE_SMARTPTR(element);
	CAF_CM_VALIDATE_STRING(element->name);
	if (!rootName.empty()) {
		CAF_CM_VALIDATE_COND_VA3(element->name == rootName,
			"root not valid (\"%s\" != \"%s\") in %s", rootName.c_str(),
			element->name.c_str(), path.c_str());
	}

	SmartPtrCXmlElement xmlElement;
	xmlElement.CreateInstance();
	xmlElement->initialize(element, path);

	return xmlElement;
}

SmartPtrCXmlElement CXmlUtils::createRootElement(
	const std::string& rootName,
	const std::string& rootNamespace) {
	CAF_CM_STATIC_FUNC_VALIDATE("CXmlUtils", "createRootElement");
	CAF_CM_VALIDATE_STRING(rootName);
	CAF_CM_VALIDATE_STRING(rootNamespace);

	return createRootElement(rootName, rootNamespace, std::string());
}

SmartPtrCXmlElement CXmlUtils::createRootElement(
	const std::string& rootName,
	const std::string& rootNamespace,
	const std::string& schemaLocation) {
	CAF_CM_STATIC_FUNC_VALIDATE("CXmlUtils", "createRootElement");
	CAF_CM_VALIDATE_STRING(rootName);
	CAF_CM_VALIDATE_STRING(rootNamespace);
	// schemaLocation is optional

	MarkupParser::SmartPtrElement element;
	element.CreateInstance();

	SmartPtrCXmlElement xmlElement;
	xmlElement.CreateInstance();
	xmlElement->initialize(element, "createRootElement");
	xmlElement->addAttribute("xmlns:caf", rootNamespace);

	if (!schemaLocation.empty()) {
		const std::string fullSchemaLocation = rootNamespace + " " + schemaLocation;
		xmlElement->addAttribute("xmlns:xsi",
			"http://www.w3.org/2001/XMLSchema-instance");
		xmlElement->addAttribute("xsi:schemaLocation", fullSchemaLocation);
	}

	element->name = "caf:" + rootName;

	return xmlElement;
}

std::string CXmlUtils::escape(const std::string& text) {
	CAF_CM_STATIC_FUNC("CXmlUtils", "escape");

	std::string rc;
	gchar* gRc = NULL;

	try {
		CAF_CM_VALIDATE_STRING(text);

		gRc = g_markup_escape_text(text.c_str(), -1);
		if (gRc) {
			rc = gRc;
			g_free(gRc);
			gRc = NULL;
		}
	}
	CAF_CM_CATCH_DEFAULT

	try {
		if (gRc) {
			g_free(gRc);
		}
	}
	CAF_CM_CATCH_DEFAULT
	CAF_CM_THROWEXCEPTION;

	return rc;
}
