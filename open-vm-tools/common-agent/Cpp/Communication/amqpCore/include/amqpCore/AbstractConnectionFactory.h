/*
 *  Created on: May 24, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPINTEGRATIONCORE_ABSTRACTCONNECTIONFACTORY_H_
#define AMQPINTEGRATIONCORE_ABSTRACTCONNECTIONFACTORY_H_

#include "amqpCore/ConnectionFactory.h"
#include "amqpClient/api/ConnectionFactory.h"
#include "amqpClient/api/Connection.h"
#include "amqpCore/CompositeConnectionListener.h"
#include "amqpCore/ConnectionListener.h"

namespace Caf { namespace AmqpIntegration {

class AMQPINTEGRATIONCORE_LINKAGE AbstractConnectionFactory : public ConnectionFactory {
public:
	AbstractConnectionFactory();
	virtual ~AbstractConnectionFactory();

	/**
	 * @brief Initialize the connection factory
	 * @param amqpConnectionFactory the underlying Caf::AmqpClient::ConnectionFactory object
	 */
	void init(const AmqpClient::SmartPtrConnectionFactory& amqpConnectionFactory);

	virtual void setConnectionListeners(const std::deque<SmartPtrConnectionListener>& listeners);

	/**
	 * @brief Set the default protocol for connections
	 * @param protocol the default protocol for connections
	 */
	void setProtocol(const std::string& protocol);

	/**
	 * @brief Set the default host for connections
	 * @param host the default host for connections
	 */
	void setHost(const std::string& host);

	/**
	 * @brief Set the default port for connections
	 * @param port the default port for connections
	 */
	void setPort(const uint32 port);

	/**
	 * @brief Set the default host for connections
	 * @param virtualHost the default host for connections
	 */
	void setVirtualHost(const std::string& virtualHost);

	/**
	 * @brief Set the default user name for connections
	 * @param username the default user name for connections
	 */
	void setUsername(const std::string& username);

	/**
	 * @brief Set the default password for connections
	 * @param password the default password for connections
	 */
	void setPassword(const std::string& password);

	/**
	 * @brief Set the default CaCertPath for connections
	 * @param caCertPath the default CaCertPath for connections
	 */
	void setCaCertPath(const std::string& caCertPath);

	/**
	 * @brief Set the default ClientCertPath for connections
	 * @param clientCertPath the default ClientCertPath for connections
	 */
	void setClientCertPath(const std::string& clientCertPath);

	/**
	 * @brief Set the default ClientKeyPath for connections
	 * @param clientKeyPath the default ClientKeyPath for connections
	 */
	void setClientKeyPath(const std::string& clientKeyPath);

	/**
	 * @brief Set the number of connection retries
	 * @param retries
	 */
	void setRetries(const uint16 retries);

	/**
	 * @brief Set the wait period in seconds
	 * @param seconds
	 */
	void setSecondsToWait(const uint16 seconds);

	/**
	 * @brief Set the connection timeout
	 * @param connectionTimeout connection establishment timeout in milliseconds; zero for infinite
	 */
	void setConnectionTimeout(const uint32 connectionTimeout);

public: // ConnectionFactory
	virtual SmartPtrConnection createConnection() = 0;
	std::string getProtocol();
	std::string getHost();
	uint32 getPort();
	std::string getVirtualHost();
	std::string getUsername();
	std::string getPassword();
	std::string getCaCertPath();
	std::string getClientCertPath();
	std::string getClientKeyPath();
	uint16 getRetries();
	uint16 getSecondsToWait();
	virtual void addConnectionListener(const SmartPtrConnectionListener& listener);

protected:
	std::string getDefaultProtocol();

	std::string getDefaultHostName();

	SmartPtrConnection createBareConnection();

	SmartPtrConnectionListener getConnectionListener();

private:
	bool _isInitialized;
	AmqpClient::SmartPtrConnectionFactory _amqpConnectionFactory;
	SmartPtrCompositeConnectionListener _connectionListener;

	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(AbstractConnectionFactory);
};
CAF_DECLARE_SMART_POINTER(AbstractConnectionFactory);

}}

#endif
