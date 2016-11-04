/*
 *  Created: Oct 09, 2002
 *
 *	Copyright (C) 2002-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "Exception/CCafException.h"
#include "CEcmSubSystem.h"
#include "EcmSubSystemBase.h"
#include "CEcmSubSystemRegistry.h"

#ifdef WIN32
#ifdef CFC_AS_STATIC_LIB
#define IStream ::IStream
#endif
#include <comdef.h>
#ifdef CFC_AS_STATIC_LIB
#undef IStream
#endif
#endif

#include "CEcmDllManager.h"

using namespace Caf;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
std::map<std::string, HMODULE> CEcmSubSystem::m_mapModuleCache;

GRecMutex CEcmSubSystem::m_oModuleCacheMutex;

CEcmSubSystem::CEcmSubSystem(bool bIsUnloadable) :
	CAF_CM_INIT_LOG("CEcmSubSystem"),
	m_bIsUnloadable(bIsUnloadable),
	m_hModule(NULL),
	m_pfnCreateInstance(NULL),
	m_pfnCanUnloadNow(NULL)
{
}

CEcmSubSystem::~CEcmSubSystem()
{
}

bool CEcmSubSystem::IsRegistered( const std::string & rstrSubSystemIdentifier )
{
	CAF_CM_FUNCNAME_VALIDATE("IsRegistered");

	bool bRetVal = false;
	
	CAF_CM_VALIDATE_STRINGPTRA(rstrSubSystemIdentifier.c_str());
	bRetVal = CEcmSubSystemRegistry::IsRegistered( rstrSubSystemIdentifier );

	return bRetVal;
}

void CEcmSubSystem::Load(const std::string& rstrSubSystemIdentifier)
{
	CAF_CM_FUNCNAME("Load");

	// Make sure that we have sub-system name.
	CAF_CM_VALIDATE_STRINGPTRA(rstrSubSystemIdentifier.c_str());

	// If this is an unloadable sub-system...
	if(m_bIsUnloadable)
	{
		// make sure it is unlaoded before loading the next one.
		Unload();
	}
	// Otherwise...
	else
	{
		// if there is already a module loaded by this sub-system
		// object...
		if(NULL != m_hModule)
		{
			CAF_CM_EXCEPTION_VA1(E_FAIL,
								 "The sub-system %s is already loaded. Load may not be called again.",
								 rstrSubSystemIdentifier.c_str());
		}
	}

	// Create a helper to make sure we unlock the critical section.
	CAutoMutexLockUnlockRaw oCS( &m_oModuleCacheMutex );

	// Before we do the lookup again, lets check the cache to see if we
	// already have the module.
	std::map<std::string, HMODULE>::iterator iterModule =
		m_mapModuleCache.find(rstrSubSystemIdentifier);

	// If we found the module in the cache...
	if(iterModule != m_mapModuleCache.end())
	{
		// set from the cache, and we're done.
		m_hModule = iterModule->second;
		m_strSubSystemIdentifier = rstrSubSystemIdentifier;

		//CAF_CM_LOG_DEBUG_VA2("Found the subsystem in the cache - id: %s, module: %p",
		//	m_strSubSystemIdentifier.c_str(), m_hModule);
	}
	// Otherwise, look in the registry for the sub-system module.
	else
	{
		// Create a registry object.
		if (CEcmSubSystemRegistry::IsRegistered( rstrSubSystemIdentifier )) {
			std::string modulePath = CEcmSubSystemRegistry::GetModulePath( rstrSubSystemIdentifier );
			if ( modulePath.length() )
			{
				// load the DLL.
				m_hModule = CEcmDllManager::LoadLibrary( modulePath.c_str(), true );

				// Record the sub-system identifier.
				m_strSubSystemIdentifier = rstrSubSystemIdentifier;

				// Insert the sub-system id to module mapping entry.
				m_mapModuleCache.insert(
						std::pair<std::string, HMODULE>
							(rstrSubSystemIdentifier, m_hModule));

				//CAF_CM_LOG_DEBUG_VA3("Loaded the subsystem - id: %s, path: %s, module: %p",
				//	m_strSubSystemIdentifier.c_str(), modulePath.c_str(), m_hModule);
			}
			else
			{
				CAF_CM_EXCEPTION_VA1(E_FAIL,
									 "Failed to load subsystem %s - Registered but modulePath is empty",
									 rstrSubSystemIdentifier.c_str());
			}
		}
		else
		{
			CAF_CM_EXCEPTION_VA1(E_FAIL,
								 "Failed to load subsystem %s  - Not registered",
								 rstrSubSystemIdentifier.c_str());
		}
	}
}

bool CEcmSubSystem::Unload( const bool cbMustUnloadNow /* = true */ )
{
	CAF_CM_FUNCNAME("Unload");

	bool bSubsystemWasUnloaded = false; 
	// If the sub-system is unloadable...
	if(m_bIsUnloadable)
	{
		// and there is a module actually loaded...
		if(NULL != m_hModule)
		{
			// if we don't already have it...
			if(NULL == m_pfnCanUnloadNow)
			{
				std::string strErrorMessage;
				// get the address of the sub-system's can unload function.
				m_pfnCanUnloadNow = (CanUnloadNowFunc)
					CEcmDllManager::GetFunctionAddress(m_hModule, CAN_UNLOAD_NOW, strErrorMessage );

				// If we did not got the function pointer...
				if(NULL == m_pfnCanUnloadNow)
				{
					// throw an exception indicating that the can unload
					// function could not be found.
					CAF_CM_EXCEPTION_EFAIL(strErrorMessage);
				}
			}

			if(m_pfnCanUnloadNow())
			{
				// free the library, and
				CEcmDllManager::UnloadLibrary( m_hModule, true );
				// Create a helper to make sure we unlock the critical section.
				CAutoMutexLockUnlockRaw oCS( &m_oModuleCacheMutex );

				// try to find the module entry.
				std::map<std::string, HMODULE>::iterator iterModule =
						m_mapModuleCache.find(m_strSubSystemIdentifier);

				// If we found the module in the cache...
				if(iterModule != m_mapModuleCache.end())
				{
					// remove if from the cache.
					m_mapModuleCache.erase(iterModule);
				}

				bSubsystemWasUnloaded = true;
				// Reset the member variables to an initialized state.
				m_hModule = NULL;
				m_pfnCreateInstance = NULL;
				m_pfnCanUnloadNow = NULL;
				m_strSubSystemIdentifier = "";
			}
			// Otherwise, the sub-system is telling us it cannot safely be
			// unloaded at this time, if the caller indicated that it must
			// unload now we fail unload
			else if ( cbMustUnloadNow )
			{
				// throw an exception indicating the condition.
				CAF_CM_EXCEPTION_EFAIL("Unable to safely unload the sub-system at this time.");
			}
		}

		// Otherwise, they are trying to unload nothing, so let them.
	}
	// If the sub-system is not unloadable...
	else
	{
		// and there is a module actually loaded...
		if(NULL != m_hModule)
		{
			// throw an exception.
			CAF_CM_EXCEPTION_EFAIL("Unable to unload an unloadable sub-system.");
		}

		// Otherwise, they are trying to unload nothing, so let them.
	}

	return bSubsystemWasUnloaded;
}

