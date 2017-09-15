/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#include "stdafx.h"
#include "DocXmlUtils.h"

using namespace Caf;

std::string DocXmlUtils::getSchemaNamespace(
	const std::string& relSchemaNamespace) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("DocXmlUtils", "getSchemaNamespace");

	std::string schemaNamespace;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(relSchemaNamespace);

		const std::string schemaNamespaceRoot =
			AppConfigUtils::getRequiredString("schema_namespace_root");
		const std::string pathDelim = "/";
		schemaNamespace = schemaNamespaceRoot + pathDelim + relSchemaNamespace;
	}
	CAF_CM_EXIT;

	return schemaNamespace;
}

std::string DocXmlUtils::getSchemaLocation(
	const std::string& relSchemaLocation) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("DocXmlUtils", "getSchemaLocation");

	std::string schemaLocation;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(relSchemaLocation);

		const std::string schemaLocationRoot =
			AppConfigUtils::getRequiredString("schema_location_root");
		const std::string pathDelim = "/";
		schemaLocation = schemaLocationRoot + pathDelim + relSchemaLocation;
	}
	CAF_CM_EXIT;

	return schemaLocation;
}
