/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _IntegrationContracts_IMessageSplitter_h_
#define _IntegrationContracts_IMessageSplitter_h_


#include "Integration/IIntMessage.h"

namespace Caf {

/// TODO - describe interface
struct __declspec(novtable)
	IMessageSplitter : public ICafObject
{
	CAF_DECL_UUID("89a25ba3-113d-4efc-af46-522feda304ac")

	typedef std::deque<SmartPtrIIntMessage> CMessageCollection;
	CAF_DECLARE_SMART_POINTER(CMessageCollection);

	virtual SmartPtrCMessageCollection splitMessage(
		const SmartPtrIIntMessage& message) = 0;
};

CAF_DECLARE_SMART_INTERFACE_POINTER(IMessageSplitter);

}

#endif // #ifndef _IntegrationContracts_IMessageSplitter_h_

