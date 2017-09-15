/*
 *  Created on: May 2, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "amqpClient/api/Address.h"
#include "Exception/CCafException.h"

using namespace Caf::AmqpClient;

Address::Address() :
		_isInitialized(false),
		_protocol(PROTOCOL_AMQP),
		_port(UINT_MAX),
		CAF_CM_INIT_LOG("Address") {}

Address::~Address() {}

void Address::initialize(
		const std::string& protocol,
		const std::string& host,
		const uint32& port,
		const std::string& virtualHost) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(protocol);
	CAF_CM_VALIDATE_STRING(host);

	_protocol = translateProtocol(protocol, host);

	_protocolStr = protocol;
	_host = host;
	_port = port;
	_virtualHost = virtualHost;

	std::stringstream builder;
	builder << _protocolStr << ":"
			<< "host=" << _host << ","
			<< "port=" << _port << ","
			<< "virtualhost=" << _virtualHost;
	_toString = builder.str();

	_isInitialized = true;
}

/**
 * @return the protocol
 */
Protocol Address::getProtocol() const {
	CAF_CM_FUNCNAME_VALIDATE("getProtocol");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _protocol;
}

/**
 * @return the protocol
 */
std::string Address::getProtocolStr() const {
	CAF_CM_FUNCNAME_VALIDATE("getProtocol");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _protocolStr;
}

/**
 * @return the host name
 */
std::string Address::getHost() const {
	CAF_CM_FUNCNAME_VALIDATE("getHost");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _host;
}

/**
 * @return the port number
 */
uint32 Address::getPort() const {
	CAF_CM_FUNCNAME_VALIDATE("getPort");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _port;
}

std::string Address::getVirtualHost() const {
	CAF_CM_FUNCNAME_VALIDATE("getVirtualHost");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _virtualHost;
}

std::string Address::toString() const {
	CAF_CM_FUNCNAME_VALIDATE("toString");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _toString;
}

Protocol Address::translateProtocol(
		const std::string& protocol,
		const std::string& host) const {
	CAF_CM_FUNCNAME("translateProtocol");
	CAF_CM_VALIDATE_STRING(protocol);
	CAF_CM_VALIDATE_STRING(host);

	Protocol rc = PROTOCOL_AMQP;
	if (protocol.compare("amqp") == 0) {
		rc = PROTOCOL_AMQP;
		CAF_CM_LOG_DEBUG_VA1(
				"Parsed amqp protocol - host: %s", host.c_str());
	} else if (protocol.compare("amqps") == 0) {
		rc = PROTOCOL_AMQPS;
		CAF_CM_EXCEPTION_VA0(E_FAIL,
				"amqps protocol not yet supported");
	} else if (protocol.compare("tunnel") == 0) {
		if ((host.compare("localhost") == 0) || (host.compare("127.0.0.1") == 0)) {
			rc = PROTOCOL_TUNNEL;
			CAF_CM_LOG_DEBUG_VA1(
					"Parsed tunnel protocol - host: %s", host.c_str());
		} else {
			CAF_CM_EXCEPTION_VA1(E_FAIL,
					"Tunnel protocol only supports localhost - %s", host.c_str());
		}
	} else {
		CAF_CM_EXCEPTION_VA1(E_FAIL,
				"Unknown protocol - %s", protocol.c_str());
	}

	return rc;
}
