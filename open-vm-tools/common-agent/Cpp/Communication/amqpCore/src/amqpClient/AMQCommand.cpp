/*
 *  Created on: May 9, 2012
 *      Author: mdonahue
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "amqpClient/CAmqpFrame.h"
#include "amqpClient/amqpImpl/IContentHeader.h"
#include "amqpClient/amqpImpl/IMethod.h"
#include "amqpClient/AMQCommand.h"

using namespace Caf::AmqpClient;

AMQCommand::AMQCommand() :
	CAF_CM_INIT("AMQCommand") {
}

AMQCommand::~AMQCommand() {
}

void AMQCommand::init() {
	CAF_CM_FUNCNAME_VALIDATE("init");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_assembler);
	_assembler.CreateInstance();
	_assembler->init();
}

bool AMQCommand::handleFrame(const SmartPtrCAmqpFrame& frame) {
	CAF_CM_FUNCNAME_VALIDATE("handleFrame");
	CAF_CM_PRECOND_ISINITIALIZED(_assembler);
	return _assembler->handleFrame(frame);
}

SmartPtrCDynamicByteArray AMQCommand::getContentBody() {
	CAF_CM_FUNCNAME_VALIDATE("getContentBody");
	CAF_CM_PRECOND_ISINITIALIZED(_assembler);
	return _assembler->getContentBody();
}

SmartPtrIContentHeader AMQCommand::getContentHeader() {
	CAF_CM_FUNCNAME_VALIDATE("getContentHeader");
	CAF_CM_PRECOND_ISINITIALIZED(_assembler);
	return _assembler->getContentHeader();
}

SmartPtrIMethod AMQCommand::getMethod() {
	CAF_CM_FUNCNAME_VALIDATE("getMethod");
	CAF_CM_PRECOND_ISINITIALIZED(_assembler);
	return _assembler->getMethod();
}
