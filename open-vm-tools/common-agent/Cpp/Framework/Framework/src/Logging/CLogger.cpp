/*
 *  Author: bwilliams
 *  Created: 1/14/2011
 *
 *  Copyright (C) 2009-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "CLogger.h"

using namespace Caf;

CLogger::CLogger(const char* className) :
	_category(log4cpp::Category::getInstance(className)) {
}

CLogger::~CLogger() {
}

void CLogger::log(
	const log4cpp::Priority::PriorityLevel priority,
	const char* funcName,
	const int32 lineNumber,
	const CCafException* cafException) const {

	if(_category.isPriorityEnabled(priority) && (NULL != cafException)) {
		logVA(priority, funcName, lineNumber, "0x%08X %s", cafException->getError(),
			cafException->getFullMsg().c_str());
		logBacktrace(log4cpp::Priority::INFO, funcName, lineNumber,
			*cafException->getBacktrace());
	}
}

void CLogger::logMessage(
	const log4cpp::Priority::PriorityLevel priority,
	const char* funcName,
	const int32 lineNumber,
	const char* message) const {

	if(_category.isPriorityEnabled(priority)) {
		std::stringstream fullMsg;
		fullMsg <<
			funcName << "|" <<
			lineNumber << "|" <<
			message;

		_category.log(priority, fullMsg.str());
	}
}

void CLogger::logVA(
	const log4cpp::Priority::PriorityLevel priority,
	const char* funcName,
	const int32 lineNumber,
	const char* format,
	...) const {

	const int16 logLineLen = 1024;
	if(_category.isPriorityEnabled(priority)) {
		char buffer [logLineLen];
		va_list args;
		va_start(args, format);
#ifdef WIN32
		// Returns -1 if the buffer is truncated.
		const int rc = vsnprintf_s(buffer, logLineLen, _TRUNCATE, format, args);
		if (! ((rc > 0) || (rc == -1))) {
			::strcpy_s(buffer, "*** INTERNAL ERROR: UNABLE TO FORMAT MESSAGE ***");
#else
		const int rc = vsnprintf(buffer, logLineLen, format, args);
		if (! (rc > 0)) {
			::strcpy(buffer, "*** INTERNAL ERROR: UNABLE TO FORMAT MESSAGE ***");
#endif
		}

		std::stringstream fullMsg;
		fullMsg <<
			funcName << "|" <<
			lineNumber << "|" <<
			buffer;

		_category.log(priority, fullMsg.str());

		va_end(args);
	}
}

bool CLogger::isPriorityEnabled(const log4cpp::Priority::Value priority) const {
	return _category.isPriorityEnabled(priority);
}

log4cpp::Priority::Value CLogger::getPriority() const {
	return _category.getPriority();
}

void CLogger::setPriority(const log4cpp::Priority::Value priority) const {
	_category.setPriority(priority);
}

void CLogger::logBacktrace(
	const log4cpp::Priority::PriorityLevel priority,
	const char* funcName,
	const int32 lineNumber,
	const std::deque<std::string>& backtrace) const {

	if (backtrace.empty()) {
		logVA(priority, funcName, lineNumber, "Backtrace is empty");
	} else {
		for (std::deque<std::string>::const_iterator iter = backtrace.begin();
			iter != backtrace.end(); iter++) {
			std::string name = *iter;
			if (!name.empty()) {
				logVA(priority, funcName, lineNumber, "%s", name.c_str());
			}
		}
	}
}
