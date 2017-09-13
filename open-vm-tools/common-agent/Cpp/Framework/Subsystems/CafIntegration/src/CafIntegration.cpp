/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (c) 2010 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#include "stdafx.h"

using namespace Caf;

CEcmSubSystemModule _Module;

CAF_BEGIN_OBJECT_MAP(ObjectMap)
	CAF_OBJECT_ENTRY(CErrorToResponseTransformer)
	CAF_OBJECT_ENTRY(CErrorToResponseTransformerInstance)
	CAF_OBJECT_ENTRY(CPayloadHeaderEnricher)
	CAF_OBJECT_ENTRY(CPayloadHeaderEnricherInstance)
	CAF_OBJECT_ENTRY(CEnvelopeToPayloadTransformer)
	CAF_OBJECT_ENTRY(CEnvelopeToPayloadTransformerInstance)
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
