/*
 *  Created on: Jun 13, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _IntegrationContracts_IINTEGRATIONAPPCONTEXT_H_
#define _IntegrationContracts_IINTEGRATIONAPPCONTEXT_H_


#include "ICafObject.h"

#include "Integration/IIntegrationObject.h"

namespace Caf {

struct __declspec(novtable) IIntegrationAppContext : public ICafObject {
	CAF_DECL_UUID("CC12C628-50C1-4E74-998D-3A9C961FA06F")

	virtual SmartPtrIIntegrationObject getIntegrationObject(
			const std::string& id) const = 0;

	virtual void getIntegrationObject(
			const IID& iid,
			void **ppv) const = 0;

	typedef std::deque<SmartPtrICafObject> CObjectCollection;
	CAF_DECLARE_SMART_POINTER(CObjectCollection);

	virtual SmartPtrCObjectCollection getIntegrationObjects(const IID& iid) const = 0;
};

CAF_DECLARE_SMART_INTERFACE_POINTER(IIntegrationAppContext);

}

#endif
