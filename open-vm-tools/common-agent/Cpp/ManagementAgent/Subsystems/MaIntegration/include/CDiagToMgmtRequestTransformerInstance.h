/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CDiagToMgmtRequestTransformerInstance_h_
#define CDiagToMgmtRequestTransformerInstance_h_


#include "Integration/IIntegrationComponentInstance.h"

#include "CEnvelopeToPayloadTransformerInstance.h"
#include "Common/IAppContext.h"
#include "Doc/CafCoreTypesDoc/COperationDoc.h"
#include "Doc/DiagTypesDoc/CDiagCollectInstancesDoc.h"
#include "Doc/DiagTypesDoc/CDiagDeleteValueCollectionDoc.h"
#include "Doc/DiagTypesDoc/CDiagSetValueCollectionDoc.h"
#include "Doc/MgmtTypesDoc/CMgmtCollectInstancesCollectionDoc.h"
#include "Doc/MgmtTypesDoc/CMgmtCollectInstancesDoc.h"
#include "Doc/MgmtTypesDoc/CMgmtInvokeOperationCollectionDoc.h"
#include "Doc/MgmtTypesDoc/CMgmtInvokeOperationDoc.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Integration/IIntegrationObject.h"
#include "Integration/ITransformer.h"

namespace Caf {

class CDiagToMgmtRequestTransformerInstance :
	public TCafSubSystemObjectRoot<CDiagToMgmtRequestTransformerInstance>,
	public IIntegrationObject,
	public IIntegrationComponentInstance,
	public ITransformer {
public:
	struct SExpandedFileAlias {
		std::string _filePath;
		std::string _encoding;
	};
	CAF_DECLARE_SMART_POINTER(SExpandedFileAlias);

public:
	CDiagToMgmtRequestTransformerInstance();
	virtual ~CDiagToMgmtRequestTransformerInstance();

	CAF_DECLARE_OBJECT_IDENTIFIER(_sObjIdDiagToMgmtRequestTransformerInstance)

	CAF_BEGIN_INTERFACE_MAP(CDiagToMgmtRequestTransformerInstance)
		CAF_INTERFACE_ENTRY(IIntegrationObject)
		CAF_INTERFACE_ENTRY(IIntegrationComponentInstance)
		CAF_INTERFACE_ENTRY(ITransformer)
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

public: // ITransformer
	SmartPtrIIntMessage transformMessage(
		const SmartPtrIIntMessage& message);

private:
	SmartPtrCMgmtCollectInstancesCollectionDoc createMgmtCollectInstancesCollection(
		const SmartPtrCDiagCollectInstancesDoc& diagCollectInstances) const;

	SmartPtrCMgmtInvokeOperationCollectionDoc createMgmtInvokeOperationCollection(
		const SmartPtrCDiagSetValueCollectionDoc& diagSetValueCollection,
		const SmartPtrCDiagDeleteValueCollectionDoc& diagDeleteValueCollection) const;

	SmartPtrCMgmtCollectInstancesDoc createCollectInstances(
		const UUID& jobId,
		const SmartPtrSExpandedFileAlias& expandedFileAlias) const;

	SmartPtrCOperationDoc createSetValueOperation(
		const std::string& valueName,
		const std::deque<std::string>& valueCollection,
		const SmartPtrSExpandedFileAlias& expandedFileAlias) const;

	SmartPtrCOperationDoc createDeleteValueOperation(
		const std::string& valueName,
		const SmartPtrSExpandedFileAlias& expandedFileAlias) const;

	SmartPtrCMgmtInvokeOperationDoc createInvokeOperation(
		const UUID& jobId,
		const SmartPtrCOperationDoc operation) const;

	std::deque<CDiagToMgmtRequestTransformerInstance::SmartPtrSExpandedFileAlias> expandFileAliases() const;

	SmartPtrSExpandedFileAlias expandFileAlias(
		const std::string& fileAlias) const;

	std::string findUriParameter(
		const std::string& parameterName,
		const UriUtils::SUriRecord& uri) const;

private:
	bool _isInitialized;
	std::string _id;

	std::string _fileAliasPrefix;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CDiagToMgmtRequestTransformerInstance);
};

}

#endif // #ifndef CDiagToMgmtRequestTransformerInstance_h_
