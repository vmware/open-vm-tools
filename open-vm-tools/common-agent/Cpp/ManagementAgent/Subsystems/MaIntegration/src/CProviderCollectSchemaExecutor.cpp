/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Doc/DocXml/ProviderInfraXml/ProviderInfraXmlRoots.h"
#include "Doc/DocXml/ProviderResultsXml/ProviderResultsXmlRoots.h"
#include "Doc/DocXml/ResponseXml/ResponseXmlRoots.h"

#include "Common/CLoggingSetter.h"
#include "Doc/CafCoreTypesDoc/CAttachmentCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CAttachmentDoc.h"
#include "Doc/CafCoreTypesDoc/CFullyQualifiedClassGroupDoc.h"
#include "Doc/ProviderInfraDoc/CClassCollectionDoc.h"
#include "Doc/ProviderInfraDoc/CProviderRegDoc.h"
#include "Doc/ProviderInfraDoc/CSchemaSummaryDoc.h"
#include "Doc/ProviderResultsDoc/CSchemaDoc.h"
#include "Doc/ResponseDoc/CProviderResponseDoc.h"
#include "Doc/SchemaTypesDoc/CActionClassDoc.h"
#include "Doc/SchemaTypesDoc/CDataClassDoc.h"
#include "Integration/IIntMessage.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "Exception/CCafException.h"
#include "CProviderCollectSchemaExecutor.h"
#include "Integration/Caf/CCafMessageCreator.h"
#include "Integration/Caf/CCafMessagePayloadParser.h"

using namespace Caf;

CProviderCollectSchemaExecutor::CProviderCollectSchemaExecutor() :
	_isInitialized(false),
	CAF_CM_INIT_LOG("CProviderCollectSchemaExecutor") {
}

CProviderCollectSchemaExecutor::~CProviderCollectSchemaExecutor() {
}

void CProviderCollectSchemaExecutor::initializeBean(
		const IBean::Cargs& ctorArgs,
		const IBean::Cprops& properties) {
	CAF_CM_FUNCNAME_VALIDATE("initializeBean");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STL_EMPTY(ctorArgs);
	CAF_CM_VALIDATE_STL_EMPTY(properties);

	const std::string schemaCacheDirPath = AppConfigUtils::getRequiredString(
		_sProviderHostArea, _sConfigSchemaCacheDir);
	const std::string schemaCacheDirPathExp = CStringUtils::expandEnv(schemaCacheDirPath);
	if (!FileSystemUtils::doesDirectoryExist(schemaCacheDirPathExp)) {
		CAF_CM_LOG_INFO_VA1(
			"Schema cache directory does not exist... creating - %s",
			schemaCacheDirPathExp.c_str());
		FileSystemUtils::createDirectory(schemaCacheDirPathExp);
	}

	const std::string invokersDir = AppConfigUtils::getRequiredString(
		_sProviderHostArea, _sConfigInvokersDir);
	const std::string invokersDirExp = CStringUtils::expandEnv(invokersDir);
	if (!FileSystemUtils::doesDirectoryExist(invokersDirExp)) {
		CAF_CM_LOG_INFO_VA1(
			"Invokers directory does not exist... creating - %s",
			invokersDirExp.c_str());
		FileSystemUtils::createDirectory(invokersDirExp);
	}

	_schemaCacheDirPath = schemaCacheDirPathExp;
	_invokersDir = invokersDirExp;
	_isInitialized = true;
}

void CProviderCollectSchemaExecutor::terminateBean() {

}

