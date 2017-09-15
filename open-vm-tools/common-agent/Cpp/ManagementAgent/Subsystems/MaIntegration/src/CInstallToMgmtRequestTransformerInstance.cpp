/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Doc/DocXml/CafInstallRequestXml/InstallProviderJobXml.h"
#include "Doc/DocXml/CafInstallRequestXml/UninstallProviderJobXml.h"

#include "Common/IAppContext.h"
#include "Doc/CafCoreTypesDoc/CClassSpecifierDoc.h"
#include "Doc/CafCoreTypesDoc/CFullyQualifiedClassGroupDoc.h"
#include "Doc/CafCoreTypesDoc/COperationDoc.h"
#include "Doc/CafCoreTypesDoc/CParameterCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CRequestInstanceParameterDoc.h"
#include "Doc/CafCoreTypesDoc/CRequestParameterDoc.h"
#include "Doc/CafInstallRequestDoc/CGetInventoryJobDoc.h"
#include "Doc/CafInstallRequestDoc/CInstallProviderJobDoc.h"
#include "Doc/CafInstallRequestDoc/CInstallRequestDoc.h"
#include "Doc/CafInstallRequestDoc/CUninstallProviderJobDoc.h"
#include "Doc/MgmtRequestDoc/CMgmtRequestDoc.h"
#include "Doc/MgmtTypesDoc/CMgmtBatchDoc.h"
#include "Doc/MgmtTypesDoc/CMgmtCollectInstancesCollectionDoc.h"
#include "Doc/MgmtTypesDoc/CMgmtCollectInstancesDoc.h"
#include "Doc/MgmtTypesDoc/CMgmtInvokeOperationCollectionDoc.h"
#include "Doc/MgmtTypesDoc/CMgmtInvokeOperationDoc.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Xml/XmlUtils/CXmlElement.h"
#include "Exception/CCafException.h"
#include "CInstallToMgmtRequestTransformerInstance.h"
#include "Integration/Caf/CCafMessagePayloadParser.h"
#include "Integration/Caf/CCafMessageCreator.h"

using namespace Caf;

CInstallToMgmtRequestTransformerInstance::CInstallToMgmtRequestTransformerInstance() :
	_isInitialized(false),
	_fileAliasPrefix("installFileAlias_"),
	CAF_CM_INIT_LOG("CInstallToMgmtRequestTransformerInstance") {
}

CInstallToMgmtRequestTransformerInstance::~CInstallToMgmtRequestTransformerInstance() {
}

void CInstallToMgmtRequestTransformerInstance::initialize(
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

std::string CInstallToMgmtRequestTransformerInstance::getId() const {
	CAF_CM_FUNCNAME_VALIDATE("getId");

	std::string rc;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		rc = _id;
	}
	CAF_CM_EXIT;

	return rc;
}

void CInstallToMgmtRequestTransformerInstance::wire(
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

SmartPtrIIntMessage CInstallToMgmtRequestTransformerInstance::transformMessage(
	const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME_VALIDATE("transformMessage");

	SmartPtrIIntMessage newMessage;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		const SmartPtrCInstallRequestDoc installRequestDoc =
				CCafMessagePayloadParser::getInstallRequest(message->getPayload());

		const SmartPtrCMgmtCollectInstancesCollectionDoc mgmtCollectInstancesCollection = createMgmtCollectInstancesCollection(
			installRequestDoc->getBatch()->getGetInventory());

		const SmartPtrCMgmtInvokeOperationCollectionDoc mgmtInvokeOperationCollection = createMgmtInvokeOperationCollection(
			installRequestDoc->getBatch()->getInstallProvider(), installRequestDoc->getBatch()->getUninstallProvider());

		SmartPtrCMgmtBatchDoc mgmtBatch;
		mgmtBatch.CreateInstance();
		mgmtBatch->initialize(
			SmartPtrCMgmtCollectSchemaDoc(),
			mgmtCollectInstancesCollection,
			mgmtInvokeOperationCollection);

		SmartPtrCMgmtRequestDoc mgmtRequest;
		mgmtRequest.CreateInstance();
		mgmtRequest->initialize(
			installRequestDoc->getClientId(),
			installRequestDoc->getRequestId(),
			installRequestDoc->getPmeId(),
			installRequestDoc->getRequestHeader(),
			mgmtBatch,
			installRequestDoc->getAttachmentCollection());

		newMessage = CCafMessageCreator::create(mgmtRequest, message->getHeaders());
	}
	CAF_CM_EXIT;

	return newMessage;
}

