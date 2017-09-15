/*
 *  Author: bwilliams
 *  Created: Nov 16, 2015
 *
 *  Copyright (c) 2015 Vmware, Inc.  All rights reserved.
 *  -- VMware Confidential
 *
 */

#ifndef CLocalSecurityDoc_h_
#define CLocalSecurityDoc_h_

namespace Caf {

/// A simple container for objects of type CLocalSecurityDoc
class CLocalSecurityDoc {
public:
	CLocalSecurityDoc() :
		_isInitialized(false) {}
	virtual ~CLocalSecurityDoc() {}

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
			const std::string& localId = std::string(),
			const std::string& privateKey = std::string(),
			const std::string& cert = std::string(),
			const std::string& privateKeyPath = std::string(),
			const std::string& certPath = std::string()) {
		if (! _isInitialized) {
			_localId = localId;
			_privateKey = privateKey;
			_cert = cert;

			_privateKeyPath = privateKeyPath;
			_certPath = certPath;

			_isInitialized = true;
		}
	}

public:
	/// Accessor for the LocalId
	std::string getLocalId() const {
		return _localId;
	}

	/// Accessor for the PrivateKey
	std::string getPrivateKey() const {
		return _privateKey;
	}

	/// Accessor for the Cert
	std::string getCert() const {
		return _cert;
	}

	/// Accessor for the PrivateKeyPath
	std::string getPrivateKeyPath() const {
		return _privateKeyPath;
	}

	/// Accessor for the CertPath
	std::string getCertPath() const {
		return _certPath;
	}

private:
	std::string _localId;
	std::string _privateKey;
	std::string _cert;

	std::string _privateKeyPath;
	std::string _certPath;

	bool _isInitialized;

private:
	CAF_CM_DECLARE_NOCOPY(CLocalSecurityDoc);
};

CAF_DECLARE_SMART_POINTER(CLocalSecurityDoc);

}

#endif
