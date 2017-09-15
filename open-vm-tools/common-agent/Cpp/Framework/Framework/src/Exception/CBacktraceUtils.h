/*
 *	 Author: bwilliams
 *  Created: Oct 30, 2014
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CBacktraceUtils_H_
#define CBacktraceUtils_H_

namespace Caf {

// See http://stackoverflow.com/questions/77005/how-to-generate-a-stacktrace-when-my-gcc-c-app-crashes
class EXCEPTION_LINKAGE CBacktraceUtils {
public:
	static std::deque<std::string> getBacktrace();

#ifndef WIN32
private:
	static std::string demangleName(char* name);
#endif

private:
	CBacktraceUtils();
	~CBacktraceUtils();
	CBacktraceUtils(const CBacktraceUtils&);
	CBacktraceUtils& operator=(const CBacktraceUtils&);
};

}

#endif /* CBacktraceUtils_H_ */
