/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Common/IAppContext.h"
#include "Doc/CafCoreTypesDoc/CClassSpecifierDoc.h"
#include "Doc/CafCoreTypesDoc/CFullyQualifiedClassGroupDoc.h"
#include "Doc/CafCoreTypesDoc/CRequestConfigDoc.h"
#include "Doc/CafCoreTypesDoc/CRequestHeaderDoc.h"
#include "Doc/MgmtRequestDoc/CMgmtRequestDoc.h"
#include "Doc/MgmtTypesDoc/CMgmtBatchDoc.h"
#include "Doc/MgmtTypesDoc/CMgmtCollectInstancesCollectionDoc.h"
#include "Doc/MgmtTypesDoc/CMgmtCollectInstancesDoc.h"
#include "Doc/MgmtTypesDoc/CMgmtCollectSchemaDoc.h"
#include "Doc/MgmtTypesDoc/CMgmtInvokeOperationCollectionDoc.h"
#include "Doc/MgmtTypesDoc/CMgmtInvokeOperationDoc.h"
#include "Doc/ProviderRequestDoc/CProviderBatchDoc.h"
#include "Doc/ProviderRequestDoc/CProviderCollectInstancesCollectionDoc.h"
#include "Doc/ProviderRequestDoc/CProviderCollectInstancesDoc.h"
#include "Doc/ProviderRequestDoc/CProviderCollectSchemaRequestDoc.h"
#include "Doc/ProviderRequestDoc/CProviderInvokeOperationCollectionDoc.h"
#include "Doc/ProviderRequestDoc/CProviderInvokeOperationDoc.h"
#include "Doc/ProviderRequestDoc/CProviderRequestConfigDoc.h"
#include "Doc/ProviderRequestDoc/CProviderRequestDoc.h"
#include "Doc/ProviderRequestDoc/CProviderRequestHeaderDoc.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "CSinglePmeRequestSplitterInstance.h"
#include "Exception/CCafException.h"
#include "Integration/Caf/CCafMessageCreator.h"
#include "Integration/Caf/CCafMessagePayloadParser.h"
#include "Integration/Caf/CCafMessagePayload.h"

using namespace Caf;

CSinglePmeRequestSplitterInstance::CSinglePmeRequestSplitterInstance() :
		_isInitialized(false),
		CAF_CM_INIT_LOG("CSinglePmeRequestSplitterInstance") {
}

CSinglePmeRequestSplitterInstance::~CSinglePmeRequestSplitterInstance() {
}

void CSinglePmeRequestSplitterInstance::initialize(
		const IBean::Cargs& ctorArgs,
		const IBean::Cprops& properties,
		const SmartPtrIDocument& configSection) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(configSection);

	_id = configSection->findRequiredAttribute("id");
	_schemaCacheManager.CreateInstance();
	_schemaCacheManager->initialize();

	_isInitialized = true;
}

std::string CSinglePmeRequestSplitterInstance::getId() const {
	return _id;
}

void CSinglePmeRequestSplitterInstance::wire(
		const SmartPtrIAppContext& appContext,
		const SmartPtrIChannelResolver& channelResolver) {
	CAF_CM_FUNCNAME_VALIDATE("wire");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(appContext);
	CAF_CM_VALIDATE_INTERFACE(channelResolver);
}

