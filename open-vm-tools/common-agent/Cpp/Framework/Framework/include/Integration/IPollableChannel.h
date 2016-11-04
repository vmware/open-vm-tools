/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _IntegrationContracts_IPollableChannel_h_
#define _IntegrationContracts_IPollableChannel_h_


#include "Integration/Dependencies/CPollerMetadata.h"
#include "Integration/IIntMessage.h"

namespace Caf {

/// TODO - describe interface
struct __declspec(novtable)
	IPollableChannel : public ICafObject
{
	CAF_DECL_UUID("a7ce8841-f37d-489a-a299-e148c5ff6b11")

	virtual SmartPtrIIntMessage receive() = 0;
	virtual SmartPtrIIntMessage receive(const int32 timeout) = 0;
	virtual SmartPtrCPollerMetadata getPollerMetadata() const = 0;
};

CAF_DECLARE_SMART_INTERFACE_POINTER(IPollableChannel);

}

#endif // #ifndef _IntegrationContracts_IPollableChannel_h_

