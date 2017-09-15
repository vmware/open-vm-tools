/*
 *  Created on: May 21, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef BASICCANCELOKMETHOD_H_
#define BASICCANCELOKMETHOD_H_

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Implementation of AMQP basic.cancel-ok
 */
class BasicCancelOkMethod :
	public TMethodImpl<BasicCancelOkMethod>,
	public AmqpMethods::Basic::CancelOk {
	METHOD_DECL(
		AmqpMethods::Basic::CancelOk,
		AMQP_BASIC_CANCEL_OK_METHOD,
		"basic.cancel-ok",
		false)

public:
	BasicCancelOkMethod();
	virtual ~BasicCancelOkMethod();

public: // IMethod
	void init(const amqp_method_t * const method);

public: // AmqpMethods::Basic::CancelOk
	std::string getConsumerTag();

private:
	std::string _consumerTag;
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(BasicCancelOkMethod);
};
CAF_DECLARE_SMART_QI_POINTER(BasicCancelOkMethod);

}}

#endif /* BASICCANCELOKMETHOD_H_ */
