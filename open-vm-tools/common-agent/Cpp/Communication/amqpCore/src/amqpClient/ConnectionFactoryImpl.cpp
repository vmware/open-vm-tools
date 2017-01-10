/*
 *  Created on: May 2, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpClient/AMQConnection.h"
#include "amqpClient/api/Address.h"
#include "amqpClient/api/CertInfo.h"
#include "amqpClient/api/Connection.h"
#include "amqpClient/api/ConnectionFactory.h"
#include "amqpClient/ConnectionFactoryImpl.h"

using namespace Caf::AmqpClient;

SmartPtrConnectionFactory AMQPCLIENT_LINKAGE Caf::AmqpClient::createConnectionFactory() {
	SmartPtrConnectionFactoryImpl factory;
	factory.CreateInstance();
	return factory;
}

ConnectionFactoryImpl::ConnectionFactoryImpl() :
	_protocol(DEFAULT_PROTOCOL),
	_host(DEFAULT_HOST),
	_port(USE_DEFAULT_PORT),
	_virtualHost(DEFAULT_VHOST),
	_username(DEFAULT_USER),
	_password(DEFAULT_PASS),
	_requestedChannelMax(DEFAULT_CHANNEL_MAX),
	_requestedFrameMax(DEFAULT_FRAME_MAX),
	_requestedHeartbeat(DEFAULT_HEARTBEAT),
	_connectionTimeout(DEFAULT_CONNECTION_TIMEOUT),
	_consumerThreadCount(DEFAULT_CONSUMER_THREAD_COUNT),
	_retries(DEFAULT_CONNECTION_RETRIES),
	_secondsToWait(DEFAULT_CONNECTION_SECONDS_TO_WAIT),
	CAF_CM_INIT("ConnectionFactoryImpl") {
}

ConnectionFactoryImpl::~ConnectionFactoryImpl() {
}

std::string ConnectionFactoryImpl::getProtocol() const {
	return _protocol;
}

void ConnectionFactoryImpl::setProtocol(const std::string& protocol) {
	_protocol = protocol;
}

std::string ConnectionFactoryImpl::getHost() const {
	return _host;
}

void ConnectionFactoryImpl::setHost(const std::string& host) {
	_host = host;
}

uint32 ConnectionFactoryImpl::getPort() const {
	return portOrDefault(_port);
}

void ConnectionFactoryImpl::setPort(const uint32 port) {
	_port = port;
}

std::string ConnectionFactoryImpl::getVirtualHost() const {
	return _virtualHost;
}

void ConnectionFactoryImpl::setVirtualHost(const std::string& virtualHost) {
	_virtualHost = virtualHost;
}

std::string ConnectionFactoryImpl::getUsername() const {
	return _username;
}

void ConnectionFactoryImpl::setUsername(const std::string& username) {
	_username = username;
}

std::string ConnectionFactoryImpl::getPassword() const {
	return _password;
}

void ConnectionFactoryImpl::setPassword(const std::string& password) {
	_password = password;
}

std::string ConnectionFactoryImpl::getCaCertPath() const {
	return _caCertPath;
}

void ConnectionFactoryImpl::setCaCertPath(const std::string& caCertPath) {
	_caCertPath = caCertPath;
}

std::string ConnectionFactoryImpl::getClientCertPath() const {
	return _clientCertPath;
}

void ConnectionFactoryImpl::setClientCertPath(const std::string& clientCertPath) {
	_clientCertPath = clientCertPath;
}

std::string ConnectionFactoryImpl::getClientKeyPath() const {
	return _clientKeyPath;
}

void ConnectionFactoryImpl::setClientKeyPath(const std::string& clientKeyPath) {
	_clientKeyPath = clientKeyPath;
}

uint32 ConnectionFactoryImpl::getRequestedChannelMax() const {
	return _requestedChannelMax;
}

void ConnectionFactoryImpl::setRequestedChannelMax(const uint32 requestedChannelMax) {
	_requestedChannelMax = requestedChannelMax;
}

uint32 ConnectionFactoryImpl::getRequestedFrameMax() const {
	return _requestedFrameMax;
}

void ConnectionFactoryImpl::setRequestedFrameMax(const uint32 requestedFrameMax) {
	_requestedFrameMax = requestedFrameMax;
}

uint32 ConnectionFactoryImpl::getRequestedHeartbeat() const {
	return _requestedHeartbeat;
}

void ConnectionFactoryImpl::setRequestedHeartbeat(const uint32 requestedHeartbeat) {
	_requestedHeartbeat = requestedHeartbeat;
}

uint32 ConnectionFactoryImpl::getConnectionTimeout() const {
	return _connectionTimeout;
}

void ConnectionFactoryImpl::setConnectionTimeout(const uint32 connectionTimeout) {
	_connectionTimeout = connectionTimeout;
}

uint32 ConnectionFactoryImpl::getConsumerThreadCount() const {
	return _consumerThreadCount;
}

void ConnectionFactoryImpl::setConsumerThreadCount(const uint32 threadCount) {
	CAF_CM_FUNCNAME_VALIDATE("setConsumerThreadCount");
	CAF_CM_VALIDATE_NOTZERO(threadCount);
	_consumerThreadCount = threadCount;
}

uint16 ConnectionFactoryImpl::getRetries() const {
	return _retries;
}

void ConnectionFactoryImpl::setRetries(const uint16 retries) {
	_retries = retries;
}

uint16 ConnectionFactoryImpl::getSecondsToWait() const {
	return _secondsToWait;
}

void ConnectionFactoryImpl::setSecondsToWait(const uint16 seconds) {
	_secondsToWait = seconds;
}

SmartPtrConnection ConnectionFactoryImpl::newConnection() {
	SmartPtrAddress address;
	address.CreateInstance();
	address->initialize(getProtocol(), getHost(), getPort(), getVirtualHost());

	SmartPtrCertInfo certInfo;
	if (! getCaCertPath().empty() &&
			! getClientCertPath().empty() &&
			! getClientKeyPath().empty()) {
		certInfo.CreateInstance();
		certInfo->initialize(getCaCertPath(), getClientCertPath(), getClientKeyPath());
	}

	return newConnection(address, certInfo);
}

SmartPtrConnection ConnectionFactoryImpl::newConnection(
		const SmartPtrAddress& address,
		const SmartPtrCertInfo& certInfo) {
	SmartPtrAMQConnection conn;
	conn.CreateInstance();
	conn->init(
			_username,
			_password,
			address,
			certInfo,
			_requestedFrameMax,
			_requestedChannelMax,
			_requestedHeartbeat,
			_connectionTimeout,
			_consumerThreadCount,
			_retries,
			_secondsToWait);
	conn->start();
	return conn;
}

uint32 ConnectionFactoryImpl::portOrDefault(const uint32 port) const {
	return (USE_DEFAULT_PORT == port) ? DEFAULT_AMQP_PORT : port;
}
