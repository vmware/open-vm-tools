/*
 *	 Author: bwilliams
 *  Created: Jan 21, 2011
 *
 *	Copyright (c) 2011 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef CPersistenceUtils_H_
#define CPersistenceUtils_H_

#include <DocContracts.h>

namespace Caf {

class COMMONAGGREGATOR_LINKAGE CPersistenceUtils {
public:
	static SmartPtrCPersistenceDoc loadPersistence(
			const std::string& persistenceDir);

	static SmartPtrCLocalSecurityDoc loadLocalSecurity(
			const std::string& persistenceDir);

	static SmartPtrCRemoteSecurityCollectionDoc loadRemoteSecurityCollection(
			const std::string& persistenceDir);

	static SmartPtrCPersistenceProtocolDoc loadPersistenceProtocol(
			const std::string& persistenceDir);

	static SmartPtrCAmqpBrokerDoc loadAmqpBroker(
			const std::string& persistenceDir);

	static SmartPtrCAmqpBrokerDoc loadAmqpBroker(
			const SmartPtrCPersistenceProtocolDoc& persistenceProtocol);

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

	static void savePersistenceProtocol(
			const SmartPtrCPersistenceProtocolDoc& persistenceProtocol,
			const std::string& persistenceDir);

private:
	static std::string loadTextFile(
			const std::string& dir,
			const std::string& file);

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
