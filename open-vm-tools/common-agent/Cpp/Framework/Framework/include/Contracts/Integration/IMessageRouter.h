/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (c) 2010 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef _IntegrationContracts_IMessageRouter_h_
#define _IntegrationContracts_IMessageRouter_h_

namespace Caf {

/// TODO - describe interface
struct __declspec(novtable)
	IMessageRouter : public ICafObject
{
	CAF_DECL_UUID("27ed0739-e527-469b-882f-196d532be0bd")

	virtual void routeMessage(const SmartPtrIIntMessage& message) = 0;
};

CAF_DECLARE_SMART_INTERFACE_POINTER(IMessageRouter);

}

#endif // #ifndef _IntegrationContracts_IMessageRouter_h_

