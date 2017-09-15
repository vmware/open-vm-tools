/*
 *  Created on: May 2, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpClient/CAmqpAuthMechanism.h"
#include "AmqpAuthPlain.h"
#include "AmqpUtil.h"

using namespace Caf::AmqpClient;

AMQPStatus AmqpAuthPlain::AMQP_AuthPlainCreateClient(
		SmartPtrCAmqpAuthMechanism& mech,
		const std::string& username,
		const std::string& password) {
	CAF_CM_STATIC_FUNC_VALIDATE("AmqpAuthPlain", "AMQP_AuthPlainCreateClient");
	CAF_CM_VALIDATE_STRING(username);
	// password is optional

	mech.CreateInstance();
	return mech->createClient(username, password);
}
