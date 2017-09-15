/*
 *  Created on: Aug 6, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _IntegrationContracts_IPhased_h
#define _IntegrationContracts_IPhased_h


#include "ICafObject.h"

namespace Caf {

struct __declspec(novtable) IPhased : public ICafObject {
	CAF_DECL_UUID("CAE354D0-E212-4030-8CB7-23C92D59C6A3")

	virtual int32 getPhase() const = 0;
};
CAF_DECLARE_SMART_INTERFACE_POINTER(IPhased);

};

#endif
