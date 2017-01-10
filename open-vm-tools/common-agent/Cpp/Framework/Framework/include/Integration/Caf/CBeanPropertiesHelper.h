/*
 *	 Author: bwilliams
 *  Created: Jul 2009
 *
 *	Copyright (C) 2009-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CBeanPropertiesHelper_h_
#define CBeanPropertiesHelper_h_

namespace Caf {

CAF_DECLARE_CLASS_AND_SMART_POINTER(CBeanPropertiesHelper);

class INTEGRATIONCAF_LINKAGE CBeanPropertiesHelper {
public:
	static SmartPtrCBeanPropertiesHelper create(
			const IBean::Cprops& properties);

public:
	CBeanPropertiesHelper();
	virtual ~CBeanPropertiesHelper();

public:
	void initialize(
			const IBean::Cprops& properties);

public:
	std::string getRequiredString(
			const std::string& key) const;

	std::string getOptionalString(
			const std::string& key,
			const std::string& defaultVal = std::string()) const;

	bool getRequiredBool(
			const std::string& key) const;

	bool getOptionalBool(
			const std::string& key,
			const bool defaultVal = false) const;

private:
	bool m_isInitialized;
	IBean::Cprops _properties;

private:
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(CBeanPropertiesHelper);
};

}

#endif // #ifndef CBeanPropertiesHelper_h_
