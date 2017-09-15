/*
 *  Created on: Jan 26, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _IntegrationContracts_ICHANNELINTERCEPTORSUPPORT_H_
#define _IntegrationContracts_ICHANNELINTERCEPTORSUPPORT_H_


#include "ICafObject.h"

#include "Integration/IChannelInterceptor.h"

namespace Caf {

struct __declspec(novtable)
IChannelInterceptorSupport : public ICafObject {
	CAF_DECL_UUID("C8F3CBAF-B1EB-4AD8-920C-EFE5EE25638A")

	typedef std::list<SmartPtrIChannelInterceptor> InterceptorCollection;
	virtual void setInterceptors(
			const InterceptorCollection& interceptors) = 0;
};

CAF_DECLARE_SMART_INTERFACE_POINTER(IChannelInterceptorSupport);

}

#endif /* _IntegrationContracts_ICHANNELINTERCEPTORSUPPORT_H_ */
