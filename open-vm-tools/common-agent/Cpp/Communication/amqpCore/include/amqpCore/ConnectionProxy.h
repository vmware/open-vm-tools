/*
 *  Created on: May 25, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPINTEGRATIONCORE_CONNECTIONPROXY_H_
#define AMQPINTEGRATIONCORE_CONNECTIONPROXY_H_

#include "amqpClient/api/Connection.h"

namespace Caf { namespace AmqpIntegration {

/**
 * @ingroup IntObjImpl
 * @brief Interface on objects used to proxy connection objects for the various
 * connection factories.
 */
struct __declspec(novtable) ConnectionProxy : public Connection {

	/**
	 * @brief Return the proxied Connection object
	 * @return the proxied connection
	 */
	virtual SmartPtrConnection getTargetConnection() = 0;
};
CAF_DECLARE_SMART_INTERFACE_POINTER(ConnectionProxy);

}}

#endif
