/*
 *  Author: bwilliams
 *  Created: Nov 16, 2015
 *
 *  Copyright (c) 2015 Vmware, Inc.  All rights reserved.
 *  -- VMware Confidential
 *
 */

#ifndef CPersistenceProtocolDoc_h_
#define CPersistenceProtocolDoc_h_

namespace Caf {

/// A simple container for objects of type CPersistenceProtocolDoc
class CPersistenceProtocolDoc {
public:
	CPersistenceProtocolDoc() :
		_isInitialized(false) {}
	virtual ~CPersistenceProtocolDoc() {}

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
			const std::string& protocolName = std::string(),
			const std::string& uri = std::string(),
			const std::string& uriAmqp = std::string(),
			const std::string& uriTunnel = std::string(),
			const std::string& tlsCert = std::string(),
			const std::string& tlsProtocol = std::string(),
			const Cdeqstr& tlsCipherCollection = Cdeqstr(),
			const SmartPtrCCertCollectionDoc& tlsCertCollection = SmartPtrCCertCollectionDoc(),
			const std::string& uriAmqpPath = std::string(),
			const std::string& uriTunnelPath = std::string(),
			const std::string& tlsCertPath = std::string(),
			const SmartPtrCCertPathCollectionDoc& tlsCertPathCollection = SmartPtrCCertPathCollectionDoc()) {
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

public:
	/// Accessor for the ProtocolName
	std::string getProtocolName() const {
		return _protocolName;
	}

	/// Accessor for the Uri
	std::string getUri() const {
		return _uri;
	}

	/// Accessor for the UriAmqp
	std::string getUriAmqp() const {
		return _uriAmqp;
	}

	/// Accessor for the UriTunnel
	std::string getUriTunnel() const {
		return _uriTunnel;
	}

	/// Accessor for the TlsCert
	std::string getTlsCert() const {
		return _tlsCert;
	}

	/// Accessor for the TlsProtocol
	std::string getTlsProtocol() const {
		return _tlsProtocol;
	}

	/// Accessor for the tlsCipherCollection
	Cdeqstr getTlsCipherCollection() const {
		return _tlsCipherCollection;
	}

	/// Accessor for the TlsCertCollection
	SmartPtrCCertCollectionDoc getTlsCertCollection() const {
		return _tlsCertCollection;
	}

	/// Accessor for the UriAmqpPath
	std::string getUriAmqpPath() const {
		return _uriAmqpPath;
	}

	/// Accessor for the UriTunnelPath
	std::string getUriTunnelPath() const {
		return _uriTunnelPath;
	}

	/// Accessor for the TlsCertPath
	std::string getTlsCertPath() const {
		return _tlsCertPath;
	}

	/// Accessor for the TlsCertPathCollection
	SmartPtrCCertPathCollectionDoc getTlsCertPathCollection() const {
		return _tlsCertPathCollection;
	}

private:
	std::string _protocolName;
	std::string _uri;
	std::string _uriAmqp;
	std::string _uriTunnel;
	std::string _tlsCert;
	std::string _tlsProtocol;
	Cdeqstr _tlsCipherCollection;
	SmartPtrCCertCollectionDoc _tlsCertCollection;

	std::string _uriAmqpPath;
	std::string _uriTunnelPath;
	std::string _tlsCertPath;
	SmartPtrCCertPathCollectionDoc _tlsCertPathCollection;

	bool _isInitialized;

private:
	CAF_CM_DECLARE_NOCOPY(CPersistenceProtocolDoc);
};

CAF_DECLARE_SMART_POINTER(CPersistenceProtocolDoc);

}

#endif
