/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _IntegrationContracts_IMessageProcessor_h_
#define _IntegrationContracts_IMessageProcessor_h_


#include "Integration/IIntMessage.h"

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

