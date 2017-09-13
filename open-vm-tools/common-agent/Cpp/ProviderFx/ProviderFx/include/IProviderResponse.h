/*
 *	Author: brets
 *	Created: October 28, 2015
 *
 *	Copyright (c) 2015 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef _ProviderContracts_IProviderResponse_H_
#define _ProviderContracts_IProviderResponse_H_

namespace Caf {

struct __declspec(novtable) IProviderResponse {

	virtual void addInstance(const SmartPtrCDataClassInstanceDoc instance) = 0;

	virtual void addAttachment(const SmartPtrCAttachmentDoc attachment) = 0;
};

//CAF_DECLARE_SMART_INTERFACE_POINTER(IProviderResponse);

}

#endif // #ifndef _ProviderContracts_IProviderResponse_H_
