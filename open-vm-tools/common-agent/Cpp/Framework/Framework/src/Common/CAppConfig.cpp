/*
 *	 Author: mdonahue
 *  Created: Jan 19, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Common/CConfigParams.h"
#include "Common/CLoggingUtils.h"
#include "Exception/CCafException.h"
#include "Common/IAppConfigWrite.h"
#include "Common/IAppConfig.h"
#include "Common/IConfigParams.h"
#include "CAppConfig.h"

using namespace Caf;

SmartPtrIAppConfig Caf::getAppConfig() {
	return CAppConfig::getInstance();
}

SmartPtrIAppConfig Caf::getAppConfig(const std::string& configFile) {
	return CAppConfig::getInstance(configFile);
}

SmartPtrIAppConfig Caf::getAppConfig(const Cdeqstr& configFileCollection) {
	return CAppConfig::getInstance(configFileCollection);
}

SmartPtrIAppConfig Caf::getAppConfigAppend(const std::string& configFile) {
	return CAppConfig::getInstanceAppend(configFile);
}

SmartPtrIAppConfig Caf::getAppConfigAppend(const Cdeqstr& configFileCollection) {
	return CAppConfig::getInstanceAppend(configFileCollection);
}

SmartPtrIAppConfigWrite Caf::getAppConfigWrite() {
	return CAppConfig::getInstanceWrite();
}

SmartPtrIAppConfigWrite Caf::getAppConfigWrite(const std::string& configFile) {
	return CAppConfig::getInstanceWrite(configFile);
}

SmartPtrIAppConfigWrite Caf::getAppConfigWrite(const Cdeqstr& configFileCollection) {
	return CAppConfig::getInstanceWrite(configFileCollection);
}

const char* CAppConfig::_sGlobalsSectionName = "globals";
GRecMutex CAppConfig::_sOpMutex;
SmartPtrCAppConfig CAppConfig::_sInstance;

CAppConfig::CAppConfig() :
	_isInitialized(false),
	_envPattern(NULL),
	_varPattern(NULL),
	CAF_CM_INIT("CAppConfig") {
}

CAppConfig::~CAppConfig() {
	for (CGlobalReplacements::iterator iter = _globalReplacements.begin(); iter
		!= _globalReplacements.end(); ++iter) {
		g_regex_unref(iter->first);
	}

	if (_envPattern) {
		g_regex_unref(_envPattern);
	}

	if (_varPattern) {
		g_regex_unref(_varPattern);
	}
}

void CAppConfig::initialize() {
	CAF_CM_FUNCNAME("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);

	CAutoMutexLockUnlockRaw oLock(&_sOpMutex);

	std::string appConfigEnv;
	CEnvironmentUtils::readEnvironmentVar("CAF_APPCONFIG", appConfigEnv);

	if (appConfigEnv.empty()) {
		CAF_CM_EXCEPTION_VA0(ERROR_TAG_NOT_FOUND, "CAF_APPCONFIG env var isn't set.");
	}

	const Cdeqstr configFileCollection = CStringUtils::split(appConfigEnv, ';');

	initialize(configFileCollection);
}

void CAppConfig::initialize(const std::string& configFile) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(configFile);

	Cdeqstr configFileCollection;
	configFileCollection.push_back(configFile);

	initialize(configFileCollection);
}

void CAppConfig::initialize(const Cdeqstr& configFileCollection) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STL(configFileCollection);

	CAutoMutexLockUnlockRaw oLock(&_sOpMutex);

	for (TConstIterator<Cdeqstr> strIter(configFileCollection); strIter; strIter++) {
		const std::string configFile = *strIter;
		const std::string configPath = calcConfigPath(configFile);
		if (! configPath.empty()) {
			_configFileCollection.push_back(configPath);
		}
	}

	// Add a pattern for environment variable lookup
	_envPattern = g_regex_new(
		"\\$\\{env\\:(.+)\\}",
		(GRegexCompileFlags)(G_REGEX_OPTIMIZE | G_REGEX_UNGREEDY | G_REGEX_RAW),
		(GRegexMatchFlags)0,
		NULL);

	_varPattern = g_regex_new(
		"\\$\\{var\\:(.+)\\}",
		(GRegexCompileFlags)(G_REGEX_OPTIMIZE | G_REGEX_UNGREEDY | G_REGEX_RAW),
		(GRegexMatchFlags)0,
		NULL);

	_globals = internalLoadParameters(_sGlobalsSectionName);
	validateGlobals(_globals);

	_isInitialized = true;
}

void CAppConfig::append(const std::string& configFile) {
	CAF_CM_FUNCNAME_VALIDATE("append");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(configFile);

	Cdeqstr configFileCollection;
	configFileCollection.push_back(configFile);

	append(configFileCollection);
}

void CAppConfig::append(const Cdeqstr& configFileCollection) {
	CAF_CM_FUNCNAME_VALIDATE("append");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STL(configFileCollection);

	CAutoMutexLockUnlockRaw oLock(&_sOpMutex);

	for (TConstIterator<Cdeqstr> strIter(configFileCollection); strIter; strIter++) {
		const std::string configFile = *strIter;
		const std::string configPath = calcConfigPath(configFile);
		if (! configPath.empty()) {
			_configFileCollection.push_back(configPath);
		}
	}
}

SmartPtrIConfigParams CAppConfig::getParameters(const std::string& sectionName) {
	CAF_CM_FUNCNAME_VALIDATE("getParameters");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(sectionName);

	SmartPtrIConfigParams params;
	CAutoMutexLockUnlockRaw oLock(&_sOpMutex);
	CParamSections::const_iterator cachedSection = _cachedSections.find(sectionName);
	if (_cachedSections.end() == cachedSection) {
		if (::strcmp(sectionName.c_str(), _sGlobalsSectionName) == 0) {
			params = _globals;
		} else {
			params = internalLoadParameters(sectionName);
			_cachedSections.insert(CParamSections::value_type(sectionName, params));
		}
	} else {
		params = cachedSection->second;
	}

	return params;
}

bool CAppConfig::getString(
	const std::string& sectionName,
	const std::string& parameterName,
	std::string& value,
	const IConfigParams::EParamDisposition disposition) {
	CAF_CM_FUNCNAME("getString");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(sectionName);
	CAF_CM_VALIDATE_STRING(parameterName);

	bool paramFound = false;
	SmartPtrIConfigParams params = getParameters(sectionName);
	GVariant* param = params->lookup(
		parameterName.c_str(),
		IConfigParams::PARAM_OPTIONAL);
	if (param) {
		if (g_variant_is_of_type(param, G_VARIANT_TYPE_STRING)) {
			value = g_variant_get_string(param, NULL);
			paramFound = true;
		} else {
			CAF_CM_EXCEPTION_VA1(DISP_E_TYPEMISMATCH, "%s exists but is not a string.", parameterName.c_str());
		}
	} else {
		if (IConfigParams::PARAM_REQUIRED == disposition) {
			CAF_CM_EXCEPTION_VA2(ERROR_TAG_NOT_FOUND,
				"Required config parameter [%s] is missing from section [%s]",
				parameterName.c_str(),
				sectionName.c_str());
		}
	}

	return paramFound;
}

bool CAppConfig::getUint32(
	const std::string& sectionName,
	const std::string& parameterName,
	uint32& value,
	const IConfigParams::EParamDisposition disposition) {
	CAF_CM_FUNCNAME("getUint32");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(sectionName);
	CAF_CM_VALIDATE_STRING(parameterName);

	bool paramFound = false;

	SmartPtrIConfigParams params = getParameters(sectionName);
	GVariant* param = params->lookup(
		parameterName.c_str(),
		IConfigParams::PARAM_OPTIONAL);
	if (param) {
		if (g_variant_is_of_type(param, G_VARIANT_TYPE_INT32)) {
			value = static_cast<uint32>(g_variant_get_int32(param));
			paramFound = true;
		} else {
			std::string valueStr;
			getString(sectionName, parameterName, valueStr, disposition);
			value = CStringConv::fromString<uint32>(valueStr);
		}
	} else {
		if (IConfigParams::PARAM_REQUIRED == disposition) {
			CAF_CM_EXCEPTION_VA2(ERROR_TAG_NOT_FOUND,
				"Required config parameter [%s] is missing from section [%s]",
				parameterName.c_str(),
				sectionName.c_str());
		}
	}

	return paramFound;
}

bool CAppConfig::getInt32(
	const std::string& sectionName,
	const std::string& parameterName,
	int32& value,
	const IConfigParams::EParamDisposition disposition) {
	CAF_CM_FUNCNAME("getInt32");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(sectionName);
	CAF_CM_VALIDATE_STRING(parameterName);

	bool paramFound = false;

	SmartPtrIConfigParams params = getParameters(sectionName);
	GVariant* param = params->lookup(
		parameterName.c_str(),
		IConfigParams::PARAM_OPTIONAL);
	if (param) {
		if (g_variant_is_of_type(param, G_VARIANT_TYPE_INT32)) {
			value = g_variant_get_int32(param);
			paramFound = true;
		} else {
			std::string valueStr;
			getString(sectionName, parameterName, valueStr, disposition);
			value = CStringConv::fromString<int32>(valueStr);
		}
	} else {
		if (IConfigParams::PARAM_REQUIRED == disposition) {
			CAF_CM_EXCEPTION_VA2(ERROR_TAG_NOT_FOUND,
				"Required config parameter [%s] is missing from section [%s]",
				parameterName.c_str(),
				sectionName.c_str());
		}
	}

	return paramFound;
}

bool CAppConfig::getBoolean(
	const std::string& sectionName,
	const std::string& parameterName,
	bool& value,
	const IConfigParams::EParamDisposition disposition) {
	CAF_CM_FUNCNAME("getBoolean");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(sectionName);
	CAF_CM_VALIDATE_STRING(parameterName);

	value = false;

	std::string tmpValue;
	bool paramFound = getString(
		sectionName,
		parameterName,
		tmpValue,
		IConfigParams::PARAM_OPTIONAL);

	if (paramFound) {
		if ("true" == tmpValue) {
			value = true;
		} else if ("false" == tmpValue) {
			value = false;
		} else {
			CAF_CM_EXCEPTION_VA1(DISP_E_TYPEMISMATCH, "%s exists but is not a boolean (true or false).", parameterName.c_str());
		}
	} else {
		if (IConfigParams::PARAM_REQUIRED == disposition) {
			CAF_CM_EXCEPTION_VA2(ERROR_TAG_NOT_FOUND,
				"Required config parameter [%s] is missing from section [%s]",
				parameterName.c_str(),
				sectionName.c_str());
		}
	}

	return paramFound;
}

bool CAppConfig::getGlobalString(
	const std::string& parameterName,
	std::string& value,
	const IConfigParams::EParamDisposition disposition) {
	return getString(_sGlobalsSectionName, parameterName, value, disposition);
}

bool CAppConfig::getGlobalUint32(
	const std::string& parameterName,
	uint32& value,
	const IConfigParams::EParamDisposition disposition) {
	return getUint32(_sGlobalsSectionName, parameterName, value, disposition);
}

bool CAppConfig::getGlobalInt32(
	const std::string& parameterName,
	int32& value,
	const IConfigParams::EParamDisposition disposition) {
	return getInt32(_sGlobalsSectionName, parameterName, value, disposition);
}

bool CAppConfig::getGlobalBoolean(
	const std::string& parameterName,
	bool& value,
	const IConfigParams::EParamDisposition disposition) {
	return getBoolean(_sGlobalsSectionName, parameterName, value, disposition);
}

void CAppConfig::setString(
	const std::string& sectionName,
	const std::string& parameterName,
	const std::string& value) {
	CAF_CM_FUNCNAME_VALIDATE("setString");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(sectionName);
	CAF_CM_VALIDATE_STRING(parameterName);
	CAF_CM_VALIDATE_STRING(value);

	SmartPtrIConfigParams params = getParameters(sectionName);
	params->insert(g_strdup(parameterName.c_str()), g_variant_new_string(
		value.c_str()));
}

void CAppConfig::setUint32(
	const std::string& sectionName,
	const std::string& parameterName,
	const uint32& value) {
	CAF_CM_FUNCNAME_VALIDATE("setUint32");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(sectionName);
	CAF_CM_VALIDATE_STRING(parameterName);

	SmartPtrIConfigParams params = getParameters(sectionName);
	params->insert(g_strdup(parameterName.c_str()), g_variant_new_int32(
		value));
}

void CAppConfig::setInt32(
	const std::string& sectionName,
	const std::string& parameterName,
	const int32& value) {
	CAF_CM_FUNCNAME_VALIDATE("setInt32");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(sectionName);
	CAF_CM_VALIDATE_STRING(parameterName);

	SmartPtrIConfigParams params = getParameters(sectionName);
	params->insert(g_strdup(parameterName.c_str()), g_variant_new_int32(
		value));
}

void CAppConfig::setBoolean(
	const std::string& sectionName,
	const std::string& parameterName,
	const bool& value) {
	CAF_CM_FUNCNAME_VALIDATE("setBoolean");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(sectionName);
	CAF_CM_VALIDATE_STRING(parameterName);

	SmartPtrIConfigParams params = getParameters(sectionName);
	params->insert(g_strdup(parameterName.c_str()), g_variant_new_boolean(
		value));
}

void CAppConfig::setGlobalString(
	const std::string& parameterName,
	const std::string& value) {
	setString(_sGlobalsSectionName, parameterName, value);
}

void CAppConfig::setGlobalUint32(
	const std::string& parameterName,
	const uint32& value) {
	setUint32(_sGlobalsSectionName, parameterName, value);
}

void CAppConfig::setGlobalInt32(
	const std::string& parameterName,
	const int32& value) {
	setInt32(_sGlobalsSectionName, parameterName, value);
}

void CAppConfig::setGlobalBoolean(
	const std::string& parameterName,
	const bool& value) {
	setBoolean(_sGlobalsSectionName, parameterName, value);
}

std::string CAppConfig::resolveValue(const std::string& value) {
	CAF_CM_FUNCNAME("resolveValue");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	std::string rc = value;
	GMatchInfo *matchInfo = NULL;
	GError* error = NULL;

	try {
		if (value.length()) {
			gchar *match = NULL;
			if (g_regex_match(
					_varPattern,
					rc.c_str(),
					G_REGEX_MATCH_NOTBOL,
					&matchInfo)) {
				match = g_match_info_fetch(matchInfo, 1);
				CAF_CM_VALIDATE_STRINGPTRA(match);
				std::string varName(match);
				g_free(match);
				match = NULL;
				size_t pos = varName.find(':', 4);
				std::string section = _sGlobalsSectionName;
				if (pos != std::string::npos) {
					section = varName.substr(0, pos);
					varName = varName.substr(pos + 1, varName.length() - pos - 1);
				}

				std::string configVal;
				bool resolved = false;
				// one of these will work - the other two will fail
				try {
					getString(
							section,
							varName,
							configVal,
							IConfigParams::PARAM_REQUIRED);
					resolved = true;
				} catch (CCafException *ex) {
					if (ex->getError() == DISP_E_TYPEMISMATCH) {
						ex->Release();
					} else {
						throw ex;
					}
				}

				if (!resolved) {
					try {
						int32 uval = 0;
						getInt32(
								section,
								varName,
								uval,
								IConfigParams::PARAM_REQUIRED);
						configVal = CStringConv::toString<int32>(uval);
						resolved = true;
					} catch (CCafException *ex) {
						if (ex->getError() == DISP_E_TYPEMISMATCH) {
							ex->Release();
						} else {
							throw ex;
						}
					}
				}

				if (!resolved) {
					try {
						bool bval = false;
						getBoolean(
								section,
								varName,
								bval,
								IConfigParams::PARAM_REQUIRED);
						configVal = bval ? "true" : "false";
						resolved = true;
					} catch (CCafException *ex) {
						if (ex->getError() == DISP_E_TYPEMISMATCH) {
							ex->Release();
						} else {
							throw ex;
						}
					}
				}

				if (!resolved) {
					CAF_CM_EXCEPTION_VA1(
							ERROR_INVALID_DATA,
							"Unable to resolve %s",
							value.c_str());
				}

				gchar *replaced = g_regex_replace_literal(
						_varPattern,
						rc.c_str(),
						-1,
						0,
						configVal.c_str(),
						G_REGEX_MATCH_NOTBOL,
						&error);
				if (error) {
					throw error;
				}
				rc = replaced;
				g_free(replaced);
			}
			g_match_info_free(matchInfo);
			matchInfo = NULL;

			if (g_regex_match(
					_envPattern,
					rc.c_str(),
					G_REGEX_MATCH_NOTBOL,
					&matchInfo)) {
				match = g_match_info_fetch(matchInfo, 1);
				CAF_CM_VALIDATE_STRINGPTRA(match);
				std::string envVarName(match);
				g_free(match);
				match = NULL;
				#ifdef WIN32
				char* dupbuf = NULL;
				size_t duplen = 0;
				errno_t duprc = ::_dupenv_s(&dupbuf, &duplen, envVarName.c_str());
				const std::string dupval(dupbuf && ::strlen(dupbuf) ? dupbuf : "");
				::free(dupbuf);
				dupbuf = NULL;
				const char* envVarValue = dupval.c_str();
				#else
				const char* envVarValue = ::getenv(envVarName.c_str());
				#endif

				if (envVarValue && ::strlen(envVarValue)) {
					gchar* replaced = g_regex_replace_literal(
						_envPattern,
						rc.c_str(),
						-1,
						0,
						envVarValue,
						G_REGEX_MATCH_NOTBOL,
						&error);
					if (error) {
						throw error;
					}
					rc = replaced;
					g_free(replaced);
				} else {
					CAF_CM_EXCEPTION_VA1(
							ERROR_TAG_NOT_FOUND,
							"Referenced environment variable is not set: %s",
							envVarName.c_str());
				}

			}
			g_match_info_free(matchInfo);
			matchInfo = NULL;
		}
	}
	CAF_CM_CATCH_ALL;
	if (matchInfo) {
		g_match_info_free(matchInfo);
	}
	CAF_CM_THROWEXCEPTION;
	return rc;
}

SmartPtrIAppConfig CAppConfig::getInstance() {
	CAutoMutexLockUnlockRaw oLock(&_sOpMutex);
	if (!_sInstance) {
		SmartPtrCAppConfig appConfig;
		appConfig.CreateInstance();
		appConfig->initialize();
		_sInstance = appConfig;
	}

	return _sInstance;
}

SmartPtrIAppConfig CAppConfig::getInstance(const std::string& configFile) {
	CAutoMutexLockUnlockRaw oLock(&_sOpMutex);
	_sInstance = NULL;
	SmartPtrCAppConfig appConfig;
	appConfig.CreateInstance();
	appConfig->initialize(configFile);
	_sInstance = appConfig;

	return _sInstance;
}

SmartPtrIAppConfig CAppConfig::getInstance(const Cdeqstr& configFileCollection) {
	CAutoMutexLockUnlockRaw oLock(&_sOpMutex);
	_sInstance = NULL;
	SmartPtrCAppConfig appConfig;
	appConfig.CreateInstance();
	appConfig->initialize(configFileCollection);
	_sInstance = appConfig;

	return _sInstance;
}

SmartPtrIAppConfig CAppConfig::getInstanceAppend(
		const std::string& configFile) {
	CAF_CM_STATIC_FUNC_VALIDATE("CAppConfig", "getInstanceAppend");
	CAutoMutexLockUnlockRaw oLock(&_sOpMutex);
	CAF_CM_VALIDATE_SMARTPTR(_sInstance);
	_sInstance->append(configFile);

	return _sInstance;
}

SmartPtrIAppConfig CAppConfig::getInstanceAppend(
		const Cdeqstr& configFileCollection) {
	CAF_CM_STATIC_FUNC_VALIDATE("CAppConfig", "getInstanceAppend");
	CAutoMutexLockUnlockRaw oLock(&_sOpMutex);
	CAF_CM_VALIDATE_SMARTPTR(_sInstance);
	_sInstance->append(configFileCollection);

	return _sInstance;
}

SmartPtrIAppConfigWrite CAppConfig::getInstanceWrite() {
	CAutoMutexLockUnlockRaw oLock(&_sOpMutex);
	if (!_sInstance) {
		SmartPtrCAppConfig appConfig;
		appConfig.CreateInstance();
		appConfig->initialize();
		_sInstance = appConfig;
	}

	return _sInstance;
}

SmartPtrIAppConfigWrite CAppConfig::getInstanceWrite(const std::string& configFile) {
	CAutoMutexLockUnlockRaw oLock(&_sOpMutex);
	_sInstance = NULL;
	SmartPtrCAppConfig appConfig;
	appConfig.CreateInstance();
	appConfig->initialize(configFile);
	_sInstance = appConfig;

	return _sInstance;
}

SmartPtrIAppConfigWrite CAppConfig::getInstanceWrite(const Cdeqstr& configFileCollection) {
	CAutoMutexLockUnlockRaw oLock(&_sOpMutex);
	_sInstance = NULL;
	SmartPtrCAppConfig appConfig;
	appConfig.CreateInstance();
	appConfig->initialize(configFileCollection);
	_sInstance = appConfig;

	return _sInstance;
}

SmartPtrIConfigParams CAppConfig::internalLoadParameters(const std::string& sectionName) {
	CAF_CM_FUNCNAME_VALIDATE("internalLoadParameters");
	CAF_CM_VALIDATE_STRING(sectionName);

	// Always return at least an empty collection
	SmartPtrCConfigParams configParams;
	configParams.CreateInstance();
	configParams->initialize(
		sectionName,
		CConfigParams::EKeysManaged,
		CConfigParams::EValuesManaged);

	for (TConstIterator<Cdeqstr> strIter(_configFileCollection); strIter; strIter++) {
		const std::string configFile = *strIter;
		internalLoadParameters(sectionName, configFile, configParams);
	}

	return configParams;
}

void CAppConfig::internalLoadParameters(
		const std::string& sectionName,
		const std::string& configFileName,
		const SmartPtrCConfigParams& configParams) {
	CAF_CM_FUNCNAME("internalLoadParameters");
	CAF_CM_VALIDATE_STRING(sectionName);
	CAF_CM_VALIDATE_STRING(configFileName);
	CAF_CM_VALIDATE_SMARTPTR(configParams);

	GKeyFile* configFile = NULL;
	GError* configError = NULL;
	gchar** keys = NULL;
	gchar* sValue = NULL;
	GMatchInfo* matchInfo = NULL;
	gchar* match = NULL;

	try {
		try {
			configFile = g_key_file_new();
			g_key_file_load_from_file(
				configFile,
				configFileName.c_str(),
				G_KEY_FILE_NONE,
				&configError);
			if (configError) {
				throw configError;
			}

			gsize numKeys = 0;
			keys = g_key_file_get_keys(
				configFile,
				sectionName.c_str(),
				&numKeys,
				&configError);

			if (numKeys) {
				bool isGlobals = (::strcmp(sectionName.c_str(), _sGlobalsSectionName)
					== 0);

				for (gsize idx = 0; idx < numKeys; idx++) {
					// There is no way to tell if a value is an integer or string.
					// We want to insert the value as either string or int32 so
					// try to read the value as an integer.  If it cannot be read
					// as an integer then insert it as a string.
					gint iValue = g_key_file_get_integer(
						configFile,
						sectionName.c_str(),
						keys[idx],
						&configError);
					if (!configError) {
						configParams->insert(g_strdup(keys[idx]), g_variant_new_int32(
							iValue));

						if (isGlobals) {
							// create a regular expression to apply against other
							// values in other sections
							std::string pattern("\\$\\{");
							pattern += keys[idx];
							pattern += "\\}";
							GRegex* regex = g_regex_new(
								pattern.c_str(),
								(GRegexCompileFlags)(G_REGEX_OPTIMIZE | G_REGEX_UNGREEDY | G_REGEX_RAW),
								(GRegexMatchFlags)0,
								&configError);
							if (configError) {
								throw configError;
							}

							const std::string iValueStr = CStringConv::toString<int32>(iValue);
							_globalReplacements.insert(CGlobalReplacements::value_type(
								regex, iValueStr));
						}
					} else {
						if (G_KEY_FILE_ERROR_INVALID_VALUE != configError->code) {
							throw configError;
						} else {
							g_error_free(configError);
							configError = NULL;
							sValue = g_key_file_get_string(
								configFile,
								sectionName.c_str(),
								keys[idx],
								&configError);
							if (configError) {
								throw configError;
							}

							// Check the value to see if it references an environment variable
							if (g_regex_match(
								_envPattern,
								sValue,
								G_REGEX_MATCH_NOTBOL,
								&matchInfo)) {
								// Replace the reference with the env variable value
								match = g_match_info_fetch(matchInfo, 1);
								if (*match) {
									std::string envVarName(match);
									g_free(match);
									match = NULL;
									#ifdef WIN32
									char* dupbuf = NULL;
									size_t duplen = 0;
									errno_t duprc = ::_dupenv_s(&dupbuf, &duplen, envVarName.c_str());
									const std::string dupval(dupbuf && ::strlen(dupbuf) ? dupbuf : "");
									::free(dupbuf);
									dupbuf = NULL;
									const char* envVarValue = dupval.c_str();
									#else
									const char* envVarValue = ::getenv(envVarName.c_str());
									#endif
									if (envVarValue && ::strlen(envVarValue)) {
										gchar* newValue = g_regex_replace_literal(
											_envPattern,
											sValue,
											-1,
											0,
											envVarValue,
											G_REGEX_MATCH_NOTBOL,
											&configError);
										if (configError) {
											throw configError;
										}

										g_free(sValue);
										sValue = newValue;
									} else {
										CAF_CM_EXCEPTION_VA1(ERROR_TAG_NOT_FOUND,
											"Referenced environment variable is not set: %s",
											envVarName.c_str());
									}
								} else {
									CAF_CM_EXCEPTION_VA0(ERROR_INTERNAL_ERROR,
										"${env:var} matched but subexpression #1 is null.");
								}
							}
							g_match_info_free(matchInfo);
							matchInfo = NULL;

							// Replace occurrences of ${global_var_name} with the appropriate global value
							for (CGlobalReplacements::const_iterator pattern =
								_globalReplacements.begin(); pattern
								!= _globalReplacements.end(); ++pattern) {
								if (g_regex_match(
									pattern->first,
									sValue,
									G_REGEX_MATCH_NOTBOL,
									NULL)) {
									gchar* newValue = g_regex_replace_literal(
										pattern->first,
										sValue,
										-1,
										0,
										pattern->second.c_str(),
										G_REGEX_MATCH_NOTBOL,
										&configError);
									if (configError) {
										throw configError;
									}

									g_free(sValue);
									sValue = newValue;
								}
							}

							// Add the variable to the collection
							configParams->insert(g_strdup(keys[idx]), g_variant_new_string(
								sValue));

							if (isGlobals) {
								// create a regular expression to apply against other
								// values in other sections
								std::string pattern("\\$\\{");
								pattern += keys[idx];
								pattern += "\\}";
								GRegex* regex = g_regex_new(
									pattern.c_str(),
									(GRegexCompileFlags)(G_REGEX_OPTIMIZE | G_REGEX_UNGREEDY | G_REGEX_RAW),
									(GRegexMatchFlags)0,
									&configError);
								if (configError) {
									throw configError;
								}

								_globalReplacements.insert(CGlobalReplacements::value_type(
									regex, sValue));
							}
						}

						g_free(sValue);
						sValue = NULL;
					}
				}
			}
		} catch (GError *e) {
			CAF_CM_EXCEPTION_VA0(e->code, e->message);
		}
	} catch (CCafException *e) {
		_cm_exception_ = e;
	}

	if (configFile) {
		g_key_file_free(configFile);
	}

	if (configError) {
		g_error_free(configError);
	}

	if (keys) {
		g_strfreev(keys);
	}

	if (sValue) {
		g_free(sValue);
	}

	if (matchInfo) {
		g_match_info_free(matchInfo);
	}

	if (match) {
		g_free(match);
	}

	if (_cm_exception_) {
		throw _cm_exception_;
	}
}

void CAppConfig::validateGlobals(const SmartPtrIConfigParams& globals) {
	CAF_CM_FUNCNAME("validateGlobals");
	CAF_CM_VALIDATE_INTERFACE(globals);

	GVariant* log_dir = globals->lookup(
		_sAppConfigGlobalParamLogDir,
		IConfigParams::PARAM_REQUIRED);
	CAF_CM_ASSERT(g_variant_is_of_type(log_dir, G_VARIANT_TYPE_STRING));
	const char* log_dir_val = g_variant_get_string(log_dir, NULL);
	CAF_CM_VALIDATE_STRINGPTRA(log_dir_val);

	GVariant* log_file = globals->lookup(
		_sAppConfigGlobalParamLogConfigFile,
		IConfigParams::PARAM_REQUIRED);
	CAF_CM_ASSERT(g_variant_is_of_type(log_file, G_VARIANT_TYPE_STRING));
	const char* log_file_val = g_variant_get_string(log_file, NULL);
	CAF_CM_VALIDATE_STRINGPTRA(log_file_val);
	CLoggingUtils::setStartupConfigFile(log_file_val);

	GVariant* thread_stack_size_kb = globals->lookup(
		_sAppConfigGlobalThreadStackSizeKb,
		IConfigParams::PARAM_REQUIRED);
	CAF_CM_ASSERT(g_variant_is_of_type(thread_stack_size_kb, G_VARIANT_TYPE_INT32));
}

std::string CAppConfig::calcConfigPath(
		const std::string& configFile) const {
	CAF_CM_FUNCNAME_VALIDATE("calcConfigPath");
	CAF_CM_VALIDATE_STRING(configFile);

	std::string rc;
	if (g_file_test(configFile.c_str(), G_FILE_TEST_IS_REGULAR)) {
		rc = configFile;
	}

	if (rc.empty()) {
		const std::string currentConfigPath = calcCurrentConfigPath(configFile);
		if (g_file_test(currentConfigPath.c_str(), G_FILE_TEST_IS_REGULAR)) {
			rc = currentConfigPath;
		}
	}

	if (rc.empty()) {
		const std::string pmeConfigPath = calcDefaultConfigPath("pme", configFile);
		if (g_file_test(pmeConfigPath.c_str(), G_FILE_TEST_IS_REGULAR)) {
			rc = pmeConfigPath;
		}
	}

	if (rc.empty()) {
		const std::string clientConfigPath = calcDefaultConfigPath("client", configFile);
		if (g_file_test(clientConfigPath.c_str(), G_FILE_TEST_IS_REGULAR)) {
			rc = clientConfigPath;
		}
	}

	return rc;
}

std::string CAppConfig::calcCurrentConfigPath(
		const std::string& configFile) const {
	CAF_CM_FUNCNAME_VALIDATE("calcCurrentConfigPath");
	CAF_CM_VALIDATE_STRING(configFile);

	const std::string currentDir = FileSystemUtils::getCurrentDir();
	const std::string configPath = FileSystemUtils::buildPath(
			currentDir, configFile);

	return configPath;
}

std::string CAppConfig::calcDefaultConfigPath(
		const std::string& area,
		const std::string& configFile) const {
	CAF_CM_FUNCNAME_VALIDATE("calcDefaultConfigPath");
	CAF_CM_VALIDATE_STRING(area);
	CAF_CM_VALIDATE_STRING(configFile);

	const std::string configDir = calcDefaultConfigDir(area);
	const std::string configPath = FileSystemUtils::buildPath(
			configDir, configFile);

	return configPath;
}

#ifdef WIN32
std::string CAppConfig::calcDefaultConfigDir(
		const std::string& area) const {
	CAF_CM_FUNCNAME("calcDefaultConfigDir");
	CAF_CM_VALIDATE_STRING(area);

	std::string programData;
	CEnvironmentUtils::readEnvironmentVar("ProgramData", programData);
	if (programData.empty()) {
		CAF_CM_EXCEPTION_VA0(ERROR_TAG_NOT_FOUND, "ProgramData env var isn't set.");
	}

	const std::string configDir = FileSystemUtils::buildPath(
			programData, "VMware", "VMware CAF", area, "config");

	return configDir;
}
#else
std::string CAppConfig::calcDefaultConfigDir(
		const std::string& area) const {
	CAF_CM_FUNCNAME_VALIDATE("calcDefaultConfigDir");
	CAF_CM_VALIDATE_STRING(area);

	const std::string configDir = FileSystemUtils::buildPath(
			G_DIR_SEPARATOR_S, "etc", "vmware-caf", area, "config");

	return configDir;
}
#endif
