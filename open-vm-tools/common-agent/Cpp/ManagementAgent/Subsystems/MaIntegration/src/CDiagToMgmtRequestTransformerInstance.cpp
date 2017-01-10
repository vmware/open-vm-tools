/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Doc/DocXml/DiagRequestXml/DiagRequestXmlRoots.h"

#include "CEnvelopeToPayloadTransformerInstance.h"
#include "Common/IAppContext.h"
#include "Doc/CafCoreTypesDoc/CClassSpecifierDoc.h"
#include "Doc/CafCoreTypesDoc/CFullyQualifiedClassGroupDoc.h"
#include "Doc/CafCoreTypesDoc/COperationDoc.h"
#include "Doc/CafCoreTypesDoc/CParameterCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CPropertyDoc.h"
#include "Doc/CafCoreTypesDoc/CRequestInstanceParameterDoc.h"
#include "Doc/CafCoreTypesDoc/CRequestParameterDoc.h"
#include "Doc/DiagRequestDoc/CDiagRequestDoc.h"
#include "Doc/DiagTypesDoc/CDiagCollectInstancesDoc.h"
#include "Doc/DiagTypesDoc/CDiagDeleteValueCollectionDoc.h"
#include "Doc/DiagTypesDoc/CDiagDeleteValueDoc.h"
#include "Doc/DiagTypesDoc/CDiagSetValueCollectionDoc.h"
#include "Doc/DiagTypesDoc/CDiagSetValueDoc.h"
#include "Doc/MgmtRequestDoc/CMgmtRequestDoc.h"
#include "Doc/MgmtTypesDoc/CMgmtBatchDoc.h"
#include "Doc/MgmtTypesDoc/CMgmtCollectInstancesCollectionDoc.h"
#include "Doc/MgmtTypesDoc/CMgmtCollectInstancesDoc.h"
#include "Doc/MgmtTypesDoc/CMgmtInvokeOperationCollectionDoc.h"
#include "Doc/MgmtTypesDoc/CMgmtInvokeOperationDoc.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "CDiagToMgmtRequestTransformerInstance.h"
#include "Exception/CCafException.h"
#include "Integration/Caf/CCafMessageCreator.h"

using namespace Caf;

CDiagToMgmtRequestTransformerInstance::CDiagToMgmtRequestTransformerInstance() :
	_isInitialized(false),
	_fileAliasPrefix("diagFileAlias_"),
	CAF_CM_INIT_LOG("CDiagToMgmtRequestTransformerInstance") {
}

CDiagToMgmtRequestTransformerInstance::~CDiagToMgmtRequestTransformerInstance() {
}

void CDiagToMgmtRequestTransformerInstance::initialize(
	const IBean::Cargs& ctorArgs,
	const IBean::Cprops& properties,
	const SmartPtrIDocument& configSection) {

	CAF_CM_FUNCNAME_VALIDATE("initialize");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_INTERFACE(configSection);

		_id = configSection->findRequiredAttribute("id");

		_isInitialized = true;
	}
	CAF_CM_EXIT;
}

std::string CDiagToMgmtRequestTransformerInstance::getId() const {
	CAF_CM_FUNCNAME_VALIDATE("getId");

	std::string rc;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		rc = _id;
	}
	CAF_CM_EXIT;

	return rc;
}

void CDiagToMgmtRequestTransformerInstance::wire(
	const SmartPtrIAppContext& appContext,
	const SmartPtrIChannelResolver& channelResolver) {
	CAF_CM_FUNCNAME_VALIDATE("wire");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_INTERFACE(appContext);
		CAF_CM_VALIDATE_INTERFACE(channelResolver);

	}
	CAF_CM_EXIT;
}

