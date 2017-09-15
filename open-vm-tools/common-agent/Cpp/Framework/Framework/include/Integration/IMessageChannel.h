/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _IntegrationContracts_IMessageChannel_h_
#define _IntegrationContracts_IMessageChannel_h_


#include "Integration/IIntMessage.h"

namespace Caf {

/// TODO - describe interface
struct __declspec(novtable)
	IMessageChannel : public ICafObject
{
	CAF_DECL_UUID("d5192d01-9c26-4c1c-8966-66d7a108bcbf")

	virtual bool send(
		const SmartPtrIIntMessage& message) = 0;

	virtual bool send(
		const SmartPtrIIntMessage& message,
		const int32 timeout) = 0;
};

CAF_DECLARE_SMART_INTERFACE_POINTER(IMessageChannel);

}

#endif // #ifndef _IntegrationContracts_IMessageChannel_h_