void CEcmSubSystem::CreateInstance( const std::string & rstrIdentifier, const IID & riid, void ** ppv )
{
	CAF_CM_FUNCNAME( "CreateInstance" );
	
	// If the sub-system is not loaded...
	if(NULL == m_hModule)
	{
		// throw an exception indicating that the sub-system must be
		// loaded before object instance can be created from it.
		CAF_CM_EXCEPTION_EFAIL("No sub-system is loaded.  You must call Load before "
							   "object instances can be created.");
	}

	// If we don't already have a pointer to the CreateInstance
	// function provided by the sub-system DLL...
	if(NULL == m_pfnCreateInstance)
	{
		//CAF_CM_LOG_DEBUG_VA2("Getting CreateInstance - id: %s, module: %p",
		//	rstrIdentifier.c_str(), m_hModule);

		std::string strErrorMessage;
		// get the address of the CreateInstance function.
		m_pfnCreateInstance =
			(void (*)( const char* rstrIdentifier, const IID & riid, void ** ppv ))
			CEcmDllManager::GetFunctionAddress(m_hModule, CREATE_INSTANCE, strErrorMessage );

		// If we did not get the function pointer...
		if(NULL == m_pfnCreateInstance)
		{
			// throw an exception indicating that the create instance
			// could not be found.
			CAF_CM_EXCEPTION_EFAIL(strErrorMessage);
		}
	}

	// Call the function, passing in the supplied arguments.
	m_pfnCreateInstance(rstrIdentifier.c_str(), riid, ppv);
}


////////////////////////////////////////////////////////////////////////
//
//  CEcmSubSystem::CEcmSubSystem( CEcmSubSystem ) 
//
//  overloaded copy constructor
//
////////////////////////////////////////////////////////////////////////
CEcmSubSystem::CEcmSubSystem( const CEcmSubSystem&  crCEcmSubSystem ) :
		CAF_CM_INIT_LOG("CEcmSubSystem")
{
	m_bIsUnloadable = crCEcmSubSystem.m_bIsUnloadable;
	m_strSubSystemIdentifier = crCEcmSubSystem.m_strSubSystemIdentifier;
	m_hModule = crCEcmSubSystem.m_hModule;
	m_pfnCreateInstance = crCEcmSubSystem.m_pfnCreateInstance;
	m_pfnCanUnloadNow = crCEcmSubSystem.m_pfnCanUnloadNow;
}



////////////////////////////////////////////////////////////////////////
//
//  CEcmSubSystem::operator=()
//
//  overloaded = operator
//
////////////////////////////////////////////////////////////////////////
CEcmSubSystem& CEcmSubSystem::operator=( const CEcmSubSystem& crCEcmSubSystem ) 
{
	m_bIsUnloadable = crCEcmSubSystem.m_bIsUnloadable;
	m_strSubSystemIdentifier = crCEcmSubSystem.m_strSubSystemIdentifier;
	m_hModule = crCEcmSubSystem.m_hModule;
	m_pfnCreateInstance = crCEcmSubSystem.m_pfnCreateInstance;
	m_pfnCanUnloadNow = crCEcmSubSystem.m_pfnCanUnloadNow;
	
	return *this;

}



////////////////////////////////////////////////////////////////////////
//
//  CEcmSubSystem::IsUnloadable()
//
//  returns true if this subsystem is unloadable, false if it isn't 
//
////////////////////////////////////////////////////////////////////////
bool CEcmSubSystem::IsUnloadable() const
{
	return m_bIsUnloadable;
}

	

////////////////////////////////////////////////////////////////////////
//
//  CEcmSubSystem::GetSubSystemID()
//
//  returns the subystem identifier
//
////////////////////////////////////////////////////////////////////////
std::string CEcmSubSystem::GetSubSystemID() const
{
	return m_strSubSystemIdentifier;
}


////////////////////////////////////////////////////////////////////////
//
//  CEcmSubSystem::operator<()
//
//  overloaded < operator
//
////////////////////////////////////////////////////////////////////////
bool CEcmSubSystem::operator< ( const CEcmSubSystem& crCEcmSubsystemRight ) const
{
	return (::strcmp(m_strSubSystemIdentifier.c_str(), crCEcmSubsystemRight.GetSubSystemID().c_str()) < 0);
}
