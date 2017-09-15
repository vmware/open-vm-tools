/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

namespace Caf {
	const char* _sObjIdGuestAuthenticatorInstance = "com.vmware.commonagent.maintegration.guestauthenticatorinstance";
	const char* _sObjIdGuestAuthenticator = "com.vmware.commonagent.maintegration.guestauthenticator";
}

using namespace Caf;

CEcmSubSystemModule _Module;

CAF_BEGIN_OBJECT_MAP(ObjectMap)
#ifndef __APPLE__
	CAF_OBJECT_ENTRY(CGuestAuthenticator)
	CAF_OBJECT_ENTRY(CGuestAuthenticatorInstance)
#endif
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
