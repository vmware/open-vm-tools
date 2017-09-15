/*
 *  Created on: Jul 23, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Common/IAppConfig.h"
#include "Common/IAppContext.h"
#include "IBean.h"
#include "IVariant.h"
#include "Integration/Core/CExpressionHandler.h"
#include "Common/CCafRegex.h"
#include "Exception/CCafException.h"

using namespace Caf;

CExpressionHandler::CExpressionHandler() :
	_isInitialized(false),
	CAF_CM_INIT("CExpressionHandler") {
}

CExpressionHandler::~CExpressionHandler() {
}

void CExpressionHandler::init(
		const SmartPtrIAppConfig& appConfig,
		const SmartPtrIAppContext& appContext,
		const std::string& expression) {
	CAF_CM_FUNCNAME("init");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(appConfig);
	CAF_CM_VALIDATE_INTERFACE(appContext);
	CAF_CM_VALIDATE_STRING(expression);

	CCafRegex regex;
	regex.initialize("\\@(\\w+)\\.(\\w+)\\((.*)\\)");
	if (!regex.isMatched(expression)) {
		CAF_CM_EXCEPTIONEX_VA1(
				InvalidArgumentException,
				0,
				"Invalid expression syntax '%s'",
				expression.c_str());
	}
	_beanName = regex.match(expression, 1);
	_methodName = regex.match(expression, 2);
	if (!_beanName.length() || !_methodName.length()) {
		CAF_CM_EXCEPTIONEX_VA1(
				InvalidArgumentException,
				0,
				"Invalid expression syntax '%s'",
				expression.c_str());
	}

	SmartPtrIBean bean = appContext->getBean(_beanName);
	_invoker.QueryInterface(bean, false);
	if (!_invoker) {
		CAF_CM_EXCEPTIONEX_VA1(
				NoSuchInterfaceException,
				0,
				"bean '%s' does not support the IExpressionInvoker interface",
				_beanName.c_str());
	}

	const std::string paramSubstr = regex.match(expression, 3);
	Cdeqstr paramExprs = CStringUtils::split(paramSubstr, ',');
	CCafRegex regexLiteral;
	regexLiteral.initialize("'(.*)'");
	for (TConstIterator<Cdeqstr> paramExpr(paramExprs); paramExpr; paramExpr++) {
		std::string trimmedExpr = CStringUtils::trim(*paramExpr);
		if (regexLiteral.isMatched(trimmedExpr)) {
			std::string val = regexLiteral.match(trimmedExpr, 1);
			if (val.length()) {
				_methodParams.push_back(appConfig->resolveValue(val));
			} else {
				_methodParams.push_back("");
			}
		} else {
			CAF_CM_EXCEPTIONEX_VA1(
					InvalidArgumentException,
					0,
					"Invalid expression syntax '%s'",
					expression.c_str());
		}
	}
	_isInitialized = true;
}

SmartPtrIVariant CExpressionHandler::evaluate(const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME_VALIDATE("evaluate");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _invoker->invokeExpression(_methodName, _methodParams, message);
}

std::string CExpressionHandler::getBeanName() const {
	CAF_CM_FUNCNAME_VALIDATE("getBeanName");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _beanName;
}

std::string CExpressionHandler::getMethodName() const {
	CAF_CM_FUNCNAME_VALIDATE("getMethodName");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _methodName;
}

Cdeqstr CExpressionHandler::getMethodParameters() const {
	CAF_CM_FUNCNAME_VALIDATE("getMethodParameters");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _methodParams;
}

std::string CExpressionHandler::toString() const {
	std::stringstream result;
	result
		<< "invoke @"
		<< _beanName
		<< '.'
		<< _methodName
		<< '(';
	bool first = true;
	for (TConstIterator<Cdeqstr> param(_methodParams); param; param++) {
		if (!first) {
			result << ", ";
		}
		result
			<< '\''
			<< *param
			<< '\'';
		first = false;
	}
	result << ')';
	return result.str();
}
