/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _IntegrationContracts_ITransformer_h_
#define _IntegrationContracts_ITransformer_h_


#include "Integration/IIntMessage.h"

namespace Caf {

/// TODO - describe interface
struct __declspec(novtable)
	ITransformer : public ICafObject
{
	CAF_DECL_UUID("1f2a6ecb-f842-4e09-82a8-89eaf64ec98b")

	virtual SmartPtrIIntMessage transformMessage(
		const SmartPtrIIntMessage& message) = 0;
};

CAF_DECLARE_SMART_INTERFACE_POINTER(ITransformer);

}

#endif // #ifndef _IntegrationContracts_ITransformer_h_

