/*
 *  Created on: May 2, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CONNECTIONFACTORYIMPL_H_
#define CONNECTIONFACTORYIMPL_H_

#include "amqpClient/api/ConnectionFactory.h"
#include "amqpClient/api/Address.h"
#include "amqpClient/api/CertInfo.h"
#include "amqpClient/api/Connection.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Implementation of ConnectionFactory
 */
class ConnectionFactoryImpl : public ConnectionFactory {
public:
	ConnectionFactoryImpl();
	virtual ~ConnectionFactoryImpl();

public: // ConectionFactory
	std::string getProtocol() const;
	void setProtocol(const std::string& host);
	std::string getHost() const;
	void setHost(const std::string& host);
	uint32 getPort() const;
	void setPort(const uint32 port);
	std::string getVirtualHost() const;
	void setVirtualHost(const std::string& virtualHost);
	std::string getUsername() const;
	void setUsername(const std::string& username);
	std::string getPassword() const;
	void setPassword(const std::string& password);
	std::string getCaCertPath() const;
	void setCaCertPath(const std::string& caCertPath);
	std::string getClientCertPath() const;
	void setClientCertPath(const std::string& clientCertPath);
	std::string getClientKeyPath() const;
	void setClientKeyPath(const std::string& clientKeyPath);
	uint32 getRequestedChannelMax() const;
	void setRequestedChannelMax(const uint32 requestedChannelMax);
	uint32 getRequestedFrameMax() const;
	void setRequestedFrameMax(const uint32 requestedFrameMax);
	uint32 getRequestedHeartbeat() const;
	void setRequestedHeartbeat(const uint32 requestedHeartbeat);
	uint32 getConnectionTimeout() const;
	void setConnectionTimeout(const uint32 connectionTimeout);
	uint32 getConsumerThreadCount() const;
	void setConsumerThreadCount(const uint32 threadCount);
	uint16 getRetries() const;
	void setRetries(const uint16 retries);
	uint16 getSecondsToWait() const;
	void setSecondsToWait(const uint16 seconds);

	SmartPtrConnection newConnection();
	SmartPtrConnection newConnection(
			const SmartPtrAddress& address,
			const SmartPtrCertInfo& certInfo);

private:
	uint32 portOrDefault(const uint32 port) const;

private:
	std::string _protocol;
	std::string _host;
	uint32 _port;
	std::string _virtualHost;
	std::string _username;
	std::string _password;
	std::string _caCertPath;
	std::string _clientCertPath;
	std::string _clientKeyPath;
	uint32 _requestedChannelMax;
	uint32 _requestedFrameMax;
	uint32 _requestedHeartbeat;
	uint32 _connectionTimeout;
	uint32 _consumerThreadCount;
	uint16 _retries;
	uint16 _secondsToWait;

	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(ConnectionFactoryImpl);
};
CAF_DECLARE_SMART_POINTER(ConnectionFactoryImpl);

}}

#endif
