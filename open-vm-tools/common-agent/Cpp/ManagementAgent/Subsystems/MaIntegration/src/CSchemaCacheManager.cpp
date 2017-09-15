/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Doc/DocXml/ProviderInfraXml/ProviderInfraXmlRoots.h"

#include "Doc/CafCoreTypesDoc/CFullyQualifiedClassGroupDoc.h"
#include "Doc/ProviderInfraDoc/CClassCollectionDoc.h"
#include "Doc/ProviderInfraDoc/CSchemaSummaryDoc.h"
#include "CSchemaCacheManager.h"
#include "Exception/CCafException.h"

using namespace Caf;

bool Caf::operator<(
	const CClassId& lhs,
	const CClassId& rhs) {
	bool rc = false;

	if (lhs._fqc->getClassNamespace() < rhs._fqc->getClassNamespace()) {
		rc = true;
	} else if (lhs._fqc->getClassNamespace() == rhs._fqc->getClassNamespace()) {
		if (lhs._fqc->getClassName() < rhs._fqc->getClassName()) {
			rc = true;
		} else if (lhs._fqc->getClassName() == rhs._fqc->getClassName()) {
			if (lhs._fqc->getClassVersion() < rhs._fqc->getClassVersion()) {
				rc = true;
			}
		}
	}

	return rc;
}

CSchemaCacheManager::CSchemaCacheManager() :
	_isInitialized(false),
	CAF_CM_INIT_LOG("CSchemaCacheManager") {
}

CSchemaCacheManager::~CSchemaCacheManager() {
}

void CSchemaCacheManager::initialize() {

	CAF_CM_FUNCNAME("initialize");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);

		const std::string schemaCacheDirPath =
			AppConfigUtils::getRequiredString(_sProviderHostArea, _sConfigSchemaCacheDir);
		const std::string schemaCacheDirPathExp = CStringUtils::expandEnv(schemaCacheDirPath);
		if (!FileSystemUtils::doesDirectoryExist(schemaCacheDirPathExp)) {
			CAF_CM_EXCEPTIONEX_VA1(FileNotFoundException, ERROR_FILE_NOT_FOUND,
				"Schema cache directory does not exist: %s", schemaCacheDirPathExp.c_str());
		}

		_schemaCacheDirPath = schemaCacheDirPathExp;
		_isInitialized = true;
	}
	CAF_CM_EXIT;
}

std::string CSchemaCacheManager::findProvider(
	const SmartPtrCFullyQualifiedClassGroupDoc& fqc) {

	CAF_CM_FUNCNAME_VALIDATE("findProvider");

	std::string providerUri;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_SMARTPTR(fqc);

		CClassId classId;
		classId._fqc = fqc;

		CClassCollection::const_iterator iter = _classCollection.find(classId);
		if (iter == _classCollection.end()) {
			CAF_CM_LOG_INFO_VA1("Provider not found... refreshing cache - %s", classId.toString().c_str());

			const uint16 maxWaitSecs = 10;
			waitForSchemaCacheCreation(_schemaCacheDirPath, maxWaitSecs);

			processSchemaSummaries(_schemaCacheDirPath, _classCollection);

			CClassCollection::const_iterator iter2 = _classCollection.find(classId);
			if (iter2 == _classCollection.end()) {
				CAF_CM_LOG_WARN_VA1("Provider not found even after refreshing the cache - %s", classId.toString().c_str());
			} else {
				providerUri = iter2->second;
			}
		} else {
			providerUri = iter->second;
		}
	}
	CAF_CM_EXIT;

	return providerUri;
}

void CSchemaCacheManager::processSchemaSummaries(
	const std::string& schemaCacheDirPath,
	CClassCollection& classCollection) const {
	CAF_CM_FUNCNAME_VALIDATE("processSchemaSummaries");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(schemaCacheDirPath);

		const FileSystemUtils::DirectoryItems schemaCacheDirItems =
			FileSystemUtils::itemsInDirectory(schemaCacheDirPath,
				FileSystemUtils::REGEX_MATCH_ALL);

		if (schemaCacheDirItems.directories.empty()) {
			CAF_CM_LOG_WARN_VA1(
				"Schema cache is empty - %s", schemaCacheDirPath.c_str());
		}

		for (TConstIterator<FileSystemUtils::Directories> schemaCacheDirIter(
			schemaCacheDirItems.directories); schemaCacheDirIter; schemaCacheDirIter++) {
			const std::string providerSchemaCacheDir = *schemaCacheDirIter;

			const std::string providerSchemaCacheDirPath = FileSystemUtils::buildPath(
				schemaCacheDirPath, providerSchemaCacheDir);
			const std::string schemaSummaryFilePath = FileSystemUtils::findOptionalFile(
				providerSchemaCacheDirPath, _sSchemaSummaryFilename);

			if (schemaSummaryFilePath.empty()) {
				CAF_CM_LOG_WARN_VA1(
					"Schema cache directory found without schema summary file... might be a timing issue - %s",
					providerSchemaCacheDirPath.c_str());
			} else {
				CAF_CM_LOG_DEBUG_VA1("Found schema cache summary file - %s", schemaSummaryFilePath.c_str());

				const SmartPtrCSchemaSummaryDoc schemaSummary =
					XmlRoots::parseSchemaSummaryFromFile(schemaSummaryFilePath);

				addNewClasses(schemaSummary, schemaSummaryFilePath, classCollection);
			}
		}
	}
	CAF_CM_EXIT;
}

