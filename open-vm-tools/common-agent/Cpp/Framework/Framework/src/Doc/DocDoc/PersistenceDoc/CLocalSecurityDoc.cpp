/*
 *  Author: bwilliams
 *  Created: Nov 16, 2015
 *
 *  Copyright (C) 2015-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#include "stdafx.h"
#include "Doc/PersistenceDoc/CLocalSecurityDoc.h"

using namespace Caf;

/// A simple container for objects of type CLocalSecurityDoc
CLocalSecurityDoc::CLocalSecurityDoc() :
	_isInitialized(false) {}
CLocalSecurityDoc::~CLocalSecurityDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CLocalSecurityDoc::initialize(
		const std::string& localId,
		const std::string& privateKey,
		const std::string& cert,
		const std::string& privateKeyPath,
		const std::string& certPath) {
	if (! _isInitialized) {
		_localId = localId;
		_privateKey = privateKey;
		_cert = cert;

		_privateKeyPath = privateKeyPath;
		_certPath = certPath;

		_isInitialized = true;
	}
}

/// Accessor for the LocalId
std::string CLocalSecurityDoc::getLocalId() const {
	return _localId;
}

/// Accessor for the PrivateKey
std::string CLocalSecurityDoc::getPrivateKey() const {
	return _privateKey;
}

/// Accessor for the Cert
std::string CLocalSecurityDoc::getCert() const {
	return _cert;
}

/// Accessor for the PrivateKeyPath
std::string CLocalSecurityDoc::getPrivateKeyPath() const {
	return _privateKeyPath;
}

/// Accessor for the CertPath
std::string CLocalSecurityDoc::getCertPath() const {
	return _certPath;
}







