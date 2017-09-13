/*
 *	Author: bwilliams
 *	Created: May 3, 2012
 *
 *	Copyright (c) 2012 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef CProviderDriver_h_
#define CProviderDriver_h_

namespace Caf {

struct IInvokedProvider; // Forward declaration

/// Sends responses/errors back to the client.
class PROVIDERFX_LINKAGE CProviderDriver {
private:
	CProviderDriver(IInvokedProvider& provider);
	virtual ~CProviderDriver();

public:
	static int processProviderCommandline(IInvokedProvider& provider, int argc, char* argv[]);

private:
	int processProviderCommandline(int argc, char* argv[]);

	void collectSchema(const std::string& outputDir) const;

	void executeRequest(const std::string& requestPath) const;

	void executeCollectInstances(
		const SmartPtrCProviderRequestDoc request,
		const SmartPtrCProviderCollectInstancesDoc doc) const;

	void executeInvokeOperation(
		const SmartPtrCProviderRequestDoc request,
		const SmartPtrCProviderInvokeOperationDoc doc) const;

	SmartPtrCRequestIdentifierDoc createRequestId(
		const SmartPtrCProviderRequestDoc request,
		const SmartPtrCActionClassDoc actionClass,
		const UUID jobId) const;

	SmartPtrCActionClassDoc findActionClass(
		const std::string srchClassNamespace,
		const std::string srchClassName,
		const std::string srchClassVersion,
		const std::string srchOperationName) const;

	void saveProviderResponse(
		const std::string attachmentFilePath) const;

private:
	IInvokedProvider& _provider;
	const SmartPtrCSchemaDoc _schema;
	const std::string _providerNamespace;
	const std::string _providerName;
	const std::string _providerVersion;
	std::vector<std::string> _commandLineArgs;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CProviderDriver);
};

}

#endif // #ifndef CProviderDriver_h_
