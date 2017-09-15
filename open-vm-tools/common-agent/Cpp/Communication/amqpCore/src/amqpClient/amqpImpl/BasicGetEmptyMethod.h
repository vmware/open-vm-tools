/*
 *  Created on: May 16, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef BASICGETEMPTYMETHOD_H_
#define BASICGETEMPTYMETHOD_H_

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Implementation of AMQP basic.get-empty
 */
class BasicGetEmptyMethod :
	public TMethodImpl<BasicGetEmptyMethod>,
	public AmqpMethods::Basic::GetEmpty {
	METHOD_DECL(
		AmqpMethods::Basic::GetEmpty,
		AMQP_BASIC_GET_EMPTY_METHOD,
		"basic.get-empty",
		false)

public:
	BasicGetEmptyMethod();
	virtual ~BasicGetEmptyMethod();

public: // IMethod
	void init(const amqp_method_t * const method);

public: // AmqpMethods::Basic::GetEmpty

private:
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(BasicGetEmptyMethod);
};
CAF_DECLARE_SMART_QI_POINTER(BasicGetEmptyMethod);

}}

#endif /* BASICGETEMPTYMETHOD_H_ */
