/*
 *  Created on: May 21, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef BASICRECOVEROKMETHOD_H_
#define BASICRECOVEROKMETHOD_H_

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Implementation of AMQP basic.recover-ok
 */
class BasicRecoverOkMethod :
	public TMethodImpl<BasicRecoverOkMethod>,
	public AmqpMethods::Basic::RecoverOk {
	METHOD_DECL(
		AmqpMethods::Basic::RecoverOk,
		AMQP_BASIC_RECOVER_OK_METHOD,
		"basic.recover-ok",
		false)

public:
	BasicRecoverOkMethod();
	virtual ~BasicRecoverOkMethod();

public: // IMethod
	void init(const amqp_method_t * const method);

public: // AmqpMethods::Basic::BasicRecoverOk

private:
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(BasicRecoverOkMethod);
};
CAF_DECLARE_SMART_QI_POINTER(BasicRecoverOkMethod);

}}

#endif /* BASICRECOVEROKMETHOD_H_ */
