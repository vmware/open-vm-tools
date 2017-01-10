/*
 *	Author: bwilliams
 *	Created: May 3, 2012
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CTestInfraProvider_h_
#define CTestInfraProvider_h_


#include "IInvokedProvider.h"

#include "Doc/ProviderResultsDoc/CSchemaDoc.h"
#include "Doc/SchemaTypesDoc/CDataClassInstanceDoc.h"

namespace Caf {

/// Sends responses/errors back to the client.
class CTestInfraProvider : public IInvokedProvider {
public:
	CTestInfraProvider();
	virtual ~CTestInfraProvider();

public: // IInvokedProvider
	const std::string getProviderNamespace() const {
		return "cafTestInfra";
	}

	const std::string getProviderName() const {
		return "CafTestInfraProvider";
	}

	const std::string getProviderVersion() const {
		return "1.0.0";
	}

	const SmartPtrCSchemaDoc getSchema() const;

	void collect(const IProviderRequest& request, IProviderResponse& response) const;

	void invoke(const IProviderRequest& request, IProviderResponse& response) const;

private:
	SmartPtrCDataClassInstanceDoc createDataClassInstance(
		const std::string& name,
		const std::string& value) const;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CTestInfraProvider);
};

}

#endif // #ifndef CTestInfraProvider_h_
