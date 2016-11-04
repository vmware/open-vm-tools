/*
 *	Author: bwilliams
 *  Created: Oct 20, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "Integration/Core/CIntException.h"

#define CAF_CM_DEFINE_INTEGRATION_EXCEPTION_CLASS(_exclass_) \
	_exclass_::_exclass_() : CCafException( #_exclass_ ) {} \
	_exclass_::~_exclass_() {} \
	void _exclass_::throwSelf() { throw this; }

using namespace Caf;

CAF_CM_DEFINE_INTEGRATION_EXCEPTION_CLASS(FatalListenerStartupException)
CAF_CM_DEFINE_INTEGRATION_EXCEPTION_CLASS(ListenerExecutionFailedException)
CAF_CM_DEFINE_INTEGRATION_EXCEPTION_CLASS(MessageDeliveryException)

CIntException::CIntException() :
	_isInitialized(false),
	_errorCode(0),
	_backtrace(NULL),
	CAF_CM_INIT("CIntException") {
}

CIntException::~CIntException() {
}

void CIntException::initialize(const CCafException* cafException) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_PTR(cafException);

	_exceptionClassName = cafException->getExceptionClassName();
	_message = cafException->getMsg();
	_className = cafException->getClassName();
	_funcName = cafException->getFuncName();
	_errorCode = cafException->getError();
	_backtrace = cafException->getBacktrace();
	_fullMsg = cafException->getFullMsg();

	_isInitialized = true;
}

std::string CIntException::getExceptionClassName() const {
	CAF_CM_FUNCNAME_VALIDATE("getExceptionClassName");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _exceptionClassName;
}

std::string CIntException::getMsg() const {
	CAF_CM_FUNCNAME_VALIDATE("getMsg");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _message;
}

std::string CIntException::getClassName() const {
	CAF_CM_FUNCNAME_VALIDATE("getClassName");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _className;
}

std::string CIntException::getFuncName() const {
	CAF_CM_FUNCNAME_VALIDATE("getFuncName");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _funcName;
}

HRESULT CIntException::getError() const {
	CAF_CM_FUNCNAME_VALIDATE("getError");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _errorCode;
}

std::deque<std::string>* CIntException::getBacktrace() const {
	CAF_CM_FUNCNAME_VALIDATE("getBacktrace");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _backtrace;
}

std::string CIntException::getFullMsg() const {
	CAF_CM_FUNCNAME_VALIDATE("getFullMsg");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _fullMsg;
}
