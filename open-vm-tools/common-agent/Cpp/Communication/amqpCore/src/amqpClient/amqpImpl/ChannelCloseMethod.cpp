/*
 *  Created on: May 17, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "ChannelCloseMethod.h"

using namespace Caf::AmqpClient;

ChannelCloseMethod::ChannelCloseMethod() :
	_replyCode(0),
	_classId(0),
	_methodId(0),
	CAF_CM_INIT("ChannelCloseMethod") {
}

ChannelCloseMethod::~ChannelCloseMethod() {
}

void ChannelCloseMethod::init(const amqp_method_t * const method) {
	CAF_CM_FUNCNAME("init");
	CAF_CM_VALIDATE_PTR(method);
	CAF_CM_ASSERT(AMQP_CHANNEL_CLOSE_METHOD == method->id);
	const amqp_channel_close_t * const decoded =
			reinterpret_cast<const amqp_channel_close_t * const>(method->decoded);
	_replyCode = decoded->reply_code;
	_replyText = AMQUtil::amqpBytesToString(&decoded->reply_text);
	_classId = decoded->class_id;
	_methodId = decoded->method_id;
}

uint16 ChannelCloseMethod::getReplyCode() {
	return _replyCode;
}

std::string ChannelCloseMethod::getReplyText() {
	return _replyText;
}

uint16 ChannelCloseMethod::getClassId() {
	return _classId;
}

uint16 ChannelCloseMethod::getMethodId() {
	return _methodId;
}
