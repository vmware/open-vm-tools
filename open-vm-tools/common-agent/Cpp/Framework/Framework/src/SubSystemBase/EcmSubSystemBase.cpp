/*
 *  Created: Oct 09, 2002
 *
 *	Copyright (C) 2002-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "Exception/CCafException.h"
#include "EcmSubSystemBase.h"
#include <string.h>
#include <stdlib.h>

using namespace Caf;

//////////////////////////////////////////////////////////////////////////////
// ECM Sub-System Constant Definitions
//////////////////////////////////////////////////////////////////////////////

// The value name for retrieving the module's path
const char* Caf::MODULE_PATH_VALUE_NAME = "ModulePath";

// The DLL exported function names
const char* Caf::CREATE_INSTANCE = "CafCreateInstance";
const char* Caf::CAN_UNLOAD_NOW = "CafDllCanUnloadNow";

////////////////////////////////////////////////////////////////////////
//
// Global implementation of CreateObject - helper functions for
// TEcmSmartPtr that can't go inside of TEcmSmartPtr because gcc barfs.
//
////////////////////////////////////////////////////////////////////////

// Suppress warning about throwing an exception: we want C linkage,
// but we also want to throw exceptions
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4297)	// function assumed not to throw an exception but does
#endif

// The function is extern "C" and /EHc is specified
extern "C" SUBSYSTEMBASE_LINKAGE void Caf::CreateObject (const char *cszObjectId, const IID &criid, void **ppv)
{
	CAF_CM_STATIC_FUNC_VALIDATE( "SubSystemBase", "CreateObject" );
	CAF_CM_VALIDATE_STRINGPTRA(cszObjectId);
	CAF_CM_VALIDATE_PTR(ppv);

	#ifdef WIN32
	char* objId = ::_strdup(cszObjectId);
	#else
	char* objId = ::strdup(cszObjectId);
	#endif
	
	try
	{
		// Determine the type of object to create based on Id.
		// QI object id's are of the form <subsystem_id>:ClassName.
//		char *szColon = ::strchr(objId, ':');
//
//		if (szColon)
//		{
//			// Create a QI object
//			*szColon = NULL;
//			CreateQIObject(objId, szColon + 1, criid, ppv);
//		}
//		else
//		{
			// Create a subsystem object
			CEcmSubSystem oSubSystem;
			oSubSystem.Load(cszObjectId);
			oSubSystem.CreateInstance(cszObjectId, criid, ppv);
//		}
	}
	catch (CCafException *e)
	{
		::free(objId);
		throw e;
	}
	catch (std::exception& e)
	{
		::free(objId);
		throw e;
	}
	catch (...)
	{
		::free(objId);
		throw;
	}

	::free(objId);
}

//extern "C" SUBSYSTEMBASE_LINKAGE void Caf::CreateQIObject (const char *cszFactoryId, const char* cszClassName, const IID &criid, void **ppv)
//{
//	CAF_CM_STATIC_FUNC( "SubSystemBase", "CreateObject" );
//	CAF_CM_VALIDATE_STRINGPTRA(cszFactoryId);
//	CAF_CM_VALIDATE_STRINGPTRA(cszClassName);
//	CAF_CM_VALIDATE_PTR(ppv);
//
////	SmartSbICfcClassFactory spsFactory;
////	spsFactory.CreateInstance(cwszFactoryId);
////	*ppv = NULL;
////	spsFactory->CreateObject(cwszClassName, criid, ppv);
////	CAF_CM_ASSERT_MSG(*ppv, "Failed to create object");
//}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
