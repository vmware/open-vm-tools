/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _IntegrationContracts_ILifecycle_h_
#define _IntegrationContracts_ILifecycle_h_

namespace Caf {

/// TODO - describe interface
struct __declspec(novtable)
	ILifecycle : public ICafObject
{
	CAF_DECL_UUID("180845f8-c956-46b3-8a1b-ef5061cc927a")

	virtual void start(const uint32 timeoutMs) = 0;
	virtual void stop(const uint32 timeoutMs) = 0;
	virtual bool isRunning() const = 0;
};

CAF_DECLARE_SMART_INTERFACE_POINTER(ILifecycle);

}

#endif // #ifndef _IntegrationContracts_ILifecycle_h_

