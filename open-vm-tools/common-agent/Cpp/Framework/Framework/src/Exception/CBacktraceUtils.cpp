/*k
 *	Author: bwilliams
 *  Created: Oct 30, 2014
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "stdlib.h"
#include "CBacktraceUtils.h"
#include <CommonDefines.h>

#ifdef WIN32
#include "StackWalker.h"
#else
#include <execinfo.h>
#include <cxxabi.h>
#endif

using namespace Caf;

#ifdef WIN32
class StackWalkerToDeque : public StackWalker {
public:
   StackWalkerToDeque() : depth(0) {}

   std::deque<std::string> getBacktrace() {
      return backtrace;
   }

private:
   static const int STACK_DEPTH_IGNORE = 3;
   int depth;
   std::deque<std::string> backtrace;

protected:
   virtual void OnSymInit(LPCSTR szSearchPath, DWORD symOptions, LPCSTR szUserName) {
      // Do nothing to suppress default OnSymInit output
   }

   virtual void OnLoadModule(LPCSTR img, LPCSTR mod, DWORD64 baseAddr, DWORD size, DWORD result, LPCSTR symType, LPCSTR pdbName, ULONGLONG fileVersion) {
      // Do nothing to suppress default OnLoadModule output
   }

   virtual void OnDbgHelpErr(LPCSTR szFuncName, DWORD gle, DWORD64 addr){
      // Do nothing to suppress default OnDbgHelpErr output
   }

   virtual void OnOutput(LPCSTR szText) {
      try {
         std::string line = szText;
         if (STACK_DEPTH_IGNORE < depth++) {
            backtrace.push_back(CStringUtils::trimRight(line));
         }
      }  catch (...) {
         backtrace.push_back("CBacktraceUtils::getBacktrace() threw an exception");
      }
    }
};

std::deque<std::string> CBacktraceUtils::getBacktrace() {
	StackWalkerToDeque sw;
	sw.ShowCallstack();
	return sw.getBacktrace();
}

#else
std::deque<std::string> CBacktraceUtils::getBacktrace() {
	char** messages = NULL;
	std::deque<std::string> backtrace;
	try {
		const int32 maxStackEntries = 50;
		void *stackEntries[maxStackEntries];
		const size_t size = ::backtrace(stackEntries, maxStackEntries);

		messages = ::backtrace_symbols(stackEntries, size);

		/* skip first stack frame (points here) */
		for (size_t i = 1; i < size && messages != NULL; ++i) {
			const std::string name = demangleName(messages[i]);
			backtrace.push_back(name);
		}
	}
	catch (...) {
		backtrace.push_back("CBacktraceUtils::getBacktrace() threw an exception");
	}

	if (NULL != messages) {
		::free(messages);
	}

	return backtrace;
}

std::string CBacktraceUtils::demangleName(char* name) {
	std::string rc;
	if (NULL != name) {
		char* mangled_name = 0;
		char* offset_begin = 0;
		char* offset_end = 0;

		// find parentheses and +address offset surrounding mangled name
		for (char *p = name; *p; ++p) {
			if (*p == '(') {
				mangled_name = p;
			} else if (*p == '+') {
				offset_begin = p;
			} else if (*p == ')') {
				offset_end = p;
				break;
			}
		}

		// if the line could be processed, attempt to demangle the symbol
		if (mangled_name && offset_begin && offset_end && mangled_name < offset_begin) {
			*mangled_name++ = '\0';
			*offset_begin++ = '\0';
			*offset_end++ = '\0';

			int32 status;
			char* real_name = abi::__cxa_demangle(mangled_name, 0, 0, &status);

			// if demangling is successful, output the demangled function name
			if (status == 0) {
				rc = real_name;
			}
			// otherwise, output the mangled function name
			else {
				rc = mangled_name;
			}

			::free(real_name);
		}
		// otherwise, print the whole line
		else {
			rc = name;
		}
	}

	return rc;
}
#endif
