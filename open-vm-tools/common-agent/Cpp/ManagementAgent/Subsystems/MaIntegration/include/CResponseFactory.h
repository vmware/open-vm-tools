/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CResponseFactory_h_
#define CResponseFactory_h_


#include "Doc/CafCoreTypesDoc/CAttachmentCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CAttachmentDoc.h"
#include "Doc/ProviderRequestDoc/CProviderCollectSchemaRequestDoc.h"
#include "Doc/ProviderRequestDoc/CProviderRequestDoc.h"
#include "Doc/ResponseDoc/CManifestCollectionDoc.h"
#include "Doc/ResponseDoc/CManifestDoc.h"
#include "Doc/ResponseDoc/CResponseDoc.h"

namespace Caf {

/// Builds the response XML document.
class CResponseFactory {
public:
	static SmartPtrCResponseDoc createResponse(
		const SmartPtrCProviderCollectSchemaRequestDoc& providerCollectSchemaRequest,
		const std::string& outputDir,
		const std::string& schemaCacheDir);

	static SmartPtrCResponseDoc createResponse(
		const SmartPtrCProviderRequestDoc& providerRequest,
		const std::string& outputDir);

private:
	static void findAndStoreGlobalAttachmentsAndProviderResponses(
		const std::string& outputDir,
		const std::string& schemaCacheDir,
		SmartPtrCManifestCollectionDoc& manifestCollection,
		SmartPtrCAttachmentCollectionDoc& attachmentCollection);

	static void findAndStoreProviderResponses(
		const std::string& outputDir,
		const std::string& schemaCacheDir,
		std::map<std::string, SmartPtrCAttachmentDoc>& globalAttachmentCollection,
		std::deque<SmartPtrCManifestDoc>& manifestCollection);

	static void findAndStoreGlobalAttachments(
		const std::string& outputDir,
		std::map<std::string, SmartPtrCAttachmentDoc>& globalAttachmentCollection);

	static void resolveAndStoreGlobalAttachments(
		const std::deque<SmartPtrCAttachmentDoc> attachmentCollectionInner,
		const std::string& outputDir,
		const std::string& schemaCacheDir,
		std::map<std::string, SmartPtrCAttachmentDoc>& globalAttachmentCollection);

	static void storeGlobalAttachments(
		const std::string& attachmentName,
		const std::string& attachmentType,
		const std::deque<std::string>& attachmentPathCollection,
		const std::string& baseDir,
		std::map<std::string, SmartPtrCAttachmentDoc>& attachmentCollection);

	static void storeGlobalAttachment(
		const std::string& attachmentName,
		const std::string& attachmentType,
		const std::string& attachmentPath,
		const std::string& baseDir,
		std::map<std::string, SmartPtrCAttachmentDoc>& attachmentCollection);

	static void resolveAttachmentPath(
		const std::string& attachmentPath,
		const std::string& baseDir,
		std::string& relPath,
		std::string& attachmentPathNew);

	static std::string removeLeadingChars(
		const std::string& sourceStr,
		const char leadingChar);

private:
	CAF_CM_DECLARE_NOCREATE(CResponseFactory);
};

CAF_DECLARE_SMART_POINTER(CResponseFactory);

}

#endif // #ifndef CResponseFactory_h_
