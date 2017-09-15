/*
 *  Created on: May 11, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPCLIENTAPI_CONTENTHEADER_H_
#define AMQPCLIENTAPI_CONTENTHEADER_H_


#include "ICafObject.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApi
 * @brief Base interface for all AMQP content headers
 */
struct __declspec(novtable) ContentHeader : public ICafObject {

	/** @return the content header class id */
	virtual uint16 getClassId() = 0;

	/** @return the content header friendly class name */
	virtual std::string getClassName() = 0;
};
CAF_DECLARE_SMART_INTERFACE_POINTER(ContentHeader);

}}

#endif
