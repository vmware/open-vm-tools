/*
 *  Created on: Aug 16, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "CCmsMessageTransformer.h"
#include "CCmsMessageTransformerInstance.h"
#include "CEventTopicCalculatorInstance.h"
#include "CIncomingMessageHandler.h"
#include "CIncomingMessageHandlerInstance.h"
#include "COutgoingMessageHandler.h"
#include "CProtocolHeaderEnricher.h"
#include "CProtocolHeaderEnricherInstance.h"
#include "CReplyToCacher.h"
#include "CReplyToCacherInstance.h"
#include "CReplyToResolverInstance.h"

namespace Caf {
	const char* _sObjIdCommIntegrationCmsMessageTransformer = "com.vmware.caf.comm.integration.cmsmessagetransformer";
	const char* _sObjIdCommIntegrationCmsMessageTransformerInstance = "com.vmware.caf.comm.integration.cmsmessagetransformerinstance";
	const char* _sObjIdCommIntegrationEventTopicCalculator = "com.vmware.caf.comm.integration.eventtopiccalculator";
	const char* _sObjIdCommIntegrationIncomingMessageHandler = "com.vmware.caf.comm.integration.incomingmessagehandler";
	const char* _sObjIdCommIntegrationIncomingMessageHandlerInstance = "com.vmware.caf.comm.integration.incomingmessagehandlerinstance";
	const char* _sObjIdCommIntegrationOutgoingMessageHandler = "com.vmware.caf.comm.integration.outgoingmessagehandler";
	const char* _sObjIdCommIntegrationProtocolHeaderEnricher = "com.vmware.caf.comm.integration.protocolheaderenricher";
	const char* _sObjIdCommIntegrationProtocolHeaderEnricherInstance = "com.vmware.caf.comm.integration.protocolheaderenricherinstance";
	const char* _sObjIdCommIntegrationReplyToCacher = "com.vmware.caf.comm.integration.replytocacher";
	const char* _sObjIdCommIntegrationReplyToCacherInstance = "com.vmware.caf.comm.integration.replytocacherinstance";
	const char* _sObjIdCommIntegrationReplyToResolver = "com.vmware.caf.comm.integration.replytoresolver";
}

CEcmSubSystemModule _Module;

using namespace Caf;

CAF_BEGIN_OBJECT_MAP(ObjectMap)
	CAF_OBJECT_ENTRY(CCmsMessageTransformer)
	CAF_OBJECT_ENTRY(CCmsMessageTransformerInstance)
	CAF_OBJECT_ENTRY(CEventTopicCalculatorInstance)
	CAF_OBJECT_ENTRY(CIncomingMessageHandler)
	CAF_OBJECT_ENTRY(CIncomingMessageHandlerInstance)
	CAF_OBJECT_ENTRY(COutgoingMessageHandler)
	CAF_OBJECT_ENTRY(CProtocolHeaderEnricher)
	CAF_OBJECT_ENTRY(CProtocolHeaderEnricherInstance)
	CAF_OBJECT_ENTRY(CReplyToCacher)
	CAF_OBJECT_ENTRY(CReplyToCacherInstance)
	CAF_OBJECT_ENTRY(CReplyToResolverInstance)
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
