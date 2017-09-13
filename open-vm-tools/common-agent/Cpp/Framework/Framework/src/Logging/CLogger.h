/*
 *  Author: bwilliams
 *  Created: 1/14/2011
 *
 *  Copyright (c) 2009-2011 Vmware, Inc.  All rights reserved.
 *  -- VMware Confidential
 */

#ifndef CLogger_h_
#define CLogger_h_

namespace Caf {

class LOGGING_LINKAGE CLogger
{
public:
	CLogger(const char* className);
	virtual ~CLogger();

public:
	void log(
		const log4cpp::Priority::PriorityLevel priority,
		const char* funcName,
		const int32 lineNumber,
		const CCafException* cafException) const;

	void logMessage(
		const log4cpp::Priority::PriorityLevel priority,
		const char* funcName,
		const int32 lineNumber,
		const char* message) const;

	void logVA(
		const log4cpp::Priority::PriorityLevel priority,
		const char* funcName,
		const int32 lineNumber,
		const char* format,
		...) const;

	void logBacktrace(
		const log4cpp::Priority::PriorityLevel priority,
		const char* funcName,
		const int32 lineNumber,
		const std::deque<std::string>& backtrace) const;

	bool isPriorityEnabled(const log4cpp::Priority::PriorityLevel priority) const;

private:
	log4cpp::Category& _category;

private:
	CLogger(const CLogger&);
	CLogger& operator=(const CLogger&);
};

}

#endif // #define CLogger_h_
