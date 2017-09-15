/*
 *  Created on: May 22, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef BASICQOSOKMETHOD_H_
#define BASICQOSOKMETHOD_H_

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Implementation of AMQP basic.qos-ok
 */
class BasicQosOkMethod :
	public TMethodImpl<BasicQosOkMethod>,
	public AmqpMethods::Basic::QosOk {
	METHOD_DECL(
		AmqpMethods::Basic::QosOk,
		AMQP_BASIC_QOS_OK_METHOD,
		"basic.qos-ok",
		false)

public:
	BasicQosOkMethod();
	virtual ~BasicQosOkMethod();

public: // IMethod
	void init(const amqp_method_t * const method);

public: // AmqpMethods::Basic::QosOk

private:
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(BasicQosOkMethod);
};
CAF_DECLARE_SMART_QI_POINTER(BasicQosOkMethod);

}}

#endif /* BASICQOSOKMETHOD_H_ */
