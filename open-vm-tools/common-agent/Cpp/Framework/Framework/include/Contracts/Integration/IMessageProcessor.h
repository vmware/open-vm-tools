/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (c) 2010 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef _IntegrationContracts_IMessageProcessor_h_
#define _IntegrationContracts_IMessageProcessor_h_

namespace Caf {

/// TODO - describe interface
struct __declspec(novtable)
	IMessageProcessor : public ICafObject
{
	CAF_DECL_UUID("68770787-c44e-457e-bf8d-20c64d37bfee")

	virtual SmartPtrIIntMessage processMessage(
		const SmartPtrIIntMessage& message) = 0;
};

CAF_DECLARE_SMART_INTERFACE_POINTER(IMessageProcessor);

}

#endif // #ifndef _IntegrationContracts_IMessageProcessor_h_

