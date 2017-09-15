/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (c) 2010 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef _MaIntegration_CPersistenceMerge_h_
#define _MaIntegration_CPersistenceMerge_h_

using namespace Caf;

/// TODO - describe class
class CPersistenceMerge {
public:
	static SmartPtrCPersistenceDoc mergePersistence(
			const SmartPtrCPersistenceDoc& persistenceLoaded,
			const SmartPtrCPersistenceDoc& persistenceIn);

private:
	static SmartPtrCLocalSecurityDoc mergeLocalSecurity(
			const SmartPtrCLocalSecurityDoc& localSecurityLoaded,
			const SmartPtrCLocalSecurityDoc& localSecurityIn);

	static SmartPtrCPersistenceProtocolCollectionDoc mergePersistenceProtocolCollection(
			const SmartPtrCPersistenceProtocolCollectionDoc& persistenceProtocolCollectionLoaded,
			const SmartPtrCPersistenceProtocolCollectionDoc& persistenceProtocolCollectionIn);

	static std::deque<SmartPtrCPersistenceProtocolDoc> mergePersistenceProtocolCollectionInner(
			const std::deque<SmartPtrCPersistenceProtocolDoc>& persistenceProtocolCollectionInnerLoaded,
			const std::deque<SmartPtrCPersistenceProtocolDoc>& persistenceProtocolCollectionInnerIn);

	static SmartPtrCPersistenceProtocolDoc mergePersistenceProtocol(
			const SmartPtrCPersistenceProtocolDoc& persistenceProtocolLoaded,
			const SmartPtrCPersistenceProtocolDoc& persistenceProtocolIn);

	static SmartPtrCRemoteSecurityCollectionDoc mergeRemoteSecurityCollection(
			const SmartPtrCRemoteSecurityCollectionDoc& remoteSecurityCollectionLoaded,
			const SmartPtrCRemoteSecurityCollectionDoc& remoteSecurityCollectionIn);

	static std::deque<SmartPtrCRemoteSecurityDoc> mergeRemoteSecurityCollectionInner(
			const std::deque<SmartPtrCRemoteSecurityDoc>& remoteSecurityCollectionInnerLoaded,
			const std::deque<SmartPtrCRemoteSecurityDoc>& remoteSecurityCollectionInnerIn);

	static SmartPtrCRemoteSecurityDoc mergeRemoteSecurity(
			const SmartPtrCRemoteSecurityDoc& remoteSecurityLoaded,
			const SmartPtrCRemoteSecurityDoc& remoteSecurityIn);

	static SmartPtrCCertCollectionDoc mergeCertCollection(
			const SmartPtrCCertCollectionDoc& certCollectionLoaded,
			const SmartPtrCCertCollectionDoc& certCollectionIn);

	static std::string mergeStrings(
			const std::string& strLoaded,
			const std::string& strIn);

	static std::string mergeUri(
			const std::string& uriLoaded,
			const std::string& uriIn);

	static Cdeqstr mergeDeqstr(
			const Cdeqstr& deqstrPreferred,
			const Cdeqstr& deqstrOther);

private:
	CAF_CM_DECLARE_NOCREATE(CPersistenceMerge);
};

#endif // #ifndef _MaIntegration_CPersistenceMerge_h_