void CSchemaCacheManager::addNewClasses(
	const SmartPtrCSchemaSummaryDoc& schemaSummary,
	const std::string& schemaSummaryFilePath,
	CClassCollection& classCollection) const {
	CAF_CM_FUNCNAME("addNewClasses");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_SMARTPTR(schemaSummary);
		CAF_CM_VALIDATE_STRING(schemaSummaryFilePath);

		const std::string invokerPath = schemaSummary->getInvokerPath();

		std::string providerUri;
		if (! invokerPath.empty()) {
			const std::string invokerPathExp = CStringUtils::expandEnv(invokerPath);
			if (FileSystemUtils::doesFileExist(invokerPathExp)) {
				const std::string invokerPathExpTmp =
					FileSystemUtils::normalizePathWithForward(invokerPathExp);
				providerUri = "file:///" + invokerPathExpTmp;
			} else {
				CAF_CM_LOG_ERROR_VA2(
					"Invoker path does not exist - invokerPath: %s, filePath: %s",
					invokerPathExp.c_str(), schemaSummaryFilePath.c_str());
			}
		} else {
			CAF_CM_EXCEPTIONEX_VA1(InvalidArgumentException, E_INVALIDARG,
				"Schema Summary file missing invokerRelPath - %s", schemaSummaryFilePath.c_str());
		}

		if (! providerUri.empty()) {
			const SmartPtrCClassCollectionDoc classCollectionDoc = schemaSummary->getClassCollection();
			const std::deque<SmartPtrCFullyQualifiedClassGroupDoc> fqcCollection = classCollectionDoc->getFullyQualifiedClass();

			for (TConstIterator<std::deque<SmartPtrCFullyQualifiedClassGroupDoc> > fqcIter(fqcCollection);
				fqcIter; fqcIter++) {
				CClassId classId;
				classId._fqc = *fqcIter;

				if (classCollection.find(classId) == classCollection.end()) {
					CAF_CM_LOG_DEBUG_VA1("Adding class %s", classId.toString().c_str());
					classCollection.insert(std::make_pair(classId, providerUri));
				}
			}
		}
	}
	CAF_CM_EXIT;
}

void CSchemaCacheManager::waitForSchemaCacheCreation(
	const std::string& schemaCacheDir,
	const uint16 maxWaitSecs) const {
	CAF_CM_FUNCNAME_VALIDATE("waitForSchemaCacheCreation");
	CAF_CM_VALIDATE_STRING(schemaCacheDir);

	const std::string providerRegDir = AppConfigUtils::getRequiredString(
		_sProviderHostArea, _sConfigProviderRegDir);
	const std::string providerRegDirExp = CStringUtils::expandEnv(providerRegDir);

	if (FileSystemUtils::doesDirectoryExist(providerRegDirExp)) {
		size_t numSchemaCacheItems = 0;
		size_t numProviderRegItems = 0;
		for (uint16 retry = 0; retry < maxWaitSecs; retry++) {
			numSchemaCacheItems = FileSystemUtils::itemsInDirectory(schemaCacheDir,
				FileSystemUtils::REGEX_MATCH_ALL).directories.size();
			numProviderRegItems = FileSystemUtils::itemsInDirectory(providerRegDir,
				FileSystemUtils::REGEX_MATCH_ALL).files.size();

			if (numSchemaCacheItems >= numProviderRegItems) {
				break;
			}

			CThreadUtils::sleep(1000);
		}

		if (numSchemaCacheItems < numProviderRegItems) {
			CAF_CM_LOG_WARN_VA5(
				"Schema cache initialization not complete - schemaCache: %s::%d, providerReg: %s::%d, maxWaitSecs: %d",
				schemaCacheDir.c_str(), numSchemaCacheItems, providerRegDir.c_str(),
				numProviderRegItems, maxWaitSecs);
		}
	} else {
		CAF_CM_LOG_WARN_VA1("Provider Reg directory does not exist - %s",
			providerRegDirExp.c_str());
	}
}
