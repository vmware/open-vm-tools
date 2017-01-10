/*
 *	 Author: brets
 *  Created: Dec 3, 2015
 *
 *	Copyright (C) 2015-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "Doc/CafCoreTypesDoc/CafCoreTypesDocTypes.h"
#include "Doc/CafCoreTypesDoc/CafCoreTypesDocTypes.h"

#include "Doc/ProviderResultsDoc/CSchemaDoc.h"
#include "Doc/SchemaTypesDoc/CActionClassDoc.h"
#include "Doc/SchemaTypesDoc/CClassIdentifierDoc.h"
#include "Doc/SchemaTypesDoc/CClassInstancePropertyDoc.h"
#include "Doc/SchemaTypesDoc/CClassPropertyDoc.h"
#include "Doc/SchemaTypesDoc/CCmdlMetadataDoc.h"
#include "Doc/SchemaTypesDoc/CCollectMethodDoc.h"
#include "Doc/SchemaTypesDoc/CDataClassDoc.h"
#include "Doc/SchemaTypesDoc/CDataClassInstanceDoc.h"
#include "Doc/SchemaTypesDoc/CDataClassPropertyDoc.h"
#include "Doc/SchemaTypesDoc/CDataClassSubInstanceDoc.h"
#include "Doc/SchemaTypesDoc/CInstanceParameterDoc.h"
#include "Doc/SchemaTypesDoc/CMethodDoc.h"
#include "Doc/SchemaTypesDoc/CMethodParameterDoc.h"
#include "../include/CProviderDocHelper.h"

using namespace Caf;

SmartPtrCClassPropertyDoc CProviderDocHelper::createClassProperty(
		const std::string name,
		const PROPERTY_TYPE type,
		const bool required,
		const bool key,
		const bool list) {

	SmartPtrCClassPropertyDoc doc;
	doc.CreateInstance();
	doc->initialize(name, type, std::deque<std::string>(), required, key, list);
	return doc;
}

SmartPtrCClassIdentifierDoc CProviderDocHelper::createClassIdentifier(
		const std::string namespaceVal,
		const std::string name,
		const std::string version) {
	SmartPtrCClassIdentifierDoc doc;
	doc.CreateInstance();
	doc->initialize(namespaceVal, name, version);
	return doc;
}

SmartPtrCClassInstancePropertyDoc CProviderDocHelper::createClassInstanceProperty(
		const std::string name,
		const SmartPtrCClassIdentifierDoc type,
		const bool required,
		const bool transientVal,
		const bool list) {

	std::deque<SmartPtrCClassIdentifierDoc> types;
	types.push_back(type);

	SmartPtrCClassInstancePropertyDoc doc;
	doc.CreateInstance();
	doc->initialize(name, types, required, transientVal, list);
	return doc;
}

SmartPtrCDataClassDoc CProviderDocHelper::createDataClass(
		const std::string namespaceVal,
		const std::string name,
		const std::string version,
		const std::deque<SmartPtrCClassPropertyDoc> properties,
		const std::deque<SmartPtrCClassInstancePropertyDoc> instanceProperties) {

	SmartPtrCDataClassDoc doc;
	doc.CreateInstance();
	doc->initialize(namespaceVal, name, version, properties, instanceProperties);
	return doc;
}

SmartPtrCMethodParameterDoc CProviderDocHelper::createMethodParameter(
		const std::string name,
		const PARAMETER_TYPE type,
		const bool isOptional,
		const bool isList) {
	SmartPtrCMethodParameterDoc doc;
	doc.CreateInstance();
	doc->initialize(name, type, isOptional, isList);
	return doc;
}

SmartPtrCCollectMethodDoc CProviderDocHelper::createCollectMethod(
		const std::string name,
		const std::deque<SmartPtrCMethodParameterDoc> parameters,
		const std::deque<SmartPtrCInstanceParameterDoc> instanceParameters) {
	SmartPtrCCollectMethodDoc doc;
	doc.CreateInstance();
	doc->initialize(name, parameters, instanceParameters);
	return doc;
}

SmartPtrCInstanceParameterDoc CProviderDocHelper::createInstanceParameter(
		const std::string name,
		const std::string instanceNamespace,
		const std::string instanceName,
		const std::string instanceVersion,
		const bool isOptional,
		const bool isList) {
	SmartPtrCInstanceParameterDoc doc;
	doc.CreateInstance();
	doc->initialize(name, instanceNamespace, instanceName, instanceVersion, isOptional, isList);
	return doc;
}

SmartPtrCMethodDoc CProviderDocHelper::createMethod(
		const std::string name,
		const std::deque<SmartPtrCMethodParameterDoc> parameters,
		const std::deque<SmartPtrCInstanceParameterDoc> instanceParameters) {
	SmartPtrCMethodDoc doc;
	doc.CreateInstance();
	doc->initialize(name, parameters, instanceParameters);
	return doc;
}

SmartPtrCActionClassDoc CProviderDocHelper::createActionClass(
		const std::string namespaceVal,
		const std::string name,
		const std::string version,
		const SmartPtrCCollectMethodDoc collectMethod,
		const std::deque<SmartPtrCMethodDoc> methodCollection) {

	SmartPtrCActionClassDoc doc;
	doc.CreateInstance();
	doc->initialize(namespaceVal, name, version, collectMethod, methodCollection);
	return doc;
}

SmartPtrCSchemaDoc CProviderDocHelper::createSchema(
		const std::deque<SmartPtrCDataClassDoc> dataClasses,
		const std::deque<SmartPtrCActionClassDoc> actionClasses) {
	SmartPtrCSchemaDoc doc;
	doc.CreateInstance();
	doc->initialize(dataClasses, actionClasses);
	return doc;
}

SmartPtrCDataClassPropertyDoc CProviderDocHelper::createDataClassProperty(
		const std::string name,
		const std::string value) {

	SmartPtrCDataClassPropertyDoc doc;
	doc.CreateInstance();
	doc->initialize(name, std::deque<SmartPtrCCmdlMetadataDoc>(), value);
	return doc;
}

SmartPtrCDataClassSubInstanceDoc CProviderDocHelper::createDataClassSubInstance(
		const std::string name,
		const std::deque<SmartPtrCDataClassPropertyDoc> properties) {

	SmartPtrCDataClassSubInstanceDoc doc;
	doc.CreateInstance();
	doc->initialize(
			name,
			std::deque<SmartPtrCCmdlMetadataDoc>(),
			properties,
			std::deque<SmartPtrCDataClassSubInstanceDoc>(),
			SmartPtrCCmdlUnionDoc());
	return doc;
}

SmartPtrCDataClassInstanceDoc CProviderDocHelper::createDataClassInstance(
		const std::string namespaceVal,
		const std::string name,
		const std::string version,
		const std::deque<SmartPtrCDataClassPropertyDoc> properties,
		const std::deque<SmartPtrCDataClassSubInstanceDoc> instanceProperties) {

	SmartPtrCDataClassInstanceDoc doc;
	doc.CreateInstance();
	doc->initialize(
			namespaceVal,
			name,
			version,
			std::deque<SmartPtrCCmdlMetadataDoc>(),
			properties,
			instanceProperties,
			SmartPtrCCmdlUnionDoc());
	return doc;
}