SmartPtrIIntMessage CDiagToMgmtRequestTransformerInstance::transformMessage(
	const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME_VALIDATE("transformMessage");

	SmartPtrIIntMessage newMessage;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		const std::string diagRequestXml = message->getPayloadStr();

		CAF_CM_LOG_DEBUG_VA1("diagRequestXml - %s", diagRequestXml.c_str());

		const SmartPtrCDiagRequestDoc diagRequestDoc =
			XmlRoots::parseDiagRequestFromString(diagRequestXml);

		const SmartPtrCMgmtCollectInstancesCollectionDoc mgmtCollectInstancesCollection = createMgmtCollectInstancesCollection(
			diagRequestDoc->getBatch()->getCollectInstances());

		const SmartPtrCMgmtInvokeOperationCollectionDoc mgmtInvokeOperationCollection = createMgmtInvokeOperationCollection(
			diagRequestDoc->getBatch()->getSetValueCollection(), diagRequestDoc->getBatch()->getDeleteValueCollection());

		SmartPtrCMgmtBatchDoc mgmtBatch;
		mgmtBatch.CreateInstance();
		mgmtBatch->initialize(
			SmartPtrCMgmtCollectSchemaDoc(),
			mgmtCollectInstancesCollection,
			mgmtInvokeOperationCollection);

		SmartPtrCMgmtRequestDoc mgmtRequest;
		mgmtRequest.CreateInstance();
		mgmtRequest->initialize(
			diagRequestDoc->getClientId(),
			diagRequestDoc->getRequestId(),
			diagRequestDoc->getPmeId(),
			diagRequestDoc->getRequestHeader(),
			mgmtBatch,
			SmartPtrCAttachmentCollectionDoc());

		newMessage = CCafMessageCreator::create(mgmtRequest, message->getHeaders());
	}
	CAF_CM_EXIT;

	return newMessage;
}

SmartPtrCMgmtCollectInstancesCollectionDoc CDiagToMgmtRequestTransformerInstance::createMgmtCollectInstancesCollection(
	const SmartPtrCDiagCollectInstancesDoc& diagCollectInstances) const {
	CAF_CM_FUNCNAME_VALIDATE("createMgmtCollectInstancesCollection");

	SmartPtrCMgmtCollectInstancesCollectionDoc mgmtCollectInstancesCollection;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		// diagCollectInstances is optional

		if (! diagCollectInstances.IsNull()) {
			std::deque<SmartPtrCMgmtCollectInstancesDoc> mgmtCollectInstancesCollectionInner;

			UUID jobId = diagCollectInstances->getJobId();
			const std::deque<SmartPtrSExpandedFileAlias> expandedFileAliasCollection = expandFileAliases();
			for(TConstIterator<std::deque<SmartPtrSExpandedFileAlias> > expandedFileAliasIter(expandedFileAliasCollection);
				expandedFileAliasIter; expandedFileAliasIter++) {
				const SmartPtrSExpandedFileAlias expandedFileAlias = *expandedFileAliasIter;

				const SmartPtrCMgmtCollectInstancesDoc mgmtCollectInstances =
					createCollectInstances(jobId, expandedFileAlias);

				mgmtCollectInstancesCollectionInner.push_back(mgmtCollectInstances);

				jobId = CStringUtils::createRandomUuidRaw();
			}

			mgmtCollectInstancesCollection.CreateInstance();
			mgmtCollectInstancesCollection->initialize(
				mgmtCollectInstancesCollectionInner);
		}
	}
	CAF_CM_EXIT;

	return mgmtCollectInstancesCollection;
}

