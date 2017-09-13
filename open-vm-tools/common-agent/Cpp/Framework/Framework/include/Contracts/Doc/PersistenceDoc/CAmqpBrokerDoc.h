/*
 *  Author: bwilliams
 *  Created: Nov 16, 2015
 *
 *  Copyright (c) 2015 Vmware, Inc.  All rights reserved.
 *  -- VMware Confidential
 *
 */

#ifndef CAmqpBrokerDoc_h_
#define CAmqpBrokerDoc_h_

namespace Caf {

/// A simple container for objects of type CAmqpBrokerDoc
class CAmqpBrokerDoc {
public:
	CAmqpBrokerDoc() :
		_isInitialized(false) {}
	virtual ~CAmqpBrokerDoc() {}

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
			const std::string& amqpBrokerId = std::string(),
			const std::string& uri = std::string(),
			const std::string& tlsCert = std::string(),
			const std::string& tlsProtocol = std::string(),
			const Cdeqstr& tlsCipherCollection = Cdeqstr(),
			const SmartPtrCCertCollectionDoc& tlsCertCollection = SmartPtrCCertCollectionDoc(),
			const std::string& tlsCertPath = std::string(),
			const SmartPtrCCertPathCollectionDoc& tlsCertPathCollection = SmartPtrCCertPathCollectionDoc()) {
		if (! _isInitialized) {
			_amqpBrokerId = amqpBrokerId;
			_uri = uri;
			_tlsCert = tlsCert;
			_tlsProtocol = tlsProtocol;
			_tlsCipherCollection = tlsCipherCollection;
			_tlsCertCollection = tlsCertCollection;

			_tlsCertPath = tlsCertPath;
			_tlsCertPathCollection = tlsCertPathCollection;

			_isInitialized = true;
		}
	}

public:
	/// Accessor for the AmqpBrokerId
	std::string getAmqpBrokerId() const {
		return _amqpBrokerId;
	}

	/// Accessor for the Uri
	std::string getUri() const {
		return _uri;
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

	/// Accessor for the TlsCertPath
	std::string getTlsCertPath() const {
		return _tlsCertPath;
	}

	/// Accessor for the TlsCertPathCollection
	SmartPtrCCertPathCollectionDoc getTlsCertPathCollection() const {
		return _tlsCertPathCollection;
	}

private:
	std::string _amqpBrokerId;
	std::string _uri;
	std::string _tlsCert;
	std::string _tlsProtocol;
	Cdeqstr _tlsCipherCollection;
	SmartPtrCCertCollectionDoc _tlsCertCollection;

	std::string _tlsCertPath;
	SmartPtrCCertPathCollectionDoc _tlsCertPathCollection;

	bool _isInitialized;

private:
	CAF_CM_DECLARE_NOCOPY(CAmqpBrokerDoc);
};

CAF_DECLARE_SMART_POINTER(CAmqpBrokerDoc);

}

#endif
