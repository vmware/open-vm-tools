/*
 *  Author: bwilliams
 *  Created: Nov 16, 2015
 *
 *  Copyright (C) 2015-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#ifndef CPersistenceProtocolDoc_h_
#define CPersistenceProtocolDoc_h_


#include "Doc/PersistenceDoc/CCertCollectionDoc.h"
#include "Doc/PersistenceDoc/CCertPathCollectionDoc.h"

namespace Caf {

/// A simple container for objects of type CPersistenceProtocolDoc
class PERSISTENCEDOC_LINKAGE CPersistenceProtocolDoc {
public:
	CPersistenceProtocolDoc();
	virtual ~CPersistenceProtocolDoc();

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
			const SmartPtrCCertPathCollectionDoc& tlsCertPathCollection = SmartPtrCCertPathCollectionDoc());

public:
	/// Accessor for the ProtocolName
	std::string getProtocolName() const;

	/// Accessor for the Uri
	std::string getUri() const;

	/// Accessor for the UriAmqp
	std::string getUriAmqp() const;

	/// Accessor for the UriTunnel
	std::string getUriTunnel() const;

	/// Accessor for the TlsCert
	std::string getTlsCert() const;

	/// Accessor for the TlsProtocol
	std::string getTlsProtocol() const;

	/// Accessor for the tlsCipherCollection
	Cdeqstr getTlsCipherCollection() const;

	/// Accessor for the TlsCertCollection
	SmartPtrCCertCollectionDoc getTlsCertCollection() const;

	/// Accessor for the UriAmqpPath
	std::string getUriAmqpPath() const;

	/// Accessor for the UriTunnelPath
	std::string getUriTunnelPath() const;

	/// Accessor for the TlsCertPath
	std::string getTlsCertPath() const;

	/// Accessor for the TlsCertPathCollection
	SmartPtrCCertPathCollectionDoc getTlsCertPathCollection() const;

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