SmartPtrCMgmtInvokeOperationCollectionDoc CDiagToMgmtRequestTransformerInstance::createMgmtInvokeOperationCollection(
	const SmartPtrCDiagSetValueCollectionDoc& diagSetValueCollection,
	const SmartPtrCDiagDeleteValueCollectionDoc& diagDeleteValueCollection) const {
	CAF_CM_FUNCNAME_VALIDATE("createMgmtInvokeOperationCollection");

	SmartPtrCMgmtInvokeOperationCollectionDoc mgmtInvokeOperationCollection;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		// diagSetValueCollection is optional
		// diagDeleteValueCollection is optional

		std::deque<SmartPtrCMgmtInvokeOperationDoc> mgmtInvokeOperationCollectionInner;

		if (! diagSetValueCollection.IsNull()) {
			const std::deque<SmartPtrCDiagSetValueDoc> diagSetValueCollectionInner =
				diagSetValueCollection->getSetValueCollection();
			for(TConstIterator<std::deque<SmartPtrCDiagSetValueDoc> > diagSetValueIter(diagSetValueCollectionInner);
				diagSetValueIter; diagSetValueIter++) {
				const SmartPtrCDiagSetValueDoc diagSetValue = *diagSetValueIter;

				const std::string fileAlias = diagSetValue->getFileAlias();
				const SmartPtrSExpandedFileAlias expandedFileAlias = expandFileAlias(fileAlias);

				const SmartPtrCPropertyDoc valueProperty = diagSetValue->getValue();
				const SmartPtrCOperationDoc setValueOperation = createSetValueOperation(
					valueProperty->getName(), valueProperty->getValue(), expandedFileAlias);

				SmartPtrCMgmtInvokeOperationDoc mgmtInvokeOperation = createInvokeOperation(
					diagSetValue->getJobId(), setValueOperation);

				mgmtInvokeOperationCollectionInner.push_back(mgmtInvokeOperation);
			}
		}

		if (! diagDeleteValueCollection.IsNull()) {
			const std::deque<SmartPtrCDiagDeleteValueDoc> diagDeleteValueCollectionInner =
				diagDeleteValueCollection->getDeleteValueCollection();
			for(TConstIterator<std::deque<SmartPtrCDiagDeleteValueDoc> > diagDeleteValueIter(diagDeleteValueCollectionInner);
				diagDeleteValueIter; diagDeleteValueIter++) {
				const SmartPtrCDiagDeleteValueDoc diagDeleteValue = *diagDeleteValueIter;

				const std::string fileAlias = diagDeleteValue->getFileAlias();
				const SmartPtrSExpandedFileAlias expandedFileAlias = expandFileAlias(fileAlias);

				const SmartPtrCOperationDoc setValueOperation = createDeleteValueOperation(
					diagDeleteValue->getValueName(), expandedFileAlias);

				SmartPtrCMgmtInvokeOperationDoc mgmtInvokeOperation = createInvokeOperation(
					diagDeleteValue->getJobId(), setValueOperation);

				mgmtInvokeOperationCollectionInner.push_back(mgmtInvokeOperation);
			}
		}

		if (! mgmtInvokeOperationCollectionInner.empty()) {
			mgmtInvokeOperationCollection.CreateInstance();
			mgmtInvokeOperationCollection->initialize(
				mgmtInvokeOperationCollectionInner);
		}
	}
	CAF_CM_EXIT;

	return mgmtInvokeOperationCollection;
}

SmartPtrCMgmtCollectInstancesDoc CDiagToMgmtRequestTransformerInstance::createCollectInstances(
	const UUID& jobId,
	const SmartPtrSExpandedFileAlias& expandedFileAlias) const {
	CAF_CM_FUNCNAME_VALIDATE("createCollectInstances");

	SmartPtrCMgmtCollectInstancesDoc mgmtCollectInstances;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_GUID(jobId);
		CAF_CM_VALIDATE_SMARTPTR(expandedFileAlias);

		SmartPtrCFullyQualifiedClassGroupDoc fullyQualifiedClass;
		fullyQualifiedClass.CreateInstance();
		fullyQualifiedClass->initialize("caf", "ConfigActions", "1.0.0");

		SmartPtrCClassSpecifierDoc classSpecifier;
		classSpecifier.CreateInstance();
		classSpecifier->initialize(fullyQualifiedClass, SmartPtrCClassFiltersDoc());

		const SmartPtrCRequestParameterDoc filePathParameter =
			ParameterUtils::createParameter("filePath", expandedFileAlias->_filePath);
		const SmartPtrCRequestParameterDoc encodingParameter =
			ParameterUtils::createParameter("encoding", expandedFileAlias->_encoding);

		std::deque<SmartPtrCRequestParameterDoc> parameterCollectionInner;
		parameterCollectionInner.push_back(filePathParameter);
		parameterCollectionInner.push_back(encodingParameter);

		SmartPtrCParameterCollectionDoc parameterCollection;
		parameterCollection.CreateInstance();
		parameterCollection->initialize(
			parameterCollectionInner, std::deque<SmartPtrCRequestInstanceParameterDoc>());

		mgmtCollectInstances.CreateInstance();
		mgmtCollectInstances->initialize(jobId, classSpecifier, parameterCollection);
	}
	CAF_CM_EXIT;

	return mgmtCollectInstances;
}

