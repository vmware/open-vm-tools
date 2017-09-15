/*
 *	Author: bwilliams
 *	Created: May 3, 2012
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CProviderInstaller_h_
#define CProviderInstaller_h_


#include "Doc/CafCoreTypesDoc/CAttachmentCollectionDoc.h"
#include "Doc/CafInstallRequestDoc/CInstallProviderJobDoc.h"
#include "Doc/CafInstallRequestDoc/CInstallProviderSpecDoc.h"
#include "Doc/CafInstallRequestDoc/CUninstallProviderJobDoc.h"

namespace Caf {

class CProviderInstaller {
public:
	typedef std::deque<SmartPtrCInstallProviderSpecDoc> CInstallProviderSpecCollection;
	CAF_DECLARE_SMART_POINTER(CInstallProviderSpecCollection);

	struct CInstallProviderMatch {
		CInstallUtils::MATCH_STATUS _matchStatus;
		SmartPtrCInstallProviderSpecDoc _matchedInstallProviderSpec;
	};
	CAF_DECLARE_SMART_POINTER(CInstallProviderMatch);

public:
	static void installProvider(
		const SmartPtrCInstallProviderJobDoc& installProviderJob,
		const SmartPtrCAttachmentCollectionDoc& attachmentCollection,
		const std::string& outputDir);

	static void uninstallProvider(
		const SmartPtrCUninstallProviderJobDoc& uninstallProviderJob,
		const std::string& outputDir);

	static SmartPtrCInstallProviderSpecCollection readInstallProviderSpecs();

private:
	static void installProviderLow(
		const SmartPtrCInstallProviderJobDoc& installProviderJob,
		const SmartPtrCAttachmentCollectionDoc& attachmentCollection,
		const std::string& outputDir);

	static void uninstallProviderLow(
		const SmartPtrCInstallProviderSpecDoc& installProviderSpec,
		const std::string& outputDir);

	static void storeInstallProviderSpec(
		const SmartPtrCInstallProviderSpecDoc& installProviderSpec);

	static SmartPtrCInstallProviderMatch matchInstallProviderSpec(
		const SmartPtrCInstallProviderSpecDoc& installProviderSpec);

	static SmartPtrCInstallProviderSpecDoc createInstallProviderSpec(
		const SmartPtrCInstallProviderJobDoc& installProviderJob);

	static void logDebug(
		const std::string& message,
		const SmartPtrCInstallProviderSpecDoc& installProviderSpec1,
		const SmartPtrCInstallProviderSpecDoc& installProviderSpec2);

	static void logWarn(
		const std::string& message,
		const SmartPtrCInstallProviderSpecDoc& installProviderSpec1,
		const SmartPtrCInstallProviderSpecDoc& installProviderSpec2);

	static void cleanupProvider(
		const SmartPtrCInstallProviderSpecDoc& installProviderSpec);

	static std::string calcProviderFqn(
		const SmartPtrCInstallProviderSpecDoc& installProviderSpec);

	static std::string calcProviderFqn(
		const SmartPtrCUninstallProviderJobDoc& uninstallProviderJob);

private:
	CAF_CM_DECLARE_NOCREATE(CProviderInstaller);
};

}

#endif // #ifndef CProviderInstaller_h_
