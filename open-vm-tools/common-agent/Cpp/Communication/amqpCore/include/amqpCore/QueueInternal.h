/*
 *  Created on: Jun 14, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPINTEGRATIONCORE_QUEUEINTERNAL_H_
#define AMQPINTEGRATIONCORE_QUEUEINTERNAL_H_


#include "ICafObject.h"

#include "amqpCore/Queue.h"

namespace Caf { namespace AmqpIntegration {

/**
 * @ingroup IntObjImpl
 * @brief Interface allowing for internal management of queue integration objects
 */
struct __declspec(novtable) QueueInternal : public ICafObject {
	CAF_DECL_UUID("E15F4DA8-C4DC-4813-82B9-1B15C69915D1")

	/**
	 * @brief Sets the delegated Queue object
	 * @param queue the delegated queue
	 */
	virtual void setQueueInternal(SmartPtrQueue queue) = 0;
};
CAF_DECLARE_SMART_INTERFACE_POINTER(QueueInternal);

}}
#endif
