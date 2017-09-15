/*
 *  Created on: May 21, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef BASICRETURNMETHOD_H_
#define BASICRETURNMETHOD_H_

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Implementation of AMQP basic.return
 */
class BasicReturnMethod :
	public TMethodImpl<BasicReturnMethod>,
	public AmqpMethods::Basic::Return {
	METHOD_DECL(
		AmqpMethods::Basic::Return,
		AMQP_BASIC_RETURN_METHOD,
		"basic.return",
		true)

public:
	BasicReturnMethod();
	virtual ~BasicReturnMethod();

public: // IMethod
	void init(const amqp_method_t * const method);

public: // AmqpMethods::Basic::Return
	uint16 getReplyCode();
	std::string getReplyText();
	std::string getExchange();
	std::string getRoutingKey();

private:
	uint16 _replyCode;
	std::string _replyText;
	std::string _exchange;
	std::string _routingKey;
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(BasicReturnMethod);
};
CAF_DECLARE_SMART_QI_POINTER(BasicReturnMethod);

}}

#endif /* BASICRETURNMETHOD_H_ */
