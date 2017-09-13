/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (c) 2010 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#include "stdafx.h"
#include "IntegrationObjects.h"

namespace Caf {

namespace MaIntegration {
	const char* _sObjIdIntegrationObjects = "com.vmware.commonagent.maintegration.integrationobjects";
}}

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
