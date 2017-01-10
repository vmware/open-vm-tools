/*
 *	Author: bwilliams
 *	Created: May 3, 2012
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CInstallProvider_h_
#define CInstallProvider_h_


#include "IInvokedProvider.h"
#include "Doc/CafInstallRequestDoc/CafInstallRequestDocTypes.h"

#include "Doc/CafInstallRequestDoc/CInstallProviderSpecDoc.h"
#include "Doc/ProviderResultsDoc/CSchemaDoc.h"
#include "Doc/SchemaTypesDoc/CDataClassInstanceDoc.h"

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