SmartPtrCOperationDoc CDiagToMgmtRequestTransformerInstance::createSetValueOperation(
	const std::string& valueName,
	const std::deque<std::string>& valueCollection,
	const SmartPtrSExpandedFileAlias& expandedFileAlias) const {
	CAF_CM_FUNCNAME_VALIDATE("createSetValueOperation");

	SmartPtrCOperationDoc operation;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(valueName);
		CAF_CM_VALIDATE_STL(valueCollection);
		CAF_CM_VALIDATE_SMARTPTR(expandedFileAlias);

		const SmartPtrCRequestParameterDoc filePathParameter =
			ParameterUtils::createParameter("filePath", expandedFileAlias->_filePath);
		const SmartPtrCRequestParameterDoc encodingParameter =
			ParameterUtils::createParameter("encoding", expandedFileAlias->_encoding);
		const SmartPtrCRequestParameterDoc valueNameParameter =
			ParameterUtils::createParameter("valueName", valueName);
		const SmartPtrCRequestParameterDoc valueDataParameter =
			ParameterUtils::createParameter("valueData", valueCollection);

		std::deque<SmartPtrCRequestParameterDoc> parameterCollectionInner;
		parameterCollectionInner.push_back(filePathParameter);
		parameterCollectionInner.push_back(encodingParameter);
		parameterCollectionInner.push_back(valueNameParameter);
		parameterCollectionInner.push_back(valueDataParameter);

		SmartPtrCParameterCollectionDoc parameterCollection;
		parameterCollection.CreateInstance();
		parameterCollection->initialize(
			parameterCollectionInner, std::deque<SmartPtrCRequestInstanceParameterDoc>());

		operation.CreateInstance();
		operation->initialize("setValue", parameterCollection);
	}
	CAF_CM_EXIT;

	return operation;
}

SmartPtrCOperationDoc CDiagToMgmtRequestTransformerInstance::createDeleteValueOperation(
	const std::string& valueName,
	const SmartPtrSExpandedFileAlias& expandedFileAlias) const {
	CAF_CM_FUNCNAME_VALIDATE("createDeleteValueOperation");

	SmartPtrCOperationDoc operation;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(valueName);
		CAF_CM_VALIDATE_SMARTPTR(expandedFileAlias);

		const SmartPtrCRequestParameterDoc filePathParameter =
			ParameterUtils::createParameter("filePath", expandedFileAlias->_filePath);
		const SmartPtrCRequestParameterDoc encodingParameter =
			ParameterUtils::createParameter("encoding", expandedFileAlias->_encoding);
		const SmartPtrCRequestParameterDoc valueNameParameter =
			ParameterUtils::createParameter("valueName", valueName);

		std::deque<SmartPtrCRequestParameterDoc> parameterCollectionInner;
		parameterCollectionInner.push_back(filePathParameter);
		parameterCollectionInner.push_back(encodingParameter);
		parameterCollectionInner.push_back(valueNameParameter);

		SmartPtrCParameterCollectionDoc parameterCollection;
		parameterCollection.CreateInstance();
		parameterCollection->initialize(
			parameterCollectionInner, std::deque<SmartPtrCRequestInstanceParameterDoc>());

		operation.CreateInstance();
		operation->initialize("deleteValue", parameterCollection);
	}
	CAF_CM_EXIT;

	return operation;
}

SmartPtrCMgmtInvokeOperationDoc CDiagToMgmtRequestTransformerInstance::createInvokeOperation(
	const UUID& jobId,
	const SmartPtrCOperationDoc operation) const {
	CAF_CM_FUNCNAME_VALIDATE("createInvokeOperation");

	SmartPtrCMgmtInvokeOperationDoc mgmtInvokeOperation;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_GUID(jobId);
		CAF_CM_VALIDATE_SMARTPTR(operation);

		SmartPtrCFullyQualifiedClassGroupDoc fullyQualifiedClass;
		fullyQualifiedClass.CreateInstance();
		fullyQualifiedClass->initialize("caf", "ConfigActions", "1.0.0");

		SmartPtrCClassSpecifierDoc classSpecifier;
		classSpecifier.CreateInstance();
		classSpecifier->initialize(fullyQualifiedClass, SmartPtrCClassFiltersDoc());

		mgmtInvokeOperation.CreateInstance();
		mgmtInvokeOperation->initialize(jobId, classSpecifier, operation);
	}
	CAF_CM_EXIT;

	return mgmtInvokeOperation;
}