SmartPtrCMgmtCollectInstancesCollectionDoc CInstallToMgmtRequestTransformerInstance::createMgmtCollectInstancesCollection(
	const SmartPtrCGetInventoryJobDoc& getInventoryJob) const {
	CAF_CM_FUNCNAME_VALIDATE("createMgmtCollectInstancesCollection");

	SmartPtrCMgmtCollectInstancesCollectionDoc mgmtCollectInstancesCollection;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		// getInventoryJob is optional

		if (! getInventoryJob.IsNull()) {
			UUID jobId = getInventoryJob->getJobId();
			const SmartPtrCMgmtCollectInstancesDoc mgmtCollectInstances =
				createCollectInstances(jobId);

			std::deque<SmartPtrCMgmtCollectInstancesDoc> mgmtCollectInstancesCollectionInner;
			mgmtCollectInstancesCollectionInner.push_back(mgmtCollectInstances);

			mgmtCollectInstancesCollection.CreateInstance();
			mgmtCollectInstancesCollection->initialize(
				mgmtCollectInstancesCollectionInner);
		}
	}
	CAF_CM_EXIT;

	return mgmtCollectInstancesCollection;
}

SmartPtrCMgmtInvokeOperationCollectionDoc CInstallToMgmtRequestTransformerInstance::createMgmtInvokeOperationCollection(
	const SmartPtrCInstallProviderJobDoc& installProviderJob,
	const SmartPtrCUninstallProviderJobDoc& uninstallProviderJob) const {
	CAF_CM_FUNCNAME_VALIDATE("createMgmtInvokeOperationCollection");

	SmartPtrCMgmtInvokeOperationCollectionDoc mgmtInvokeOperationCollection;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		// installSetValueCollection is optional
		// installDeleteValueCollection is optional

		std::deque<SmartPtrCMgmtInvokeOperationDoc> mgmtInvokeOperationCollectionInner;

		if (! installProviderJob.IsNull()) {
			const SmartPtrCOperationDoc installProviderJobOperation = createInstallProviderOperation(installProviderJob);

			SmartPtrCMgmtInvokeOperationDoc mgmtInvokeOperation = createInvokeOperation(installProviderJobOperation);

			mgmtInvokeOperationCollectionInner.push_back(mgmtInvokeOperation);
		}

		if (! uninstallProviderJob.IsNull()) {
			const SmartPtrCOperationDoc uninstallProviderJobOperation = createUninstallProviderJobOperation(uninstallProviderJob);

			SmartPtrCMgmtInvokeOperationDoc mgmtInvokeOperation = createInvokeOperation(uninstallProviderJobOperation);

			mgmtInvokeOperationCollectionInner.push_back(mgmtInvokeOperation);
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

SmartPtrCMgmtCollectInstancesDoc CInstallToMgmtRequestTransformerInstance::createCollectInstances(
	const UUID& jobId) const {
	CAF_CM_FUNCNAME_VALIDATE("createCollectInstances");

	SmartPtrCMgmtCollectInstancesDoc mgmtCollectInstances;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_GUID(jobId);

		SmartPtrCFullyQualifiedClassGroupDoc fullyQualifiedClass;
		fullyQualifiedClass.CreateInstance();
		fullyQualifiedClass->initialize("caf", "InstallActions", "1.0.0");

		SmartPtrCClassSpecifierDoc classSpecifier;
		classSpecifier.CreateInstance();
		classSpecifier->initialize(fullyQualifiedClass, SmartPtrCClassFiltersDoc());

		SmartPtrCParameterCollectionDoc parameterCollection;
		parameterCollection.CreateInstance();
		parameterCollection->initialize(
			std::deque<SmartPtrCRequestParameterDoc>(),
			std::deque<SmartPtrCRequestInstanceParameterDoc>());

		mgmtCollectInstances.CreateInstance();
		mgmtCollectInstances->initialize(jobId, classSpecifier, parameterCollection);
	}
	CAF_CM_EXIT;

	return mgmtCollectInstances;
}

SmartPtrCOperationDoc CInstallToMgmtRequestTransformerInstance::createInstallProviderOperation(
	const SmartPtrCInstallProviderJobDoc& installProviderJob) const {
	CAF_CM_FUNCNAME_VALIDATE("createInstallProviderOperation");

	SmartPtrCOperationDoc operation;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_SMARTPTR(installProviderJob);

		const std::string installProviderJobStr =
			saveInstallProviderJobToString(installProviderJob);
		std::deque<std::string> valueCollection;
		valueCollection.push_back(installProviderJobStr);

		SmartPtrCRequestInstanceParameterDoc instanceParameter;
		instanceParameter.CreateInstance();
		instanceParameter->initialize(
			"installProviderJob", "caf", "InstallProviderJob", "1.0.0", valueCollection);

		std::deque<SmartPtrCRequestInstanceParameterDoc> instanceParameterCollectionInner;
		instanceParameterCollectionInner.push_back(instanceParameter);

		SmartPtrCParameterCollectionDoc parameterCollection;
		parameterCollection.CreateInstance();
		parameterCollection->initialize(
			std::deque<SmartPtrCRequestParameterDoc>(), instanceParameterCollectionInner);

		operation.CreateInstance();
		operation->initialize("installProviderJob", parameterCollection);
	}
	CAF_CM_EXIT;

	return operation;
}

