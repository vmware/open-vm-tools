/*
 *  Created on: May 21, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef BASICCONSUMEOKMETHOD_H_
#define BASICCONSUMEOKMETHOD_H_

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Implementation of AMQP basic.consume-ok
 */
class BasicConsumeOkMethod :
	public TMethodImpl<BasicConsumeOkMethod>,
	public AmqpMethods::Basic::ConsumeOk {
	METHOD_DECL(
		AmqpMethods::Basic::ConsumeOk,
		AMQP_BASIC_CONSUME_OK_METHOD,
		"basic.consume-ok",
		false)

public:
	BasicConsumeOkMethod();
	virtual ~BasicConsumeOkMethod();

public: // IMethod
	void init(const amqp_method_t * const method);

public: // AmqpMethods::Basic::ConsumeOk
	std::string getConsumerTag();

private:
	std::string _consumerTag;
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(BasicConsumeOkMethod);
};
CAF_DECLARE_SMART_QI_POINTER(BasicConsumeOkMethod);

}}

#endif /* BASICCONSUMEOKMETHOD_H_ */
