/*
 *  Author: bwilliams
 *  Created: Nov 16, 2015
 *
 *  Copyright (c) 2015 Vmware, Inc.  All rights reserved.
 *  -- VMware Confidential
 *
 */

#ifndef CPersistenceDoc_h_
#define CPersistenceDoc_h_

namespace Caf {

/// A simple container for objects of type PersistenceEnvelope
class CPersistenceDoc {
public:
	CPersistenceDoc() :
		_isInitialized(false) {}
	virtual ~CPersistenceDoc() {}

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const SmartPtrCLocalSecurityDoc& localSecurity = SmartPtrCLocalSecurityDoc(),
		const SmartPtrCRemoteSecurityCollectionDoc& remoteSecurityCollection = SmartPtrCRemoteSecurityCollectionDoc(),
		const SmartPtrCPersistenceProtocolDoc& persistenceProtocol = SmartPtrCPersistenceProtocolDoc(),
		const std::string version = "1.0") {
		if (! _isInitialized) {
			_localSecurity = localSecurity;
			_remoteSecurityCollection = remoteSecurityCollection;
			_persistenceProtocol = persistenceProtocol;
			_version = version;

			_isInitialized = true;
		}
	}

public:
	/// Accessor for the LocalSecurity
	SmartPtrCLocalSecurityDoc getLocalSecurity() const {
		return _localSecurity;
	}

	/// Accessor for the Protocol Collection
	SmartPtrCRemoteSecurityCollectionDoc getRemoteSecurityCollection() const {
		return _remoteSecurityCollection;
	}

	/// Accessor for the PersistenceProtocol
	SmartPtrCPersistenceProtocolDoc getPersistenceProtocol() const {
		return _persistenceProtocol;
	}

	/// Accessor for the version
	std::string getVersion() const {
		return _version;
	}

private:
	SmartPtrCLocalSecurityDoc _localSecurity;
	SmartPtrCRemoteSecurityCollectionDoc _remoteSecurityCollection;
	SmartPtrCPersistenceProtocolDoc _persistenceProtocol;
	std::string _version;

	bool _isInitialized;

private:
	CAF_CM_DECLARE_NOCOPY(CPersistenceDoc);
};

CAF_DECLARE_SMART_POINTER(CPersistenceDoc);

}

#endif
