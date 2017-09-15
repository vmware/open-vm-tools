/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "MaIntegration.h"
#include "IntegrationObjects.h"
#include "CAttachmentRequestTransformerInstance.h"
#include "CDiagToMgmtRequestTransformerInstance.h"
#include "CSinglePmeRequestSplitterInstance.h"

namespace Caf {

namespace MaIntegration {
	const char* _sObjIdIntegrationObjects = "com.vmware.commonagent.maintegration.integrationobjects";
}}

namespace Caf {
	const char* _sObjIdCollectSchemaExecutor = "com.vmware.commonagent.maintegration.collectschemaexecutor";
	const char* _sObjIdProviderCollectSchemaExecutor = "com.vmware.commonagent.maintegration.providercollectschemaexecutor";
	const char* _sObjIdProviderExecutor = "com.vmware.commonagent.maintegration.providerexecutor";
	const char* _sObjIdSinglePmeRequestSplitterInstance = "com.vmware.commonagent.maintegration.singlepmerequestsplitterinstance";
	const char* _sObjIdSinglePmeRequestSplitter = "com.vmware.commonagent.maintegration.singlepmerequestsplitter";
	const char* _sObjIdDiagToMgmtRequestTransformerInstance = "com.vmware.commonagent.maintegration.diagtomgmtrequesttransformerinstance";
	const char* _sObjIdDiagToMgmtRequestTransformer = "com.vmware.commonagent.maintegration.diagtomgmtrequesttransformer";
	const char* _sObjIdInstallToMgmtRequestTransformerInstance = "com.vmware.commonagent.maintegration.installtomgmtrequesttransformerinstance";
	const char* _sObjIdInstallToMgmtRequestTransformer = "com.vmware.commonagent.maintegration.installtomgmtrequesttransformer";
	const char* _sObjIdPersistenceNamespaceDb = "com.vmware.commonagent.maintegration.persistencenamespacedb";
	const char* _sObjIdConfigEnv = "com.vmware.commonagent.maintegration.configenv";

	const char* _sObjIdAttachmentRequestTransformerInstance = "com.vmware.commonagent.maintegration.attachmentrequesttransformerinstance";
	const char* _sObjIdAttachmentRequestTransformer = "com.vmware.commonagent.maintegration.attachmentrequesttransformer";
	const char* _sObjIdVersionTransformerInstance = "com.vmware.commonagent.maintegration.versiontransformerinstance";
	const char* _sObjIdVersionTransformer = "com.vmware.commonagent.maintegration.versiontransformer";
}

CEcmSubSystemModule _Module;

using namespace Caf::MaIntegration;

CAF_BEGIN_OBJECT_MAP(ObjectMap)
	CAF_OBJECT_ENTRY(CAttachmentRequestTransformer)
	CAF_OBJECT_ENTRY(CAttachmentRequestTransformerInstance)
	CAF_OBJECT_ENTRY(CCollectSchemaExecutor)
	CAF_OBJECT_ENTRY(CConfigEnv)
	CAF_OBJECT_ENTRY(CDiagToMgmtRequestTransformer)
	CAF_OBJECT_ENTRY(CDiagToMgmtRequestTransformerInstance)
	CAF_OBJECT_ENTRY(CInstallToMgmtRequestTransformer)
	CAF_OBJECT_ENTRY(CInstallToMgmtRequestTransformerInstance)
	CAF_OBJECT_ENTRY(CPersistenceNamespaceDb)
	CAF_OBJECT_ENTRY(CProviderCollectSchemaExecutor)
	CAF_OBJECT_ENTRY(CProviderExecutor)
	CAF_OBJECT_ENTRY(CSinglePmeRequestSplitter)
	CAF_OBJECT_ENTRY(CSinglePmeRequestSplitterInstance)
	CAF_OBJECT_ENTRY(CVersionTransformer)
	CAF_OBJECT_ENTRY(CVersionTransformerInstance)
	CAF_OBJECT_ENTRY(IntegrationObjects)
CAF_END_OBJECT_MAP()

CAF_DECLARE_SUBSYSTEM_EXPORTS()

extern "C" BOOL APIENTRY DllMain(HINSTANCE hModule, uint32 dwReason, LPVOID)
{
	try {
		if (DLL_PROCESS_ATTACH == dwReason)
		{
			// initialize the sub-system module
			_Module.Init(ObjectMap, hModule);
		}
		else if (DLL_PROCESS_DETACH == dwReason)
		{
			// Terminate the sub-system module
			_Module.Term();
		}
	} catch (std::runtime_error) {
		::exit(2);
	} catch (...) {
		::exit(2);
	}

    return TRUE;
}
