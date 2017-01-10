/*
 *	Author: bwilliams
 *	Created: May 3, 2012
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Doc/CafCoreTypesDoc/CAttachmentCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CAttachmentDoc.h"
#include "Exception/CCafException.h"
#include "CPackageExecutor.h"

using namespace Caf;

void CPackageExecutor::executePackage(
	const SmartPtrCAttachmentDoc& startupAttachment,
	const std::string& startupArgument,
	const SmartPtrCAttachmentDoc& packageAttachment,
	const std::string& packageArguments,
	const SmartPtrCAttachmentCollectionDoc& supportingAttachmentCollection,
	const std::string& outputDir) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CPackageExecutor", "executePackage");

	CAF_CM_ENTER
	{
		CAF_CM_VALIDATE_SMARTPTR(startupAttachment);
		CAF_CM_VALIDATE_STRING(startupArgument);
		CAF_CM_VALIDATE_SMARTPTR(packageAttachment);
		// packageArguments is optional
		// supportingAttachmentCollection is optional
		CAF_CM_VALIDATE_STRING(outputDir);

		const std::string installInvoker = createInstallInvoker(startupAttachment,
			startupArgument, packageAttachment, packageArguments,
			supportingAttachmentCollection, outputDir);

		runInstallInvoker(installInvoker, outputDir);
	}
	CAF_CM_EXIT;
}

std::string CPackageExecutor::createInstallInvoker(
	const SmartPtrCAttachmentDoc& startupAttachment,
	const std::string& startupArgument,
	const SmartPtrCAttachmentDoc& packageAttachment,
	const std::string& packageArguments,
	const SmartPtrCAttachmentCollectionDoc& supportingAttachmentCollection,
	const std::string& outputDir) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CPackageExecutor", "createInstallInvoker");

	std::string installInvoker;

	CAF_CM_ENTER
	{
		CAF_CM_VALIDATE_SMARTPTR(startupAttachment);
		CAF_CM_VALIDATE_STRING(startupArgument);
		CAF_CM_VALIDATE_SMARTPTR(packageAttachment);
		// packageArguments is optional
		// supportingAttachmentCollection is optional
		CAF_CM_VALIDATE_STRING(outputDir);

		const std::string inputDir = CPathBuilder::getRootConfigDir(_sConfigInputDir);
#ifdef WIN32
		const std::string installProviderHeaderPath =
			FileSystemUtils::buildPath(inputDir, "installProviderHeader.bat");
#else
		const std::string installProviderHeaderPath =
			FileSystemUtils::buildPath(inputDir, "installProviderHeader.sh");
#endif
		const std::string installProviderHeader = FileSystemUtils::loadTextFile(
			installProviderHeaderPath);

		std::string attachmentUris = "";
		if (!supportingAttachmentCollection.IsNull()) {
			const std::deque<SmartPtrCAttachmentDoc> attachmentCollectionInner =
				supportingAttachmentCollection->getAttachment();
			for (TConstIterator<std::deque<SmartPtrCAttachmentDoc> > attachmentIter(
				attachmentCollectionInner); attachmentIter; attachmentIter++) {
				const SmartPtrCAttachmentDoc attachment = *attachmentIter;
				attachmentUris += attachment->getUri() + ";";
			}
		}

		const std::string startupAttachmentUri = startupAttachment->getUri();
		std::string startupAttachmentFile = UriUtils::parseRequiredFilePath(
			startupAttachmentUri);

		const std::string packageAttachmentUri = packageAttachment->getUri();
		std::string packageAttachmentFile = UriUtils::parseRequiredFilePath(
			packageAttachmentUri);

		const std::string packageArgumentsTmp = packageArguments.empty() ? ""
			: packageArguments;

#ifdef WIN32
        const std::string newLine = "\r\n";
		const std::string fileContents =
			installProviderHeader + newLine
			+ std::string("set CAF_PACKAGE_FILE=") + packageAttachmentFile + newLine
			+ std::string("set CAF_PACKAGE_ARGS=") + packageArgumentsTmp + newLine
			+ std::string("set CAF_ATTACHMENT_URIS=") + attachmentUris + newLine
			+ startupAttachmentFile + " " + startupArgument + newLine;
		installInvoker = FileSystemUtils::buildPath(outputDir, "CafInstallInvoker.bat");
#else
        const std::string newLine = "\n";
		const std::string fileContents =
			installProviderHeader + newLine
			+ std::string("export CAF_PACKAGE_FILE=") + packageAttachmentFile + newLine
			+ std::string("export CAF_PACKAGE_ARGS=") + packageArgumentsTmp + newLine
			+ std::string("export CAF_ATTACHMENT_URIS=") + attachmentUris + newLine
			+ startupAttachmentFile + " " + startupArgument + newLine;
		installInvoker = FileSystemUtils::buildPath(outputDir, "CafInstallInvoker");
#endif

		const uint32 exeByOwner = 0100;
		const uint32 writeByOwner = 0200;
		const uint32 readByOwner = 0400;
		const uint32 fileMode = exeByOwner | writeByOwner | readByOwner;

		FileSystemUtils::saveTextFile(installInvoker, fileContents);
		FileSystemUtils::chmod(installInvoker, fileMode);
		FileSystemUtils::chmod(startupAttachmentFile, fileMode);
		FileSystemUtils::chmod(packageAttachmentFile, fileMode);
	}
	CAF_CM_EXIT;

	return installInvoker;
}

void CPackageExecutor::runInstallInvoker(
	const std::string& installInvoker,
	const std::string& outputDir) {
	CAF_CM_STATIC_FUNC_LOG("CPackageExecutor", "runInstallInvoker");

	CAF_CM_ENTER
	{
		CAF_CM_VALIDATE_STRING(installInvoker);
		CAF_CM_VALIDATE_STRING(outputDir);

		if (!FileSystemUtils::doesFileExist(installInvoker)) {
			CAF_CM_EXCEPTIONEX_VA1(FileNotFoundException, ERROR_FILE_NOT_FOUND,
				"Install invoker not found - %s", installInvoker.c_str());
		}

		const std::string stdoutPath = FileSystemUtils::buildPath(outputDir,
			_sStdoutFilename);
		const std::string stderrPath = FileSystemUtils::buildPath(outputDir,
			_sStderrFilename);

		Cdeqstr argv;
		argv.push_back(installInvoker);

		ProcessUtils::runSyncToFiles(argv, stdoutPath, stderrPath);
	}
	CAF_CM_EXIT;
}
