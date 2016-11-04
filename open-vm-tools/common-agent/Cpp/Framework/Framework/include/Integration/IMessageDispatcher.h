/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _IntegrationContracts_IMessageDispatcher_h_
#define _IntegrationContracts_IMessageDispatcher_h_


#include "Integration/IIntMessage.h"
#include "Integration/IMessageHandler.h"

namespace Caf {

/// TODO - describe interface
struct __declspec(novtable)
	IMessageDispatcher : public ICafObject
{
	CAF_DECL_UUID("639d8ead-6406-4c41-9a36-74d065f5dd6b")

	virtual void addHandler(
		const SmartPtrIMessageHandler& messageHandler) = 0;

	virtual void removeHandler(
		const SmartPtrIMessageHandler& messageHandler) = 0;

	virtual bool dispatch(
		const SmartPtrIIntMessage& message) = 0;
};

CAF_DECLARE_SMART_INTERFACE_POINTER(IMessageDispatcher);

}

#endif // #ifndef _IntegrationContracts_IMessageDispatcher_h_

