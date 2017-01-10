/*
 *	Author: bwilliams
 *	Created: May 3, 2012
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CPackageInstaller_h_
#define CPackageInstaller_h_


#include "Doc/CafCoreTypesDoc/CAttachmentCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CAttachmentDoc.h"
#include "Doc/CafCoreTypesDoc/CAttachmentNameCollectionDoc.h"
#include "Doc/CafInstallRequestDoc/CFullPackageElemDoc.h"
#include "Doc/CafInstallRequestDoc/CInstallPackageSpecDoc.h"
#include "Doc/CafInstallRequestDoc/CInstallProviderSpecDoc.h"
#include "Doc/CafInstallRequestDoc/CMinPackageElemDoc.h"

namespace Caf {

/// Sends responses/errors back to the client.
class CPackageInstaller {
public:
	typedef std::deque<SmartPtrCInstallPackageSpecDoc> CInstallPackageSpecCollection;
	CAF_DECLARE_SMART_POINTER(CInstallPackageSpecCollection);

	struct CInstallPackageMatch {
		CInstallUtils::MATCH_STATUS _matchStatus;
		SmartPtrCInstallPackageSpecDoc _matchedInstallPackageSpec;
	};
	CAF_DECLARE_SMART_POINTER(CInstallPackageMatch);

public:
	static void installPackages(
		const std::deque<SmartPtrCFullPackageElemDoc>& fullPackageElemCollection,
		const SmartPtrCAttachmentCollectionDoc& attachmentCollection,
		const std::string& outputDir);

	static void uninstallPackages(
		const std::deque<SmartPtrCMinPackageElemDoc>& minPackageElemCollection,
		const std::deque<SmartPtrCInstallProviderSpecDoc>& installProviderSpecCollection,
		const std::string& outputDir);

private:
	static void installPackage(
		const SmartPtrCInstallPackageSpecDoc& installPackageSpec,
		const SmartPtrCInstallPackageSpecDoc& uninstallPackageSpec,
		const std::string& outputDir);

	static void executePackage(
		const SmartPtrCInstallPackageSpecDoc& installPackageSpec,
		const std::string& startupArgument,
		const std::string& outputDir);

	static SmartPtrCInstallPackageSpecDoc resolveAndCopyAttachments(
		const SmartPtrCInstallPackageSpecDoc& installPackageSpec);

	static void saveInstallPackageSpec(
		const SmartPtrCInstallPackageSpecDoc& installPackageSpec);

	static std::map<int32, SmartPtrCFullPackageElemDoc> orderFullPackageElems(
		const std::deque<SmartPtrCFullPackageElemDoc>& fullPackageElemCollection);

	static std::map<int32, SmartPtrCMinPackageElemDoc> orderMinPackageElems(
		const std::deque<SmartPtrCMinPackageElemDoc>& minPackageElemCollection);

	static SmartPtrCAttachmentCollectionDoc resolveAttachments(
		const SmartPtrCAttachmentNameCollectionDoc& attachmentNameCollection,
		const SmartPtrCAttachmentCollectionDoc& attachmentCollection);

	static SmartPtrCAttachmentCollectionDoc copyAttachments(
		const SmartPtrCAttachmentDoc& startupAttachment,
		const SmartPtrCAttachmentDoc& packageAttachment,
		const SmartPtrCAttachmentCollectionDoc& supportingAttachmentCollection,
		const std::string& outputDir);

	static SmartPtrCInstallPackageMatch matchInstallPackageSpec(
		const SmartPtrCInstallPackageSpecDoc& installPackageSpec);

	static SmartPtrCInstallPackageSpecCollection readInstallPackageSpecs();

	static uint32 countPackageReferences(
		const SmartPtrCInstallPackageSpecDoc& installPackageSpec,
		const std::deque<SmartPtrCInstallProviderSpecDoc>& installProviderSpecCollection);

	static void logDebug(
		const std::string& message,
		const SmartPtrCInstallPackageSpecDoc& installPackageSpec1,
		const SmartPtrCInstallPackageSpecDoc& installPackageSpec2);

	static void logWarn(
		const std::string& message,
		const SmartPtrCInstallPackageSpecDoc& installPackageSpec1,
		const SmartPtrCInstallPackageSpecDoc& installPackageSpec2);

	static void cleanupPackage(
		const SmartPtrCInstallPackageMatch& installPackageMatch);

private:
	CAF_CM_DECLARE_NOCREATE(CPackageInstaller);
};

}

#endif // #ifndef CPackageInstaller_h_