std::deque<CDiagToMgmtRequestTransformerInstance::SmartPtrSExpandedFileAlias>
CDiagToMgmtRequestTransformerInstance::expandFileAliases() const {
	CAF_CM_FUNCNAME_VALIDATE("expandFileAliases");

	std::deque<SmartPtrSExpandedFileAlias> expandedFileAliasCollection;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		const std::string diagFileAliases = AppConfigUtils::getRequiredString("provider", "diagFileAliases");
		const Cdeqstr diagFileAliasCollection = CStringUtils::split(diagFileAliases, ':');

		for(TConstIterator<Cdeqstr> diagFileAliasIter(diagFileAliasCollection);
			diagFileAliasIter; diagFileAliasIter++) {
			const std::string fileAlias = *diagFileAliasIter;

			const SmartPtrSExpandedFileAlias expandedFileAlias = expandFileAlias(fileAlias);
			if (! expandedFileAlias.IsNull()) {
				expandedFileAliasCollection.push_back(expandedFileAlias);
			}
		}
	}
	CAF_CM_EXIT;

	return expandedFileAliasCollection;
}

CDiagToMgmtRequestTransformerInstance::SmartPtrSExpandedFileAlias
CDiagToMgmtRequestTransformerInstance::expandFileAlias(
	const std::string& fileAlias) const {
	CAF_CM_FUNCNAME_VALIDATE("expandFileAlias");

	SmartPtrSExpandedFileAlias expandedFileAlias;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		const std::string fullFileAlias = _fileAliasPrefix + fileAlias;
		const std::string diagFileUriStr = AppConfigUtils::getOptionalString("provider", fullFileAlias);

		if (diagFileUriStr.empty()) {
			CAF_CM_LOG_WARN_VA1(
				"Diag file alias not found in appconfig file - alias: %s", fullFileAlias.c_str());
		} else {
			UriUtils::SUriRecord diagFileUri;
			UriUtils::parseUriString(diagFileUriStr, diagFileUri);
			if (diagFileUri.protocol.compare("file") != 0) {
				CAF_CM_LOG_WARN_VA2(
					"Diag file alias URI must use \'file\' protocol - alias: %s, uri: %s",
					fullFileAlias.c_str(), diagFileUriStr.c_str())
			} else {
				UriUtils::SFileUriRecord fileUriRecord;
				UriUtils::parseFileAddress(diagFileUri.address, fileUriRecord);

				const std::string diagFile = fileUriRecord.path;
				if (! FileSystemUtils::doesFileExist(diagFile)) {
					CAF_CM_LOG_WARN_VA2(
						"Diag file alias file not found - alias: %s, file: %s",
						fullFileAlias.c_str(), diagFile.c_str());
				} else {
					expandedFileAlias.CreateInstance();
					expandedFileAlias->_filePath = diagFile;
					expandedFileAlias->_encoding = findUriParameter("encoding", diagFileUri);
				}
			}
		}
	}
	CAF_CM_EXIT;

	return expandedFileAlias;
}

std::string CDiagToMgmtRequestTransformerInstance::findUriParameter(
	const std::string& parameterName,
	const UriUtils::SUriRecord& uri) const {
	CAF_CM_FUNCNAME("findUriParameter");

	std::string parameterValue;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(parameterName);

		std::map<std::string, std::string> parameters = uri.parameters;
		std::map<std::string, std::string>::const_iterator iterParameter = parameters.find(parameterName);
		if (iterParameter == parameters.end()) {
			CAF_CM_EXCEPTIONEX_VA3(InvalidArgumentException, E_INVALIDARG,
				"URI does not contain required parameter - parameter: %s, protocol: %s, address: %s",
				parameterName.c_str(), uri.protocol.c_str(), uri.address.c_str());
		}

		parameterValue = iterParameter->second;
	}
	CAF_CM_EXIT;

	return parameterValue;
}
