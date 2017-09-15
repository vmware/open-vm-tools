/*
 *	Author: bwilliams
 *	Created: May 3, 2012
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Doc/DocXml/CafInstallRequestXml/CafInstallRequestXmlRoots.h"

#include "Doc/CafCoreTypesDoc/CAttachmentCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CAttachmentDoc.h"
#include "Doc/CafCoreTypesDoc/CAttachmentNameCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CInlineAttachmentDoc.h"
#include "Doc/CafInstallRequestDoc/CFullPackageElemDoc.h"
#include "Doc/CafInstallRequestDoc/CInstallPackageSpecDoc.h"
#include "Doc/CafInstallRequestDoc/CInstallProviderSpecDoc.h"
#include "Doc/CafInstallRequestDoc/CMinPackageElemDoc.h"
#include "Doc/CafInstallRequestDoc/CPackageDefnDoc.h"
#include "CPackageInstaller.h"

using namespace Caf;

void CPackageInstaller::installPackages(
	const std::deque<SmartPtrCFullPackageElemDoc>& fullPackageElemCollection,
	const SmartPtrCAttachmentCollectionDoc& attachmentCollection,
	const std::string& outputDir) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CPackageInstaller", "installPackages");

	CAF_CM_ENTER
	{
		CAF_CM_VALIDATE_STL(fullPackageElemCollection);
		CAF_CM_VALIDATE_SMARTPTR(attachmentCollection);
		CAF_CM_VALIDATE_STRING(outputDir);

		const std::map<int32, SmartPtrCFullPackageElemDoc> orderedFullPackageElemCollection =
			orderFullPackageElems(fullPackageElemCollection);

		for (TConstMapIterator<std::map<int32, SmartPtrCFullPackageElemDoc> > fullPackageElemIter(
			orderedFullPackageElemCollection); fullPackageElemIter; fullPackageElemIter++) {
			const SmartPtrCFullPackageElemDoc fullPackageElem = *fullPackageElemIter;

			const SmartPtrCPackageDefnDoc installPackageDefn =
				fullPackageElem->getInstallPackage();

			SmartPtrCInstallPackageSpecDoc installPackageSpec;
			installPackageSpec.CreateInstance();
			installPackageSpec->initialize(fullPackageElem->getPackageNamespace(),
				fullPackageElem->getPackageName(), fullPackageElem->getPackageVersion(),
				installPackageDefn->getStartupAttachmentName(),
				installPackageDefn->getPackageAttachmentName(),
				installPackageDefn->getSupportingAttachmentNameCollection(), attachmentCollection,
				installPackageDefn->getArguments());

			const SmartPtrCPackageDefnDoc uninstallPackageDefn =
				fullPackageElem->getUninstallPackage();

			SmartPtrCInstallPackageSpecDoc uninstallPackageSpec;
			uninstallPackageSpec.CreateInstance();
			uninstallPackageSpec->initialize(fullPackageElem->getPackageNamespace(),
				fullPackageElem->getPackageName(), fullPackageElem->getPackageVersion(),
				uninstallPackageDefn->getStartupAttachmentName(),
				uninstallPackageDefn->getPackageAttachmentName(),
				uninstallPackageDefn->getSupportingAttachmentNameCollection(),
				attachmentCollection, uninstallPackageDefn->getArguments());

			installPackage(installPackageSpec, uninstallPackageSpec, outputDir);
		}
	}
	CAF_CM_EXIT;
}

void CPackageInstaller::uninstallPackages(
	const std::deque<SmartPtrCMinPackageElemDoc>& minPackageElemCollection,
	const std::deque<SmartPtrCInstallProviderSpecDoc>& installProviderSpecCollection,
	const std::string& outputDir) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CPackageInstaller", "uninstallPackages");

	CAF_CM_ENTER
	{
		CAF_CM_VALIDATE_STL(minPackageElemCollection);
		// installProviderSpecCollection is optional
		CAF_CM_VALIDATE_STRING(outputDir);

		const std::map<int32, SmartPtrCMinPackageElemDoc>
			orderedProviderPackageElemCollection = orderMinPackageElems(
				minPackageElemCollection);

		for (TConstMapIterator<std::map<int32, SmartPtrCMinPackageElemDoc> >
			minPackageElemIter(orderedProviderPackageElemCollection); minPackageElemIter; minPackageElemIter++) {
			const SmartPtrCMinPackageElemDoc minPackageElem = *minPackageElemIter;

			const std::string installPackageDir = CPathBuilder::calcInstallPackageDir(
				minPackageElem->getPackageNamespace(), minPackageElem->getPackageName(),
				minPackageElem->getPackageVersion());

			const std::string installPackageSpecPath = FileSystemUtils::buildPath(
				installPackageDir, _sInstallPackageSpecFilename);

			const SmartPtrCInstallPackageSpecDoc installPackageSpec =
				XmlRoots::parseInstallPackageSpecFromFile(installPackageSpecPath);

			const uint32 packageRefCnt = countPackageReferences(installPackageSpec,
				installProviderSpecCollection);
			if (packageRefCnt == 1) {
				try {
					CPackageInstaller::executePackage(installPackageSpec, "-uninstall", outputDir);
				} catch (ProcessFailedException* ex) {
					CAF_CM_LOG_DEBUG_VA1("Removing package directory - %s", installPackageDir.c_str());
					FileSystemUtils::recursiveRemoveDirectory(installPackageDir);
					ex->throwSelf();
				}

				CAF_CM_LOG_DEBUG_VA1("Removing package directory - %s", installPackageDir.c_str());
				FileSystemUtils::recursiveRemoveDirectory(installPackageDir);
			} else {
				CAF_CM_LOG_WARN_VA4("Package referenced from more than one provider... not uninstalling - %s::%s::%s = %d",
					minPackageElem->getPackageNamespace().c_str(), minPackageElem->getPackageName().c_str(),
					minPackageElem->getPackageVersion().c_str(), packageRefCnt);
			}
		}
	}
	CAF_CM_EXIT;
}

void CPackageInstaller::installPackage(
	const SmartPtrCInstallPackageSpecDoc& installPackageSpec,
	const SmartPtrCInstallPackageSpecDoc& uninstallPackageSpec,
	const std::string& outputDir) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CPackageInstaller", "installPackage");

	CAF_CM_ENTER
	{
		CAF_CM_VALIDATE_SMARTPTR(installPackageSpec);
		CAF_CM_VALIDATE_SMARTPTR(uninstallPackageSpec);
		CAF_CM_VALIDATE_STRING(outputDir);

		const SmartPtrCInstallPackageMatch installPackageMatch = matchInstallPackageSpec(
			installPackageSpec);

		switch (installPackageMatch->_matchStatus) {
			case CInstallUtils::MATCH_NOTEQUAL: {
				const SmartPtrCInstallPackageSpecDoc resolvedInstallPackageSpec =
					resolveAndCopyAttachments(uninstallPackageSpec);
				executePackage(installPackageSpec, "-install", outputDir);
				saveInstallPackageSpec(resolvedInstallPackageSpec);
			}
			break;
			case CInstallUtils::MATCH_VERSION_EQUAL: {
				logWarn("Package already installed", installPackageSpec,
					installPackageMatch->_matchedInstallPackageSpec);
			}
			break;
			case CInstallUtils::MATCH_VERSION_LESS: {
				logWarn("More recent package already installed", installPackageSpec,
					installPackageMatch->_matchedInstallPackageSpec);
			}
			break;
			case CInstallUtils::MATCH_VERSION_GREATER: {
				logWarn("Upgrading installed version", installPackageSpec,
					installPackageMatch->_matchedInstallPackageSpec);

				try {
					executePackage(installPackageMatch->_matchedInstallPackageSpec,
						"-upgrade_uninstall", outputDir);
				} catch (ProcessFailedException* ex) {
					cleanupPackage(installPackageMatch);
					ex->throwSelf();
				}

				cleanupPackage(installPackageMatch);

				const SmartPtrCInstallPackageSpecDoc resolvedInstallPackageSpec =
					resolveAndCopyAttachments(uninstallPackageSpec);
				executePackage(installPackageSpec, "-upgrade_install", outputDir);
				saveInstallPackageSpec(resolvedInstallPackageSpec);
			}
			break;
		}
	}
	CAF_CM_EXIT;
}

void CPackageInstaller::executePackage(
	const SmartPtrCInstallPackageSpecDoc& installPackageSpec,
	const std::string& startupArgument,
	const std::string& outputDir) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CPackageInstaller", "executePackage");

	CAF_CM_ENTER
	{
		CAF_CM_VALIDATE_SMARTPTR(installPackageSpec);
		CAF_CM_VALIDATE_STRING(outputDir);

		const SmartPtrCAttachmentCollectionDoc attachmentCollection =
			installPackageSpec->getAttachmentCollection();

		const SmartPtrCAttachmentDoc startupAttachment = AttachmentUtils::findRequiredAttachment(
			installPackageSpec->getStartupAttachmentName(), attachmentCollection);
		const SmartPtrCAttachmentDoc packageAttachment = AttachmentUtils::findRequiredAttachment(
			installPackageSpec->getPackageAttachmentName(), attachmentCollection);

		const SmartPtrCAttachmentNameCollectionDoc attachmentNameCollection =
			installPackageSpec->getSupportingAttachmentNameCollection();
		const SmartPtrCAttachmentCollectionDoc supportingAttachmentCollection =
			resolveAttachments(attachmentNameCollection, attachmentCollection);

		const std::string packageDir = CPathBuilder::calcDir(
			installPackageSpec->getPackageNamespace(), installPackageSpec->getPackageName(),
			installPackageSpec->getPackageVersion(), outputDir);

		const std::string packageArguments = installPackageSpec->getArguments();

		if (!FileSystemUtils::doesDirectoryExist(packageDir)) {
			FileSystemUtils::createDirectory(packageDir);
		}

		CPackageExecutor::executePackage(startupAttachment, startupArgument,
			packageAttachment, packageArguments, supportingAttachmentCollection, packageDir);
	}
	CAF_CM_EXIT;
}

SmartPtrCInstallPackageSpecDoc CPackageInstaller::resolveAndCopyAttachments(
	const SmartPtrCInstallPackageSpecDoc& installPackageSpec) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CPackageInstaller", "resolveAndCopyAttachments");

	SmartPtrCInstallPackageSpecDoc installPackageSpecRc;

	CAF_CM_ENTER
	{
		CAF_CM_VALIDATE_SMARTPTR(installPackageSpec);

		const std::string installPackageDir = CPathBuilder::calcInstallPackageDir(
			installPackageSpec->getPackageNamespace(), installPackageSpec->getPackageName(),
			installPackageSpec->getPackageVersion());

		const SmartPtrCAttachmentCollectionDoc attachmentCollection =
			installPackageSpec->getAttachmentCollection();

		const SmartPtrCAttachmentNameCollectionDoc attachmentNameCollection =
			installPackageSpec->getSupportingAttachmentNameCollection();

		const SmartPtrCAttachmentCollectionDoc resolvedAttachmentCollection = resolveAttachments(
			attachmentNameCollection, attachmentCollection);

		const SmartPtrCAttachmentDoc startupAttachment = AttachmentUtils::findRequiredAttachment(
			installPackageSpec->getStartupAttachmentName(), attachmentCollection);
		const SmartPtrCAttachmentDoc packageAttachment = AttachmentUtils::findRequiredAttachment(
			installPackageSpec->getPackageAttachmentName(), attachmentCollection);

		const SmartPtrCAttachmentCollectionDoc copiedAttachmentCollection = copyAttachments(
			startupAttachment, packageAttachment, resolvedAttachmentCollection, installPackageDir);

		installPackageSpecRc.CreateInstance();
		installPackageSpecRc->initialize(installPackageSpec->getPackageNamespace(),
			installPackageSpec->getPackageName(), installPackageSpec->getPackageVersion(),
			installPackageSpec->getStartupAttachmentName(),
			installPackageSpec->getPackageAttachmentName(), attachmentNameCollection,
			copiedAttachmentCollection, installPackageSpec->getArguments());
	}
	CAF_CM_EXIT;

	return installPackageSpecRc;
}

void CPackageInstaller::saveInstallPackageSpec(
	const SmartPtrCInstallPackageSpecDoc& installPackageSpec) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CPackageInstaller", "saveInstallPackageSpec");

	CAF_CM_ENTER
	{
		CAF_CM_VALIDATE_SMARTPTR(installPackageSpec);

		const std::string installPackageDir = CPathBuilder::calcInstallPackageDir(
			installPackageSpec->getPackageNamespace(), installPackageSpec->getPackageName(),
			installPackageSpec->getPackageVersion());

		const std::string installPackageSpecPath = FileSystemUtils::buildPath(installPackageDir,
			_sInstallPackageSpecFilename);

		XmlRoots::saveInstallPackageSpecToFile(installPackageSpec, installPackageSpecPath);
	}
	CAF_CM_EXIT;
}

std::map<int32, SmartPtrCFullPackageElemDoc> CPackageInstaller::orderFullPackageElems(
	const std::deque<SmartPtrCFullPackageElemDoc>& fullPackageElemCollection) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CPackageInstaller", "orderFullPackageElems");

	std::map<int32, SmartPtrCFullPackageElemDoc> orderedFullPackageElemCollection;

	CAF_CM_ENTER
	{
		CAF_CM_VALIDATE_STL(fullPackageElemCollection);

		for (TConstIterator<std::deque<SmartPtrCFullPackageElemDoc> > fullPackageElemIter(
			fullPackageElemCollection); fullPackageElemIter; fullPackageElemIter++) {
			const SmartPtrCFullPackageElemDoc fullPackageElem = *fullPackageElemIter;
			orderedFullPackageElemCollection.insert(
				std::make_pair(fullPackageElem->getIndex(), fullPackageElem));
		}
	}
	CAF_CM_EXIT;

	return orderedFullPackageElemCollection;
}

std::map<int32, SmartPtrCMinPackageElemDoc> CPackageInstaller::orderMinPackageElems(
	const std::deque<SmartPtrCMinPackageElemDoc>& minPackageElemCollection) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CPackageInstaller", "orderProviderPackageElems");

	std::map<int32, SmartPtrCMinPackageElemDoc> orderedMinPackageElemCollection;

	CAF_CM_ENTER
	{
		CAF_CM_VALIDATE_STL(minPackageElemCollection);

		for (TConstIterator<std::deque<SmartPtrCMinPackageElemDoc> > minPackageElemIter(
			minPackageElemCollection); minPackageElemIter; minPackageElemIter++) {
			const SmartPtrCMinPackageElemDoc minPackageElem = *minPackageElemIter;
			orderedMinPackageElemCollection.insert(
				std::make_pair(minPackageElem->getIndex(), minPackageElem));
		}
	}
	CAF_CM_EXIT;

	return orderedMinPackageElemCollection;
}

SmartPtrCAttachmentCollectionDoc CPackageInstaller::resolveAttachments(
	const SmartPtrCAttachmentNameCollectionDoc& attachmentNameCollection,
	const SmartPtrCAttachmentCollectionDoc& attachmentCollection) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CPackageInstaller", "resolveAttachments");

	SmartPtrCAttachmentCollectionDoc rc;

	CAF_CM_ENTER
	{
		// attachmentNameCollection is optional
		CAF_CM_VALIDATE_SMARTPTR(attachmentCollection);

		if (!attachmentNameCollection.IsNull()) {
			const std::deque<std::string> attachmentNameCollectionInner =
				attachmentNameCollection->getName();

			std::deque<SmartPtrCAttachmentDoc> attachmentCollectionInner;
			for (TConstIterator<std::deque<std::string> > attachmentNameIter(
				attachmentNameCollectionInner); attachmentNameIter; attachmentNameIter++) {
				const std::string attachmentName = *attachmentNameIter;

				const SmartPtrCAttachmentDoc attachment = AttachmentUtils::findRequiredAttachment(
					attachmentName, attachmentCollection);
				attachmentCollectionInner.push_back(attachment);
			}

			if (!attachmentCollectionInner.empty()) {
				rc.CreateInstance();
				rc->initialize(attachmentCollectionInner,
					std::deque<SmartPtrCInlineAttachmentDoc>());
			}
		}
	}
	CAF_CM_EXIT;

	return rc;
}

SmartPtrCAttachmentCollectionDoc CPackageInstaller::copyAttachments(
	const SmartPtrCAttachmentDoc& startupAttachment,
	const SmartPtrCAttachmentDoc& packageAttachment,
	const SmartPtrCAttachmentCollectionDoc& supportingAttachmentCollection,
	const std::string& outputDir) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CPackageInstaller", "copyAttachments");

	SmartPtrCAttachmentCollectionDoc attachmentCollectionRc;

	CAF_CM_ENTER
	{
		CAF_CM_VALIDATE_SMARTPTR(startupAttachment);
		CAF_CM_VALIDATE_SMARTPTR(packageAttachment);
		// supportingAttachmentCollection is optional
		CAF_CM_VALIDATE_STRING(outputDir);

		std::deque<SmartPtrCAttachmentDoc> attachmentCollectionInner;

		attachmentCollectionInner.push_back(startupAttachment);
		attachmentCollectionInner.push_back(packageAttachment);

		if (!supportingAttachmentCollection.IsNull()) {
			for (TConstIterator<std::deque<SmartPtrCAttachmentDoc> > supportingAttachmentIter(
				supportingAttachmentCollection->getAttachment()); supportingAttachmentIter; supportingAttachmentIter++) {
				attachmentCollectionInner.push_back(*supportingAttachmentIter);
			}
		}

		std::deque<SmartPtrCAttachmentDoc> attachmentCollectionInnerRc;
		for (TConstIterator<std::deque<SmartPtrCAttachmentDoc> > attachmentIter(
			attachmentCollectionInner); attachmentIter; attachmentIter++) {
			const SmartPtrCAttachmentDoc attachment = *attachmentIter;

			const std::string attachmentUri = attachment->getUri();
			const std::string attachmentFilePath = UriUtils::parseRequiredFilePath(attachmentUri);

			UriUtils::SUriRecord uriRecord;
			UriUtils::parseUriString(attachmentUri, uriRecord);

			std::string dstAttachmentDir = outputDir;
			std::string relPath = FileSystemUtils::getBasename(attachmentFilePath);
			const std::map<std::string, std::string>::const_iterator iterParam =
				uriRecord.parameters.find("relPath");
			if (iterParam != uriRecord.parameters.end()) {
				relPath = iterParam->second;
				const std::string tmpPath = FileSystemUtils::buildPath(outputDir, relPath);
				dstAttachmentDir = FileSystemUtils::getDirname(tmpPath);
			} else {
				CAF_CM_LOG_DEBUG_VA1("Attachment URI does not contain relPath - %s", attachmentUri.c_str());
			}

			if (!FileSystemUtils::doesDirectoryExist(dstAttachmentDir)) {
				FileSystemUtils::createDirectory(dstAttachmentDir);
			}

			const std::string dstAttachmentFilePath = FileSystemUtils::buildPath(outputDir,
				relPath);

			if (FileSystemUtils::doesFileExist(dstAttachmentFilePath)) {
				CAF_CM_LOG_WARN_VA2("Destination file already exists... not copying \"%s\" to \"%s\"",
					attachmentFilePath.c_str(), dstAttachmentFilePath.c_str());
			} else {
				CAF_CM_LOG_DEBUG_VA2("Copying attachment from \"%s\" to \"%s\"",
					attachmentFilePath.c_str(), dstAttachmentFilePath.c_str());
				FileSystemUtils::copyFile(attachmentFilePath, dstAttachmentFilePath);
			}

			std::string dstAttachmentUri = "file:///" + dstAttachmentFilePath;
			if (!relPath.empty()) {
				dstAttachmentUri += "?relPath=" + relPath;
			}

			SmartPtrCAttachmentDoc dstAttachment;
			dstAttachment.CreateInstance();
			dstAttachment->initialize(attachment->getName(), attachment->getType(),
				dstAttachmentUri, false, attachment->getCmsPolicy());

			attachmentCollectionInnerRc.push_back(dstAttachment);
		}

		attachmentCollectionRc.CreateInstance();
		attachmentCollectionRc->initialize(attachmentCollectionInnerRc,
			std::deque<SmartPtrCInlineAttachmentDoc>());
	}
	CAF_CM_EXIT;

	return attachmentCollectionRc;
}

CPackageInstaller::SmartPtrCInstallPackageMatch CPackageInstaller::matchInstallPackageSpec(
	const SmartPtrCInstallPackageSpecDoc& installPackageSpec) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CPackageInstaller", "matchInstallPackageSpec");

	SmartPtrCInstallPackageMatch installPackageMatch;

	CAF_CM_ENTER
	{
		CAF_CM_VALIDATE_SMARTPTR(installPackageSpec);

		installPackageMatch.CreateInstance();
		installPackageMatch->_matchStatus = CInstallUtils::MATCH_NOTEQUAL;

		const SmartPtrCInstallPackageSpecCollection installPackageSpecCollection =
			readInstallPackageSpecs();

		if (!installPackageSpecCollection.IsNull()) {
			const std::string packageNamespace = installPackageSpec->getPackageNamespace();
			const std::string packageName = installPackageSpec->getPackageName();
			const std::string packageVersion = installPackageSpec->getPackageVersion();

			for (TConstIterator<std::deque<SmartPtrCInstallPackageSpecDoc> >
				installPackageSpecIter(*installPackageSpecCollection); installPackageSpecIter; installPackageSpecIter++) {
				const SmartPtrCInstallPackageSpecDoc installPackageSpecCur =
					*installPackageSpecIter;

				const std::string packageNamespaceCur =
					installPackageSpecCur->getPackageNamespace();
				const std::string packageNameCur = installPackageSpecCur->getPackageName();

				if ((packageNamespace.compare(packageNamespaceCur) == 0) && (packageName.compare(
					packageNameCur) == 0)) {
					const std::string packageVersionCur =
						installPackageSpecCur->getPackageVersion();
					installPackageMatch->_matchStatus = CInstallUtils::compareVersions(
						packageVersion, packageVersionCur);
					if (installPackageMatch->_matchStatus != CInstallUtils::MATCH_NOTEQUAL) {
						installPackageMatch->_matchedInstallPackageSpec = installPackageSpecCur;
						break;
					}
				} else {
					logDebug("Package did not match", installPackageSpec, installPackageSpecCur);
				}
			}
		}
	}
	CAF_CM_EXIT;

	return installPackageMatch;
}

CPackageInstaller::SmartPtrCInstallPackageSpecCollection CPackageInstaller::readInstallPackageSpecs() {
	CAF_CM_STATIC_FUNC_LOG_ONLY("CPackageInstaller", "readInstallPackageSpecs");

	SmartPtrCInstallPackageSpecCollection installPackageSpecCollection;

	CAF_CM_ENTER
	{
		const std::string installPackageDir = CPathBuilder::calcInstallPackageDir();

		const std::deque<std::string> installPackageSpecFiles =
			FileSystemUtils::findOptionalFiles(installPackageDir, _sInstallPackageSpecFilename);

		if (installPackageSpecFiles.empty()) {
			CAF_CM_LOG_WARN_VA2("No package install specs found - dir: %s, filename: %s",
				installPackageDir.c_str(), _sInstallPackageSpecFilename);
		} else {
			installPackageSpecCollection.CreateInstance();

			for (TConstIterator<std::deque<std::string> > installPackageSpecFileIter(
				installPackageSpecFiles); installPackageSpecFileIter; installPackageSpecFileIter++) {
				const std::string installPackageSpecFilePath = *installPackageSpecFileIter;

				CAF_CM_LOG_DEBUG_VA1("Found package install spec - %s",
					installPackageSpecFilePath.c_str());

				const SmartPtrCInstallPackageSpecDoc installPackageSpec =
					XmlRoots::parseInstallPackageSpecFromFile(installPackageSpecFilePath);

				installPackageSpecCollection->push_back(installPackageSpec);
			}
		}
	}
	CAF_CM_EXIT;

	return installPackageSpecCollection;
}

uint32 CPackageInstaller::countPackageReferences(
	const SmartPtrCInstallPackageSpecDoc& installPackageSpec,
	const std::deque<SmartPtrCInstallProviderSpecDoc>& installProviderSpecCollection) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CPackageInstaller", "countPackageReferences");

	uint32 refCnt = 0;

	CAF_CM_ENTER
	{
		CAF_CM_VALIDATE_SMARTPTR(installPackageSpec);
		// installProviderSpecCollection is optional

		const std::string packageNamespace = installPackageSpec->getPackageNamespace();
		const std::string packageName = installPackageSpec->getPackageName();
		const std::string packageVersion = installPackageSpec->getPackageVersion();

		for (TConstIterator<std::deque<SmartPtrCInstallProviderSpecDoc> > installProviderSpecIter(
			installProviderSpecCollection); installProviderSpecIter; installProviderSpecIter++) {
			const SmartPtrCInstallProviderSpecDoc installProviderSpec = *installProviderSpecIter;

			const std::deque<SmartPtrCMinPackageElemDoc> minPackageElemCollection =
				installProviderSpec->getPackageCollection();
			for (TConstIterator<std::deque<SmartPtrCMinPackageElemDoc> >
				minPackageElemIter(minPackageElemCollection); minPackageElemIter; minPackageElemIter++) {
				const SmartPtrCMinPackageElemDoc minPackageElem =
					*minPackageElemIter;

				const std::string packageNamespaceCur = minPackageElem->getPackageNamespace();
				const std::string packageNameCur = minPackageElem->getPackageName();

				if ((packageNamespace.compare(packageNamespaceCur) == 0) && (packageName.compare(
					packageNameCur) == 0)) {
					const std::string packageVersionCur = minPackageElem->getPackageVersion();
					const CInstallUtils::MATCH_STATUS matchStatus =
						CInstallUtils::compareVersions(packageVersion, packageVersionCur);
					if (matchStatus != CInstallUtils::MATCH_NOTEQUAL) {
						refCnt++;
					}
				}
			}
		}

		CAF_CM_LOG_DEBUG_VA4("Package ref cnt - %s::%s::%s = %d",
			packageNamespace.c_str(), packageName.c_str(), packageVersion.c_str(), refCnt);
	}
	CAF_CM_EXIT;

	return refCnt;
}

void CPackageInstaller::logDebug(
	const std::string& message,
	const SmartPtrCInstallPackageSpecDoc& installPackageSpec1,
	const SmartPtrCInstallPackageSpecDoc& installPackageSpec2) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CPackageInstaller", "logDebug");

	CAF_CM_ENTER
	{
		CAF_CM_VALIDATE_STRING(message);
		CAF_CM_VALIDATE_SMARTPTR(installPackageSpec1);
		CAF_CM_VALIDATE_SMARTPTR(installPackageSpec2);

		const std::string fullMessage = message + " - %s::%s::%s, %s::%s::%s";
		CAF_CM_LOG_DEBUG_VA6(fullMessage.c_str(),
			installPackageSpec1->getPackageNamespace().c_str(),
			installPackageSpec1->getPackageName().c_str(),
			installPackageSpec1->getPackageVersion().c_str(),
			installPackageSpec2->getPackageNamespace().c_str(),
			installPackageSpec2->getPackageName().c_str(),
			installPackageSpec2->getPackageVersion().c_str());
	}
	CAF_CM_EXIT;
}

void CPackageInstaller::logWarn(
	const std::string& message,
	const SmartPtrCInstallPackageSpecDoc& installPackageSpec1,
	const SmartPtrCInstallPackageSpecDoc& installPackageSpec2) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CPackageInstaller", "logWarn");

	CAF_CM_ENTER
	{
		CAF_CM_VALIDATE_STRING(message);
		CAF_CM_VALIDATE_SMARTPTR(installPackageSpec1);
		CAF_CM_VALIDATE_SMARTPTR(installPackageSpec2);

		const std::string fullMessage = message + " - %s::%s::%s, %s::%s::%s";
		CAF_CM_LOG_DEBUG_VA6(fullMessage.c_str(),
			installPackageSpec1->getPackageNamespace().c_str(),
			installPackageSpec1->getPackageName().c_str(),
			installPackageSpec1->getPackageVersion().c_str(),
			installPackageSpec2->getPackageNamespace().c_str(),
			installPackageSpec2->getPackageName().c_str(),
			installPackageSpec2->getPackageVersion().c_str());
	}
	CAF_CM_EXIT;
}

void CPackageInstaller::cleanupPackage(
	const SmartPtrCInstallPackageMatch& installPackageMatch) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CPackageInstaller", "cleanupPackage");
	CAF_CM_VALIDATE_SMARTPTR(installPackageMatch);

	const std::string installPackageDir = CPathBuilder::calcInstallPackageDir(
		installPackageMatch->_matchedInstallPackageSpec->getPackageNamespace(),
		installPackageMatch->_matchedInstallPackageSpec->getPackageName(),
		installPackageMatch->_matchedInstallPackageSpec->getPackageVersion());
	CAF_CM_LOG_DEBUG_VA1("Removing package directory - %s", installPackageDir.c_str());
	FileSystemUtils::recursiveRemoveDirectory(installPackageDir);
}