IMessageSplitter::SmartPtrCMessageCollection CSinglePmeRequestSplitterInstance::splitMessage(
		const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME_VALIDATE("splitMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(message);

	CAF_CM_LOG_DEBUG_VA1("Called - %s", _id.c_str());

	SmartPtrCMessageCollection messageCollection;
	messageCollection.CreateInstance();

	const SmartPtrCMgmtRequestDoc mgmtRequest =
			CCafMessagePayloadParser::getMgmtRequest(message->getPayload());

	const SmartPtrCMgmtBatchDoc mgmtBatch = mgmtRequest->getBatch();

	const SmartPtrCProviderRequestHeaderDoc providerRequestHeader =
			convertRequestHeader(mgmtRequest->getRequestHeader());

	const std::string configOutputDir = AppConfigUtils::getRequiredString(
			_sConfigOutputDir);
	const std::string randomUuidStr = CStringUtils::createRandomUuid();
	const std::string absRandomUuidDir = FileSystemUtils::buildPath(configOutputDir,
			_sProviderHostArea, randomUuidStr);

	createDirectory(absRandomUuidDir);
	saveRequest(absRandomUuidDir, message->getPayload());

	// Process the collect schema job first since it follows a
	// completely different line of execution.
	const SmartPtrCMgmtCollectSchemaDoc mgmtCollectSchema =
			mgmtBatch->getCollectSchema();
	if (!mgmtCollectSchema.IsNull()) {
		const std::string relProviderNumDir = FileSystemUtils::buildPath(randomUuidStr,
				CStringConv::toString<uint32>(0));
		const std::string absProviderNumDir = FileSystemUtils::buildPath(
				absRandomUuidDir, CStringConv::toString<uint32>(0));

		const SmartPtrCProviderCollectSchemaRequestDoc providerCollectSchemaRequest =
				createCollectSchemaRequest(mgmtRequest, mgmtCollectSchema,
						providerRequestHeader, absProviderNumDir);

		const std::string relFilename = FileSystemUtils::buildPath(relProviderNumDir,
				_sProviderRequestFilename);

		const SmartPtrIIntMessage messageNew = CCafMessageCreator::create(
				providerCollectSchemaRequest, relFilename, relProviderNumDir,
				message->getHeaders());

		messageCollection->push_back(messageNew);
	}

	SmartPtrCProviderJobsCollection providerJobsCollection;
	providerJobsCollection.CreateInstance();
	addCollectInstancesJobs(mgmtBatch->getCollectInstancesCollection(),
			providerJobsCollection);
	addInvokeOperationJobs(mgmtBatch->getInvokeOperationCollection(),
			providerJobsCollection);

	uint32 providerCnt = 1;
	for (TConstIterator<CProviderJobsCollection> providerJobsIter(
			*providerJobsCollection); providerJobsIter; providerJobsIter++) {
		const std::string providerUri = providerJobsIter->first;
		const SmartPtrCSplitterJobsCollection jobsCollection = providerJobsIter->second;

		const std::string provderCntStr = CStringConv::toString<uint32>(providerCnt);
		const std::string absProviderNumDir = FileSystemUtils::buildPath(absRandomUuidDir, provderCntStr);

		const SmartPtrCProviderRequestDoc providerRequest = createProviderRequest(
				mgmtRequest, jobsCollection, providerRequestHeader, absProviderNumDir);

		const std::string relProviderNumDir = FileSystemUtils::buildPath(randomUuidStr, provderCntStr);
//		const std::string relFilename = FileSystemUtils::buildPath(relProviderNumDir,
//				_sProviderRequestFilename);
//		const std::string relProviderNumDir = "";
		const std::string relFilename = randomUuidStr + "_" + provderCntStr + "_" + _sProviderRequestFilename;

		const SmartPtrIIntMessage messageNew = CCafMessageCreator::create(providerRequest,
				relFilename, relProviderNumDir, providerUri, message->getHeaders());

		messageCollection->push_back(messageNew);

		providerCnt++;
	}

	return messageCollection;
}

SmartPtrCProviderCollectSchemaRequestDoc CSinglePmeRequestSplitterInstance::createCollectSchemaRequest(
		const SmartPtrCMgmtRequestDoc& mgmtRequest,
		const SmartPtrCMgmtCollectSchemaDoc& mgmtCollectSchema,
		const SmartPtrCProviderRequestHeaderDoc& providerRequestHeader,
		const std::string& outputDir) const {
	CAF_CM_FUNCNAME_VALIDATE("createCollectSchemaRequest");
	CAF_CM_VALIDATE_SMARTPTR(mgmtRequest);
	CAF_CM_VALIDATE_SMARTPTR(mgmtCollectSchema);
	CAF_CM_VALIDATE_SMARTPTR(providerRequestHeader);
	CAF_CM_VALIDATE_STRING(outputDir);

	SmartPtrCProviderCollectSchemaRequestDoc providerCollectSchemaRequest;
	providerCollectSchemaRequest.CreateInstance();
	providerCollectSchemaRequest->initialize(mgmtRequest->getClientId(),
			mgmtRequest->getRequestId(), mgmtRequest->getPmeId(),
			mgmtCollectSchema->getJobId(), outputDir, providerRequestHeader);

	return providerCollectSchemaRequest;
}

SmartPtrCProviderRequestDoc CSinglePmeRequestSplitterInstance::createProviderRequest(
		const SmartPtrCMgmtRequestDoc& mgmtRequest,
		const SmartPtrCSplitterJobsCollection& jobsCollection,
		const SmartPtrCProviderRequestHeaderDoc& providerRequestHeader,
		const std::string& outputDir) const {
	CAF_CM_FUNCNAME_VALIDATE("createProviderRequest");
	CAF_CM_VALIDATE_SMARTPTR(mgmtRequest);
	CAF_CM_VALIDATE_SMARTPTR(jobsCollection);
	CAF_CM_VALIDATE_SMARTPTR(providerRequestHeader);
	CAF_CM_VALIDATE_STRING(outputDir);

	std::deque<SmartPtrCProviderCollectInstancesDoc> collectInstancesCollectionInner;
	std::deque<SmartPtrCProviderInvokeOperationDoc> invokeOperationCollectionInner;
	for (TConstIterator<std::deque<SmartPtrCSplitterJob> > jobIter(*jobsCollection);
			jobIter; jobIter++) {
		const SmartPtrCSplitterJob job = *jobIter;

		if (!job->_mgmtCollectInstances.IsNull()) {
			const UUID jobId = job->_mgmtCollectInstances->getJobId();
			const std::string jobIdStr = BasePlatform::UuidToString(jobId);
			const std::string jobOutputDir = FileSystemUtils::buildPath(outputDir,
					jobIdStr);
			if (FileSystemUtils::doesDirectoryExist(jobOutputDir)) {
				FileSystemUtils::recursiveRemoveDirectory(jobOutputDir);
			}
			FileSystemUtils::createDirectory(jobOutputDir);

			SmartPtrCProviderCollectInstancesDoc providerCollectInstances;
			providerCollectInstances.CreateInstance();
			providerCollectInstances->initialize(job->_fqc->getClassNamespace(),
					job->_fqc->getClassName(), job->_fqc->getClassVersion(),
					job->_mgmtCollectInstances->getJobId(), jobOutputDir,
					job->_mgmtCollectInstances->getParameterCollection());

			collectInstancesCollectionInner.push_back(providerCollectInstances);
		}

		if (!job->_mgmtInvokeOperation.IsNull()) {
			const UUID jobId = job->_mgmtInvokeOperation->getJobId();
			const std::string jobIdStr = BasePlatform::UuidToString(jobId);
			const std::string jobOutputDir = FileSystemUtils::buildPath(outputDir,
					jobIdStr);
			if (FileSystemUtils::doesDirectoryExist(jobOutputDir)) {
				FileSystemUtils::recursiveRemoveDirectory(jobOutputDir);
			}
			FileSystemUtils::createDirectory(jobOutputDir);

			SmartPtrCProviderInvokeOperationDoc providerInvokeOperation;
			providerInvokeOperation.CreateInstance();
			providerInvokeOperation->initialize(job->_fqc->getClassNamespace(),
					job->_fqc->getClassName(), job->_fqc->getClassVersion(),
					job->_mgmtInvokeOperation->getJobId(), jobOutputDir,
					job->_mgmtInvokeOperation->getOperation());

			invokeOperationCollectionInner.push_back(providerInvokeOperation);
		}
	}

	SmartPtrCProviderCollectInstancesCollectionDoc collectInstancesCollection;
	if (!collectInstancesCollectionInner.empty()) {
		collectInstancesCollection.CreateInstance();
		collectInstancesCollection->initialize(collectInstancesCollectionInner);
	}

	SmartPtrCProviderInvokeOperationCollectionDoc invokeOperationCollection;
	if (!invokeOperationCollectionInner.empty()) {
		invokeOperationCollection.CreateInstance();
		invokeOperationCollection->initialize(invokeOperationCollectionInner);
	}

	SmartPtrCProviderBatchDoc providerBatch;
	providerBatch.CreateInstance();
	providerBatch->initialize(outputDir, collectInstancesCollection,
			invokeOperationCollection);

	SmartPtrCProviderRequestDoc providerRequest;
	providerRequest.CreateInstance();
	providerRequest->initialize(mgmtRequest->getClientId(),
			mgmtRequest->getRequestId(), mgmtRequest->getPmeId(), providerRequestHeader,
			providerBatch, mgmtRequest->getAttachmentCollection());

	return providerRequest;
}

void CSinglePmeRequestSplitterInstance::addCollectInstancesJobs(
		const SmartPtrCMgmtCollectInstancesCollectionDoc& mgmtCollectInstancesCollection,
		SmartPtrCProviderJobsCollection& providerJobsCollection) const {
	CAF_CM_FUNCNAME_VALIDATE("addCollectInstancesJobs");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	// mgmtCollectInstancesCollection is optional
	CAF_CM_VALIDATE_SMARTPTR(providerJobsCollection);

	if (!mgmtCollectInstancesCollection.IsNull()) {
		const std::deque<SmartPtrCMgmtCollectInstancesDoc> mgmtCollectInstancesCollectionInner =
				mgmtCollectInstancesCollection->getCollectInstancesCollection();
		for (TConstIterator<std::deque<SmartPtrCMgmtCollectInstancesDoc> > mgmtCollectInstancesIter(
				mgmtCollectInstancesCollectionInner); mgmtCollectInstancesIter;
				mgmtCollectInstancesIter++) {
			const SmartPtrCMgmtCollectInstancesDoc mgmtCollectInstances =
					*mgmtCollectInstancesIter;

			const SmartPtrCClassSpecifierDoc classSpecifier =
					mgmtCollectInstances->getClassSpecifier();
			const SmartPtrCClassCollection fqcCollection = resolveClassSpecifier(
					classSpecifier);

			for (TConstIterator<std::deque<SmartPtrCFullyQualifiedClassGroupDoc> > fqcIter(
					*fqcCollection); fqcIter; fqcIter++) {
				const SmartPtrCFullyQualifiedClassGroupDoc fqc = *fqcIter;
				const std::string providerUri = findProviderUri(fqc);

				SmartPtrCSplitterJob job;
				job.CreateInstance();
				job->_fqc = fqc;
				job->_mgmtCollectInstances = mgmtCollectInstances;

				CProviderJobsCollection::const_iterator fndIter =
						providerJobsCollection->find(providerUri);
				if (fndIter == providerJobsCollection->end()) {
					SmartPtrCSplitterJobsCollection jobsCollection;
					jobsCollection.CreateInstance();
					jobsCollection->push_back(job);
					providerJobsCollection->insert(
							std::make_pair(providerUri, jobsCollection));
				} else {
					fndIter->second->push_back(job);
				}
			}
		}
	}
}

void CSinglePmeRequestSplitterInstance::addInvokeOperationJobs(
		const SmartPtrCMgmtInvokeOperationCollectionDoc& mgmtInvokeOperationCollection,
		SmartPtrCProviderJobsCollection& providerJobsCollection) const {
	CAF_CM_FUNCNAME_VALIDATE("addInvokeOperationJobs");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	// mgmtInvokeOperationCollection is optional
	CAF_CM_VALIDATE_SMARTPTR(providerJobsCollection);

	if (!mgmtInvokeOperationCollection.IsNull()) {
		const std::deque<SmartPtrCMgmtInvokeOperationDoc> mgmtInvokeOperationCollectionInner =
				mgmtInvokeOperationCollection->getInvokeOperationCollection();
		for (TConstIterator<std::deque<SmartPtrCMgmtInvokeOperationDoc> > mgmtInvokeOperationIter(
				mgmtInvokeOperationCollectionInner); mgmtInvokeOperationIter;
				mgmtInvokeOperationIter++) {
			const SmartPtrCMgmtInvokeOperationDoc mgmtInvokeOperation =
					*mgmtInvokeOperationIter;

			const SmartPtrCClassSpecifierDoc classSpecifier =
					mgmtInvokeOperation->getClassSpecifier();
			const SmartPtrCClassCollection fqcCollection = resolveClassSpecifier(
					classSpecifier);

			for (TConstIterator<std::deque<SmartPtrCFullyQualifiedClassGroupDoc> > fqcIter(
					*fqcCollection); fqcIter; fqcIter++) {
				const SmartPtrCFullyQualifiedClassGroupDoc fqc = *fqcIter;
				const std::string providerUri = findProviderUri(fqc);

				SmartPtrCSplitterJob job;
				job.CreateInstance();
				job->_fqc = fqc;
				job->_mgmtInvokeOperation = mgmtInvokeOperation;

				CProviderJobsCollection::const_iterator fndIter =
						providerJobsCollection->find(providerUri);
				if (fndIter == providerJobsCollection->end()) {
					SmartPtrCSplitterJobsCollection jobsCollection;
					jobsCollection.CreateInstance();
					jobsCollection->push_back(job);
					providerJobsCollection->insert(
							std::make_pair(providerUri, jobsCollection));
				} else {
					fndIter->second->push_back(job);
				}
			}
		}
	}
}

CSinglePmeRequestSplitterInstance::SmartPtrCClassCollection CSinglePmeRequestSplitterInstance::resolveClassSpecifier(
		const SmartPtrCClassSpecifierDoc& classSpecifier) const {
	CAF_CM_FUNCNAME("resolveClassSpecifier");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(classSpecifier);

	SmartPtrCClassCollection classCollection;
	classCollection.CreateInstance();

	const SmartPtrCFullyQualifiedClassGroupDoc fullyQualifiedClass =
			classSpecifier->getFullyQualifiedClass();
	if (!fullyQualifiedClass.IsNull()) {
		classCollection->push_back(fullyQualifiedClass);
	}

	if (classCollection->empty()) {
		CAF_CM_EXCEPTION_VA0(ERROR_INVALID_DATA,
				"Failed to resolve to any fully-qualified classes");
	}

	return classCollection;
}

std::string CSinglePmeRequestSplitterInstance::findProviderUri(
		const SmartPtrCFullyQualifiedClassGroupDoc& fqc) const {
	CAF_CM_FUNCNAME("findProviderUri");

	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(fqc);

	const std::string providerUri = _schemaCacheManager->findProvider(fqc);

	if (providerUri.empty()) {
		CAF_CM_EXCEPTIONEX_VA3(NoSuchElementException, ERROR_NOT_FOUND,
				"Provider not found for %s::%s::%s with status %d",
				fqc->getClassNamespace().c_str(), fqc->getClassName().c_str(),
				fqc->getClassVersion().c_str());
	}

	return providerUri;
}

void CSinglePmeRequestSplitterInstance::createDirectory(
		const std::string& directory) const {
	CAF_CM_FUNCNAME_VALIDATE("createDirectory");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(directory);

	if (FileSystemUtils::doesDirectoryExist(directory)) {
		CAF_CM_LOG_WARN_VA1(
				"Directory already exists (perhaps from a previous failed run)... removing - %s",
				directory.c_str());
		FileSystemUtils::recursiveRemoveDirectory(directory);
	}

	CAF_CM_LOG_DEBUG_VA1("Creating directory - %s", directory.c_str());
	FileSystemUtils::createDirectory(directory);
}

void CSinglePmeRequestSplitterInstance::saveRequest(
		const std::string& outputDir,
		const SmartPtrCDynamicByteArray& payload) const {
	CAF_CM_FUNCNAME_VALIDATE("saveRequest");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(outputDir);
	CAF_CM_VALIDATE_SMARTPTR(payload);

	const std::string singlePmeRequestPath = FileSystemUtils::buildPath(outputDir,
			_sPayloadRequestFilename);

	CCafMessagePayload::saveToFile(payload, singlePmeRequestPath);
}

SmartPtrCProviderRequestHeaderDoc CSinglePmeRequestSplitterInstance::convertRequestHeader(
		const SmartPtrCRequestHeaderDoc& requestHeader) const {
	CAF_CM_FUNCNAME_VALIDATE("convertRequestHeader");
	CAF_CM_VALIDATE_SMARTPTR(requestHeader);

	const SmartPtrCRequestConfigDoc requestConfig = requestHeader->getRequestConfig();

	SmartPtrCProviderRequestConfigDoc providerRequestConfig;
	providerRequestConfig.CreateInstance();
	providerRequestConfig->initialize(requestConfig->getResponseFormatType(),
			requestConfig->getLoggingLevelCollection());

	SmartPtrCProviderRequestHeaderDoc providerRequestHeader;
	providerRequestHeader.CreateInstance();
	providerRequestHeader->initialize(providerRequestConfig,
			requestHeader->getEchoPropertyBag());

	return providerRequestHeader;
}
