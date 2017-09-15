/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (c) 2010 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef _MaIntegration_CConfigEnvMerge_h_
#define _MaIntegration_CConfigEnvMerge_h_

using namespace Caf;

/// TODO - describe class
class CConfigEnvMerge {
public:
	static SmartPtrCPersistenceDoc mergePersistence(
			const SmartPtrCPersistenceDoc& persistence,
			const std::string& cacertPath,
			const std::string& vcidPath);

private:
	static std::deque<SmartPtrCPersistenceProtocolDoc> mergePersistenceProtocolCollectionInner(
			const std::deque<SmartPtrCPersistenceProtocolDoc>& persistenceProtocolCollectionInner,
			const std::string& protocol,
			const std::string& vcid,
			const std::string& cacert);

	static std::string mergeUri(
			const std::string& srcUri,
			const std::string& protocol,
			const std::string& vcid);

	static SmartPtrCCertCollectionDoc mergeTlsCertCollection(
			const SmartPtrCCertCollectionDoc& tlsCertCollection,
			const std::string& cacert);

private:
	static bool isTunnelEnabled();

private:
	CAF_CM_DECLARE_NOCREATE(CConfigEnvMerge);
};

#endif // #ifndef _MaIntegration_CConfigEnvMerge_h_
