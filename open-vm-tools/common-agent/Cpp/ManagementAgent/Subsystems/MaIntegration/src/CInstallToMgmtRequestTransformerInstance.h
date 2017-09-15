/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CInstallToMgmtRequestTransformerInstance_h_
#define CInstallToMgmtRequestTransformerInstance_h_


#include "Integration/IIntegrationComponentInstance.h"

#include "Common/IAppContext.h"
#include "Doc/CafCoreTypesDoc/COperationDoc.h"
#include "Doc/CafInstallRequestDoc/CGetInventoryJobDoc.h"
#include "Doc/CafInstallRequestDoc/CInstallProviderJobDoc.h"
#include "Doc/CafInstallRequestDoc/CUninstallProviderJobDoc.h"
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

class CInstallToMgmtRequestTransformerInstance :
	public TCafSubSystemObjectRoot<CInstallToMgmtRequestTransformerInstance>,
	public IIntegrationObject,
	public IIntegrationComponentInstance,
	public ITransformer {

public:
	CInstallToMgmtRequestTransformerInstance();
	virtual ~CInstallToMgmtRequestTransformerInstance();

	CAF_DECLARE_OBJECT_IDENTIFIER(_sObjIdInstallToMgmtRequestTransformerInstance)

	CAF_BEGIN_INTERFACE_MAP(CInstallToMgmtRequestTransformerInstance)
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
		const SmartPtrCGetInventoryJobDoc& getInventoryJob) const;

	SmartPtrCMgmtInvokeOperationCollectionDoc createMgmtInvokeOperationCollection(
		const SmartPtrCInstallProviderJobDoc& installProviderJob,
		const SmartPtrCUninstallProviderJobDoc& uninstallProviderJob) const;

	SmartPtrCMgmtCollectInstancesDoc createCollectInstances(
		const UUID& jobId) const;

	SmartPtrCMgmtInvokeOperationDoc createInvokeOperation(
		const SmartPtrCOperationDoc& operation) const;

	SmartPtrCOperationDoc createInstallProviderOperation(
		const SmartPtrCInstallProviderJobDoc& installProviderJob) const;

	SmartPtrCOperationDoc createUninstallProviderJobOperation(
		const SmartPtrCUninstallProviderJobDoc& uninstallProviderJob) const;

	static std::string saveInstallProviderJobToString(
		const SmartPtrCInstallProviderJobDoc& installProviderJob);

	static std::string saveUninstallProviderJobToString(
		const SmartPtrCUninstallProviderJobDoc& uninstallProviderJob);

private:
	bool _isInitialized;
	std::string _id;

	std::string _fileAliasPrefix;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CInstallToMgmtRequestTransformerInstance);
};

}

#endif // #ifndef CInstallToMgmtRequestTransformerInstance_h_
