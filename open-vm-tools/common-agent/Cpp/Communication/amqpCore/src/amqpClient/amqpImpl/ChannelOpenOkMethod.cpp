/*
 *  Created on: May 10, 2012
 *      Author: mdonahue
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "ChannelOpenOkMethod.h"

using namespace Caf::AmqpClient;

ChannelOpenOkMethod::ChannelOpenOkMethod() :
	CAF_CM_INIT("ChannelOpenOkMethod") {
}

ChannelOpenOkMethod::~ChannelOpenOkMethod() {
}

void ChannelOpenOkMethod::init(const amqp_method_t * const method) {
	CAF_CM_FUNCNAME("init");
	CAF_CM_VALIDATE_PTR(method);
	CAF_CM_ASSERT(AMQP_CHANNEL_OPEN_OK_METHOD == method->id);
	const amqp_bytes_t * const bytes =
			reinterpret_cast<const amqp_bytes_t * const>(method->decoded);
	_channelId.CreateInstance();
	if (bytes->len) {
		_channelId->memCpy(bytes->bytes, bytes->len);
	}
}

SmartPtrCDynamicByteArray ChannelOpenOkMethod::getChannelId() {
	return _channelId;
}
