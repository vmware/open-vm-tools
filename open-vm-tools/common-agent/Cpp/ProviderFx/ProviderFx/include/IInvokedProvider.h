/*
 *	Author: brets
 *	Created: October 28, 2015
 *
 *	Copyright (c) 2015 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef _ProviderContracts_IInvokedProvider_H_
#define _ProviderContracts_IInvokedProvider_H_

namespace Caf {

struct IProviderRequest; // Forward declaration
struct IProviderResponse; // Forward declaration

struct __declspec(novtable) IInvokedProvider {

	virtual const std::string getProviderNamespace() const = 0;
	virtual const std::string getProviderName() const = 0;
	virtual const std::string getProviderVersion() const = 0;

	virtual const SmartPtrCSchemaDoc getSchema() const = 0;

	virtual void collect(const IProviderRequest& request, IProviderResponse& response) const = 0;

	virtual void invoke(const IProviderRequest& request, IProviderResponse& response) const = 0;
};

//CAF_DECLARE_SMART_INTERFACE_POINTER(IInvokedProvider);

}

#endif // #ifndef _ProviderContracts_IInvokedProvider_H_
