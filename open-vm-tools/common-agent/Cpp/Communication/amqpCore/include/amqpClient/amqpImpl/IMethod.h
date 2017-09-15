/*
 *  Created on: May 10, 2012
 *      Author: mdonahue
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef IMETHOD_H_
#define IMETHOD_H_

#include "amqpClient/api/Method.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Base interface for AMQP method implementation objects
 */
struct __declspec(novtable) IMethod : public Method {
	CAF_DECL_UUID("08ba9874-b34c-4afe-bfdc-a12fffaefddb")

	/**
	 * @brief Initialize the object from c-api method data
	 * @param method the method data
	 */
	virtual void init(const amqp_method_t * const method) = 0;

	/**
	 * @retval true if the method expects a content header
	 * @retval false if the method does not expect a content header
	 */
	virtual bool hasContent() = 0;
};
CAF_DECLARE_SMART_INTERFACE_POINTER(IMethod);

}}

#endif /* IMETHOD_H_ */
