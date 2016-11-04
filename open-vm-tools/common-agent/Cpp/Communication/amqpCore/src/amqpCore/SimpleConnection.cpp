/*
 *  Created on: May 25, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpClient/api/Channel.h"
#include "amqpCore/SimpleConnection.h"

using namespace Caf::AmqpIntegration;

SimpleConnection::SimpleConnection() :
	CAF_CM_INIT("SimpleConnection") {
}

SimpleConnection::~SimpleConnection() {
}

void SimpleConnection::init(const AmqpClient::SmartPtrConnection& delegate) {
	CAF_CM_FUNCNAME_VALIDATE("init");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_delegate);
	CAF_CM_VALIDATE_SMARTPTR(delegate);
	_delegate = delegate;
}

AmqpClient::SmartPtrChannel SimpleConnection::createChannel() {
	CAF_CM_FUNCNAME_VALIDATE("createChannel");
	CAF_CM_PRECOND_ISINITIALIZED(_delegate);
	return _delegate->createChannel();
}

void SimpleConnection::close() {
	CAF_CM_FUNCNAME_VALIDATE("close");
	CAF_CM_PRECOND_ISINITIALIZED(_delegate);
	_delegate->close();
}

bool SimpleConnection::isOpen() {
	return _delegate && _delegate->isOpen();
}
