/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Doc/DocXml/ResponseXml/ResponseXmlRoots.h"

#include "Doc/DocUtils/EnumConvertersXml.h"

#include "Doc/CafCoreTypesDoc/CAttachmentCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CAttachmentDoc.h"
#include "Doc/CafCoreTypesDoc/CInlineAttachmentDoc.h"
#include "Doc/ProviderRequestDoc/CProviderCollectSchemaRequestDoc.h"
#include "Doc/ProviderRequestDoc/CProviderRequestDoc.h"
#include "Doc/ResponseDoc/CManifestCollectionDoc.h"
#include "Doc/ResponseDoc/CManifestDoc.h"
#include "Doc/ResponseDoc/CProviderResponseDoc.h"
#include "Doc/ResponseDoc/CResponseDoc.h"
#include "Doc/ResponseDoc/CResponseHeaderDoc.h"
#include "CResponseFactory.h"
#include "Exception/CCafException.h"

using namespace Caf;

SmartPtrCResponseDoc CResponseFactory::createResponse(
	const SmartPtrCProviderCollectSchemaRequestDoc& providerCollectSchemaRequest,
	const std::string& outputDir,
	const std::string& schemaCacheDir) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CResponseFactory", "createResponse");
	CAF_CM_VALIDATE_SMARTPTR(providerCollectSchemaRequest);
	CAF_CM_VALIDATE_STRING(outputDir);
	CAF_CM_VALIDATE_STRING(schemaCacheDir);

	SmartPtrCManifestCollectionDoc manifestCollection;
	SmartPtrCAttachmentCollectionDoc attachmentCollection;
	findAndStoreGlobalAttachmentsAndProviderResponses(outputDir, schemaCacheDir,
		manifestCollection, attachmentCollection);

	SmartPtrCResponseHeaderDoc responseHeader;
	responseHeader.CreateInstance();
	responseHeader->initialize();

	SmartPtrCResponseDoc response;
	response.CreateInstance();
	response->initialize(
			providerCollectSchemaRequest->getClientId(),
			providerCollectSchemaRequest->getRequestId(),
			providerCollectSchemaRequest->getPmeId(),
			responseHeader,
			manifestCollection,
			attachmentCollection,
			SmartPtrCStatisticsDoc());

	return response;
}

SmartPtrCResponseDoc CResponseFactory::createResponse(
	const SmartPtrCProviderRequestDoc& providerRequest,
	const std::string& outputDir) {
	CAF_CM_STATIC_FUNC_LOG("CResponseFactory", "createResponse");
	CAF_CM_VALIDATE_SMARTPTR(providerRequest);
	CAF_CM_VALIDATE_STRING(outputDir);

	SmartPtrCManifestCollectionDoc manifestCollection;
	SmartPtrCAttachmentCollectionDoc attachmentCollection;
	findAndStoreGlobalAttachmentsAndProviderResponses(outputDir, std::string(),
		manifestCollection, attachmentCollection);

	if (manifestCollection.IsNull() && attachmentCollection.IsNull()) {
		CAF_CM_EXCEPTIONEX_VA1(FileNotFoundException, ERROR_FILE_NOT_FOUND,
			"Did not find any attachments - %s", outputDir.c_str());
	}

	SmartPtrCResponseHeaderDoc responseHeader;
	responseHeader.CreateInstance();
	responseHeader->initialize();

	SmartPtrCResponseDoc response;
	response.CreateInstance();
	response->initialize(
			providerRequest->getClientId(),
			providerRequest->getRequestId(),
			providerRequest->getPmeId(),
			responseHeader,
			manifestCollection,
			attachmentCollection,
		SmartPtrCStatisticsDoc());

	return response;
}

