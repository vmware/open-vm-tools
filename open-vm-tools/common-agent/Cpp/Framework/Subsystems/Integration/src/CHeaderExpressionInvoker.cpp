/*
 *  Created on: Jul 23, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "IVariant.h"
#include "Integration/IIntMessage.h"
#include "Exception/CCafException.h"
#include "CHeaderExpressionInvoker.h"

using namespace Caf;

CHeaderExpressionInvoker::CHeaderExpressionInvoker() :
	_isInitialized(false),
	CAF_CM_INIT("CHeaderExpressionInvoker") {
}

CHeaderExpressionInvoker::~CHeaderExpressionInvoker() {
}

void CHeaderExpressionInvoker::initializeBean(
			const IBean::Cargs& ctorArgs,
			const IBean::Cprops& properties) {
	CAF_CM_FUNCNAME("initializeBean");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_ASSERT(ctorArgs.size() == 0);
	CAF_CM_ASSERT(properties.size() == 0);
	_isInitialized = true;
}

void CHeaderExpressionInvoker::terminateBean() {
}

SmartPtrIVariant CHeaderExpressionInvoker::invokeExpression(
			const std::string& methodName,
			const Cdeqstr& methodParams,
			const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME("invokeExpression");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(methodName);
	CAF_CM_VALIDATE_STL(methodParams);
	CAF_CM_VALIDATE_INTERFACE(message);

	SmartPtrIVariant result;
	const IIntMessage::SmartPtrCHeaders& headers = message->getHeaders();
	if (methodName == "containsKey") {
		CAF_CM_ASSERT(1 == methodParams.size());
		result = CVariant::createBool(headers->end() != headers->find(methodParams.front()));
	} else if (methodName == "notContainsKey") {
		CAF_CM_ASSERT(1 == methodParams.size());
		result = CVariant::createBool(headers->end() == headers->find(methodParams.front()));
	} else if (methodName == "toString") {
		CAF_CM_ASSERT(1 == methodParams.size());
		const SmartPtrIVariant value = message->findRequiredHeader(methodParams.front());
		result = CVariant::createString(value->toString());
	} else if (methodName == "prependToString") {
		CAF_CM_ASSERT(2 == methodParams.size());
		const SmartPtrIVariant value = message->findRequiredHeader(methodParams.front());
		result = CVariant::createString(methodParams.back() + value->toString());
	} else if (methodName == "appendToString") {
		CAF_CM_ASSERT(2 == methodParams.size());
		const SmartPtrIVariant value = message->findRequiredHeader(methodParams.front());
		result = CVariant::createString(value->toString() + methodParams.back());
	} else if (methodName == "isEqualString") {
		CAF_CM_ASSERT(2 == methodParams.size());
		const SmartPtrIVariant value = message->findRequiredHeader(methodParams.front());
		result = CVariant::createBool(value->toString().compare(methodParams.back()) == 0);
	} else if (methodName == "isNotEqualString") {
		CAF_CM_ASSERT(2 == methodParams.size());
		const SmartPtrIVariant value = message->findRequiredHeader(methodParams.front());
		result = CVariant::createBool(value->toString().compare(methodParams.back()) != 0);
	} else {
		CAF_CM_EXCEPTIONEX_VA1(
				NoSuchMethodException,
				0,
				"%s",
				methodName.c_str());
	}
	return result;
}
