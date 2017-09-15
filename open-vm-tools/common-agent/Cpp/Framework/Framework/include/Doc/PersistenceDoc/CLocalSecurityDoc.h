/*
 *  Author: bwilliams
 *  Created: Nov 16, 2015
 *
 *  Copyright (C) 2015-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#ifndef CLocalSecurityDoc_h_
#define CLocalSecurityDoc_h_

namespace Caf {

/// A simple container for objects of type CLocalSecurityDoc
class PERSISTENCEDOC_LINKAGE CLocalSecurityDoc {
public:
	CLocalSecurityDoc();
	virtual ~CLocalSecurityDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
			const std::string& localId = std::string(),
			const std::string& privateKey = std::string(),
			const std::string& cert = std::string(),
			const std::string& privateKeyPath = std::string(),
			const std::string& certPath = std::string());

public:
	/// Accessor for the LocalId
	std::string getLocalId() const;

	/// Accessor for the PrivateKey
	std::string getPrivateKey() const;

	/// Accessor for the Cert
	std::string getCert() const;

	/// Accessor for the PrivateKeyPath
	std::string getPrivateKeyPath() const;

	/// Accessor for the CertPath
	std::string getCertPath() const;

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
