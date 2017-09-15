/*
 *  Created: Oct 09, 2002
 *
 *	Copyright (C) 2002-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "Exception/CCafException.h"
#include "CEcmSubSystemModule.h"
#include "CEcmSubSystemRegistry.h"
#include "CEcmDllManager.h"
#include "TCafSubSystemCreator.h"

using namespace Caf;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CEcmSubSystemModule::CEcmSubSystemModule() :
	CAF_CM_INIT_LOG("CEcmSubSystemModule"),
	m_pObjectEntries(NULL),
	m_hInstance(NULL),
	m_lLockCount(0)
{

}

CEcmSubSystemModule::~CEcmSubSystemModule()
{
}

void CEcmSubSystemModule::Init(const _CAF_OBJECT_ENTRY* const pObjectEntries,
							   const HINSTANCE hInstance)
{
	CAF_CM_FUNCNAME("Init");

	try
	{
		// Initialize the QI maps of all registered objects
		// to prevent multithreading problems
		uint32 dwIdx = 0;
		while (pObjectEntries[dwIdx].pfnGetEntries)
		{
			// Ignore the return value - we are just causing
			// the QI map to initialize by calling the func
			(void)pObjectEntries[dwIdx].pfnGetEntries();

			CAF_CM_LOG_DEBUG_VA3("Initializing object entries - index: %d, objId: %s, module: %p",
				dwIdx, pObjectEntries[dwIdx].pfnIdentifier(), hInstance);

			++dwIdx;
		}
	
		// Initialize the member variables
		m_pObjectEntries = pObjectEntries;
		m_hInstance = hInstance;
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_CLEAREXCEPTION;
}

void CEcmSubSystemModule::Term()
{
	CAF_CM_FUNCNAME("Term");

	try
	{
		// Loop through the Object Entries table and release the singletons
		uint32 dwIndex = 0;
		while(m_pObjectEntries && m_pObjectEntries[dwIndex].pfnCreator)
		{
			// If this is a sub-system...
			if(m_pObjectEntries[dwIndex].bIsSingleton && m_pObjectEntries[dwIndex].pCachedObject)
			{
				( m_pObjectEntries[dwIndex].pCachedObject )->Release();
				m_pObjectEntries[dwIndex].pCachedObject = NULL;
			}

			// Increment the index.
			++ dwIndex;
		}
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_CLEAREXCEPTION;

	// Set the instance handle to NULL.
	m_hInstance = NULL;

	// Set the object entries pointer to NULL.
	m_pObjectEntries = NULL;
}

void CEcmSubSystemModule::Lock()
{
	g_atomic_int_inc(&m_lLockCount);
}

void CEcmSubSystemModule::Unlock()
{
	(void)g_atomic_int_dec_and_test(&m_lLockCount);
}

void CEcmSubSystemModule::CreateInstance(const char* crstrIdentifier, const IID& criid, void** ppv)
{
	CAF_CM_FUNCNAME("CreateInstance");

	CAF_CM_VALIDATE_STRINGPTRA(crstrIdentifier);
	CAF_CM_VALIDATE_PTR(ppv);
		
	// Initialize the out-param
	*ppv = NULL;

	// Declare flag indicating that the identifier was found in the map.
	bool bFoundIdentifier = false;

	// Declare and index for iterating over the object map.
	uint32 dwIndex = 0;

	// Loop through the Object Entries table...
	CAF_CM_LOG_DEBUG_VA1("Looking for ID - %s", crstrIdentifier);
	while(m_pObjectEntries && m_pObjectEntries[dwIndex].pfnCreator)
	{
		// comparing the supplied identifier to the identifier in the
		// table.  If we find a match...
		CAF_CM_LOG_DEBUG_VA2("Comparing to - index: %d, objId: %s",
			dwIndex, m_pObjectEntries[dwIndex].pfnIdentifier());
		if(::strcmp(crstrIdentifier, m_pObjectEntries[dwIndex].pfnIdentifier()) == 0)
		{
			// indicate we found the identifier
			bFoundIdentifier = true;

#ifdef IMPLEMENT_SINGLETONS
			// If this is the subsystem singleton manager...
			if(::strcmp(crstrIdentifier, _sObjIdSubsystemSingletonManager) == 0)
			{
				// Check for a cached object.
				if(m_pObjectEntries[dwIndex].pCachedObject)
				{
					// Use the cached object.
					m_pObjectEntries[dwIndex].pCachedObject->QueryInterface(criid, ppv);
				}
				else
				{
					// Call the creator function associated with the identifier.
					createInstanceException = true;
					ICafObject* pEcmObject = 0;
					TCafSubSystemCreator<void>::CreateInstance( m_pObjectEntries[dwIndex].pfnCreator,
																CAF_IIDOF(ICafObject),
																reinterpret_cast<void**>(&pEcmObject));

					// Cache the object.
					pEcmObject->QueryInterface(criid, ppv);
					m_pObjectEntries[dwIndex].pCachedObject = pEcmObject;
					createInstanceException = false;
				}
			}
			// If this is a singleton...
			else if(m_pObjectEntries[dwIndex].bIsSingleton)
			{
				// Create a pointer to a subsystem singleton manager.
				IEcmSubsystemSingletonManager* piSingletonMgr = NULL;

				CAF_CM_TRY
				{
					// Create a subsystem object.
					CEcmSubSystem oSubSystem;

					// See if the subsystem is registered before trying to load it.
					if(oSubSystem.IsRegistered(gs_cstrObjIdEcmSubsystemSingletonManagerSubsystem))
					{
						// Load the subsystem singleton manager subsystem.
						oSubSystem.Load(gs_cstrObjIdEcmSubsystemSingletonManagerSubsystem);

						// Create an instance of the subsystem singleton manager object.
						oSubSystem.CreateInstance(gs_cstrObjIdEcmSubsystemSingletonManagerSubsystem,
							CSI_IIDOF(IEcmSubsystemSingletonManager), reinterpret_cast<void**>(&piSingletonMgr));
					}
				}
				CAF_CM_CATCH_KEEPEXCEPTION;

				// If we did not get an exception or is the singleton manager not registered.
				if(!(CAF_CM_ISEXCEPTION) && piSingletonMgr)
				{
					// Declare a IEcmObject pointer for local use.
					IEcmObject* pObject = NULL;

					// Check for a cached object.
					piSingletonMgr->GetSingleton(crstrIdentifier, &pObject);

					// Make sure the pointer is good.
					if(pObject)
					{
						// Query for the interface.
						pObject->QueryInterface(criid, ppv);

						// Release the object.
						pObject->Release();
						pObject = NULL;
					}
					// No cached object, so...
					else
					{
						// Call the creator function associated with the identifier.
						createInstanceException = true;
						TCafSubSystemCreator<void>::CreateInstance( m_pObjectEntries[dwIndex].pfnCreator,
																	CSI_IIDOF(IEcmObject),
																	reinterpret_cast<void**>(&pObject));

						// Query for the interface.
						pObject->QueryInterface(criid, ppv);
						createInstanceException = false;

						// Cache the object.
						piSingletonMgr->RegisterSingleton(crstrIdentifier, pObject);

						// Release the object we got back from CreateInstance.
						pObject->Release();
						pObject = NULL;
					}

					// Release the singleton manager.
					piSingletonMgr->Release();
					piSingletonMgr = NULL;
				}
				// We got an exception or the subsystem singleton manager is not registered.
				else
				{
					// Clean up the exception.
					if(CAF_CM_ISEXCEPTION)
					{
						CAF_CM_CLEAREXCEPTION;
					}

					// Release the piSingletonMgr if it is not null
					if(piSingletonMgr)
					{
						piSingletonMgr->Release();
						piSingletonMgr = NULL;
					}

					// Check for a cached object.
					if(m_pObjectEntries[dwIndex].pCachedObject)
					{
						// Use the cached object.
						m_pObjectEntries[dwIndex].pCachedObject->QueryInterface(criid, ppv);
					}
					else
					{
						// Call the creator function associated with the identifier.
						createInstanceException = true;
						IEcmObject* pEcmObject = 0;
						TCafSubSystemCreator<void>::CreateInstance( m_pObjectEntries[dwIndex].pfnCreator,
																	CSI_IIDOF(IEcmObject),
																	reinterpret_cast<void**>(&pEcmObject));

						// Cache the object.
						pEcmObject->QueryInterface(criid, ppv);
						m_pObjectEntries[dwIndex].pCachedObject = pEcmObject;
						createInstanceException = false;
					}
				}
			}
			else

#endif // IMPLEMENT_SINGLETONS
			{
				// call the creator function associated with the identifier.
				TCafSubSystemCreator<void>::CreateInstance( m_pObjectEntries[dwIndex].pfnCreator,
															criid,
															ppv);
			}

			// If we are here then an object was created
			break;
		}

		// Increment the index.
		++ dwIndex;
	}

	// If we were unable to find the identifier in the map...
	if(!bFoundIdentifier)
	{
		CAF_CM_EXCEPTION_VA1(E_FAIL,
							 "Unable to find object with provided identifier [%s]",
							 crstrIdentifier);
	}
}

bool CEcmSubSystemModule::CanUnload()
{
	gint value = g_atomic_int_get(&m_lLockCount);
	return (value == 0 );
}

HINSTANCE CEcmSubSystemModule::GetModuleHandle()
{
	return m_hInstance;
}
