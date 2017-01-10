/*
 *	Author: brets
 *	Created: November 5, 2015
 *
 *	Copyright (C) 2015-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CProviderRequest_h_
#define CProviderRequest_h_


#include "IProviderRequest.h"

#include "Doc/CafCoreTypesDoc/CAttachmentCollectionDoc.h"
#include "Doc/ProviderRequestDoc/CProviderCollectInstancesDoc.h"
#include "Doc/ProviderRequestDoc/CProviderInvokeOperationDoc.h"
#include "Doc/ProviderRequestDoc/CProviderRequestDoc.h"

namespace Caf {

// Request information forwarded to the provider
class CProviderRequest : public IProviderRequest {
public:
	CProviderRequest(const SmartPtrCProviderRequestDoc request,
			const std::vector<std::string> commandLine) :
				_request(request),
				_commandLine(commandLine) {
	}
	virtual ~CProviderRequest() {};

public: // IProviderRequest
	const UUID getClientId() const {
		return _request->getClientId();
	}

	const UUID getRequestId() const {
		return _request->getRequestId();
	}

	const std::string getPmeId() const {
		return _request->getPmeId();
	}

	const std::vector<std::string> getCommandLine() const {
		return _commandLine;
	}

	const SmartPtrCAttachmentCollectionDoc getAttachments() const {
		return _request->getAttachmentCollection();
	}

	const SmartPtrCProviderCollectInstancesDoc getCollectInstances() const {
		return _collectInstances;
	}
	const SmartPtrCProviderInvokeOperationDoc getInvokeOperations() const {
		return _invokeOperations;
	}

public:
	void setCollectInstances(SmartPtrCProviderCollectInstancesDoc doc) {
		_collectInstances = doc;
		_invokeOperations = NULL;
	}

	void setInvokeOperations(SmartPtrCProviderInvokeOperationDoc doc) {
		_collectInstances = NULL;
		_invokeOperations = doc;
	}

private:
	const SmartPtrCProviderRequestDoc _request;
	const std::vector<std::string> _commandLine;
	SmartPtrCProviderCollectInstancesDoc _collectInstances;
	SmartPtrCProviderInvokeOperationDoc _invokeOperations;
};

}

#endif // #ifndef CProviderRequest_h_
