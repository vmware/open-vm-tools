/*
 *	Author: bwilliams
 *	Created: May 3, 2012
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Doc/CafCoreTypesDoc/CAttachmentCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CAttachmentDoc.h"
#include "Doc/CafCoreTypesDoc/COperationDoc.h"
#include "Doc/CafCoreTypesDoc/CParameterCollectionDoc.h"
#include "Doc/ProviderRequestDoc/CProviderInvokeOperationDoc.h"
#include "Doc/ProviderResultsDoc/CSchemaDoc.h"
#include "Doc/SchemaTypesDoc/CActionClassDoc.h"
#include "Doc/SchemaTypesDoc/CDataClassDoc.h"
#include "Doc/SchemaTypesDoc/CMethodDoc.h"
#include "Doc/SchemaTypesDoc/CMethodParameterDoc.h"
#include "Exception/CCafException.h"
#include "CRemoteCommandProvider.h"
#include "IProviderRequest.h"

using namespace Caf;

CRemoteCommandProvider::CRemoteCommandProvider() :
	CAF_CM_INIT_LOG("CRemoteCommandProvider") {
}

CRemoteCommandProvider::~CRemoteCommandProvider() {
}

const SmartPtrCSchemaDoc CRemoteCommandProvider::getSchema() const {
	std::deque<SmartPtrCMethodParameterDoc> m1Params;
	m1Params.push_back(CProviderDocHelper::createMethodParameter("scriptAttachmentName", PARAMETER_STRING, false));
	m1Params.push_back(CProviderDocHelper::createMethodParameter("scriptParameters", PARAMETER_STRING, false));
	m1Params.push_back(CProviderDocHelper::createMethodParameter("attachmentNames", PARAMETER_STRING, false));

	std::deque<SmartPtrCMethodParameterDoc> m2Params;
	m2Params.push_back(CProviderDocHelper::createMethodParameter("inlineScript", PARAMETER_STRING, false));
	m2Params.push_back(CProviderDocHelper::createMethodParameter("scriptParameters", PARAMETER_STRING, true, true));
	m2Params.push_back(CProviderDocHelper::createMethodParameter("attachmentNames", PARAMETER_STRING, true, true));

	std::deque<SmartPtrCMethodDoc> methods;
	methods.push_back(CProviderDocHelper::createMethod("executeScript", m1Params));
	methods.push_back(CProviderDocHelper::createMethod("executeInlineScript", m2Params));

	std::deque<SmartPtrCActionClassDoc> actionClasses;
	actionClasses.push_back(
			CProviderDocHelper::createActionClass(
			"caf",
			"RemoteCommandActions",
			"1.0.0",
			SmartPtrCCollectMethodDoc(),
			methods));

	return CProviderDocHelper::createSchema(std::deque<SmartPtrCDataClassDoc>(), actionClasses);
}

void CRemoteCommandProvider::collect(const IProviderRequest& request, IProviderResponse& response) const {
	CAF_CM_FUNCNAME("collect");

	CAF_CM_EXCEPTIONEX_VA0(UnsupportedOperationException, E_NOTIMPL,
		"Collect not implemented for Remote Commands")
}

void CRemoteCommandProvider::invoke(const IProviderRequest& request, IProviderResponse& response) const {
	CAF_CM_FUNCNAME("invoke");

	SmartPtrCProviderInvokeOperationDoc doc = request.getInvokeOperations();
	CAF_CM_VALIDATE_SMARTPTR(doc);

	const SmartPtrCOperationDoc operation = doc->getOperation();
	const std::string operationName = operation->getName();

	const SmartPtrCParameterCollectionDoc parameterCollection =
		operation->getParameterCollection();

	const std::string outputDir = doc->getOutputDir();
	const std::string scriptResultsDir =
		FileSystemUtils::buildPath(outputDir, "scriptResults");
	if (! FileSystemUtils::doesDirectoryExist(scriptResultsDir)) {
		FileSystemUtils::createDirectory(scriptResultsDir);
	}

	const std::deque<std::string> scriptParameters =
		ParameterUtils::findOptionalParameterAsStringCollection("scriptParameters",
			parameterCollection);
	const std::deque<std::string> attachmentNames =
		ParameterUtils::findOptionalParameterAsStringCollection("attachmentNames",
			parameterCollection);

	const std::string attachmentUris = createAttachmentUris(
		attachmentNames, request.getAttachments());

	std::string scriptPath;
	if (operationName.compare("executeScript") == 0) {
		const std::string scriptAttachmentName =
			ParameterUtils::findRequiredParameterAsString("scriptAttachmentName",
				parameterCollection);

		SmartPtrCAttachmentDoc scriptAttachment = AttachmentUtils::findRequiredAttachment(
			scriptAttachmentName, request.getAttachments());
		CAF_CM_VALIDATE_BOOL(scriptAttachment->getIsReference() == FALSE);
		const std::string attachmentUri = scriptAttachment->getUri();

		UriUtils::SUriRecord uriRecord;
		UriUtils::parseUriString(attachmentUri, uriRecord);

		CAF_CM_LOG_DEBUG_VA3("Parsed URI - Uri: %s, protocol: %s, address: %s",
			attachmentUri.c_str(), uriRecord.protocol.c_str(),
			uriRecord.address.c_str());

		CAF_CM_VALIDATE_BOOL(uriRecord.protocol.compare("file") == 0);
        UriUtils::SFileUriRecord scriptFileUriRecord;
        UriUtils::parseFileAddress(uriRecord.address, scriptFileUriRecord);
		scriptPath = scriptFileUriRecord.path;
	} else if (operationName.compare("executeInlineScript") == 0) {
		const std::string inlineScript =
			ParameterUtils::findRequiredParameterAsString("inlineScript",
				parameterCollection);

#ifdef WIN32
		scriptPath = FileSystemUtils::buildPath(outputDir, "script.bat");
#else
		scriptPath = FileSystemUtils::buildPath(outputDir, "_script_");
#endif
		FileSystemUtils::saveTextFile(scriptPath, inlineScript);
	} else {
		CAF_CM_EXCEPTIONEX_VA1(InvalidArgumentException, E_INVALIDARG,
			"Invalid operation name (must be \'executeScript\' or \'executeInlineScript\') - %s",
			operationName.c_str())
	}

	FileSystemUtils::chmod(scriptPath);
	executeScript(scriptPath, scriptResultsDir, scriptParameters, attachmentUris);
}

void CRemoteCommandProvider::executeScript(
	const std::string& scriptPath,
	const std::string& scriptResultsDir,
	const std::deque<std::string>& scriptParameters,
	const std::string& attachmentUris) const {
	CAF_CM_FUNCNAME_VALIDATE("executeScript");
	CAF_CM_VALIDATE_STRING(scriptPath);
	CAF_CM_VALIDATE_STRING(scriptResultsDir);
	// scriptParameters are optional
	// attachmentUris are optional

	Cdeqstr argv;
	argv.push_back(scriptPath);
	argv.push_back("-o");
	argv.push_back(scriptResultsDir);
	if (! attachmentUris.empty()) {
		argv.push_back("-u");
		argv.push_back(attachmentUris);
	}
	if (! scriptParameters.empty()) {
		for(TConstIterator<Cdeqstr> scriptParameterIter(scriptParameters);
			scriptParameterIter; scriptParameterIter++) {
			argv.push_back(*scriptParameterIter);
		}
	}

	const std::string stdoutPath = FileSystemUtils::buildPath(
		scriptResultsDir, _sStdoutFilename);
	const std::string stderrPath = FileSystemUtils::buildPath(
		scriptResultsDir, _sStderrFilename);

	ProcessUtils::runSyncToFiles(argv, stdoutPath, stderrPath);

}

std::string CRemoteCommandProvider::createAttachmentUris(
	const std::deque<std::string>& attachmentNames,
	const SmartPtrCAttachmentCollectionDoc& attachmentCollection) const {
	CAF_CM_FUNCNAME_VALIDATE("createAttachmentUris");

	std::string rc;
	if (! attachmentNames.empty()) {
		CAF_CM_VALIDATE_SMARTPTR(attachmentCollection);

        rc += "\"";
		for(TConstIterator<Cdeqstr> attachmentNameIter(attachmentNames);
			attachmentNameIter; attachmentNameIter++) {
			const std::string attachmentName = *attachmentNameIter;
			const SmartPtrCAttachmentDoc attachment =
				AttachmentUtils::findRequiredAttachment(
					attachmentName, attachmentCollection);
			rc += attachment->getName() + "^" + attachment->getUri() + "|";
		}
        rc += "\"";
	}
	return rc;
}
