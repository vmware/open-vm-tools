/*
 *  Created on: Jan 25, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _IntegrationContracts_ICHANNELINTERCEPTORINSTANCE_H_
#define _IntegrationContracts_ICHANNELINTERCEPTORINSTANCE_H_

namespace Caf {

struct __declspec(novtable)
	IChannelInterceptorInstance : public ICafObject
{
	CAF_DECL_UUID("566C38A8-FF13-4E31-814E-A18130C009F6")

	virtual uint32 getOrder() const = 0;

	virtual bool isChannelIdMatched(const std::string& channelId) const = 0;
};

CAF_DECLARE_SMART_INTERFACE_POINTER(IChannelInterceptorInstance);

}
#endif /* _IntegrationContracts_ICHANNELINTERCEPTORINSTANCE_H_ */
