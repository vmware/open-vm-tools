/*
 *	 Author: bwilliams
 *  Created: Jan 21, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CLoggingUtils_H_
#define CLoggingUtils_H_

#ifndef WIN32
#include <sys/time.h>
#include "Common/CLoggingUtils.h"
#endif

namespace Caf {

class CLoggingUtils;
CAF_DECLARE_SMART_POINTER(CLoggingUtils);

class COMMONAGGREGATOR_LINKAGE CLoggingUtils {
private:
    typedef std::map<std::string, std::string> PropertyMap;

public:
	static bool isConsoleAppenderUsed();
	static void setStartupConfigFile(
			const std::string& configFile = "log4cpp_config",
			const std::string& logDir = std::string());
	static std::string getConfigFile();
	static void resetConfigFile();
	static void setLogDir(const std::string& logDir);

public:
	CLoggingUtils();

private:
	static SmartPtrCLoggingUtils getInstance();

	static void loadConfig(const std::string& configFile);

	void loadProperties();

private:
	static GRecMutex _sOpMutex;
	static SmartPtrCLoggingUtils _sInstance;

	std::string _configFile;
	PropertyMap _properties;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CLoggingUtils);
};

}

#endif /* CLoggingUtils_H_ */
