/*
 *  Created on: May 22, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "ChannelCloseOkFromServerMethod.h"

using namespace Caf::AmqpClient;

ChannelCloseOkFromServerMethod::ChannelCloseOkFromServerMethod() :
	CAF_CM_INIT("ChannelCloseOkFromServerMethod") {
}

ChannelCloseOkFromServerMethod::~ChannelCloseOkFromServerMethod() {
}

void ChannelCloseOkFromServerMethod::init(const amqp_method_t * const method) {
	CAF_CM_FUNCNAME("init");
	CAF_CM_VALIDATE_PTR(method);
	CAF_CM_ASSERT(AMQP_CHANNEL_CLOSE_OK_METHOD == method->id);
}
