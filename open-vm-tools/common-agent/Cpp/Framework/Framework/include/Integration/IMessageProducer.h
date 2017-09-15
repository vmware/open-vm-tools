/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _IntegrationContracts_IMessageProducer_h_
#define _IntegrationContracts_IMessageProducer_h_

namespace Caf {

/// TODO - describe interface
struct __declspec(novtable)
	IMessageProducer : public ICafObject
{
	CAF_DECL_UUID("14430bc5-8556-48f8-b37f-c2f24a50d8dd")

	virtual bool isMessageProducer() const = 0;
};

CAF_DECLARE_SMART_INTERFACE_POINTER(IMessageProducer);

}

#endif // #ifndef _IntegrationContracts_IMessageProducer_h_

