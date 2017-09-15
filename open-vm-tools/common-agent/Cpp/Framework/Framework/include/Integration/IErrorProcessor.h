/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _IntegrationContracts_IErrorProcessor_h_
#define _IntegrationContracts_IErrorProcessor_h_


#include "Integration/IIntMessage.h"

namespace Caf {

/// TODO - describe interface
struct __declspec(novtable)
	IErrorProcessor : public ICafObject
{
	CAF_DECL_UUID("7ed3c23c-609a-4e42-9463-ed98da222d0a")

	virtual SmartPtrIIntMessage processErrorMessage(
		const SmartPtrIIntMessage& message) = 0;
};

CAF_DECLARE_SMART_INTERFACE_POINTER(IErrorProcessor);

}

#endif // #ifndef _IntegrationContracts_IErrorProcessor_h_

