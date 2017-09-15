/*
 *  Created on: May 7, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpClient/CAmqpChannel.h"
#include "amqpClient/ConnectionWeakReference.h"
#include "Exception/CCafException.h"

using namespace Caf::AmqpClient;

ConnectionWeakReference::ConnectionWeakReference() :
	_connection(NULL),
	CAF_CM_INIT("ConnectionWeakReference") {
	CAF_CM_INIT_THREADSAFE;
}

ConnectionWeakReference::~ConnectionWeakReference() {
}

void ConnectionWeakReference::setReference(IConnectionInt* connection) {
	CAF_CM_FUNCNAME_VALIDATE("setReference");
	CAF_CM_VALIDATE_PTR(connection);
	CAF_CM_LOCK_UNLOCK;
	_connection = connection;
}

void ConnectionWeakReference::clearReference() {
	CAF_CM_LOCK_UNLOCK;
	_connection = NULL;
}

AMQPStatus ConnectionWeakReference::amqpConnectionOpenChannel(SmartPtrCAmqpChannel& channel) {
	CAF_CM_FUNCNAME("amqpConnectionOpenChannel");
	CAF_CM_LOCK_UNLOCK;
	AMQPStatus status = AMQP_ERROR_OK;
	if (_connection) {
		status = _connection->amqpConnectionOpenChannel(channel);
	} else {
		CAF_CM_EXCEPTIONEX_VA0(
				NullPointerException,
				0,
				"The weak reference is not set");
	}
	return status;
}

void ConnectionWeakReference::notifyChannelClosedByServer(const uint16 channelNumber) {
	CAF_CM_FUNCNAME("notifyChannelClosedByServer");
	CAF_CM_LOCK_UNLOCK;
	if (_connection) {
		_connection->notifyChannelClosedByServer(channelNumber);
	} else {
		CAF_CM_EXCEPTIONEX_VA0(
				NullPointerException,
				0,
				"The weak reference is not set");
	}
}

void ConnectionWeakReference::channelCloseChannel(Channel *channel) {
	// It's okay for the weak reference to be unset in this call
	if (_connection) {
		_connection->channelCloseChannel(channel);
	}
}
