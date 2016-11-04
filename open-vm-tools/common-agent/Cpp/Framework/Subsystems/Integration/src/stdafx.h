/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef stdafx_h_
#define stdafx_h_

//{{CAF_SUBSYSTEM}}

#include <CommonDefines.h>

#include "Integration/IIntegrationObject.h"
#include <Integration.h>

namespace Caf {

// template function to create integration objects
template <typename Object>
SmartPtrIIntegrationObject CreateIntegrationObject() {
	Object object;
	object.CreateInstance();
	return object;
};

// function pointer for integration object creation
typedef SmartPtrIIntegrationObject(*FNOBJECT_CREATOR)();

// map of configuration section names to function pointers that create the
// integration object
typedef std::map<std::string, FNOBJECT_CREATOR> ObjectCreatorMap;

// map of configuration section names to a pair:
// pair.first  - function pointer that creates the integration object
// pair.second - true if the integration object produces messages
typedef std::map<
		std::string,
		std::pair<FNOBJECT_CREATOR, bool> > MessageHandlerObjectCreatorMap;
}

#endif // #ifndef stdafx_h_
