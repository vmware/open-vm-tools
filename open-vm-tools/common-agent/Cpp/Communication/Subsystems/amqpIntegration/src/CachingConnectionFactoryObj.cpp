/*
 *  Created on: Jun 4, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Doc/PersistenceDoc/CPersistenceProtocolDoc.h"
#include "amqpCore/CachingConnectionFactory.h"
#include "amqpClient/api/Connection.h"
#include "amqpCore/ConnectionListener.h"
#include "Exception/CCafException.h"
#include "CachingConnectionFactoryObj.h"

using namespace Caf::AmqpIntegration;

CachingConnectionFactoryObj::CachingConnectionFactoryObj() :
		CAF_CM_INIT_LOG("CachingConnectionFactoryObj") {
}

CachingConnectionFactoryObj::~CachingConnectionFactoryObj() {
}

void CachingConnectionFactoryObj::initializeBean(
		const IBean::Cargs& ctorArgs,
		const IBean::Cprops& properties) {
	CAF_CM_FUNCNAME("initializeBean");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_factory);
	CAF_CM_VALIDATE_STL_EMPTY(ctorArgs);

	const std::string persistenceDir = AppConfigUtils::getRequiredString(
			"persistence_dir");
	const SmartPtrCPersistenceProtocolDoc persistenceProtocol =
			CPersistenceUtils::loadPersistenceProtocol(persistenceDir);

	if (persistenceProtocol.IsNull()) {
		CAF_CM_EXCEPTIONEX_VA1(IllegalStateException, ERROR_INVALID_STATE,
				"Persistence protocol is empty... Comm must be configured - %s",
				persistenceDir.c_str());
	}

	UriUtils::SUriRecord uri;
	UriUtils::parseUriString(persistenceProtocol->getUri(), uri);

	const std::string vhost = UriUtils::findOptParameter(uri, "vhost",
			AppConfigUtils::getRequiredString("communication_amqp", "vhost"));
	const std::string connectionTimeout = UriUtils::findOptParameter(uri, "connection_timeout",
			CStringConv::toString<uint32>(
					AppConfigUtils::getRequiredUint32("communication_amqp", "connection_timeout")));
	const std::string connectionRetries = UriUtils::findOptParameter(uri, "connection_retries",
			CStringConv::toString<uint32>(
					AppConfigUtils::getRequiredUint32("communication_amqp", "connection_retries")));
	const std::string connectionSecondsToWait = UriUtils::findOptParameter(uri, "connection_seconds_to_wait",
			CStringConv::toString<uint32>(
					AppConfigUtils::getRequiredUint32("communication_amqp", "connection_seconds_to_wait")));
	const std::string channelCacheSize = UriUtils::findOptParameter(uri, "channel_cache_size",
			CStringConv::toString<uint32>(
					AppConfigUtils::getRequiredUint32("communication_amqp", "channel_cache_size")));

	CAF_CM_VALIDATE_STRING(uri.protocol);
	CAF_CM_VALIDATE_STRING(uri.host);
	CAF_CM_VALIDATE_STRING(uri.portStr);
	CAF_CM_VALIDATE_STRING(vhost);

	SmartPtrCachingConnectionFactory factory;
	factory.CreateInstance();
	factory->init();
	factory->setProtocol(uri.protocol);
	factory->setHost(uri.host);
	factory->setPort(uri.port);
	factory->setVirtualHost(vhost);
	if (!uri.username.empty()) {
		factory->setUsername(uri.username);
	}
	if (!uri.password.empty()) {
		factory->setPassword(uri.password);
	}
	if (!connectionTimeout.empty()) {
		factory->setConnectionTimeout(CStringConv::fromString<uint32>(connectionTimeout));
	}
	if (!connectionRetries.empty()) {
		factory->setRetries(CStringConv::fromString<uint16>(connectionRetries));
	}
	if (!connectionSecondsToWait.empty()) {
		factory->setSecondsToWait(
				CStringConv::fromString<uint16>(connectionSecondsToWait));
	}
	if (!channelCacheSize.empty()) {
		factory->setChannelCacheSize(CStringConv::fromString<uint32>(channelCacheSize));
	}

	_factory = factory;
}

void CachingConnectionFactoryObj::terminateBean() {
	CAF_CM_FUNCNAME("terminateBean");
	try {
		if (_factory) {
			_factory->destroy();
		}
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION
	;
	CAF_CM_CLEAREXCEPTION;
}

SmartPtrConnection CachingConnectionFactoryObj::createConnection() {
	CAF_CM_FUNCNAME_VALIDATE("createConnection");
	CAF_CM_PRECOND_ISINITIALIZED(_factory);
	return _factory->createConnection();
}

std::string CachingConnectionFactoryObj::getProtocol() {
	CAF_CM_FUNCNAME_VALIDATE("getProtocol");
	CAF_CM_PRECOND_ISINITIALIZED(_factory);
	return _factory->getProtocol();
}

std::string CachingConnectionFactoryObj::getHost() {
	CAF_CM_FUNCNAME_VALIDATE("getHost");
	CAF_CM_PRECOND_ISINITIALIZED(_factory);
	return _factory->getHost();
}

uint32 CachingConnectionFactoryObj::getPort() {
	CAF_CM_FUNCNAME_VALIDATE("getPort");
	CAF_CM_PRECOND_ISINITIALIZED(_factory);
	return _factory->getPort();
}

std::string CachingConnectionFactoryObj::getVirtualHost() {
	CAF_CM_FUNCNAME_VALIDATE("getVirtualHost");
	CAF_CM_PRECOND_ISINITIALIZED(_factory);
	return _factory->getVirtualHost();
}

std::string CachingConnectionFactoryObj::getUsername() {
	CAF_CM_FUNCNAME_VALIDATE("getUsername");
	CAF_CM_PRECOND_ISINITIALIZED(_factory);
	return _factory->getUsername();
}

std::string CachingConnectionFactoryObj::getPassword() {
	CAF_CM_FUNCNAME_VALIDATE("getPassword");
	CAF_CM_PRECOND_ISINITIALIZED(_factory);
	return _factory->getPassword();
}

std::string CachingConnectionFactoryObj::getCaCertPath() {
	CAF_CM_FUNCNAME_VALIDATE("getCaCertPath");
	CAF_CM_PRECOND_ISINITIALIZED(_factory);
	return _factory->getCaCertPath();
}

std::string CachingConnectionFactoryObj::getClientCertPath() {
	CAF_CM_FUNCNAME_VALIDATE("getClientCertPath");
	CAF_CM_PRECOND_ISINITIALIZED(_factory);
	return _factory->getClientCertPath();
}

std::string CachingConnectionFactoryObj::getClientKeyPath() {
	CAF_CM_FUNCNAME_VALIDATE("getClientKeyPath");
	CAF_CM_PRECOND_ISINITIALIZED(_factory);
	return _factory->getClientKeyPath();
}

uint16 CachingConnectionFactoryObj::getRetries() {
	CAF_CM_FUNCNAME_VALIDATE("getRetries");
	CAF_CM_PRECOND_ISINITIALIZED(_factory);
	return _factory->getRetries();
}

uint16 CachingConnectionFactoryObj::getSecondsToWait() {
	CAF_CM_FUNCNAME_VALIDATE("getSecondsToWait");
	CAF_CM_PRECOND_ISINITIALIZED(_factory);
	return _factory->getSecondsToWait();
}

void CachingConnectionFactoryObj::addConnectionListener(
		const SmartPtrConnectionListener& listener) {
	CAF_CM_FUNCNAME_VALIDATE("addConnectionListener");
	CAF_CM_PRECOND_ISINITIALIZED(_factory);
	_factory->addConnectionListener(listener);
}
