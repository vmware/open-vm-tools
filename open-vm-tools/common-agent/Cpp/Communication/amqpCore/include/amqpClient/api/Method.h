/*
 *  Created on: May 10, 2012
 *      Author: mdonahue
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPCLIENTAPI_METHOD_H_
#define AMQPCLIENTAPI_METHOD_H_


#include "ICafObject.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApi
 * @brief Base interface to AMQP methods
 */
struct __declspec(novtable) Method : public ICafObject {

	/** @return the method's class ID */
	virtual uint16 getProtocolClassId() = 0;

	/** @return the method's method ID */
	virtual uint16 getProtocolMethodId() = 0;

	/** @return the method's friendly name */
	virtual std::string getProtocolMethodName() = 0;
};
CAF_DECLARE_SMART_INTERFACE_POINTER(Method);

}}

#endif
