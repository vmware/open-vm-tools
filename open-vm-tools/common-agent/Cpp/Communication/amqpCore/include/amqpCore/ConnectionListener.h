/*
 *  Created on: Jun 2, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPINTEGRATIONCORE_CONNECTIONLISTENER_H_
#define AMQPINTEGRATIONCORE_CONNECTIONLISTENER_H_

#include "ICafObject.h"

#include "amqpCore/Connection.h"

namespace Caf { namespace AmqpIntegration {

/**
 * @ingroup IntObjImpl
 * @brief Connection event notification callback interface
 */
struct __declspec(novtable) ConnectionListener : public ICafObject {
	CAF_DECL_UUID("8F249C38-CF91-4553-9C68-3D714626A7EC")

	/**
	 * @brief Connection created event
	 * @param connection the Connection that has been created
	 */
	virtual void onCreate(const SmartPtrConnection& connection) = 0;

	/**
	 * @brief Connection closed event
	 * @param connection the Connection that has been closed
	 */
	virtual void onClose(const SmartPtrConnection& connection) = 0;
};
CAF_DECLARE_SMART_INTERFACE_POINTER(ConnectionListener);

}}

#endif /* AMQPINTEGRATIONCORE_CONNECTIONLISTENER_H_ */
