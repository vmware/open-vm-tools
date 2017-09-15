/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _MaIntegration_CConfigEnvMerge_h_
#define _MaIntegration_CConfigEnvMerge_h_


#include "Doc/PersistenceDoc/CCertCollectionDoc.h"
#include "Doc/PersistenceDoc/CPersistenceDoc.h"
#include "Doc/PersistenceDoc/CPersistenceProtocolDoc.h"

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
			const std::string& localId,
			const std::string& cacert);

	static std::string mergeLocalId(
			const SmartPtrCPersistenceDoc& persistence,
			const std::string& vcidPath);

	static std::string mergeUri(
			const SmartPtrCPersistenceProtocolDoc& persistenceProtocol,
			const std::string& localId,
			const bool isTunnelEnabled);

	static SmartPtrCCertCollectionDoc mergeTlsCertCollection(
			const SmartPtrCCertCollectionDoc& tlsCertCollection,
			const std::string& cacert);

private:
	static bool isTunnelEnabledFunc();

	static std::string loadTextFile(
			const std::string& path);

private:
	CAF_CM_DECLARE_NOCREATE(CConfigEnvMerge);
};

#endif // #ifndef _MaIntegration_CConfigEnvMerge_h_
