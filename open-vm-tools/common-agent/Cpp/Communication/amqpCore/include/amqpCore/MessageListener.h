/*
 *  Created on: Aug 2, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPINTEGRATIONCORE_MESSAGELISTENER_H_
#define AMQPINTEGRATIONCORE_MESSAGELISTENER_H_


#include "ICafObject.h"

#include "Integration/IIntMessage.h"

namespace Caf { namespace AmqpIntegration {

struct __declspec(novtable) MessageListener : public ICafObject {
	CAF_DECL_UUID("5B2B7C47-ACEF-4FB1-97A0-1594D05AB4D9")

	virtual void onMessage(const SmartPtrIIntMessage& message) = 0;
};
CAF_DECLARE_SMART_INTERFACE_POINTER(MessageListener);

}}

#endif /* AMQPINTEGRATIONCORE_MESSAGELISTENER_H_ */