SmartPtrCOperationDoc CInstallToMgmtRequestTransformerInstance::createUninstallProviderJobOperation(
	const SmartPtrCUninstallProviderJobDoc& uninstallProviderJob) const {
	CAF_CM_FUNCNAME_VALIDATE("createUninstallProviderJobOperation");

	SmartPtrCOperationDoc operation;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_SMARTPTR(uninstallProviderJob);

		const std::string& uninstallProviderJobJobStr =
			saveUninstallProviderJobToString(uninstallProviderJob);
		std::deque<std::string> valueCollection;
		valueCollection.push_back(uninstallProviderJobJobStr);

		SmartPtrCRequestInstanceParameterDoc instanceParameter;
		instanceParameter.CreateInstance();
		instanceParameter->initialize(
			"uninstallProviderJob", "caf", "InstallProviderJob", "1.0.0", valueCollection);

		std::deque<SmartPtrCRequestInstanceParameterDoc> instanceParameterCollectionInner;
		instanceParameterCollectionInner.push_back(instanceParameter);

		SmartPtrCParameterCollectionDoc parameterCollection;
		parameterCollection.CreateInstance();
		parameterCollection->initialize(
			std::deque<SmartPtrCRequestParameterDoc>(), instanceParameterCollectionInner);

		operation.CreateInstance();
		operation->initialize("uninstallProviderJob", parameterCollection);
	}
	CAF_CM_EXIT;

	return operation;
}

SmartPtrCMgmtInvokeOperationDoc CInstallToMgmtRequestTransformerInstance::createInvokeOperation(
	const SmartPtrCOperationDoc& operation) const {
	CAF_CM_FUNCNAME("createInvokeOperation");

	SmartPtrCMgmtInvokeOperationDoc mgmtInvokeOperation;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_SMARTPTR(operation);

		SmartPtrCFullyQualifiedClassGroupDoc fullyQualifiedClass;
		fullyQualifiedClass.CreateInstance();
		fullyQualifiedClass->initialize("caf", "InstallActions", "1.0.0");

		SmartPtrCClassSpecifierDoc classSpecifier;
		classSpecifier.CreateInstance();
		classSpecifier->initialize(fullyQualifiedClass, SmartPtrCClassFiltersDoc());

		UUID jobId;
		if (S_OK != ::UuidCreate(&jobId)) {
			CAF_CM_EXCEPTIONEX_VA0(InvalidHandleException, E_UNEXPECTED,
				"Failed to create the UUID");
		}

		mgmtInvokeOperation.CreateInstance();
		mgmtInvokeOperation->initialize(jobId, classSpecifier, operation);
	}
	CAF_CM_EXIT;

	return mgmtInvokeOperation;
}

std::string CInstallToMgmtRequestTransformerInstance::saveInstallProviderJobToString(
	const SmartPtrCInstallProviderJobDoc& installProviderJob) {
	CAF_CM_STATIC_FUNC_VALIDATE("XmlRoots", "saveInstallProviderJobToString");

	std::string rc;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_SMARTPTR(installProviderJob);

		const std::string schemaNamespace = DocXmlUtils::getSchemaNamespace("fx");

		const SmartPtrCXmlElement rootXml = CXmlUtils::createRootElement(
			"cafInstallProviderJob", schemaNamespace);
		InstallProviderJobXml::add(installProviderJob, rootXml);

		rc = rootXml->saveToStringRaw();
	}
	CAF_CM_EXIT;

	return rc;
}

std::string CInstallToMgmtRequestTransformerInstance::saveUninstallProviderJobToString(
	const SmartPtrCUninstallProviderJobDoc& uninstallProviderJob) {
	CAF_CM_STATIC_FUNC_VALIDATE("XmlRoots", "saveUninstallProviderJobToString");

	std::string rc;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_SMARTPTR(uninstallProviderJob);

		const std::string schemaNamespace = DocXmlUtils::getSchemaNamespace("fx");

		const SmartPtrCXmlElement rootXml = CXmlUtils::createRootElement(
			"cafUninstallProviderJob", schemaNamespace);
		UninstallProviderJobXml::add(uninstallProviderJob, rootXml);

		rc = rootXml->saveToStringRaw();
	}
	CAF_CM_EXIT;

	return rc;
}
