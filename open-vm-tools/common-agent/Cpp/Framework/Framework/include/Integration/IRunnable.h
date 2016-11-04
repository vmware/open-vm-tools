/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _IntegrationContracts_IRunnable_h_
#define _IntegrationContracts_IRunnable_h_

namespace Caf {

/// TODO - describe interface
struct __declspec(novtable)
	IRunnable : public ICafObject
{
	CAF_DECL_UUID("a3ad671c-3d04-4eba-aaa4-8dcc9c43c959")

	virtual void run() = 0;
	virtual void cancel() = 0;
};

CAF_DECLARE_SMART_INTERFACE_POINTER(IRunnable);

}

#endif // #ifndef _IntegrationContracts_IRunnable_h_

