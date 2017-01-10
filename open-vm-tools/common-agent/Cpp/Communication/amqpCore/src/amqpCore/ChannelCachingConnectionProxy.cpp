/*
 *  Created on: Jun 1, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpClient/api/Channel.h"
#include "amqpClient/api/Connection.h"
#include "amqpCore/CachingConnectionFactory.h"

using namespace Caf::AmqpIntegration;

CachingConnectionFactory::ChannelCachingConnectionProxy::ChannelCachingConnectionProxy() :
	_parent(NULL),
	CAF_CM_INIT_LOG("CachingConnectionFactory::ChannelCachingConnectionProxy") {
}

CachingConnectionFactory::ChannelCachingConnectionProxy::~ChannelCachingConnectionProxy() {
	_target = NULL;
	_parent = NULL;
}

void CachingConnectionFactory::ChannelCachingConnectionProxy::init(
		SmartPtrConnection connection,
		CachingConnectionFactory *parent) {
	_target = connection;
	_parent = parent;
}

void CachingConnectionFactory::ChannelCachingConnectionProxy::destroy() {
	CAF_CM_FUNCNAME("destroy");
	if (_target) {
		_parent->getConnectionListener()->onClose(_target);
		try {
			_target->close();
		}
		CAF_CM_CATCH_ALL;
		CAF_CM_CLEAREXCEPTION;
		try {
			_parent->reset();
		}
		CAF_CM_CATCH_ALL;
		CAF_CM_CLEAREXCEPTION;
	}
	_target = NULL;
}

SmartPtrConnection CachingConnectionFactory::ChannelCachingConnectionProxy::getTargetConnection() {
	return _target;
}

AmqpClient::SmartPtrChannel CachingConnectionFactory::ChannelCachingConnectionProxy::createChannel() {
	return _parent->getChannel();
}

void CachingConnectionFactory::ChannelCachingConnectionProxy::close() {
}

bool CachingConnectionFactory::ChannelCachingConnectionProxy::isOpen() {
	return _target && _target->isOpen();
}

AmqpClient::SmartPtrChannel CachingConnectionFactory::ChannelCachingConnectionProxy::createBareChannel() {
	return _target->createChannel();
}
