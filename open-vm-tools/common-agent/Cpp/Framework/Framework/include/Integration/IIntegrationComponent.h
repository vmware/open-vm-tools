/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _IntegrationContracts_IIntegrationComponent_h_
#define _IntegrationContracts_IIntegrationComponent_h_


#include "Integration/IDocument.h"
#include "Integration/IIntegrationObject.h"

namespace Caf {

/// TODO - describe interface
struct __declspec(novtable)
	IIntegrationComponent : public ICafObject
{
	CAF_DECL_UUID("087e1494-4abe-4bb6-ae49-48f4510e057f")

	virtual bool isResponsible(
		const SmartPtrIDocument& configSection) const = 0;

	virtual SmartPtrIIntegrationObject createObject(
		const SmartPtrIDocument& configSection) const = 0;
};

CAF_DECLARE_SMART_INTERFACE_POINTER(IIntegrationComponent);

}

#endif // #ifndef _IntegrationContracts_IIntegrationComponent_h_

