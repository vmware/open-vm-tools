/*
 *	Author: bwilliams
 *	Created: May 3, 2012
 *
 *	Copyright (c) 2012 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef CInstallProvider_h_
#define CInstallProvider_h_

namespace Caf {

/// Sends responses/errors back to the client.
class CInstallProvider : public IInvokedProvider {
public:
	CInstallProvider();
	virtual ~CInstallProvider();

public: // IInvokedProvider
	const std::string getProviderNamespace() const {
		return "caf";
	}

	const std::string getProviderName() const {
		return "InstallProvider";
	}

	const std::string getProviderVersion() const {
		return "1.0.0";
	}

	const SmartPtrCSchemaDoc getSchema() const;

	void collect(const IProviderRequest& request, IProviderResponse& response) const;

	void invoke(const IProviderRequest& request, IProviderResponse& response) const;

private:
	SmartPtrCDataClassInstanceDoc createDataClassInstance(
		const SmartPtrCInstallProviderSpecDoc& installProviderSpec) const;

	bool isCurrentOS(const PACKAGE_OS_TYPE& packageOSType) const;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CInstallProvider);
};

}

#endif // #ifndef CInstallProvider_h_
