/*
 *  Created on: May 4, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPCLIENTAPI_ADDRESS_H_
#define AMQPCLIENTAPI_ADDRESS_H_

namespace Caf { namespace AmqpClient {

/** Supported Protocols */
typedef enum {
	PROTOCOL_AMQP = 0,
	PROTOCOL_AMQPS,
	PROTOCOL_TUNNEL
} Protocol;

/**
 * @author mdonahue
 * @brief A representation of a broker network address
 */
class Address {
public:
	Address();
	virtual ~Address();

public:
	/**
	 * @brief Construct an address from a protocol, host name, and port number
	 * @param protocol the communication protocol (tcp, ssl, etc.)
	 * @param host the host name or dotted ip address
	 * @param port the port number
	 */
	void initialize(
			const std::string& protocol,
			const std::string& host,
			const uint32& port,
			const std::string& virtualHost = std::string());

	/**
	 * @return the protocol
	 */
	Protocol getProtocol() const;

	/**
	 * @return the protocol
	 */
	std::string getProtocolStr() const;

	/**
	 * @return the host name
	 */
	std::string getHost() const;

	/**
	 * @return the port number
	 */
	uint32 getPort() const;

	std::string getVirtualHost() const;

	std::string toString() const;

private:
	Protocol translateProtocol(
			const std::string& protocol,
			const std::string& host) const;

private:
	bool _isInitialized;
	std::string _protocolStr;
	Protocol _protocol;
	std::string _host;
	uint32 _port;
	std::string _virtualHost;
	std::string _toString;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(Address);
};
CAF_DECLARE_SMART_POINTER(Address);

}}

#endif
