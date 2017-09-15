/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _IntegrationContracts_ISubscribableChannel_h_
#define _IntegrationContracts_ISubscribableChannel_h_


#include "Integration/IMessageHandler.h"

namespace Caf {

/// TODO - describe interface
struct __declspec(novtable)
	ISubscribableChannel : public ICafObject
{
	CAF_DECL_UUID("14d7e980-1b98-4453-b27e-8c058fb705b9")

	virtual void subscribe(
		const SmartPtrIMessageHandler& messageHandler) = 0;
	virtual void unsubscribe(
		const SmartPtrIMessageHandler& messageHandler) = 0;
};

CAF_DECLARE_SMART_INTERFACE_POINTER(ISubscribableChannel);

}

#endif // #ifndef _IntegrationContracts_ISubscribableChannel_h_

