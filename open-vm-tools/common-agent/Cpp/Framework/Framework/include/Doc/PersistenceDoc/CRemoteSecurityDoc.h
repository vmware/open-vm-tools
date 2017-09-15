/*
 *  Author: bwilliams
 *  Created: Nov 16, 2015
 *
 *  Copyright (C) 2015-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#ifndef CRemoteSecurityDoc_h_
#define CRemoteSecurityDoc_h_


#include "Doc/PersistenceDoc/CCertCollectionDoc.h"
#include "Doc/PersistenceDoc/CCertPathCollectionDoc.h"

namespace Caf {

/// A simple container for objects of type CRemoteSecurityDoc
class PERSISTENCEDOC_LINKAGE CRemoteSecurityDoc {
public:
	CRemoteSecurityDoc();
	virtual ~CRemoteSecurityDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::string& remoteId = std::string(),
		const std::string& protocolName = std::string(),
		const std::string& cmsCert = std::string(),
		const std::string& cmsCipherName = std::string(),
		const SmartPtrCCertCollectionDoc& cmsCertCollection = SmartPtrCCertCollectionDoc(),
		const std::string& cmsCertPath = std::string(),
		const SmartPtrCCertPathCollectionDoc& cmsCertPathCollection = SmartPtrCCertPathCollectionDoc());

public:
	/// Accessor for the RemoteId
	std::string getRemoteId() const;

	/// Accessor for the ProtocolName
	std::string getProtocolName() const;

	/// Accessor for the cmsCert
	std::string getCmsCert() const;

	/// Accessor for the CmsCipher
	std::string getCmsCipherName() const;

	/// Accessor for the CertCollection
	SmartPtrCCertCollectionDoc getCmsCertCollection() const;

	/// Accessor for the cmsCertPath
	std::string getCmsCertPath() const;

	/// Accessor for the CertPathCollection
	SmartPtrCCertPathCollectionDoc getCmsCertPathCollection() const;

private:
	std::string _remoteId;
	std::string _protocolName;
	std::string _cmsCert;
	std::string _cmsCipherName;
	SmartPtrCCertCollectionDoc _cmsCertCollection;

	std::string _cmsCertPath;
	SmartPtrCCertPathCollectionDoc _cmsCertPathCollection;

	bool _isInitialized;

private:
	CAF_CM_DECLARE_NOCOPY(CRemoteSecurityDoc);
};

CAF_DECLARE_SMART_POINTER(CRemoteSecurityDoc);

}

#endif
