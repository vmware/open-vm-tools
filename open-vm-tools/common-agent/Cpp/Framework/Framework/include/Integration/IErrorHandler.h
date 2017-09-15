/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (c) 2010 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef _IntegrationContracts_IErrorHandler_h_
#define _IntegrationContracts_IErrorHandler_h_


#include "Integration/IIntMessage.h"
#include "Integration/IThrowable.h"

namespace Caf {

/// TODO - describe interface
struct __declspec(novtable)
	IErrorHandler : public ICafObject
{
	CAF_DECL_UUID("da0e8646-43fb-4d43-a31b-f736c3978d48")

public: // Read operations
	virtual void handleError(
		const SmartPtrIThrowable& throwable,
		const SmartPtrIIntMessage& message) const = 0;
};

CAF_DECLARE_SMART_INTERFACE_POINTER(IErrorHandler);

}

#endif // #ifndef _IntegrationContracts_IErrorHandler_h_

