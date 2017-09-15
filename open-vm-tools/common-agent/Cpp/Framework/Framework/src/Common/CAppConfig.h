/*
 *	 Author: mdonahue
 *  Created: Jan 19, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CAPPCONFIG_H_
#define CAPPCONFIG_H_

#include "Common/IConfigParams.h"

#include "Common/CConfigParams.h"

#include "Common/IAppConfig.h"
#include "Common/IAppConfigWrite.h"

namespace Caf {

CAF_DECLARE_CLASS_AND_SMART_POINTER(CAppConfig);

class COMMONAGGREGATOR_LINKAGE CAppConfig :
	public IAppConfig,
	public IAppConfigWrite {
public: // IAppConfig

	SmartPtrIConfigParams getParameters(const std::string& sectionName);

	bool
		getString(
			const std::string& sectionName,
			const std::string& parameterName,
			std::string& value,
			const IConfigParams::EParamDisposition disposition =
				IConfigParams::PARAM_REQUIRED);

	bool
		getUint32(
			const std::string& sectionName,
			const std::string& parameterName,
			uint32& value,
			const IConfigParams::EParamDisposition disposition =
				IConfigParams::PARAM_REQUIRED);

	bool
		getInt32(
			const std::string& sectionName,
			const std::string& parameterName,
			int32& value,
			const IConfigParams::EParamDisposition disposition =
				IConfigParams::PARAM_REQUIRED);

	bool
		getBoolean(
			const std::string& sectionName,
			const std::string& parameterName,
			bool& value,
			const IConfigParams::EParamDisposition disposition =
				IConfigParams::PARAM_REQUIRED);

	bool
		getGlobalString(
			const std::string& parameterName,
			std::string& value,
			const IConfigParams::EParamDisposition disposition =
				IConfigParams::PARAM_REQUIRED);

	bool
		getGlobalUint32(
			const std::string& parameterName,
			uint32& value,
			const IConfigParams::EParamDisposition disposition =
				IConfigParams::PARAM_REQUIRED);

	bool
		getGlobalInt32(
			const std::string& parameterName,
			int32& value,
			const IConfigParams::EParamDisposition disposition =
				IConfigParams::PARAM_REQUIRED);

	bool
		getGlobalBoolean(
			const std::string& parameterName,
			bool& value,
			const IConfigParams::EParamDisposition disposition =
				IConfigParams::PARAM_REQUIRED);

	std::string resolveValue(const std::string& value);

	static SmartPtrIAppConfig getInstance();

	static SmartPtrIAppConfig getInstance(const std::string& configFile);

	static SmartPtrIAppConfig getInstance(const Cdeqstr& configFileCollection);

	static SmartPtrIAppConfig getInstanceAppend(const std::string& configFile);

	static SmartPtrIAppConfig getInstanceAppend(const Cdeqstr& configFileCollection);

public: // IAppConfigWrite
	void setString(const std::string& sectionName,
						const std::string& parameterName,
						const std::string& value);

	void setUint32(const std::string& sectionName,
						const std::string& parameterName,
						const uint32& value);

	void setInt32(const std::string& sectionName,
					  const std::string& parameterName,
					  const int32& value);

	void setBoolean(const std::string& sectionName,
						 const std::string& parameterName,
						 const bool& value);

	void setGlobalString(const std::string& parameterName,
								const std::string& value);

	void setGlobalUint32(const std::string& parameterName,
								const uint32& value);

	void setGlobalInt32(const std::string& parameterName,
							  const int32& value);

	void setGlobalBoolean(const std::string& parameterName,
								 const bool& value);

	static SmartPtrIAppConfigWrite getInstanceWrite();

	static SmartPtrIAppConfigWrite getInstanceWrite(const std::string& configFile);

	static SmartPtrIAppConfigWrite getInstanceWrite(const Cdeqstr& configFileCollection);

private:
	CAppConfig();
	virtual ~CAppConfig();

private:
	void initialize();
	void initialize(const std::string& configFile);
	void initialize(const Cdeqstr& configFileCollection);

	void append(const std::string& configFile);
	void append(const Cdeqstr& configFileCollection);

private:
	SmartPtrIConfigParams internalLoadParameters(
			const std::string& sectionName);

	void internalLoadParameters(
			const std::string& sectionName,
			const std::string& configFileName,
			const SmartPtrCConfigParams& configParams);

	void validateGlobals(const SmartPtrIConfigParams& globals);

private:
	std::string calcCurrentConfigPath(
			const std::string& configFile) const;

	std::string calcConfigPath(
			const std::string& configFilename) const;

	std::string calcDefaultConfigPath(
			const std::string& area,
			const std::string& configFile) const;

	std::string calcDefaultConfigDir(
			const std::string& area) const;

private:
	static const char* _sGlobalsSectionName;
	static GRecMutex _sOpMutex;
	static SmartPtrCAppConfig _sInstance;
	bool _isInitialized;
	typedef std::map<std::string, SmartPtrIConfigParams> CParamSections;
	CParamSections _cachedSections;
	typedef std::map<GRegex*, std::string> CGlobalReplacements;
	CGlobalReplacements _globalReplacements;
	GRegex* _envPattern;
	GRegex* _varPattern;
	SmartPtrIConfigParams _globals;

	Cdeqstr _configFileCollection;

private:
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(CAppConfig);
	friend class TCafObject<CAppConfig>;
};

}

#endif /* CAPPCONFIG_H_ */
