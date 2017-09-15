/*
 *  Created on: May 24, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPINTEGRATIONCORE_CONNECTIONFACTORY_H_
#define AMQPINTEGRATIONCORE_CONNECTIONFACTORY_H_

#include "amqpClient/api/Connection.h"
#include "amqpCore/ConnectionListener.h"

namespace Caf { namespace AmqpIntegration {

/**
 * @ingroup IntObjImpl
 * @brief Interface for connection factory implementations
 */
struct __declspec(novtable) ConnectionFactory : ICafObject {
	CAF_DECL_UUID("D2420BC6-240C-4EAC-8518-E86A93A40035")

	/**
	 * @return a new Connection
	 */
	virtual SmartPtrConnection createConnection() = 0;

	/**
	 * @return the default protocol to use for connections
	 */
	virtual std::string getProtocol() = 0;

	/**
	 * @return the default host to use for connections
	 */
	virtual std::string getHost() = 0;

	/**
	 * @return the default port to use for connections
	 */
	virtual uint32 getPort() = 0;

	/**
	 * @return the default virtual host to use for connections
	 */
	virtual std::string getVirtualHost() = 0;

	/**
	 * @return the default user name to use for connections
	 */
	virtual std::string getUsername() = 0;

	/**
	 * @return the default password to use for connections
	 */
	virtual std::string getPassword() = 0;

	/**
	 * @return the default path to the CA Cert
	 */
	virtual std::string getCaCertPath() = 0;

	/**
	 * @return the default path to the Client Cert
	 */
	virtual std::string getClientCertPath() = 0;

	/**
	 * @return the default path to the Client Key
	 */
	virtual std::string getClientKeyPath() = 0;

	/**
	 * @return the number of connection retries
	 */
	virtual uint16 getRetries() = 0;

	/**
	 * @return the number of connection seconds to wait
	 */
	virtual uint16 getSecondsToWait() = 0;

	/**
	 * @brief Add a connection create/close callback
	 * @param listener ConnectionListener callback
	 */
	virtual void addConnectionListener(const SmartPtrConnectionListener& listener) = 0;
};
CAF_DECLARE_SMART_INTERFACE_POINTER(ConnectionFactory);

}}

#endif /* AMQPINTEGRATIONCORE_CONNECTIONFACTORY_H_ */
