/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _IntegrationContracts_IMessageHandler_h_
#define _IntegrationContracts_IMessageHandler_h_


#include "Integration/IIntMessage.h"

namespace Caf {

/// TODO - describe interface
struct __declspec(novtable)
	IMessageHandler : public ICafObject
{
	CAF_DECL_UUID("39a78b6f-f326-4739-b8ad-9e90a827745a")

	virtual void handleMessage(
		const SmartPtrIIntMessage& message) = 0;

	virtual SmartPtrIIntMessage getSavedMessage() const = 0;

	virtual void clearSavedMessage() = 0;
};

CAF_DECLARE_SMART_INTERFACE_POINTER(IMessageHandler);

}

#endif // #ifndef _IntegrationContracts_IMessageHandler_h_

