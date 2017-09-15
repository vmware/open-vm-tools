/*
 *	Author: bwilliams
 *	Created: May 3, 2012
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Doc/DocXml/CafInstallRequestXml/CafInstallRequestXmlRoots.h"

#include "Doc/CafCoreTypesDoc/CAttachmentCollectionDoc.h"
#include "Doc/CafInstallRequestDoc/CFullPackageElemDoc.h"
#include "Doc/CafInstallRequestDoc/CInstallProviderJobDoc.h"
#include "Doc/CafInstallRequestDoc/CInstallProviderSpecDoc.h"
#include "Doc/CafInstallRequestDoc/CMinPackageElemDoc.h"
#include "Doc/CafInstallRequestDoc/CUninstallProviderJobDoc.h"
#include "CProviderInstaller.h"
#include "CPackageInstaller.h"

using namespace Caf;

void CProviderInstaller::installProvider(
	const SmartPtrCInstallProviderJobDoc& installProviderJob,
	const SmartPtrCAttachmentCollectionDoc& attachmentCollection,
	const std::string& outputDir) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CProviderInstaller", "installProvider");

	CAF_CM_ENTER
	{
		CAF_CM_VALIDATE_SMARTPTR(installProviderJob);
		CAF_CM_VALIDATE_SMARTPTR(attachmentCollection);
		CAF_CM_VALIDATE_STRING(outputDir);

		const SmartPtrCInstallProviderSpecDoc installProviderSpec = createInstallProviderSpec(
			installProviderJob);

		const SmartPtrCInstallProviderMatch installProviderMatch = matchInstallProviderSpec(
			installProviderSpec);

		switch (installProviderMatch->_matchStatus) {
			case CInstallUtils::MATCH_NOTEQUAL: {
				installProviderLow(installProviderJob, attachmentCollection, outputDir);
			}
			break;
			case CInstallUtils::MATCH_VERSION_EQUAL: {
				logWarn("Provider already installed", installProviderSpec,
					installProviderMatch->_matchedInstallProviderSpec);
			}
			break;
			case CInstallUtils::MATCH_VERSION_LESS: {
				logWarn("More recent provider already installed", installProviderSpec,
					installProviderMatch->_matchedInstallProviderSpec);
			}
			break;
			case CInstallUtils::MATCH_VERSION_GREATER: {
				logWarn("Upgrading provider", installProviderSpec,
					installProviderMatch->_matchedInstallProviderSpec);

				uninstallProviderLow(installProviderMatch->_matchedInstallProviderSpec, outputDir);
				installProviderLow(installProviderJob, attachmentCollection, outputDir);
			}
			break;
		}
	}
	CAF_CM_EXIT;
}

void CProviderInstaller::uninstallProvider(
	const SmartPtrCUninstallProviderJobDoc& uninstallProviderJob,
	const std::string& outputDir) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CProviderInstaller", "uninstallProvider");

	CAF_CM_ENTER
	{
		CAF_CM_VALIDATE_SMARTPTR(uninstallProviderJob);
		CAF_CM_VALIDATE_STRING(outputDir);

		const std::string installProviderDir = CPathBuilder::calcInstallProviderDir(
			uninstallProviderJob->getProviderNamespace(), uninstallProviderJob->getProviderName(),
			uninstallProviderJob->getProviderVersion());
		const std::string installProviderSpecPath = FileSystemUtils::buildPath(installProviderDir,
			_sInstallProviderSpecFilename);

		if (FileSystemUtils::doesFileExist(installProviderSpecPath)) {
			const SmartPtrCInstallProviderSpecDoc installProviderSpec =
				XmlRoots::parseInstallProviderSpecFromFile(installProviderSpecPath);

			uninstallProviderLow(installProviderSpec, outputDir);
		} else {
			CAF_CM_LOG_INFO_VA1("Uninstall unnecessary... provider is not installed - %s",
				calcProviderFqn(uninstallProviderJob).c_str());
		}
	}
	CAF_CM_EXIT;
}

CProviderInstaller::SmartPtrCInstallProviderSpecCollection CProviderInstaller::readInstallProviderSpecs() {
	CAF_CM_STATIC_FUNC_LOG_ONLY("CProviderInstaller", "readInstallProviderSpecs");

	SmartPtrCInstallProviderSpecCollection installProviderSpecCollection;

	CAF_CM_ENTER
	{
		const std::string installProviderDir = CPathBuilder::calcInstallProviderDir();

		const std::deque<std::string> installProviderSpecFiles =
			FileSystemUtils::findOptionalFiles(installProviderDir, _sInstallProviderSpecFilename);

		if (installProviderSpecFiles.empty()) {
			CAF_CM_LOG_WARN_VA2("No provider install specs found - dir: %s, filename: %s",
				installProviderDir.c_str(), _sInstallProviderSpecFilename);
		} else {
			installProviderSpecCollection.CreateInstance();

			for (TConstIterator<std::deque<std::string> > installProviderSpecFileIter(
				installProviderSpecFiles); installProviderSpecFileIter; installProviderSpecFileIter++) {
				const std::string installProviderSpecFilePath = *installProviderSpecFileIter;

				CAF_CM_LOG_DEBUG_VA1("Found provider install spec - %s",
					installProviderSpecFilePath.c_str());

				const SmartPtrCInstallProviderSpecDoc installProviderSpec =
					XmlRoots::parseInstallProviderSpecFromFile(installProviderSpecFilePath);

				installProviderSpecCollection->push_back(installProviderSpec);
			}
		}
	}
	CAF_CM_EXIT;

	return installProviderSpecCollection;
}

void CProviderInstaller::installProviderLow(
	const SmartPtrCInstallProviderJobDoc& installProviderJob,
	const SmartPtrCAttachmentCollectionDoc& attachmentCollection,
	const std::string& outputDir) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CProviderInstaller", "installProviderLow");

	CAF_CM_ENTER
	{
		CAF_CM_VALIDATE_SMARTPTR(installProviderJob);
		CAF_CM_VALIDATE_SMARTPTR(attachmentCollection);
		CAF_CM_VALIDATE_STRING(outputDir);

		const std::deque<SmartPtrCFullPackageElemDoc> fullPackageElemCollection =
			installProviderJob->getPackageCollection();

		CPackageInstaller::installPackages(fullPackageElemCollection, attachmentCollection,
			outputDir);

		const SmartPtrCInstallProviderSpecDoc uninstallProviderSpec = createInstallProviderSpec(
			installProviderJob);
		storeInstallProviderSpec(uninstallProviderSpec);
	}
	CAF_CM_EXIT;
}

void CProviderInstaller::uninstallProviderLow(
	const SmartPtrCInstallProviderSpecDoc& installProviderSpec,
	const std::string& outputDir) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CProviderInstaller", "uninstallProviderLow");

	CAF_CM_ENTER
	{
		CAF_CM_VALIDATE_SMARTPTR(installProviderSpec);
		CAF_CM_VALIDATE_STRING(outputDir);

		const std::deque<SmartPtrCMinPackageElemDoc> minPackageElemCollection =
			installProviderSpec->getPackageCollection();

		const SmartPtrCInstallProviderSpecCollection installProviderSpecCollection =
			readInstallProviderSpecs();

		std::deque<SmartPtrCInstallProviderSpecDoc> installProviderSpecCollectionInner;
		if (!installProviderSpecCollection.IsNull()) {
			installProviderSpecCollectionInner = *installProviderSpecCollection;
		}

		try {
			CPackageInstaller::uninstallPackages(minPackageElemCollection,
				installProviderSpecCollectionInner, outputDir);
		} catch (ProcessFailedException* ex) {
			cleanupProvider(installProviderSpec);
			ex->throwSelf();
		}

		cleanupProvider(installProviderSpec);
	}
	CAF_CM_EXIT;
}

void CProviderInstaller::storeInstallProviderSpec(
	const SmartPtrCInstallProviderSpecDoc& installProviderSpec) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CProviderInstaller", "storeInstallProviderSpec");

	CAF_CM_ENTER
	{
		CAF_CM_VALIDATE_SMARTPTR(installProviderSpec);

		const std::string installProviderDir = CPathBuilder::calcInstallProviderDir(
			installProviderSpec->getProviderNamespace(), installProviderSpec->getProviderName(),
			installProviderSpec->getProviderVersion());

		const std::string installProviderSpecPath = FileSystemUtils::buildPath(installProviderDir,
			_sInstallProviderSpecFilename);

		XmlRoots::saveInstallProviderSpecToFile(installProviderSpec, installProviderSpecPath);
	}
	CAF_CM_EXIT;
}

CProviderInstaller::SmartPtrCInstallProviderMatch CProviderInstaller::matchInstallProviderSpec(
	const SmartPtrCInstallProviderSpecDoc& installProviderSpec) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CProviderInstaller", "matchInstallProviderSpec");

	SmartPtrCInstallProviderMatch installProviderMatch;

	CAF_CM_ENTER
	{
		CAF_CM_VALIDATE_SMARTPTR(installProviderSpec);

		installProviderMatch.CreateInstance();
		installProviderMatch->_matchStatus = CInstallUtils::MATCH_NOTEQUAL;

		const SmartPtrCInstallProviderSpecCollection installProviderSpecCollection =
			readInstallProviderSpecs();

		if (!installProviderSpecCollection.IsNull()) {
			const std::string providerNamespace = installProviderSpec->getProviderNamespace();
			const std::string providerName = installProviderSpec->getProviderName();
			const std::string providerVersion = installProviderSpec->getProviderVersion();

			for (TConstIterator<std::deque<SmartPtrCInstallProviderSpecDoc> >
				installProviderSpecIter(*installProviderSpecCollection); installProviderSpecIter; installProviderSpecIter++) {
				const SmartPtrCInstallProviderSpecDoc installProviderSpecCur =
					*installProviderSpecIter;

				const std::string providerNamespaceCur =
					installProviderSpecCur->getProviderNamespace();
				const std::string providerNameCur = installProviderSpecCur->getProviderName();

				if ((providerNamespace.compare(providerNamespaceCur) == 0)
					&& (providerName.compare(providerNameCur) == 0)) {
					const std::string providerVersionCur =
						installProviderSpecCur->getProviderVersion();
					installProviderMatch->_matchStatus = CInstallUtils::compareVersions(
						providerVersion, providerVersionCur);
					if (installProviderMatch->_matchStatus != CInstallUtils::MATCH_NOTEQUAL) {
						installProviderMatch->_matchedInstallProviderSpec = installProviderSpecCur;
						break;
					}
				} else {
					logDebug("Provider did not match", installProviderSpec, installProviderSpecCur);
				}
			}
		}
	}
	CAF_CM_EXIT;

	return installProviderMatch;
}

SmartPtrCInstallProviderSpecDoc CProviderInstaller::createInstallProviderSpec(
	const SmartPtrCInstallProviderJobDoc& installProviderJob) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CProviderInstaller", "createInstallProviderSpec");

	SmartPtrCInstallProviderSpecDoc installProviderSpec;

	CAF_CM_ENTER
	{
		CAF_CM_VALIDATE_SMARTPTR(installProviderJob);

		const std::deque<SmartPtrCFullPackageElemDoc> fullPackageElemCollection =
			installProviderJob->getPackageCollection();

		std::deque<SmartPtrCMinPackageElemDoc> minPackageElemCollection;
		for (TConstIterator<std::deque<SmartPtrCFullPackageElemDoc> > fullPackageElemIter(
			fullPackageElemCollection); fullPackageElemIter; fullPackageElemIter++) {
			const SmartPtrCFullPackageElemDoc fullPackageElem = *fullPackageElemIter;

			SmartPtrCMinPackageElemDoc minPackageElem;
			minPackageElem.CreateInstance();
			minPackageElem->initialize(fullPackageElem->getIndex(),
				fullPackageElem->getPackageNamespace(), fullPackageElem->getPackageName(),
				fullPackageElem->getPackageVersion());

			minPackageElemCollection.push_back(minPackageElem);
		}

		installProviderSpec.CreateInstance();
		installProviderSpec->initialize(installProviderJob->getClientId(),
			installProviderJob->getProviderNamespace(), installProviderJob->getProviderName(),
			installProviderJob->getProviderVersion(), minPackageElemCollection);
	}
	CAF_CM_EXIT;

	return installProviderSpec;
}

void CProviderInstaller::logDebug(
	const std::string& message,
	const SmartPtrCInstallProviderSpecDoc& installProviderSpec1,
	const SmartPtrCInstallProviderSpecDoc& installProviderSpec2) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CProviderInstaller", "logDebug");

	CAF_CM_ENTER
	{
		CAF_CM_VALIDATE_STRING(message);
		CAF_CM_VALIDATE_SMARTPTR(installProviderSpec1);
		CAF_CM_VALIDATE_SMARTPTR(installProviderSpec2);

		const std::string fullMessage = message + " - %s::%s::%s, %s::%s::%s";
		CAF_CM_LOG_DEBUG_VA6(fullMessage.c_str(),
			installProviderSpec1->getProviderNamespace().c_str(),
			installProviderSpec1->getProviderName().c_str(),
			installProviderSpec1->getProviderVersion().c_str(),
			installProviderSpec2->getProviderNamespace().c_str(),
			installProviderSpec2->getProviderName().c_str(),
			installProviderSpec2->getProviderVersion().c_str());
	}
	CAF_CM_EXIT;
}

void CProviderInstaller::logWarn(
	const std::string& message,
	const SmartPtrCInstallProviderSpecDoc& installProviderSpec1,
	const SmartPtrCInstallProviderSpecDoc& installProviderSpec2) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CProviderInstaller", "logWarn");

	CAF_CM_ENTER
	{
		CAF_CM_VALIDATE_STRING(message);
		CAF_CM_VALIDATE_SMARTPTR(installProviderSpec1);
		CAF_CM_VALIDATE_SMARTPTR(installProviderSpec2);

		const std::string fullMessage = message + " - %s::%s::%s, %s::%s::%s";
		CAF_CM_LOG_DEBUG_VA6(fullMessage.c_str(),
			installProviderSpec1->getProviderNamespace().c_str(),
			installProviderSpec1->getProviderName().c_str(),
			installProviderSpec1->getProviderVersion().c_str(),
			installProviderSpec2->getProviderNamespace().c_str(),
			installProviderSpec2->getProviderName().c_str(),
			installProviderSpec2->getProviderVersion().c_str());
	}
	CAF_CM_EXIT;
}

void CProviderInstaller::cleanupProvider(
	const SmartPtrCInstallProviderSpecDoc& installProviderSpec) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CProviderInstaller", "cleanupProvider");
	CAF_CM_VALIDATE_SMARTPTR(installProviderSpec);

	const std::string providerNamespace = installProviderSpec->getProviderNamespace();
	const std::string providerName = installProviderSpec->getProviderName();
	const std::string providerVersion = installProviderSpec->getProviderVersion();
	const std::string providerFqn = calcProviderFqn(installProviderSpec);

	const std::string installProviderDir = CPathBuilder::calcInstallProviderDir(
		providerNamespace, providerName, providerVersion);
	if (FileSystemUtils::doesDirectoryExist(installProviderDir)) {
		FileSystemUtils::recursiveRemoveDirectory(installProviderDir);
	}

	const std::string providerSchemaCacheDir = CPathBuilder::calcProviderSchemaCacheDir(
			providerNamespace, providerName, providerVersion);
	if (FileSystemUtils::doesDirectoryExist(providerSchemaCacheDir)) {
		FileSystemUtils::recursiveRemoveDirectory(providerSchemaCacheDir);
	}

	const std::string providerRegDir = AppConfigUtils::getRequiredString(
		_sProviderHostArea, _sConfigProviderRegDir);
	const std::string providerRegDirExp = CStringUtils::expandEnv(providerRegDir);
	const std::string providerRegFilename = providerFqn + ".xml";
	const std::string providerRegPath = FileSystemUtils::buildPath(providerRegDirExp,
		providerRegFilename);
	if (FileSystemUtils::doesFileExist(providerRegPath)) {
		FileSystemUtils::removeFile(providerRegPath);
	}

	const std::string invokersDir = AppConfigUtils::getRequiredString(
		_sProviderHostArea, _sConfigInvokersDir);
	const std::string invokersDirExp = CStringUtils::expandEnv(invokersDir);
	const std::string invokersFilename = providerFqn;
	const std::string invokersPath = FileSystemUtils::buildPath(invokersDirExp,
		invokersFilename);
	if (FileSystemUtils::doesFileExist(invokersPath)) {
		FileSystemUtils::removeFile(invokersPath);
	}
}

std::string CProviderInstaller::calcProviderFqn(
	const SmartPtrCInstallProviderSpecDoc& installProviderSpec) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CProviderInstaller", "calcProviderFqn");
	CAF_CM_VALIDATE_SMARTPTR(installProviderSpec);

	return installProviderSpec->getProviderNamespace() + "_"
		+ installProviderSpec->getProviderName() + "_"
		+ installProviderSpec->getProviderVersion();
}

std::string CProviderInstaller::calcProviderFqn(
	const SmartPtrCUninstallProviderJobDoc& uninstallProviderJob) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CProviderInstaller", "calcProviderFqn");
	CAF_CM_VALIDATE_SMARTPTR(uninstallProviderJob);

	return uninstallProviderJob->getProviderNamespace() + "_"
		+ uninstallProviderJob->getProviderName() + "_"
		+ uninstallProviderJob->getProviderVersion();
}
