/*
 *	Author: bwilliams
 *	Created: May 3, 2012
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Doc/CafCoreTypesDoc/CAttachmentCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CAttachmentDoc.h"
#include "Doc/CafCoreTypesDoc/COperationDoc.h"
#include "Doc/CafCoreTypesDoc/CParameterCollectionDoc.h"
#include "Doc/ProviderRequestDoc/CProviderInvokeOperationDoc.h"
#include "Doc/ProviderResultsDoc/CSchemaDoc.h"
#include "Doc/SchemaTypesDoc/CActionClassDoc.h"
#include "Doc/SchemaTypesDoc/CClassPropertyDoc.h"
#include "Doc/SchemaTypesDoc/CDataClassDoc.h"
#include "Doc/SchemaTypesDoc/CDataClassInstanceDoc.h"
#include "Doc/SchemaTypesDoc/CDataClassPropertyDoc.h"
#include "Doc/SchemaTypesDoc/CMethodDoc.h"
#include "Doc/SchemaTypesDoc/CMethodParameterDoc.h"
#include "Exception/CCafException.h"
#include "CTestInfraProvider.h"
#include "IProviderResponse.h"
#include "IProviderRequest.h"

using namespace Caf;

CTestInfraProvider::CTestInfraProvider() :
	CAF_CM_INIT_LOG("CTestInfraProvider") {
}

CTestInfraProvider::~CTestInfraProvider() {
}

const SmartPtrCSchemaDoc CTestInfraProvider::getSchema() const {
	std::deque<SmartPtrCClassPropertyDoc> dc1Props;
	dc1Props.push_back(CProviderDocHelper::createClassProperty("name", PROPERTY_STRING, true));
	dc1Props.push_back(CProviderDocHelper::createClassProperty("value", PROPERTY_STRING, true));

	std::deque<SmartPtrCDataClassDoc> dataClasses;
	dataClasses.push_back(CProviderDocHelper::createDataClass("cafTestInfra", "TestDataClass", "1.0.0", dc1Props));

	std::deque<SmartPtrCMethodParameterDoc> m1Params;
	m1Params.push_back(CProviderDocHelper::createMethodParameter("requestAttachmentName", PARAMETER_STRING, false));

	std::deque<SmartPtrCMethodParameterDoc> m2Params;
	m2Params.push_back(CProviderDocHelper::createMethodParameter("param1", PARAMETER_STRING, false));
	m2Params.push_back(CProviderDocHelper::createMethodParameter("param2", PARAMETER_STRING, false));

	std::deque<SmartPtrCMethodDoc> methods;
	methods.push_back(CProviderDocHelper::createMethod("echoRequest", m1Params));
	methods.push_back(CProviderDocHelper::createMethod("testMethod", m2Params));

	std::deque<SmartPtrCActionClassDoc> actionClasses;
	actionClasses.push_back(
			CProviderDocHelper::createActionClass(
			"cafTestInfra",
			"TestActionClass",
			"1.0.0",
			CProviderDocHelper::createCollectMethod("collectInstances"),
			methods));

	return CProviderDocHelper::createSchema(dataClasses, actionClasses);
}

void CTestInfraProvider::collect(const IProviderRequest& request, IProviderResponse& response) const {
	CAF_CM_ENTER {
		const SmartPtrCDataClassInstanceDoc dataClassInstance =
			createDataClassInstance("testName", "testValue");
		response.addInstance(dataClassInstance);
	}
	CAF_CM_EXIT;
}

void CTestInfraProvider::invoke(const IProviderRequest& request, IProviderResponse& response) const {
	CAF_CM_FUNCNAME("invoke");

	CAF_CM_ENTER {
		SmartPtrCProviderInvokeOperationDoc doc = request.getInvokeOperations();
		CAF_CM_VALIDATE_SMARTPTR(doc);

		const SmartPtrCOperationDoc operation = doc->getOperation();
		const std::string operationName = operation->getName();
		const SmartPtrCParameterCollectionDoc parameterCollection =
			operation->getParameterCollection();

		if (operationName.compare("testMethod") == 0) {
			const std::string param1 = ParameterUtils::findRequiredParameterAsString(
				"param1", parameterCollection);
			const std::string param2Str = ParameterUtils::findRequiredParameterAsString(
				"param2", parameterCollection);
			const int32 param2 = CStringConv::fromString<int32>(param2Str);

			CAF_CM_LOG_DEBUG_VA2("testMethod() called - param1: %s, param2: %d",
					param1.c_str(), param2);
		} else if (operationName.compare("echoRequest") == 0) {
			const std::string requestAttachmentName = ParameterUtils::findRequiredParameterAsString(
				"requestAttachmentName", parameterCollection);

			SmartPtrCAttachmentCollectionDoc attachmentCollection = request.getAttachments();
			if (attachmentCollection.IsNull()) {
				CAF_CM_EXCEPTIONEX_VA1(InvalidArgumentException, E_INVALIDARG,
					"Attachment collection is empty - %s", requestAttachmentName.c_str())
			}

			SmartPtrCAttachmentDoc attachmentFnd;
			const std::deque<SmartPtrCAttachmentDoc> attachmentCollectionInner =
					attachmentCollection->getAttachment();
			for (TConstIterator<std::deque<SmartPtrCAttachmentDoc> > attachmentIter(attachmentCollectionInner);
				attachmentIter; attachmentIter++) {
				const SmartPtrCAttachmentDoc attachment = *attachmentIter;
				const std::string attachmentName = attachment->getName();
				if (attachmentName.compare(requestAttachmentName) == 0) {
					attachmentFnd = attachment;
				}
			}

			if (attachmentFnd.IsNull()) {
				CAF_CM_EXCEPTIONEX_VA1(InvalidArgumentException, E_INVALIDARG,
					"Request Attachment not found - %s", requestAttachmentName.c_str())
			}

			response.addAttachment(attachmentFnd);
		} else {
			CAF_CM_EXCEPTIONEX_VA1(InvalidArgumentException, E_INVALIDARG,
				"Invalid operation name (must be \'echoRequest\') - %s", operationName.c_str())
		}
	}
	CAF_CM_EXIT;
}

SmartPtrCDataClassInstanceDoc CTestInfraProvider::createDataClassInstance(
	const std::string& name,
	const std::string& value) const {
	CAF_CM_FUNCNAME_VALIDATE("createDataClassInstance");
	CAF_CM_VALIDATE_STRING(name);
	CAF_CM_VALIDATE_STRING(value);

	std::deque<SmartPtrCDataClassPropertyDoc> dataClassProperties;
	dataClassProperties.push_back(CProviderDocHelper::createDataClassProperty("name", name));
	dataClassProperties.push_back(CProviderDocHelper::createDataClassProperty("value", value));

	return CProviderDocHelper::createDataClassInstance(
			"cafTestInfra",
			"TestDataClass",
			"1.0.0",
			dataClassProperties);
}
