/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "CEnvelopeToPayloadTransformerInstance.h"

using namespace Caf;

namespace Caf {
	const char* _sObjIdErrorToResponseTransformerInstance = "com.vmware.commonagent.cafintegration.errortoresponsetransformerinstance";
	const char* _sObjIdErrorToResponseTransformer = "com.vmware.commonagent.cafintegration.errortoresponsetransformer";
	const char* _sObjIdPayloadHeaderEnricherInstance = "com.vmware.commonagent.cafintegration.payloadheaderenricherinstance";
	const char* _sObjIdPayloadHeaderEnricher = "com.vmware.commonagent.cafintegration.payloadheaderenricher";
	const char* _sObjIdEnvelopeToPayloadTransformerInstance = "com.vmware.commonagent.cafintegration.envelopetopayloadtransformerinstance";
	const char* _sObjIdEnvelopeToPayloadTransformer = "com.vmware.commonagent.cafintegration.envelopetopayloadtransformer";
}

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