SmartPtrIIntMessage CProviderCollectSchemaExecutor::processMessage(
	const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME_VALIDATE("processMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(message);

	CAF_CM_LOG_DEBUG_VA2("Called - schemaCacheDirPath: %s, invokersDir: %s",
		_schemaCacheDirPath.c_str(), _invokersDir.c_str());

	SmartPtrCLoggingSetter loggingSetter;
	loggingSetter.CreateInstance();

	const SmartPtrCProviderRegDoc providerReg =
			CCafMessagePayloadParser::getProviderReg(message->getPayload());

	const std::string providerNamespace = providerReg->getProviderNamespace();
	const std::string providerName = providerReg->getProviderName();
	const std::string providerVersion = providerReg->getProviderVersion();

	std::string providerVersionNew = providerVersion;
	std::replace(providerVersionNew.begin(), providerVersionNew.end(), '.', '_');

	const std::string providerDirName =
		providerNamespace + "_" + providerName + "_" + providerVersionNew;
	const std::string providerSchemaCacheDir =
		FileSystemUtils::buildPath(_schemaCacheDirPath, providerDirName);
	const std::string providerResponsePath =
		FileSystemUtils::buildPath(providerSchemaCacheDir, _sProviderResponseFilename);

	executeProvider(providerReg, _invokersDir, providerSchemaCacheDir,
			providerResponsePath, loggingSetter);

	const std::string relFilename =
		FileSystemUtils::buildPath(providerDirName, _sProviderResponseFilename);
	const SmartPtrCDynamicByteArray providerResponse =
			FileSystemUtils::loadByteFile(providerResponsePath);

	return CCafMessageCreator::createFromProviderResponse(
			providerResponse, relFilename, message->getHeaders());
}

void CProviderCollectSchemaExecutor::executeProvider(
	const SmartPtrCProviderRegDoc& providerReg,
	const std::string& invokersDir,
	const std::string& providerSchemaCacheDir,
	const std::string& providerResponsePath,
	SmartPtrCLoggingSetter& loggingSetter) const {
	CAF_CM_FUNCNAME("executeProvider");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(providerReg);
	CAF_CM_VALIDATE_STRING(invokersDir);
	CAF_CM_VALIDATE_STRING(providerSchemaCacheDir);
	CAF_CM_VALIDATE_STRING(providerResponsePath);
	CAF_CM_VALIDATE_SMARTPTR(loggingSetter);

	const std::string providerNamespace = providerReg->getProviderNamespace();
	const std::string providerName = providerReg->getProviderName();
	const std::string providerVersion = providerReg->getProviderVersion();
	const std::string invokerRelPath = providerReg->getInvokerRelPath();

	const std::string schemaSummaryPath =
		FileSystemUtils::buildPath(providerSchemaCacheDir, _sSchemaSummaryFilename);

	if (FileSystemUtils::doesFileExist(schemaSummaryPath)) {
		CAF_CM_LOG_INFO_VA1(
			"Schema summary file already exists - %s", schemaSummaryPath.c_str());
	} else {
		std::string invokerPath;
		if (! invokerRelPath.empty()) {
			const std::string invokerRelPathExp = CStringUtils::expandEnv(invokerRelPath);
			invokerPath = FileSystemUtils::buildPath(invokersDir, invokerRelPathExp);

			if (!FileSystemUtils::doesFileExist(invokerPath)) {
				CAF_CM_EXCEPTIONEX_VA1(FileNotFoundException, ERROR_FILE_NOT_FOUND,
					"Invoker does not exist - %s", invokerPath.c_str());
			}

			setupSchemaCacheDir(providerSchemaCacheDir, loggingSetter);
			runProvider(invokerPath, providerSchemaCacheDir);
		} else {
			CAF_CM_EXCEPTIONEX_VA1(InvalidArgumentException, E_INVALIDARG,
				"Unrecognized provider URI protocol in Provider Registration file - %s", providerName.c_str());
		}

		const std::string schemaPath = findSchemaPath(providerResponsePath);
		const SmartPtrCSchemaSummaryDoc schemaSummary = createSchemaSummary(
			schemaPath, invokerPath, providerNamespace, providerName, providerVersion);

		const std::string schemaSummaryMem = XmlRoots::saveSchemaSummaryToString(schemaSummary);
		FileSystemUtils::saveTextFile(schemaSummaryPath, schemaSummaryMem);
	}
}

void CProviderCollectSchemaExecutor::setupSchemaCacheDir(
	const std::string& providerSchemaCacheDir,
	const SmartPtrCLoggingSetter& loggingSetter) const {
	CAF_CM_FUNCNAME_VALIDATE("setupSchemaCacheDir");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(providerSchemaCacheDir);
	CAF_CM_VALIDATE_SMARTPTR(loggingSetter);

	if (FileSystemUtils::doesDirectoryExist(providerSchemaCacheDir)) {
		CAF_CM_LOG_INFO_VA1(
			"Removing the schema cache directory because it appears to be incomplete - %s",
			providerSchemaCacheDir.c_str());
		FileSystemUtils::recursiveRemoveDirectory(providerSchemaCacheDir);
	}

	FileSystemUtils::createDirectory(providerSchemaCacheDir);
	loggingSetter->initialize(providerSchemaCacheDir);
}

void CProviderCollectSchemaExecutor::runProvider(
	const std::string& invokerPath,
	const std::string& providerSchemaCacheDir) const {
	CAF_CM_FUNCNAME_VALIDATE("runProvider");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(invokerPath);
	CAF_CM_VALIDATE_STRING(providerSchemaCacheDir);

	CAF_CM_LOG_DEBUG_VA2(
		"Executing the command - %s --schema -o %s",
		invokerPath.c_str(), providerSchemaCacheDir.c_str());

	std::string newProviderSchemaCacheDir = FileSystemUtils::normalizePathWithForward(
		providerSchemaCacheDir);

	Cdeqstr argv;
	argv.push_back(invokerPath);
	argv.push_back("--schema");
	argv.push_back("-o");
	argv.push_back(newProviderSchemaCacheDir);

	const std::string stdoutPath = FileSystemUtils::buildPath(
		newProviderSchemaCacheDir, _sStdoutFilename);
	const std::string stderrPath = FileSystemUtils::buildPath(
		newProviderSchemaCacheDir, _sStderrFilename);

	ProcessUtils::runSyncToFiles(argv, stdoutPath, stderrPath);
}

SmartPtrCSchemaSummaryDoc CProviderCollectSchemaExecutor::createSchemaSummary(
	const std::string& schemaPath,
	const std::string& invokerPath,
	const std::string& providerNamespace,
	const std::string& providerName,
	const std::string& providerVersion) const {
	CAF_CM_FUNCNAME_VALIDATE("createSchemaSummary");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(schemaPath);
	CAF_CM_VALIDATE_STRING(invokerPath);
	CAF_CM_VALIDATE_STRING(providerNamespace);
	CAF_CM_VALIDATE_STRING(providerName);
	CAF_CM_VALIDATE_STRING(providerVersion);

	std::deque<SmartPtrCFullyQualifiedClassGroupDoc> fqcCollection;

	const std::string schemaMem = FileSystemUtils::loadTextFile(schemaPath);
	const SmartPtrCSchemaDoc schema = XmlRoots::parseSchemaFromString(schemaMem);

	const std::deque<SmartPtrCDataClassDoc> dataClassCollection = schema->getDataClassCollection();
	for (TConstIterator<std::deque<SmartPtrCDataClassDoc> > dataClassIter(dataClassCollection);
		dataClassIter; dataClassIter++) {
		const SmartPtrCDataClassDoc dataClass = *dataClassIter;

		SmartPtrCFullyQualifiedClassGroupDoc fqc;
		fqc.CreateInstance();
		fqc->initialize(dataClass->getNamespaceVal(), dataClass->getName(), dataClass->getVersion());

		fqcCollection.push_back(fqc);
	}

	const std::deque<SmartPtrCActionClassDoc> actionClassCollection = schema->getActionClassCollection();
	for (TConstIterator<std::deque<SmartPtrCActionClassDoc> > actionClassIter(actionClassCollection);
		actionClassIter; actionClassIter++) {
		const SmartPtrCActionClassDoc actionClass = *actionClassIter;

		SmartPtrCFullyQualifiedClassGroupDoc fqc;
		fqc.CreateInstance();
		fqc->initialize(actionClass->getNamespaceVal(), actionClass->getName(), actionClass->getVersion());

		fqcCollection.push_back(fqc);
	}

	SmartPtrCClassCollectionDoc classCollection;
	classCollection.CreateInstance();
	classCollection->initialize(fqcCollection);

	SmartPtrCSchemaSummaryDoc schemaSummary;
	schemaSummary.CreateInstance();
	schemaSummary->initialize(providerNamespace, providerName, providerVersion,
		classCollection, invokerPath);

	return schemaSummary;
}

std::string CProviderCollectSchemaExecutor::findSchemaPath(
	const std::string& providerResponsePath) const {
	CAF_CM_FUNCNAME("findSchemaPath");
	CAF_CM_VALIDATE_STRING(providerResponsePath);

	const std::string providerResponseMem = FileSystemUtils::loadTextFile(providerResponsePath);
	const SmartPtrCProviderResponseDoc providerResponse =
		XmlRoots::parseProviderResponseFromString(providerResponseMem);

	std::string schemaPath;
	const SmartPtrCAttachmentCollectionDoc attachmentCollection = providerResponse->getAttachmentCollection();
	if (attachmentCollection.IsNull()) {
		CAF_CM_LOG_INFO_VA1(
			"Provider response doesn't contain an attachment collection - %s",
			providerResponsePath.c_str());
	} else {
		const std::deque<SmartPtrCAttachmentDoc> attachmentCollectionInner = attachmentCollection->getAttachment();
		if (attachmentCollectionInner.empty()) {
			CAF_CM_LOG_INFO_VA1(
				"Provider response contains an empty attachment collection - %s",
				providerResponsePath.c_str());
		} else {
			for(TConstIterator<std::deque<SmartPtrCAttachmentDoc> > attachmentIter(attachmentCollectionInner);
				attachmentIter; attachmentIter++) {
				const SmartPtrCAttachmentDoc attachment = *attachmentIter;
				const std::string attachmentName = attachment->getName();
				const std::string attachmentType = attachment->getType();

				if((attachmentType.compare("cdif") == 0) &&
					(attachmentName.find("-collectSchema-") != std::string::npos)) {
					if(! schemaPath.empty()) {
						CAF_CM_EXCEPTIONEX_VA3(DuplicateElementException, ERROR_ALREADY_EXISTS,
							"Found multiple schema files - \"%s\" and \"%s\" in %s",
							attachmentName.c_str(), schemaPath.c_str(), providerResponsePath.c_str());
					}

					const std::string attachmentUri = attachment->getUri();

					UriUtils::SUriRecord uriRecord;
					UriUtils::parseUriString(attachmentUri, uriRecord);

					if(uriRecord.protocol.compare("file") != 0) {
						CAF_CM_EXCEPTIONEX_VA3(InvalidArgumentException, ERROR_INVALID_DATA,
							"Unsupported protocol (%s != \"file\") - %s in %s",
							uriRecord.protocol.c_str(), attachmentUri.c_str(), providerResponsePath.c_str());
					}

					UriUtils::SFileUriRecord fileUriRecord;
					UriUtils::parseFileAddress(uriRecord.address, fileUriRecord);

					schemaPath = CStringUtils::expandEnv(fileUriRecord.path);
					if(! FileSystemUtils::doesFileExist(schemaPath)) {
						CAF_CM_EXCEPTIONEX_VA2(FileNotFoundException, ERROR_FILE_NOT_FOUND,
							"Schema file not found - %s in manifest %s",
							schemaPath.c_str(), providerResponsePath.c_str());
					}
				} else {
					CAF_CM_LOG_DEBUG_VA3(
						"Provider response attachment is not a cdif collectSchema - type: %s, name: %s, path: %s",
						attachmentType.c_str(), attachmentName.c_str(),
						providerResponsePath.c_str());
				}
			}
		}
	}

	if(schemaPath.empty()) {
		CAF_CM_EXCEPTIONEX_VA1(FileNotFoundException, ERROR_FILE_NOT_FOUND,
			"Schema not found in manifest - %s", providerResponsePath.c_str());
	}

	return schemaPath;
}
