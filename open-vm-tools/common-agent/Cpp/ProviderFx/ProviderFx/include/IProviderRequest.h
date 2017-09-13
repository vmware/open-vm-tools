/*
 *	Author: brets
 *	Created: October 28, 2015
 *
 *	Copyright (c) 2015 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef _ProviderContracts_IProviderRequest_H_
#define _ProviderContracts_IProviderRequest_H_

namespace Caf {

struct __declspec(novtable) IProviderRequest {

	virtual const UUID getClientId() const = 0;

	virtual const UUID getRequestId() const = 0;

	virtual const std::string getPmeId() const = 0;

	virtual const std::vector<std::string> getCommandLine() const = 0;

	virtual const SmartPtrCAttachmentCollectionDoc getAttachments() const = 0;

	virtual const SmartPtrCProviderCollectInstancesDoc getCollectInstances() const = 0;

	virtual const SmartPtrCProviderInvokeOperationDoc getInvokeOperations() const = 0;
};

//CAF_DECLARE_SMART_INTERFACE_POINTER(IProviderRequest);

}

#endif // #ifndef _ProviderContracts_IProviderRequest_H_
