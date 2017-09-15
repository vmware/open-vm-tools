/*
 *  Created on: May 24, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "CachingConnectionFactoryObj.h"
#include "SecureCachingConnectionFactoryObj.h"
#include "IntegrationObjects.h"

namespace Caf {

/** @brief CAF AMQP Integration */
namespace AmqpIntegration {
	const char* _sObjIdAmqpCachingConnectionFactory = "com.vmware.caf.comm.integration.amqp.caching.connection.factory";
	const char* _sObjIdAmqpSecureCachingConnectionFactory = "com.vmware.caf.comm.integration.amqp.secure.caching.connection.factory";
	const char* _sObjIdIntegrationObjects = "com.vmware.caf.comm.integration.objects";
}}

CEcmSubSystemModule _Module;

using namespace Caf::AmqpIntegration;

CAF_BEGIN_OBJECT_MAP(ObjectMap)
	CAF_OBJECT_ENTRY(CachingConnectionFactoryObj)
	CAF_OBJECT_ENTRY(SecureCachingConnectionFactoryObj)
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
