/*
 *  Created on: May 10, 2012
 *      Author: mdonahue
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPIMPL_H_
#define AMQPIMPL_H_


#include "amqpClient/amqpImpl/IContentHeader.h"
#include "amqpClient/amqpImpl/IMethod.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief A set of helpers to convert c-api data structures into C++ objects
 */
class AMQPImpl {
public:
	/**
	 * @brief Convert a c-api method structure into the appropriate IMethod object
	 * @param method c-api method data
	 * @return IMethod object
	 */
	static SmartPtrIMethod methodFromFrame(const amqp_method_t * const method);

	/**
	 * @brief Convert a c-api properties structure into the appropriate IContentHedaer object
	 * @param properties c-api properties data
	 * @return IContentHeader object
	 */
	static SmartPtrIContentHeader headerFromFrame(const SmartPtrCAmqpFrame& frame);
};

}}

#endif /* AMQPIMPL_H_ */
