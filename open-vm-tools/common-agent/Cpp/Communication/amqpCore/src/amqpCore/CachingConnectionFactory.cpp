/*
 *  Created on: May 25, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpClient/api/Channel.h"
#include "amqpClient/api/Connection.h"
#include "amqpCore/ChannelProxy.h"
#include "amqpCore/ConnectionListener.h"
#include "amqpCore/CachingConnectionFactory.h"

using namespace Caf::AmqpIntegration;

CachingConnectionFactory::CachingConnectionFactory() :
	_isInitialized(false),
	_isActive(true),
	_channelCacheSize(2),
	CAF_CM_INIT_LOG("CachingConnectionFactory") {
	_connectionMonitor.CreateInstance();
	_connectionMonitor->initialize();

	_cachedChannelsMonitor.CreateInstance();
	_cachedChannelsMonitor->initialize();
}

CachingConnectionFactory::~CachingConnectionFactory() {
	CAF_CM_FUNCNAME("~CachingConnectionFactory");
	if (_connection) {
		try {
			try {
				_connection->close();
			}
			CAF_CM_CATCH_ALL;
			CAF_CM_LOG_CRIT_CAFEXCEPTION;
			CAF_CM_CLEAREXCEPTION;

			if (_cachedChannels) {
				CAF_CM_LOCK_UNLOCK1(_cachedChannelsMonitor);
				_cachedChannels->clear();
				_cachedChannels = NULL;
			}
		}
		CAF_CM_CATCH_ALL;
		CAF_CM_LOG_CRIT_CAFEXCEPTION;
		CAF_CM_CLEAREXCEPTION;
	}
}

void CachingConnectionFactory::init() {
	init(std::string(),std::string());
}

void CachingConnectionFactory::init(
		const std::string& protocol,
		const std::string& host,
		const uint32 port) {
	CAF_CM_FUNCNAME_VALIDATE("init");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	AbstractConnectionFactory::init(AmqpClient::createConnectionFactory());
	setConnectionTimeout(10000);
	std::string locProtocol = protocol;
	if (locProtocol.empty()) {
		locProtocol = getDefaultProtocol();
	}
	std::string hostname = host;
	if (hostname.empty()) {
		hostname = getDefaultHostName();
	}
	setHost(hostname);
	setPort(port);
	_cachedChannels.CreateInstance();
	_isInitialized = true;
}

void CachingConnectionFactory::init(
		const std::string& protocol,
		const std::string& host) {
	init(protocol, host, AmqpClient::DEFAULT_AMQP_PORT);
}

void CachingConnectionFactory::init(
		const uint32 port) {
	init(std::string(), std::string(), port);
}

void CachingConnectionFactory::init(
		const AmqpClient::SmartPtrConnectionFactory& amqpConnectionFactory) {
	CAF_CM_FUNCNAME_VALIDATE("init");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	AbstractConnectionFactory::init(amqpConnectionFactory);
	_cachedChannels.CreateInstance();
	_isInitialized = true;
}

void CachingConnectionFactory::destroy() {
	CAF_CM_FUNCNAME_VALIDATE("destroy");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	// This method ends up calling reset() during _connection->destroy() which
	// sets _connection to NULL. To prevent the wheels from disappearing from
	// underneath the _connection, store a temporary reference to it.
	SmartPtrChannelCachingConnectionProxy connectionRef = _connection;
	CAF_CM_LOCK_UNLOCK1(_connectionMonitor);
	if (_connection) {
		_connection->destroy();
		_connection = NULL;
	}
	reset();
}

uint32 CachingConnectionFactory::getChannelCacheSize() {
	return _channelCacheSize;
}

AmqpClient::SmartPtrChannel CachingConnectionFactory::getChannel() {
	CAF_CM_FUNCNAME_VALIDATE("getChannel");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	AmqpClient::SmartPtrChannel channel;
	{
		CAF_CM_LOCK_UNLOCK1(_cachedChannelsMonitor);
		if (_cachedChannels->size()) {
			channel = _cachedChannels->front();
			_cachedChannels->pop_front();
		}
	}
	if (channel) {
		CAF_CM_LOG_DEBUG_VA1("found cached rabbit channel #%d", channel->getChannelNumber());
	} else {
		channel = newCachedChannelProxy();
	}
	return channel;
}

void CachingConnectionFactory::setConnectionListeners(
		const std::deque<SmartPtrConnectionListener>& listeners) {
	CAF_CM_FUNCNAME_VALIDATE("setConnectionListeners");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	AbstractConnectionFactory::setConnectionListeners(listeners);
	if (_connection) {
		getConnectionListener()->onCreate(_connection);
	}
}

void CachingConnectionFactory::setChannelCacheSize(uint32 cacheSize) {
	CAF_CM_FUNCNAME("setChannelCacheSize");
	CAF_CM_ASSERT(cacheSize >= 1);
	_channelCacheSize = cacheSize;
}

void CachingConnectionFactory::addConnectionListener(
		const SmartPtrConnectionListener& listener) {
	CAF_CM_FUNCNAME_VALIDATE("addConnectionListener");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(listener);
	AbstractConnectionFactory::addConnectionListener(listener);
	if (_connection) {
		listener->onCreate(_connection);
	}
}

SmartPtrConnection CachingConnectionFactory::createConnection() {
	CAF_CM_FUNCNAME_VALIDATE("createConnection");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_LOCK_UNLOCK1(_connectionMonitor);
	if (!_connection) {
		_connection.CreateInstance();
		_connection->init(createBareConnection(), this);
		getConnectionListener()->onCreate(_connection);
	}
	return _connection;
}

void CachingConnectionFactory::reset() {
	CAF_CM_FUNCNAME("reset");
	_isActive = false;
	CAF_CM_LOCK_UNLOCK1(_cachedChannelsMonitor);
	for (TSmartIterator<ProxyDeque> channel(*_cachedChannels);
			channel;
			channel++) {
		try {
			AmqpClient::SmartPtrChannel target = channel->getTargetChannel();
			if (target) {
				target->close();
			}
		}
		CAF_CM_CATCH_ALL;
		CAF_CM_LOG_CRIT_CAFEXCEPTION;
		CAF_CM_CLEAREXCEPTION;
	}
	try {
		_cachedChannels->clear();
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION;
	CAF_CM_CLEAREXCEPTION;
	_isActive = true;
	_connection = NULL;
}

SmartPtrChannelProxy CachingConnectionFactory::newCachedChannelProxy(){
	AmqpClient::SmartPtrChannel channel = createBareChannel();
	SmartPtrCachedChannelHandler proxy;
	proxy.CreateInstance();
	proxy->init(this, channel);
	return proxy;
}

AmqpClient::SmartPtrChannel CachingConnectionFactory::createBareChannel() {
	if (!_connection || !_connection->isOpen()) {
		_connection = NULL;
		createConnection();
	}
	return _connection->createBareChannel();
}
