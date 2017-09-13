/*
 *	 Author: bwilliams
 *  Created: Jan 28, 2015
 *
 *	Copyright (c) 2011 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef IAPPCONFIGWRITE_H_
#define IAPPCONFIGWRITE_H_

namespace Caf {

struct __declspec(novtable)
IAppConfigWrite : public ICafObject {
	CAF_DECL_UUID("3cf708c6-b92d-46c3-83d8-edeccecf5ba4")

	virtual void setString(
			const std::string& sectionName,
			const std::string& parameterName,
			const std::string& value) = 0;

	virtual void setUint32(
			const std::string& sectionName,
			const std::string& parameterName,
			const uint32& value) = 0;

	virtual void setBoolean(
			const std::string& sectionName,
			const std::string& parameterName,
			const bool& value) = 0;

	virtual void setGlobalString(
			const std::string& parameterName,
			const std::string& value) = 0;

	virtual void setGlobalUint32(
			const std::string& parameterName,
			const uint32& value) = 0;

	virtual void setGlobalBoolean(
			const std::string& parameterName,
			const bool& value) = 0;
};

CAF_DECLARE_SMART_INTERFACE_POINTER(IAppConfigWrite);

SmartPtrIAppConfigWrite COMMONAGGREGATOR_LINKAGE getAppConfigWrite();

SmartPtrIAppConfigWrite COMMONAGGREGATOR_LINKAGE getAppConfigWrite(const std::string& configFile);

SmartPtrIAppConfigWrite COMMONAGGREGATOR_LINKAGE getAppConfigWrite(const Cdeqstr& configFileCollection);
}

#endif /* IAPPCONFIGWRITE_H_ */
