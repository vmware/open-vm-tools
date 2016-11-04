/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _IntegrationContracts_IIntegrationObject_h_
#define _IntegrationContracts_IIntegrationObject_h_


#include "Integration/IDocument.h"

namespace Caf {

/// TODO - describe interface
struct __declspec(novtable)
	IIntegrationObject : public ICafObject
{
	CAF_DECL_UUID("295fa2c8-01a7-4102-b13e-8fcac00b3e5f")

	virtual void initialize(
		const IBean::Cargs& ctorArgs,
		const IBean::Cprops& properties,
		const SmartPtrIDocument& configSection) = 0;

	virtual std::string getId() const = 0;
};

CAF_DECLARE_SMART_INTERFACE_POINTER(IIntegrationObject);

typedef std::map<std::string, SmartPtrIIntegrationObject> CIntegrationObjectCollection;
CAF_DECLARE_SMART_POINTER(CIntegrationObjectCollection);

}

#endif // #ifndef _IntegrationContracts_IIntegrationObject_h_

