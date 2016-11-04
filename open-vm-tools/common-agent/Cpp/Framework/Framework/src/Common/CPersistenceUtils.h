/*
 *	 Author: bwilliams
 *  Created: Jan 21, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CPersistenceUtils_H_
#define CPersistenceUtils_H_

#include <DocContracts.h>

#include "Doc/PersistenceDoc/CLocalSecurityDoc.h"
#include "Doc/PersistenceDoc/CPersistenceDoc.h"
#include "Doc/PersistenceDoc/CPersistenceProtocolCollectionDoc.h"
#include "Doc/PersistenceDoc/CPersistenceProtocolDoc.h"
#include "Doc/PersistenceDoc/CRemoteSecurityCollectionDoc.h"

namespace Caf {

class COMMONAGGREGATOR_LINKAGE CPersistenceUtils {
public:
	static SmartPtrCPersistenceDoc loadPersistence(
			const std::string& persistenceDir);

	static SmartPtrCLocalSecurityDoc loadLocalSecurity(
			const std::string& persistenceDir);

	static SmartPtrCRemoteSecurityCollectionDoc loadRemoteSecurityCollection(
			const std::string& persistenceDir);

	static SmartPtrCPersistenceProtocolCollectionDoc loadPersistenceProtocolCollection(
			const std::string& persistenceDir);

	static SmartPtrCPersistenceProtocolDoc loadPersistenceProtocol(
			const std::string& persistenceDir);

	static SmartPtrCPersistenceProtocolDoc loadPersistenceProtocol(
			const SmartPtrCPersistenceProtocolCollectionDoc& persistenceProtocolCollection);

public:
	static void savePersistence(
			const SmartPtrCPersistenceDoc& persistence,
			const std::string& persistenceDir);

	static void saveLocalSecurity(
			const SmartPtrCLocalSecurityDoc& localSecurity,
			const std::string& persistenceDir);

	static void saveRemoteSecurityCollection(
			const SmartPtrCRemoteSecurityCollectionDoc& remoteSecurityCollection,
			const std::string& persistenceDir);

	static void savePersistenceProtocolCollection(
			const SmartPtrCPersistenceProtocolCollectionDoc& persistenceProtocolCollection,
			const std::string& persistenceDir,
			const std::string& uriAmqp,
			const std::string& uriTunnel);

private:
	static std::string loadTextFile(
			const std::string& dir,
			const std::string& file,
			const std::string& defaultVal = std::string(),
			const bool isTrimRight = true);

private:
	static std::string createDirectory(
			const std::string& directory,
			const std::string& subdir);

	static void saveCollection(
			const Cdeqstr& collection,
			const std::string& directory,
			const std::string& filePrefix,
			const std::string& filePostfix);

private:
	CAF_CM_DECLARE_NOCREATE(CPersistenceUtils);
};

}

#endif /* CPersistenceUtils_H_ */