void CResponseFactory::findAndStoreGlobalAttachmentsAndProviderResponses(
	const std::string& outputDir,
	const std::string& schemaCacheDir,
	SmartPtrCManifestCollectionDoc& manifestCollection,
	SmartPtrCAttachmentCollectionDoc& attachmentCollection) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CResponseFactory", "findAndStoreGlobalAttachmentsAndProviderResponses");
	CAF_CM_VALIDATE_STRING(outputDir);
	// schemaCacheDir is optional

	std::map<std::string, SmartPtrCAttachmentDoc> globalAttachmentCollection;
	std::deque<SmartPtrCManifestDoc> manifestCollectionInner;
	findAndStoreGlobalAttachments(outputDir, globalAttachmentCollection);
	findAndStoreProviderResponses(outputDir, schemaCacheDir,
		globalAttachmentCollection, manifestCollectionInner);

	if (!manifestCollectionInner.empty()) {
		manifestCollection.CreateInstance();
		manifestCollection->initialize(manifestCollectionInner);
	}

	if (!globalAttachmentCollection.empty()) {
		std::deque<SmartPtrCAttachmentDoc> globalAttachmentCollectionInner;
		for (TConstMapIterator<std::map<std::string, SmartPtrCAttachmentDoc> >
			globalAttachmentIter(globalAttachmentCollection); globalAttachmentIter; globalAttachmentIter++) {
			const SmartPtrCAttachmentDoc attachment = *globalAttachmentIter;
			globalAttachmentCollectionInner.push_back(attachment);
		}

		attachmentCollection.CreateInstance();
		attachmentCollection->initialize(globalAttachmentCollectionInner,
			std::deque<SmartPtrCInlineAttachmentDoc>());
	}
}

void CResponseFactory::findAndStoreProviderResponses(
	const std::string& outputDir,
	const std::string& schemaCacheDir,
	std::map<std::string, SmartPtrCAttachmentDoc>& globalAttachmentCollection,
	std::deque<SmartPtrCManifestDoc>& manifestCollection) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CResponseFactory", "findAndStoreProviderResponses");
	CAF_CM_VALIDATE_STRING(outputDir);
	// schemaCacheDir is optional

	const std::deque<std::string> providerResponsePathCollection =
		FileSystemUtils::findRequiredFiles(outputDir, _sProviderResponseFilename);

	for (TConstIterator<std::deque<std::string> > providerResponsePathIter(
		providerResponsePathCollection); providerResponsePathIter; providerResponsePathIter++) {
		const std::string providerResponsePath = *providerResponsePathIter;

		CAF_CM_LOG_DEBUG_VA1("Parsing provider response - %s", providerResponsePath.c_str());

		const SmartPtrCProviderResponseDoc providerResponse =
			XmlRoots::parseProviderResponseFromFile(providerResponsePath);

		const SmartPtrCManifestDoc providerResponseManifest =
			providerResponse->getManifest();
		if (!providerResponseManifest.IsNull()) {
			manifestCollection.push_back(providerResponseManifest);
		}

		const SmartPtrCAttachmentCollectionDoc providerResponseAttachmentCollection =
			providerResponse->getAttachmentCollection();

		if (!providerResponseAttachmentCollection.IsNull()) {
			std::deque<SmartPtrCAttachmentDoc>
				providerResponseAttachmentCollectionInner =
					providerResponseAttachmentCollection->getAttachment();

			resolveAndStoreGlobalAttachments(providerResponseAttachmentCollectionInner,
				outputDir, schemaCacheDir, globalAttachmentCollection);
		}
	}
}

void CResponseFactory::findAndStoreGlobalAttachments(
	const std::string& outputDir,
	std::map<std::string, SmartPtrCAttachmentDoc>& globalAttachmentCollection) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CResponseFactory", "findAndStoreGlobalAttachments");
	CAF_CM_VALIDATE_STRING(outputDir);

	const std::string parentOutputDir = FileSystemUtils::buildPath(outputDir, "..");
	const std::string requestPath = FileSystemUtils::findOptionalFile(parentOutputDir,
		_sPayloadRequestFilename);

	const std::deque<std::string> stdoutPathCollection =
		FileSystemUtils::findOptionalFiles(outputDir, _sStdoutFilename);
	const std::deque<std::string> stderrPathCollection =
		FileSystemUtils::findOptionalFiles(outputDir, _sStderrFilename);
	const std::deque<std::string> maDebugLogPathCollection =
		FileSystemUtils::findOptionalFiles(outputDir, "ma-log4cpp.log");

	if (!requestPath.empty()) {
		std::deque<std::string> requestPathCollection;
		requestPathCollection.push_back(requestPath);
		storeGlobalAttachments(std::string(), "request", requestPathCollection,
			outputDir, globalAttachmentCollection);
	}
	if (!stdoutPathCollection.empty()) {
		storeGlobalAttachments(std::string(), "stdout", stdoutPathCollection,
			outputDir, globalAttachmentCollection);
	}
	if (!stderrPathCollection.empty()) {
		storeGlobalAttachments(std::string(), "stderr", stderrPathCollection,
			outputDir, globalAttachmentCollection);
	}
	if (!maDebugLogPathCollection.empty()) {
		storeGlobalAttachments(std::string(), "log", maDebugLogPathCollection,
			outputDir, globalAttachmentCollection);
	}
}

