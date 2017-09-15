/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "CIntegrationObjectFactory.h"
#include "CErrorChannel.h"
#include "CNullChannel.h"
#include "CHeaderExpressionInvoker.h"

using namespace Caf;

namespace Caf {
	const char* _sObjIdIntegrationObjectFactory = "com.vmware.commonagent.integration.objectfactory";
	const char* _sObjIdErrorChannel = "com.vmware.commonagent.integration.channels.errorchannel";
	const char* _sObjIdNullChannel = "com.vmware.commonagent.integration.channels.nullchannel";
	const char* _sObjIdHeaderExpressionInvoker = "com.vmware.commonagent.integration.headerexpressioninvoker";
}

CEcmSubSystemModule _Module;

CAF_BEGIN_OBJECT_MAP(ObjectMap)
	CAF_OBJECT_ENTRY(CIntegrationObjectFactory)
	CAF_OBJECT_ENTRY(CErrorChannel)
	CAF_OBJECT_ENTRY(CNullChannel)
	CAF_OBJECT_ENTRY(CHeaderExpressionInvoker)
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
