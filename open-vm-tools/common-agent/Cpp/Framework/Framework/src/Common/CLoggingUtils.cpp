/*k
 *	Author: bwilliams
 *  Created: Jan 21, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "Common/CLoggingUtils.h"
#include "Exception/CCafException.h"
#include <iostream>
#include <fstream>

using namespace Caf;

GRecMutex CLoggingUtils::_sOpMutex;
SmartPtrCLoggingUtils CLoggingUtils::_sInstance;

bool CLoggingUtils::isConsoleAppenderUsed() {
	bool rc = false;

	const log4cpp::AppenderSet appenders =
		log4cpp::Category::getRoot().getAllAppenders();
	for (log4cpp::AppenderSet::const_iterator iter = appenders.begin();
		(iter != appenders.end() && !rc); iter++) {
		const log4cpp::Appender* appender = *iter;
		rc = CStringUtils::isEqualIgnoreCase(appender->getName(), "console");
	}

	return rc;
}

void CLoggingUtils::setStartupConfigFile(
		const std::string& configFile,
		const std::string& logDir) {
	CAF_CM_STATIC_FUNC("CLoggingUtils", "setStartupConfigFile");
	CAF_CM_VALIDATE_STRING(configFile);

#ifndef WIN32
	char configFileFullBuf[ 32768 ];
	::realpath(configFile.c_str(), configFileFullBuf);
	const std::string configFileFull = configFileFullBuf;
#else
	wchar_t w_configFileFullBuf[ 32768 ];
	const std::wstring w_configFile = CStringUtils::convertNarrowToWide(configFile.c_str());
	DWORD dwResult = GetFullPathName(w_configFile.c_str(), 32768, w_configFileFullBuf, NULL);
	// Convert the string back to narrow
	const std::string configFileFull = CStringUtils::convertWideToNarrow(std::wstring(w_configFileFullBuf, dwResult));
#endif

	if (!FileSystemUtils::doesFileExist(configFileFull)) {
		CAF_CM_EXCEPTION_VA1(ERROR_FILE_NOT_FOUND, "Config file does not exist - %s", configFileFull.c_str());
	}

	CAutoMutexLockUnlockRaw oLock(&_sOpMutex);
	if (_sInstance.IsNull()) {
		_sInstance.CreateInstance();
	}
	_sInstance->_configFile = configFileFull;
	_sInstance->loadProperties();

	if (logDir.empty()) {
		_sInstance->loadConfig(configFileFull);
	} else {
		setLogDir(logDir);
	}
}

SmartPtrCLoggingUtils CLoggingUtils::getInstance() {
	CAF_CM_STATIC_FUNC("CLoggingUtils", "getInstance");

	CAutoMutexLockUnlockRaw oLock(&_sOpMutex);
	if (!_sInstance) {
		CAF_CM_EXCEPTION_VA0(ERROR_INVALID_STATE, "Config file not set");
	}

	return _sInstance;
}

CLoggingUtils::CLoggingUtils() :
	CAF_CM_INIT_LOG("CLoggingUtils") {
}

void CLoggingUtils::loadConfig(const std::string& configFile) {
	CAF_CM_STATIC_FUNC_LOG("CLoggingUtils", "loadConfig");
	CAF_CM_VALIDATE_STRING(configFile);

	if (!FileSystemUtils::doesFileExist(configFile)) {
		CAF_CM_EXCEPTION_VA1(ERROR_FILE_NOT_FOUND, "Config file does not exist - %s", configFile.c_str());
	}

	// Make sure existing unmanaged appenders are cleaned up
	std::vector<log4cpp::Category*>* categories = log4cpp::Category::getCurrentCategories();
	for (std::vector<log4cpp::Category*>::const_iterator catIter = categories->begin();
		  catIter != categories->end(); catIter++) {
		log4cpp::Category* category = *catIter;
		log4cpp::AppenderSet appenders = category->getAllAppenders();
		for (log4cpp::AppenderSet::const_iterator appIter = appenders.begin(); appIter != appenders.end(); appIter++) {
			log4cpp::Appender* appender = *appIter;
			if (!category->ownsAppender(appender)) {
				// Appender lifecycle is NOT managed by the category...need to manually delete
				// First remove the appender before another thread attempts to access it during logging
				category->removeAppender(appender);
				delete appender;
			}
		}
	}

	// Use property configurator to load configuration file..to set up configured Categories and Appenders
	try {
		log4cpp::PropertyConfigurator::configure(configFile);
	} catch (log4cpp::ConfigureFailure e) {
		std::cout << "Log4cpp Error: " << e.what() << std::endl;
	}

	CAF_CM_LOG_DEBUG_VA1("Using log config file - %s", configFile.c_str());
}

std::string CLoggingUtils::getConfigFile() {
	return getInstance()->_configFile;
}

void CLoggingUtils::resetConfigFile() {
	loadConfig(getInstance()->_configFile);
}

void CLoggingUtils::setLogDir(const std::string& logDir) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CLoggingUtils", "setLogDir");
	CAF_CM_VALIDATE_STRING(logDir);

	if (!FileSystemUtils::doesDirectoryExist(logDir)) {
		CAF_CM_LOG_INFO_VA1("Creating log dir - %s", logDir.c_str());
		FileSystemUtils::createDirectory(logDir);
	}

	// Create a temporary file for storing the new config
	std::string tmpFileName = FileSystemUtils::buildPath(logDir, "log4cpp_config_tmp");
	static const std::string SRCH_STR = ".fileName";
	static const size_t SRCH_STR_SIZE = SRCH_STR.length();

	std::ofstream tmpCfg(tmpFileName.c_str(), std::ios_base::out | std::ios_base::trunc);
	PropertyMap src = getInstance()->_properties;
	for (PropertyMap::const_iterator iter = src.begin(); iter != src.end(); iter++) {
		tmpCfg << (*iter).first << "=";
		if (iter->first.rfind(SRCH_STR) == iter->first.length() - SRCH_STR_SIZE) {
			const std::string basename = FileSystemUtils::getBasename(iter->second);
			tmpCfg << FileSystemUtils::buildPath(logDir, basename);
		} else {
			tmpCfg << (*iter).second;
		}
		tmpCfg << std::endl;
	}
	tmpCfg.close();

	loadConfig(tmpFileName);
	FileSystemUtils::removeFile(tmpFileName);
}

void CLoggingUtils::loadProperties() {
	CAF_CM_FUNCNAME_VALIDATE("loadProperties");
	CAF_CM_VALIDATE_STRING(_configFile);
	_properties.clear();

	static const size_t BUFF_SIZE = 2048;
	char buffer[BUFF_SIZE];

	std::string line, property, name, value, prefix;
	size_t pos = 0;

	std::ifstream properties(_configFile.c_str());
	while (properties.getline(buffer, BUFF_SIZE)) {
		line = CStringUtils::trim(buffer);

		// Check for comment.  If at beginning, ignore whole line.  If not, extract portion up to comment
		pos = line.find('#');
		if (pos == std::string::npos) {
			property = line;
		} else if (pos > 0) {
			property = line.substr(0, pos);
		} else {
			continue;
		}

		// Parse the property into name/value
		pos = property.find('=');
		if (pos != std::string::npos) {
			name = CStringUtils::trim(property.substr(0, pos));
			value = CStringUtils::trim(property.substr(pos + 1, property.size() - pos));

			_properties[name] = value;
		} else {
			continue;
		}
	}
}
