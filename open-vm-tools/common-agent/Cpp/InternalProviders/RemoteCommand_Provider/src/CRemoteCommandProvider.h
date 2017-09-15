/*
 *	Author: bwilliams
 *	Created: May 3, 2012
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CRemoteCommandProvider_h_
#define CRemoteCommandProvider_h_


#include "IInvokedProvider.h"

#include "Doc/CafCoreTypesDoc/CAttachmentCollectionDoc.h"
#include "Doc/ProviderResultsDoc/CSchemaDoc.h"

namespace Caf {

/// Sends responses/errors back to the client.
class CRemoteCommandProvider : public IInvokedProvider {
public:
	CRemoteCommandProvider();
	virtual ~CRemoteCommandProvider();

public: // IInvokedProvider
	const std::string getProviderNamespace() const {
		return "caf";
	}

	const std::string getProviderName() const {
		return "RemoteCommandProvider";
	}

	const std::string getProviderVersion() const {
		return "1.0.0";
	}

	const SmartPtrCSchemaDoc getSchema() const;

	void collect(const IProviderRequest& request, IProviderResponse& response) const;

	void invoke(const IProviderRequest& request, IProviderResponse& response) const;

private:
	void executeScript(
		const std::string& script,
		const std::string& scriptResultsDir,
		const std::deque<std::string>& scriptParameters,
		const std::string& attachmentUris) const;

	std::string createAttachmentUris(
		const std::deque<std::string>& attachmentNamesStr,
		const SmartPtrCAttachmentCollectionDoc& attachmentCollection) const;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CRemoteCommandProvider);
};

}

#endif // #ifndef CRemoteCommandProvider_h_
