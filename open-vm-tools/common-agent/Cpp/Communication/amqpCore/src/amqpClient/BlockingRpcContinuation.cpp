/*
 *  Created on: May 15, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Exception/CCafException.h"
#include "amqpClient/AMQCommand.h"
#include "amqpClient/BlockingRpcContinuation.h"

using namespace Caf::AmqpClient;

BlockingRpcContinuation::BlockingRpcContinuation() :
	_isInitialized(false),
	CAF_CM_INIT("BlockingRpcContinuation") {
}

BlockingRpcContinuation::~BlockingRpcContinuation() {
}

void BlockingRpcContinuation::init() {
	CAF_CM_FUNCNAME_VALIDATE("init");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	_isInitialized = true;
}

SmartPtrAMQCommand BlockingRpcContinuation::getReply() {
	CAF_CM_FUNCNAME_VALIDATE("getReply");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _blocker.get();
}

SmartPtrAMQCommand BlockingRpcContinuation::getReply(uint32 timeout) {
	CAF_CM_FUNCNAME_VALIDATE("getReply");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _blocker.get(timeout);
}

SmartPtrCCafException BlockingRpcContinuation::getException() {
	CAF_CM_FUNCNAME_VALIDATE("getException");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _exception;
}
void BlockingRpcContinuation::handleCommand(const SmartPtrAMQCommand& command) {
	CAF_CM_FUNCNAME_VALIDATE("handleCommand");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	_blocker.set(command);
}

void BlockingRpcContinuation::handleAbort(SmartPtrCCafException exception) {
	CAF_CM_FUNCNAME_VALIDATE("handleAbort");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	_exception = exception;
	_blocker.set(NULL);
}
