/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Doc/DocXml/ProviderRequestXml/ProviderRequestXmlRoots.h"

#include "Integration/Caf/CCafMessageHeaders.h"
#include "Common/CLoggingSetter.h"
#include "Doc/ProviderRequestDoc/CProviderCollectSchemaRequestDoc.h"
#include "Doc/ResponseDoc/CResponseDoc.h"
#include "Integration/IIntMessage.h"
#include "CCollectSchemaExecutor.h"
#include "CResponseFactory.h"
#include "Integration/Caf/CCafMessageCreator.h"

using namespace Caf;

CCollectSchemaExecutor::CCollectSchemaExecutor() :
	_isInitialized(false),
	CAF_CM_INIT_LOG("CCollectSchemaExecutor") {
}

CCollectSchemaExecutor::~CCollectSchemaExecutor()
{
}

void CCollectSchemaExecutor::initializeBean(
	const IBean::Cargs& ctorArgs,
	const IBean::Cprops& properties) {

	CAF_CM_FUNCNAME_VALIDATE("initializeBean");

	CAF_CM_ENTER
	{
		CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STL_EMPTY(ctorArgs);
		CAF_CM_VALIDATE_STL_EMPTY(properties);

		const std::string schemaCacheDirPath =
			AppConfigUtils::getRequiredString(_sProviderHostArea, _sConfigSchemaCacheDir);
		const std::string schemaCacheDirPathExp = CStringUtils::expandEnv(schemaCacheDirPath);
		if (!FileSystemUtils::doesDirectoryExist(schemaCacheDirPathExp)) {
			CAF_CM_LOG_INFO_VA1(
				"Schema cache directory does not exist... creating - %s",
				schemaCacheDirPathExp.c_str());
			FileSystemUtils::createDirectory(schemaCacheDirPathExp);
		}

		_schemaCacheDirPath = schemaCacheDirPathExp;
		_isInitialized = true;
	}
	CAF_CM_EXIT;
}

void CCollectSchemaExecutor::terminateBean() {

}

SmartPtrIIntMessage CCollectSchemaExecutor::processMessage(
	const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME_VALIDATE("processMessage");

	SmartPtrIIntMessage newMessage;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_INTERFACE(message);

		CAF_CM_LOG_DEBUG_VA1("Called - schemaCacheDirPath: %s", _schemaCacheDirPath.c_str());

		const SmartPtrCCafMessageHeaders cafMessageHeaders =
				CCafMessageHeaders::create(message->getHeaders());

		const std::string configOutputDir =
			AppConfigUtils::getRequiredString(_sConfigOutputDir);
		const std::string relDirectory = cafMessageHeaders->getRelDirectory();
		const std::string outputDir = FileSystemUtils::buildPath(
			configOutputDir, _sProviderHostArea, relDirectory);

		SmartPtrCLoggingSetter loggingSetter;
		loggingSetter.CreateInstance();
		loggingSetter->initialize(outputDir);

		const std::string providerCollectSchemaMem = message->getPayloadStr();
		const SmartPtrCProviderCollectSchemaRequestDoc providerCollectSchemaRequest =
			XmlRoots::parseProviderCollectSchemaRequestFromString(providerCollectSchemaMem);

		CAF_CM_LOG_DEBUG_VA2("Copying directory from \"%s\" to \"%s\"",
			_schemaCacheDirPath.c_str(), outputDir.c_str());
		FileSystemUtils::recursiveCopyDirectory(_schemaCacheDirPath, outputDir);

		const SmartPtrCResponseDoc response =
			CResponseFactory::createResponse(providerCollectSchemaRequest, outputDir,
				_schemaCacheDirPath);

		const std::string randomUuidStr = CStringUtils::createRandomUuid();
		const std::string relFilename = randomUuidStr + "_" + _sResponseFilename;

		newMessage = CCafMessageCreator::createPayloadEnvelope(
				response, relFilename, message->getHeaders());
	}
	CAF_CM_EXIT;

	return newMessage;
}
