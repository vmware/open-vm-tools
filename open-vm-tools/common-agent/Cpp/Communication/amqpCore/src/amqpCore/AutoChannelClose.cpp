/*
 *  Created on: Aug 7, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpClient/api/Channel.h"
#include "AutoChannelClose.h"

using namespace Caf::AmqpIntegration;

AutoChannelClose::AutoChannelClose(AmqpClient::SmartPtrChannel channel) :
		_channel(channel) {}

AutoChannelClose::~AutoChannelClose() {
	if (_channel) {
		_channel->close();
	}
}
