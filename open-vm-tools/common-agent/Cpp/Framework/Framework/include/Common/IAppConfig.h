/*
 *	 Author: mdonahue
 *  Created: Jan 19, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef IAPPCONFIG_H_
#define IAPPCONFIG_H_

#include "Common/IConfigParams.h"

#include "ICafObject.h"
#include "Common/IAppConfig.h"

namespace Caf {

struct __declspec(novtable)
IAppConfig : public ICafObject {
	CAF_DECL_UUID("e57f2252-ce11-4d15-9338-aa928333f7a3")

	virtual SmartPtrIConfigParams getParameters(const std::string& sectionName) = 0;

	virtual bool getString(
			const std::string& sectionName,
			const std::string& parameterName,
			std::string& value,
			const IConfigParams::EParamDisposition disposition = IConfigParams::PARAM_REQUIRED) = 0;

	virtual bool getUint32(
			const std::string& sectionName,
			const std::string& parameterName,
			uint32& value,
			const IConfigParams::EParamDisposition disposition = IConfigParams::PARAM_REQUIRED) = 0;

	virtual bool getInt32(
			const std::string& sectionName,
			const std::string& parameterName,
			int32& value,
			const IConfigParams::EParamDisposition disposition = IConfigParams::PARAM_REQUIRED) = 0;

	virtual bool getBoolean(
			const std::string& sectionName,
			const std::string& parameterName,
			bool& value,
			const IConfigParams::EParamDisposition disposition = IConfigParams::PARAM_REQUIRED) = 0;

	virtual bool getGlobalString(
			const std::string& parameterName,
			std::string& value,
			const IConfigParams::EParamDisposition disposition = IConfigParams::PARAM_REQUIRED) = 0;

	virtual bool getGlobalUint32(
			const std::string& parameterName,
			uint32& value,
			const IConfigParams::EParamDisposition disposition = IConfigParams::PARAM_REQUIRED) = 0;

	virtual bool getGlobalInt32(
			const std::string& parameterName,
			int32& value,
			const IConfigParams::EParamDisposition disposition = IConfigParams::PARAM_REQUIRED) = 0;

	virtual bool getGlobalBoolean(
			const std::string& parameterName,
			bool& value,
			const IConfigParams::EParamDisposition disposition = IConfigParams::PARAM_REQUIRED) = 0;

	virtual std::string resolveValue(const std::string& value) = 0;
};

CAF_DECLARE_SMART_INTERFACE_POINTER(IAppConfig);

SmartPtrIAppConfig COMMONAGGREGATOR_LINKAGE getAppConfig();

SmartPtrIAppConfig COMMONAGGREGATOR_LINKAGE getAppConfig(const std::string& configFile);

SmartPtrIAppConfig COMMONAGGREGATOR_LINKAGE getAppConfig(const Cdeqstr& configFileCollection);

SmartPtrIAppConfig COMMONAGGREGATOR_LINKAGE getAppConfigAppend(const std::string& configFile);

SmartPtrIAppConfig COMMONAGGREGATOR_LINKAGE getAppConfigAppend(const Cdeqstr& configFileCollection);
}

#endif /* IAPPCONFIG_H_ */
