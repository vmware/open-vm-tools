/*
 *  Created on: Aug 16, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Integration/Caf/CCafMessagePayload.h"
#include "Doc/ResponseDoc/CEventKeyDoc.h"
#include "Doc/ResponseDoc/CManifestDoc.h"
#include "IVariant.h"
#include "Integration/IIntMessage.h"
#include "Common/CCafRegex.h"
#include "Exception/CCafException.h"
#include "CEventTopicCalculatorInstance.h"

using namespace Caf;

CEventTopicCalculatorInstance::CEventTopicCalculatorInstance() :
	_isInitialized(false),
	CAF_CM_INIT("CEventTopicCalculatorInstance") {
	CAF_CM_INIT_THREADSAFE;
}

CEventTopicCalculatorInstance::~CEventTopicCalculatorInstance() {
}

void CEventTopicCalculatorInstance::initializeBean(
	const IBean::Cargs& ctorArgs,
	const IBean::Cprops& properties) {
	CAF_CM_FUNCNAME_VALIDATE("initializeBean");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	_isInitialized = true;
}

void CEventTopicCalculatorInstance::terminateBean() {
}

SmartPtrIVariant CEventTopicCalculatorInstance::invokeExpression(
		const std::string& methodName,
		const Cdeqstr& methodParams,
		const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME("invokeEspression");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_ASSERT(!methodParams.size());
	SmartPtrIVariant result;

	if (methodName == "getTopic") {
		result = getTopic(message);
	} else {
		CAF_CM_EXCEPTIONEX_VA1(
				NoSuchMethodException,
				0,
				"Method '%s' is not supported by this invoker",
				methodName.c_str());
	}
	return result;
}

SmartPtrIVariant CEventTopicCalculatorInstance::getTopic(
		const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME_VALIDATE("getTopic");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	const SmartPtrCCafMessagePayload cafMessagePayload =
			CCafMessagePayload::create(message->getPayload());

	const SmartPtrCManifestDoc manifest = cafMessagePayload->getManifest();

	const std::deque<SmartPtrCEventKeyDoc> eventKeyCollection =
			cafMessagePayload->getEventKeyCollection();

	CCafRegex replaceDots;
	replaceDots.initialize("\\.");

	std::stringstream topicBld;
	topicBld << "caf.event."
		<< replaceDots.replaceLiteral(manifest->getClassName(), "_")
		<< '.'
		<< replaceDots.replaceLiteral(manifest->getClassNamespace(), "_")
		<< '.'
		<< replaceDots.replaceLiteral(manifest->getClassVersion(), "_");
	for (TConstIterator<std::deque<SmartPtrCEventKeyDoc> > key(eventKeyCollection); key; key++) {
		const SmartPtrCEventKeyDoc eventKey = *key;
		topicBld << '.' << replaceDots.replaceLiteral(eventKey->getValue(), "_");
	}

	return CVariant::createString(topicBld.str());
}

