/*
 *  Author: bwilliams
 *  Created: Nov 16, 2015
 *
 *  Copyright (C) 2015-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#include "stdafx.h"

#include "Doc/PersistenceDoc/CCertCollectionDoc.h"
#include "Doc/PersistenceDoc/CCertPathCollectionDoc.h"
#include "Doc/PersistenceDoc/CPersistenceProtocolDoc.h"

using namespace Caf;

/// A simple container for objects of type CPersistenceProtocolDoc
CPersistenceProtocolDoc::CPersistenceProtocolDoc() :
	_isInitialized(false) {}
CPersistenceProtocolDoc::~CPersistenceProtocolDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CPersistenceProtocolDoc::initialize(
		const std::string& protocolName,
		const std::string& uri,
		const std::string& uriAmqp,
		const std::string& uriTunnel,
		const std::string& tlsCert,
		const std::string& tlsProtocol,
		const Cdeqstr& tlsCipherCollection,
		const SmartPtrCCertCollectionDoc& tlsCertCollection,
		const std::string& uriAmqpPath,
		const std::string& uriTunnelPath,
		const std::string& tlsCertPath,
		const SmartPtrCCertPathCollectionDoc& tlsCertPathCollection) {
	if (! _isInitialized) {
		_protocolName = protocolName;
		_uri = uri;
		_uriAmqp = uriAmqp;
		_uriTunnel = uriTunnel;
		_tlsCert = tlsCert;
		_tlsProtocol = tlsProtocol;
		_tlsCipherCollection = tlsCipherCollection;
		_tlsCertCollection = tlsCertCollection;

		_uriAmqpPath = uriAmqpPath;
		_uriTunnelPath = uriTunnelPath;
		_tlsCertPath = tlsCertPath;
		_tlsCertPathCollection = tlsCertPathCollection;

		_isInitialized = true;
	}
}

/// Accessor for the ProtocolName
std::string CPersistenceProtocolDoc::getProtocolName() const {
	return _protocolName;
}

/// Accessor for the Uri
std::string CPersistenceProtocolDoc::getUri() const {
	return _uri;
}

/// Accessor for the UriAmqp
std::string CPersistenceProtocolDoc::getUriAmqp() const {
	return _uriAmqp;
}

/// Accessor for the UriTunnel
std::string CPersistenceProtocolDoc::getUriTunnel() const {
	return _uriTunnel;
}

/// Accessor for the TlsCert
std::string CPersistenceProtocolDoc::getTlsCert() const {
	return _tlsCert;
}

/// Accessor for the TlsProtocol
std::string CPersistenceProtocolDoc::getTlsProtocol() const {
	return _tlsProtocol;
}

/// Accessor for the tlsCipherCollection
Cdeqstr CPersistenceProtocolDoc::getTlsCipherCollection() const {
	return _tlsCipherCollection;
}

/// Accessor for the TlsCertCollection
SmartPtrCCertCollectionDoc CPersistenceProtocolDoc::getTlsCertCollection() const {
	return _tlsCertCollection;
}

/// Accessor for the UriAmqpPath
std::string CPersistenceProtocolDoc::getUriAmqpPath() const {
	return _uriAmqpPath;
}

/// Accessor for the UriTunnelPath
std::string CPersistenceProtocolDoc::getUriTunnelPath() const {
	return _uriTunnelPath;
}

/// Accessor for the TlsCertPath
std::string CPersistenceProtocolDoc::getTlsCertPath() const {
	return _tlsCertPath;
}

/// Accessor for the TlsCertPathCollection
SmartPtrCCertPathCollectionDoc CPersistenceProtocolDoc::getTlsCertPathCollection() const {
	return _tlsCertPathCollection;
}

Cdeqstr _tlsCipherCollection;






