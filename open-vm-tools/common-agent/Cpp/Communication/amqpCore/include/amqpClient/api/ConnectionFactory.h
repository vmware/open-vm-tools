/*
 *  Created on: May 4, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPCLIENTAPI_CONNECTIONFACTORY_H_
#define AMQPCLIENTAPI_CONNECTIONFACTORY_H_

#include "ICafObject.h"

#include "amqpClient/api/Address.h"
#include "amqpClient/api/CertInfo.h"
#include "amqpClient/api/Connection.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApi
 * @brief Interface to a convenience factory class to facilitate opening a
 * #Caf::AmqpClient::Connection to an AMQP broker.
 * <p>
 * ConnectionFactories are creating using the #Caf::AmqpClient::createConnectionFactory method.
 */
struct __declspec(novtable) ConnectionFactory : public ICafObject {

	/**
	 * @return the default protocol to use for connections
	 */
	virtual std::string getProtocol() const = 0;

	/**
	 * @brief Set the default protocol for connections
	 * @param protocol the default protocol for connections
	 */
	virtual void setProtocol(const std::string& protocol) = 0;

	/**
	 * @return the default host to use for connections
	 */
	virtual std::string getHost() const = 0;

	/**
	 * @brief Set the default host for connections
	 * @param host the default host for connections
	 */
	virtual void setHost(const std::string& host) = 0;

	/**
	 * @return the default port to use for connections
	 */
	virtual uint32 getPort() const = 0;

	/**
	 * @brief Set the default port for connections
	 * @param port the default port for connections
	 */
	virtual void setPort(const uint32 port) = 0;

	/**
	 * @return the default virtual host to use for connections
	 */
	virtual std::string getVirtualHost() const = 0;

	/**
	 * @brief Set the default virtual host for connections
	 * @param virtualHost the default virtual host for connections
	 */
	virtual void setVirtualHost(const std::string& virtualHost) = 0;

	/**
	 * @return the default user name to use for connections
	 */
	virtual std::string getUsername() const = 0;

	/**
	 * @brief Set the default user name for connections
	 * @param username the default user name for connections
	 */
	virtual void setUsername(const std::string& username) = 0;

	/**
	 * @return the default password to use for connections
	 */
	virtual std::string getPassword() const = 0;

	/**
	 * @brief Set the default password for connections
	 * @param password the default password for connections
	 */
	virtual void setPassword(const std::string& password) = 0;

	/**
	 * @return the default CaCertPath to use for connections
	 */
	virtual std::string getCaCertPath() const = 0;

	/**
	 * @brief Set the default CaCertPath for connections
	 * @param caCertPath the default CaCertPath for connections
	 */
	virtual void setCaCertPath(const std::string& caCertPath) = 0;

	/**
	 * @return the default ClientCertPath to use for connections
	 */
	virtual std::string getClientCertPath() const = 0;

	/**
	 * @brief Set the default ClientCertPath for connections
	 * @param clientCertPath the default ClientCertPath for connections
	 */
	virtual void setClientCertPath(const std::string& clientCertPath) = 0;

	/**
	 * @return the default ClientKeyPath to use for connections
	 */
	virtual std::string getClientKeyPath() const = 0;

	/**
	 * @brief Set the default ClientKeyPath for connections
	 * @param clientKeyPath the default ClientKeyPath for connections
	 */
	virtual void setClientKeyPath(const std::string& clientKeyPath) = 0;

	/**
	 * @return the initially requested maximum channel number; zero for unlimited
	 */
	virtual uint32 getRequestedChannelMax() const = 0;

	/**
	 * @brief Set the requested maximum channel number
	 * @param requestedChannelMax the initially requested maximum channel number; zero for unlimited
	 */
	virtual void setRequestedChannelMax(const uint32 requestedChannelMax) = 0;

	/**
	 * @return the initially requested maximum frame size; zero for unlimited
	 */
	virtual uint32 getRequestedFrameMax() const = 0;

	/**
	 * @brief Set the requested maximum frame size
	 * @param requestedFrameMax the initially requested maximum frame size; zero for unlimited
	 */
	virtual void setRequestedFrameMax(const uint32 requestedFrameMax) = 0;

	/**
	 * @return the initially requested heartbeat interval, in seconds; zero for none
	 */
	virtual uint32 getRequestedHeartbeat() const = 0;

	/**
	 * @brief Set the requested heartbeat interval
	 * @param requestedHeartbeat the initially requested heartbeat interval, in seconds; zero for none
	 */
	virtual void setRequestedHeartbeat(const uint32 requestedHeartbeat) = 0;

	/**
	 * @return the connection timeout, in milliseconds; zero for infinite
	 */
	virtual uint32 getConnectionTimeout() const = 0;

	/**
	 * @brief Set the connection timeout
	 * @param connectionTimeout connection establishment timeout in milliseconds; zero for infinite
	 */
	virtual void setConnectionTimeout(const uint32 connectionTimeout) = 0;

	/**
	 * @return the number of connection consumer processing threads
	 */
	virtual uint32 getConsumerThreadCount() const = 0;

	/**
	 * @brief Set the number of connection consumer processing threads
	 * @param threadCount the number of connection consumer processing threads
	 */
	virtual void setConsumerThreadCount(const uint32 threadCount) = 0;

	/**
	 * @return the number of connection retries
	 */
	virtual uint16 getRetries() const = 0;

	/**
	 * @brief Set the number of connection retries
	 * @param retries the number of connection retries
	 */
	virtual void setRetries(const uint16 retries) = 0;

	/**
	 * @return the number of connection seconds to wait
	 */
	virtual uint16 getSecondsToWait() const = 0;

	/**
	 * @brief Set the number of connection retries
	 * @param seconds the number of connection seconds to wait
	 */
	virtual void setSecondsToWait(const uint16 seconds) = 0;

	/**
	 * @brief Create a new broker connection
	 * @return a #Caf::AmqpClient::Connection interface to the connection
	 */
	virtual SmartPtrConnection newConnection() = 0;

	/**
	 * @brief Create a new broker connection
	 * @param address broker address to try
	 * @return a #Caf::AmqpClient::Connection interface to the connection
	 */
	virtual SmartPtrConnection newConnection(
			const SmartPtrAddress& address,
			const SmartPtrCertInfo& certInfo) = 0;
};

CAF_DECLARE_SMART_INTERFACE_POINTER(ConnectionFactory);

/**
 * @ingroup AmqpApi
 * @brief Create a new #Caf::AmqpClient::ConnectionFactory
 * @return a #Caf::AmqpClient::ConnectionFactory interface to a new connection factory
 */
SmartPtrConnectionFactory AMQPCLIENT_LINKAGE createConnectionFactory();

}}

#endif