void CResponseFactory::resolveAndStoreGlobalAttachments(
	const std::deque<SmartPtrCAttachmentDoc> attachmentCollectionInner,
	const std::string& outputDir,
	const std::string& schemaCacheDir,
	std::map<std::string, SmartPtrCAttachmentDoc>& globalAttachmentCollection) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CResponseFactory", "resolveAndStoreGlobalAttachments");
	CAF_CM_VALIDATE_STL(attachmentCollectionInner);
	CAF_CM_VALIDATE_STRING(outputDir);
	// schemaCacheDir is optional

	for (TConstIterator<std::deque<SmartPtrCAttachmentDoc> > attachmentIter(
		attachmentCollectionInner); attachmentIter; attachmentIter++) {
		const SmartPtrCAttachmentDoc attachment = *attachmentIter;

		UriUtils::SUriRecord uriRecord;
		UriUtils::parseUriString(attachment->getUri(), uriRecord);

		if (uriRecord.protocol.compare("file") == 0) {
			UriUtils::SFileUriRecord fileUriRecord;
			UriUtils::parseFileAddress(uriRecord.address, fileUriRecord);

			std::string attachmentPath = fileUriRecord.path;
			std::string attachmentPathNew = attachmentPath;
			if (!schemaCacheDir.empty()) {
				std::string relPath;
				resolveAttachmentPath(attachmentPath, outputDir, relPath,
					attachmentPathNew);
			}

			if (!attachmentPathNew.empty()) {
				storeGlobalAttachment(attachment->getName(), attachment->getType(),
					attachmentPathNew, outputDir, globalAttachmentCollection);
			}
		} else {
			globalAttachmentCollection.insert(
				std::make_pair(attachment->getUri(), attachment));
		}
	}
}

void CResponseFactory::storeGlobalAttachments(
	const std::string& attachmentName,
	const std::string& attachmentType,
	const std::deque<std::string>& attachmentPathCollection,
	const std::string& baseDir,
	std::map<std::string, SmartPtrCAttachmentDoc>& attachmentCollection) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CResponseFactory", "storeGlobalAttachments");
	// attachmentName is optional
	CAF_CM_VALIDATE_STRING(attachmentType);
	CAF_CM_VALIDATE_STL(attachmentPathCollection);
	CAF_CM_VALIDATE_STRING(baseDir);

	for (TConstIterator<std::deque<std::string> > attachmentPathIter(
		attachmentPathCollection); attachmentPathIter; attachmentPathIter++) {
		const std::string attachmentPath = *attachmentPathIter;

		storeGlobalAttachment(attachmentName, attachmentType, attachmentPath, baseDir,
			attachmentCollection);
	}
}

