/*
 *  Created on: Jun 4, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Doc/PersistenceDoc/CCertPathCollectionDoc.h"
#include "Doc/PersistenceDoc/CLocalSecurityDoc.h"
#include "Doc/PersistenceDoc/CPersistenceDoc.h"
#include "Doc/PersistenceDoc/CPersistenceProtocolDoc.h"
#include "amqpCore/CachingConnectionFactory.h"
#include "amqpClient/api/Connection.h"
#include "amqpCore/ConnectionListener.h"
#include "SecureCachingConnectionFactoryObj.h"

using namespace Caf::AmqpIntegration;

SecureCachingConnectionFactoryObj::SecureCachingConnectionFactoryObj() :
	CAF_CM_INIT_LOG("SecureCachingConnectionFactoryObj") {
}

SecureCachingConnectionFactoryObj::~SecureCachingConnectionFactoryObj() {
}

void SecureCachingConnectionFactoryObj::initializeBean(
			const IBean::Cargs& ctorArgs,
			const IBean::Cprops& properties) {
	CAF_CM_FUNCNAME_VALIDATE("initializeBean");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_factory);
	CAF_CM_VALIDATE_STL_EMPTY(ctorArgs);

	const std::string persistenceDir =
			AppConfigUtils::getRequiredString("persistence_dir");

	const SmartPtrCPersistenceDoc persistence =
			CPersistenceUtils::loadPersistence(persistenceDir);
	CAF_CM_VALIDATE_SMARTPTR(persistence);

	const SmartPtrCPersistenceProtocolDoc amqpBroker =
			CPersistenceUtils::loadPersistenceProtocol(persistence->getPersistenceProtocolCollection());
	CAF_CM_VALIDATE_SMARTPTR(amqpBroker);

	const SmartPtrCCertPathCollectionDoc tlsCertPathCollection = amqpBroker->getTlsCertPathCollection();
	CAF_CM_VALIDATE_SMARTPTR(tlsCertPathCollection);

	const SmartPtrCLocalSecurityDoc localSecurity = persistence->getLocalSecurity();
	CAF_CM_VALIDATE_SMARTPTR(localSecurity);

	UriUtils::SUriRecord uri;
	UriUtils::parseUriString(amqpBroker->getUri(), uri);

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

	const std::deque<std::string> tlsCertPathCollectionInner = tlsCertPathCollection->getCertPath();
	CAF_CM_VALIDATE_STL(tlsCertPathCollectionInner);
	CAF_CM_VALIDATE_BOOL(tlsCertPathCollectionInner.size() == 1);
	const std::string caCertPath = tlsCertPathCollectionInner.front();

	CAF_CM_VALIDATE_STRING(uri.protocol);
	CAF_CM_VALIDATE_STRING(uri.host);
	CAF_CM_VALIDATE_STRING(uri.portStr);
	CAF_CM_VALIDATE_STRING(vhost);

	CAF_CM_VALIDATE_STRING(caCertPath);
	CAF_CM_VALIDATE_STRING(localSecurity->getCertPath());
	CAF_CM_VALIDATE_STRING(localSecurity->getPrivateKeyPath());

	SmartPtrCachingConnectionFactory factory;
	factory.CreateInstance();
	factory->init();
	factory->setProtocol(uri.protocol);
	factory->setHost(uri.host);
	factory->setPort(uri.port);
	factory->setVirtualHost(vhost);
	factory->setCaCertPath(caCertPath);
	factory->setClientCertPath(localSecurity->getCertPath());
	factory->setClientKeyPath(localSecurity->getPrivateKeyPath());
	if (! uri.username.empty()) {
		factory->setUsername(uri.username);
	}
	if (! uri.password.empty()) {
		factory->setPassword(uri.password);
	}
	if (! connectionTimeout.empty()) {
		factory->setConnectionTimeout(CStringConv::fromString<uint32>(connectionTimeout));
	}
	if (! connectionRetries.empty()) {
		factory->setRetries(CStringConv::fromString<uint16>(connectionRetries));
	}
	if (! connectionSecondsToWait.empty()) {
		factory->setSecondsToWait(CStringConv::fromString<uint16>(connectionSecondsToWait));
	}
	if (! channelCacheSize.empty()) {
		factory->setChannelCacheSize(CStringConv::fromString<uint32>(channelCacheSize));
	}

	_factory = factory;
}

void SecureCachingConnectionFactoryObj::terminateBean() {
	CAF_CM_FUNCNAME("terminateBean");
	try {
		if (_factory) {
			_factory->destroy();
		}
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION;
	CAF_CM_CLEAREXCEPTION;
}

SmartPtrConnection SecureCachingConnectionFactoryObj::createConnection() {
	CAF_CM_FUNCNAME_VALIDATE("createConnection");
	CAF_CM_PRECOND_ISINITIALIZED(_factory);
	return _factory->createConnection();
}

std::string SecureCachingConnectionFactoryObj::getProtocol() {
	CAF_CM_FUNCNAME_VALIDATE("getProtocol");
	CAF_CM_PRECOND_ISINITIALIZED(_factory);
	return _factory->getProtocol();
}

std::string SecureCachingConnectionFactoryObj::getHost() {
	CAF_CM_FUNCNAME_VALIDATE("getHost");
	CAF_CM_PRECOND_ISINITIALIZED(_factory);
	return _factory->getHost();
}

uint32 SecureCachingConnectionFactoryObj::getPort() {
	CAF_CM_FUNCNAME_VALIDATE("getPort");
	CAF_CM_PRECOND_ISINITIALIZED(_factory);
	return _factory->getPort();
}

std::string SecureCachingConnectionFactoryObj::getVirtualHost() {
	CAF_CM_FUNCNAME_VALIDATE("getVirtualHost");
	CAF_CM_PRECOND_ISINITIALIZED(_factory);
	return _factory->getVirtualHost();
}

std::string SecureCachingConnectionFactoryObj::getUsername() {
	CAF_CM_FUNCNAME_VALIDATE("getUsername");
	CAF_CM_PRECOND_ISINITIALIZED(_factory);
	return _factory->getUsername();
}

std::string SecureCachingConnectionFactoryObj::getPassword() {
	CAF_CM_FUNCNAME_VALIDATE("getPassword");
	CAF_CM_PRECOND_ISINITIALIZED(_factory);
	return _factory->getPassword();
}

std::string SecureCachingConnectionFactoryObj::getCaCertPath() {
	CAF_CM_FUNCNAME_VALIDATE("getCaCertPath");
	CAF_CM_PRECOND_ISINITIALIZED(_factory);
	return _factory->getCaCertPath();
}

std::string SecureCachingConnectionFactoryObj::getClientCertPath() {
	CAF_CM_FUNCNAME_VALIDATE("getClientCertPath");
	CAF_CM_PRECOND_ISINITIALIZED(_factory);
	return _factory->getClientCertPath();
}

std::string SecureCachingConnectionFactoryObj::getClientKeyPath() {
	CAF_CM_FUNCNAME_VALIDATE("getClientKeyPath");
	CAF_CM_PRECOND_ISINITIALIZED(_factory);
	return _factory->getClientKeyPath();
}

uint16 SecureCachingConnectionFactoryObj::getRetries() {
	CAF_CM_FUNCNAME_VALIDATE("getRetries");
	CAF_CM_PRECOND_ISINITIALIZED(_factory);
	return _factory->getRetries();
}

uint16 SecureCachingConnectionFactoryObj::getSecondsToWait() {
	CAF_CM_FUNCNAME_VALIDATE("getSecondsToWait");
	CAF_CM_PRECOND_ISINITIALIZED(_factory);
	return _factory->getSecondsToWait();
}

void SecureCachingConnectionFactoryObj::addConnectionListener(const SmartPtrConnectionListener& listener) {
	CAF_CM_FUNCNAME_VALIDATE("addConnectionListener");
	CAF_CM_PRECOND_ISINITIALIZED(_factory);
	_factory->addConnectionListener(listener);
}
