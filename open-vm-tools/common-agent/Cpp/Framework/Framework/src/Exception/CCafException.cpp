/*
 *	 Author: mdonahue
 *  Created: Jan 13, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "Exception/CCafException.h"
#include <stdio.h>
#include <stdarg.h>
#include "CBacktraceUtils.h"

using namespace Caf;

CCafException::CCafException() :
	_refCnt(0),
	_isPopulated(false),
	_exceptionClassName("CCafException"),
	_errorCode(S_OK),
	_backtrace(NULL) {
}

CCafException::CCafException(const std::string& exceptionClassName) :
	_refCnt(0),
	_isPopulated(false),
	_exceptionClassName(exceptionClassName),
	_errorCode(S_OK),
	_backtrace(NULL) {
}

CCafException::~CCafException() {
	if (_backtrace != NULL) {
		delete _backtrace;
	}
}

void CCafException::throwSelf() {
	throw this;
}

void CCafException::throwAddRefedSelf() {
	this->AddRef();
	throw this;
}

void CCafException::AddRef() {
	g_atomic_int_inc(&_refCnt);
}

void CCafException::Release() {
	if (g_atomic_int_dec_and_test(&_refCnt)) {
		delete this;
	}
}

void CCafException::QueryInterface(const IID&, void**) {
	throw std::runtime_error("QueryInterface not supported");
}

void CCafException::populate(
	const std::string& message,
	const HRESULT errorCode,
	const std::string& className,
	const std::string& funcName) {
	_message = message;
	_className = className;
	_funcName = funcName;
	_errorCode = HRESULT_FROM_WIN32(errorCode);
	if (_backtrace != NULL) {
		delete _backtrace;
	}
	_backtrace = new std::deque<std::string>(CBacktraceUtils::getBacktrace());
	_isPopulated = true;
}

void CCafException::populateVA(
	const HRESULT errorCode,
	const std::string& className,
	const std::string& funcName,
	const char* format,
	...) {
	char buffer[1024];
	va_list args;
	va_start(args, format);
#ifdef WIN32
	// Returns -1 if the buffer is truncated.
	const int rc = vsnprintf_s(buffer, 1024, _TRUNCATE, format, args);
	if ((rc > 0) || (rc == -1)) {
#else
	if (vsnprintf(buffer, 1024, format, args) > 0) {
#endif
		_message = buffer;
	} else {
		_message = "*** PopulateVA() INTERNAL ERROR: UNABLE TO FORMAT MESSAGE ***";
	}
	va_end(args);
	_className = className;
	_funcName = funcName;
	_errorCode = HRESULT_FROM_WIN32(errorCode);
	if (_backtrace != NULL) {
		delete _backtrace;
	}
	_backtrace = new std::deque<std::string>(CBacktraceUtils::getBacktrace());
	_isPopulated = true;
}

bool CCafException::isPopulated() const {
	return _isPopulated;
}

std::string CCafException::getExceptionClassName() const {
	return _exceptionClassName;
}

std::string CCafException::getMsg() const {
	return _message;
}

std::string CCafException::getClassName() const {
	return _className;
}

std::string CCafException::getFuncName() const {
	return _funcName;
}

HRESULT CCafException::getError() const {
	return _errorCode;
}

std::deque<std::string>* CCafException::getBacktrace() const {
   return _backtrace;
}

std::string CCafException::getFullMsg() const {
	std::string msg("[");
	msg += _exceptionClassName;
	msg += "] ";
	msg += _className;
	msg += "::";
	msg += _funcName;
	msg += "() ";
	msg += _message;
	return msg;
}
