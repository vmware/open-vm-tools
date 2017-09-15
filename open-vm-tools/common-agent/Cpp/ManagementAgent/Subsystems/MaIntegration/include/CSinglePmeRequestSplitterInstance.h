/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CSinglePmeRequestSplitterInstance_h_
#define CSinglePmeRequestSplitterInstance_h_


#include "Integration/IIntegrationComponentInstance.h"

#include "CSchemaCacheManager.h"
#include "Common/IAppContext.h"
#include "Doc/CafCoreTypesDoc/CClassSpecifierDoc.h"
#include "Doc/CafCoreTypesDoc/CFullyQualifiedClassGroupDoc.h"
#include "Doc/CafCoreTypesDoc/CRequestHeaderDoc.h"
#include "Doc/MgmtRequestDoc/CMgmtRequestDoc.h"
#include "Doc/MgmtTypesDoc/CMgmtCollectInstancesCollectionDoc.h"
#include "Doc/MgmtTypesDoc/CMgmtCollectInstancesDoc.h"
#include "Doc/MgmtTypesDoc/CMgmtCollectSchemaDoc.h"
#include "Doc/MgmtTypesDoc/CMgmtInvokeOperationCollectionDoc.h"
#include "Doc/MgmtTypesDoc/CMgmtInvokeOperationDoc.h"
#include "Doc/ProviderRequestDoc/CProviderCollectSchemaRequestDoc.h"
#include "Doc/ProviderRequestDoc/CProviderRequestDoc.h"
#include "Doc/ProviderRequestDoc/CProviderRequestHeaderDoc.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "Integration/IIntegrationObject.h"
#include "Integration/IMessageSplitter.h"

namespace Caf {

// Moved the following typedefs from the class because Windows produced
// the warning "decorated name length exceeded, name was truncated" because
// their name included CSinglePmeRequestSplitterInstance.
struct CSplitterJob {
	SmartPtrCFullyQualifiedClassGroupDoc _fqc;
	SmartPtrCMgmtCollectInstancesDoc _mgmtCollectInstances;
	SmartPtrCMgmtInvokeOperationDoc _mgmtInvokeOperation;
};
CAF_DECLARE_SMART_POINTER(CSplitterJob);

typedef std::deque<SmartPtrCSplitterJob> CSplitterJobsCollection;
CAF_DECLARE_SMART_POINTER(CSplitterJobsCollection);

typedef std::map<std::string, SmartPtrCSplitterJobsCollection> CProviderJobsCollection;
CAF_DECLARE_SMART_POINTER(CProviderJobsCollection);

class CSinglePmeRequestSplitterInstance :
	public TCafSubSystemObjectRoot<CSinglePmeRequestSplitterInstance>,
	public IIntegrationObject,
	public IIntegrationComponentInstance,
	public IMessageSplitter {
private:
	typedef std::deque<SmartPtrCFullyQualifiedClassGroupDoc> CClassCollection;
	CAF_DECLARE_SMART_POINTER(CClassCollection);

public:
	CSinglePmeRequestSplitterInstance();
	virtual ~CSinglePmeRequestSplitterInstance();

	CAF_DECLARE_OBJECT_IDENTIFIER(_sObjIdSinglePmeRequestSplitterInstance)

	CAF_BEGIN_INTERFACE_MAP(CSinglePmeRequestSplitterInstance)
		CAF_INTERFACE_ENTRY(IIntegrationObject)
		CAF_INTERFACE_ENTRY(IIntegrationComponentInstance)
		CAF_INTERFACE_ENTRY(IMessageSplitter)
	CAF_END_INTERFACE_MAP()

public: // IIntegrationObject
	void initialize(
		const IBean::Cargs& ctorArgs,
		const IBean::Cprops& properties,
		const SmartPtrIDocument& configSection);

	std::string getId() const;

public: // IIntegrationComponentInstance
	void wire(
		const SmartPtrIAppContext& appContext,
		const SmartPtrIChannelResolver& channelResolver);

public: // IMessageSplitter
	SmartPtrCMessageCollection splitMessage(
		const SmartPtrIIntMessage& message);

private:
	SmartPtrCProviderCollectSchemaRequestDoc createCollectSchemaRequest(
		const SmartPtrCMgmtRequestDoc& mgmtRequest,
		const SmartPtrCMgmtCollectSchemaDoc& mgmtCollectSchema,
		const SmartPtrCProviderRequestHeaderDoc& providerRequestHeader,
		const std::string& outputDir) const;

	SmartPtrCProviderRequestDoc createProviderRequest(
		const SmartPtrCMgmtRequestDoc& mgmtRequest,
		const SmartPtrCSplitterJobsCollection& jobsCollection,
		const SmartPtrCProviderRequestHeaderDoc& providerRequestHeader,
		const std::string& outputDir) const;

	void addCollectInstancesJobs(
		const SmartPtrCMgmtCollectInstancesCollectionDoc& mgmtCollectInstancesCollection,
		SmartPtrCProviderJobsCollection& providerJobsCollection) const;

	void addInvokeOperationJobs(
		const SmartPtrCMgmtInvokeOperationCollectionDoc& mgmtInvokeOperationCollection,
		SmartPtrCProviderJobsCollection& providerJobsCollection) const;

	SmartPtrCClassCollection resolveClassSpecifier(
		const SmartPtrCClassSpecifierDoc& classSpecifier) const;

	std::string findProviderUri(
		const SmartPtrCFullyQualifiedClassGroupDoc& fqc) const;

	void createDirectory(
		const std::string& directory) const;

	void saveRequest(
		const std::string& outputDir,
		const SmartPtrCDynamicByteArray& payload) const;

	SmartPtrCProviderRequestHeaderDoc convertRequestHeader(
		const SmartPtrCRequestHeaderDoc& requestHeader) const;

private:
	bool _isInitialized;
	std::string _id;
	SmartPtrCSchemaCacheManager _schemaCacheManager;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CSinglePmeRequestSplitterInstance);
};

}

#endif // #ifndef CSinglePmeRequestSplitterInstance_h_
