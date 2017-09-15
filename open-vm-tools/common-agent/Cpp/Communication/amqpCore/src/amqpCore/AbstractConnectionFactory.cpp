/*
 *  Created on: May 24, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpClient/api/Connection.h"
#include "amqpCore/ConnectionListener.h"
#include "amqpCore/SimpleConnection.h"
#include "amqpCore/AbstractConnectionFactory.h"

using namespace Caf::AmqpIntegration;

AbstractConnectionFactory::AbstractConnectionFactory() :
	_isInitialized(false),
	CAF_CM_INIT("AbstractConnectionFactory") {
}

AbstractConnectionFactory::~AbstractConnectionFactory() {
}

void AbstractConnectionFactory::init(
		const AmqpClient::SmartPtrConnectionFactory& amqpConnectionFactory) {
	CAF_CM_FUNCNAME_VALIDATE("init");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(amqpConnectionFactory);
	_amqpConnectionFactory = amqpConnectionFactory;
	_connectionListener.CreateInstance();
	_isInitialized = true;
}

void AbstractConnectionFactory::setProtocol(const std::string& protocol) {
	CAF_CM_FUNCNAME_VALIDATE("setProtocol");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	_amqpConnectionFactory->setProtocol(protocol);
}

void AbstractConnectionFactory::setHost(const std::string& host) {
	CAF_CM_FUNCNAME_VALIDATE("setHost");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	_amqpConnectionFactory->setHost(host);
}

void AbstractConnectionFactory::setPort(const uint32 port) {
	CAF_CM_FUNCNAME_VALIDATE("setPort");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	_amqpConnectionFactory->setPort(port);
}

void AbstractConnectionFactory::setVirtualHost(const std::string& virtualHost) {
	CAF_CM_FUNCNAME_VALIDATE("setVirtualHost");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	_amqpConnectionFactory->setVirtualHost(virtualHost);
}

void AbstractConnectionFactory::setUsername(const std::string& username) {
	CAF_CM_FUNCNAME_VALIDATE("setUsername");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	_amqpConnectionFactory->setUsername(username);
}

void AbstractConnectionFactory::setPassword(const std::string& password) {
	CAF_CM_FUNCNAME_VALIDATE("setPassword");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	_amqpConnectionFactory->setPassword(password);
}

void AbstractConnectionFactory::setCaCertPath(const std::string& caCertPath) {
	CAF_CM_FUNCNAME_VALIDATE("setCaCertPath");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	_amqpConnectionFactory->setCaCertPath(caCertPath);
}

void AbstractConnectionFactory::setClientCertPath(const std::string& clientCertPath) {
	CAF_CM_FUNCNAME_VALIDATE("setClientCertPath");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	_amqpConnectionFactory->setClientCertPath(clientCertPath);
}

void AbstractConnectionFactory::setClientKeyPath(const std::string& clientKeyPath) {
	CAF_CM_FUNCNAME_VALIDATE("setClientKeyPath");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	_amqpConnectionFactory->setClientKeyPath(clientKeyPath);
}

void AbstractConnectionFactory::setConnectionTimeout(const uint32 connectionTimeout) {
	CAF_CM_FUNCNAME_VALIDATE("setConnectionTimeout");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	_amqpConnectionFactory->setConnectionTimeout(connectionTimeout);
}

void AbstractConnectionFactory::setRetries(const uint16 retries) {
	CAF_CM_FUNCNAME_VALIDATE("setRetries");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	_amqpConnectionFactory->setRetries(retries);
}

void AbstractConnectionFactory::setSecondsToWait(const uint16 seconds) {
	CAF_CM_FUNCNAME_VALIDATE("setSecondsToWait");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	_amqpConnectionFactory->setSecondsToWait(seconds);
}

std::string AbstractConnectionFactory::getDefaultProtocol() {
	return Caf::AmqpClient::DEFAULT_PROTOCOL;
}

std::string AbstractConnectionFactory::getDefaultHostName() {
	return g_get_host_name();
}

SmartPtrConnection AbstractConnectionFactory::createBareConnection() {
	CAF_CM_FUNCNAME_VALIDATE("createBareConnection");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	SmartPtrSimpleConnection connection;
	connection.CreateInstance();
	connection->init(_amqpConnectionFactory->newConnection());
	return connection;
}

SmartPtrConnectionListener AbstractConnectionFactory::getConnectionListener() {
	return _connectionListener;
}


void AbstractConnectionFactory::setConnectionListeners(
		const std::deque<SmartPtrConnectionListener>& listeners) {
	CAF_CM_FUNCNAME_VALIDATE("setConnectionListeners");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	_connectionListener->setDelegates(listeners);
}

void AbstractConnectionFactory::addConnectionListener(
		const SmartPtrConnectionListener& listener) {
	CAF_CM_FUNCNAME_VALIDATE("addConnectionListener");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	_connectionListener->addDelegate(listener);
}

std::string AbstractConnectionFactory::getProtocol() {
	CAF_CM_FUNCNAME_VALIDATE("getProtocol");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _amqpConnectionFactory->getProtocol();
}

std::string AbstractConnectionFactory::getHost() {
	CAF_CM_FUNCNAME_VALIDATE("getHost");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _amqpConnectionFactory->getHost();
}

uint32 AbstractConnectionFactory::getPort() {
	CAF_CM_FUNCNAME_VALIDATE("getPort");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _amqpConnectionFactory->getPort();
}

std::string AbstractConnectionFactory::getVirtualHost() {
	CAF_CM_FUNCNAME_VALIDATE("getVirtualHost");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _amqpConnectionFactory->getVirtualHost();
}

std::string AbstractConnectionFactory::getUsername() {
	CAF_CM_FUNCNAME_VALIDATE("getUsername");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _amqpConnectionFactory->getUsername();
}

std::string AbstractConnectionFactory::getPassword() {
	CAF_CM_FUNCNAME_VALIDATE("getPassword");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _amqpConnectionFactory->getPassword();
}

std::string AbstractConnectionFactory::getCaCertPath() {
	CAF_CM_FUNCNAME_VALIDATE("getCaCertPath");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _amqpConnectionFactory->getCaCertPath();
}

std::string AbstractConnectionFactory::getClientCertPath() {
	CAF_CM_FUNCNAME_VALIDATE("getClientCertPath");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _amqpConnectionFactory->getClientCertPath();
}

std::string AbstractConnectionFactory::getClientKeyPath() {
	CAF_CM_FUNCNAME_VALIDATE("getClientKeyPath");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _amqpConnectionFactory->getClientKeyPath();
}

uint16 AbstractConnectionFactory::getRetries() {
	CAF_CM_FUNCNAME_VALIDATE("getRetries");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _amqpConnectionFactory->getRetries();
}

uint16 AbstractConnectionFactory::getSecondsToWait() {
	CAF_CM_FUNCNAME_VALIDATE("getSecondsToWait");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _amqpConnectionFactory->getSecondsToWait();
}