void CResponseFactory::storeGlobalAttachment(
	const std::string& attachmentName,
	const std::string& attachmentType,
	const std::string& attachmentPath,
	const std::string& baseDir,
	std::map<std::string, SmartPtrCAttachmentDoc>& attachmentCollection) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CResponseFactory", "storeGlobalAttachment");
	// attachmentName is optional
	CAF_CM_VALIDATE_STRING(attachmentType);
	CAF_CM_VALIDATE_STRING(attachmentPath);
	CAF_CM_VALIDATE_STRING(baseDir);

	std::string relPath;
	std::string attachmentPathNew;
	resolveAttachmentPath(attachmentPath, baseDir, relPath, attachmentPathNew);

	if (!attachmentPathNew.empty()) {
		std::string attachmentNameNew = attachmentName;
		if (attachmentName.empty()) {
			attachmentNameNew = relPath;
			std::replace(attachmentNameNew.begin(), attachmentNameNew.end(), '/', '.');
		}

		attachmentPathNew = FileSystemUtils::normalizePathWithForward(attachmentPathNew);

		const std::string attachmentUri = "file:///" + attachmentPathNew + "?relPath="
			+ relPath;

		CAF_CM_LOG_DEBUG_VA3("Creating attachment - name: %s, type: %s, uri: %s",
			attachmentNameNew.c_str(), attachmentType.c_str(), attachmentUri.c_str());

		const std::string cmsPolicyStr = AppConfigUtils::getRequiredString(
				"security", "cms_policy");

		SmartPtrCAttachmentDoc attachment;
		attachment.CreateInstance();
		attachment->initialize(attachmentNameNew, attachmentType, attachmentUri, false,
				EnumConvertersXml::convertStringToCmsPolicy(cmsPolicyStr));

		attachmentCollection.insert(std::make_pair(attachmentUri, attachment));
	}
}

void CResponseFactory::resolveAttachmentPath(
	const std::string& attachmentPath,
	const std::string& baseDir,
	std::string& relPath,
	std::string& attachmentPathNew) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CResponseFactory", "resolveAttachmentPath");
	CAF_CM_VALIDATE_STRING(attachmentPath);
	CAF_CM_VALIDATE_STRING(baseDir);

	// Initialize the input variables
	relPath = std::string();
	attachmentPathNew = attachmentPath;

	std::string baseDirNew = baseDir;

	const std::string baseDirNewTmp = baseDirNew + '/';
	const std::string::size_type fndPos = attachmentPathNew.find(baseDirNewTmp);
	if (fndPos != std::string::npos) {
		relPath = attachmentPathNew;
		relPath.replace(fndPos, baseDirNewTmp.length(), "");

		relPath = removeLeadingChars(relPath, '.');
		relPath = removeLeadingChars(relPath, '/');
	}

	if (relPath.empty()) {
		relPath = FileSystemUtils::getBasename(attachmentPathNew);
		attachmentPathNew = FileSystemUtils::buildPath(baseDirNew, relPath);
		const std::string attachmentPathTmp =
			FileSystemUtils::normalizePathForPlatform(attachmentPath);

		if (attachmentPathTmp.compare(attachmentPathNew) != 0) {
			if (FileSystemUtils::doesFileExist(attachmentPathNew)) {
				bool isFileNameFound = false;
				const std::string origRelPath = relPath;
				for (uint32 index = 0; !isFileNameFound; index++) {
					relPath = CStringConv::toString<uint32>(index) + "_" + origRelPath;
					attachmentPathNew = FileSystemUtils::buildPath(baseDirNew, relPath);
					if (! FileSystemUtils::doesFileExist(attachmentPathNew)) {
						CAF_CM_LOG_WARN_VA1("File already exists... calculated new name - %s",
							attachmentPathNew.c_str());
						isFileNameFound = true;
					}
				}
			}

			if (FileSystemUtils::isRegularFile(attachmentPathTmp)) {
				CAF_CM_LOG_WARN_VA3("Attachment not in specified directory... Copying - attPath: \"%s\", goodDir: \"%s\", newPath: \"%s\"",
					attachmentPathTmp.c_str(), baseDirNew.c_str(), attachmentPathNew.c_str());
				FileSystemUtils::copyFile(attachmentPathTmp, attachmentPathNew);
			} else {
				CAF_CM_LOG_ERROR_VA3("Attachment not in specified or calculated directory - attPath: \"%s\", goodDir: \"%s\", newPath: \"%s\"",
					attachmentPathTmp.c_str(), baseDirNew.c_str(), attachmentPathNew.c_str());
				attachmentPathNew = std::string();
			}
		}
	}
}

std::string CResponseFactory::removeLeadingChars(
	const std::string& sourceStr,
	const char leadingChar) {

	std::string rc;
	if (!sourceStr.empty()) {
		bool isCharFnd = false;
		for (size_t index = 0; index < sourceStr.length(); index++) {
			if (!isCharFnd && (sourceStr[index] != leadingChar)) {
				isCharFnd = true;
			}
			if (isCharFnd) {
				rc += sourceStr[index];
			}
		}
	}

	return rc;
}
