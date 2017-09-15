/*
 *  Created: Oct 9, 2002
 *
 *	Copyright (C) 2002-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _TCafSubSystemObjectRoot_H_
#define _TCafSubSystemObjectRoot_H_

#include "EcmSubSystemBase.h"

namespace Caf {

template<typename Derived, typename LifetimeManager = TCafSubSystemObject<Derived> >
class TCafSubSystemObjectRoot
{
public:
	enum ReturnCodes {
		Success = 0x0,			// The call succeeded
		InvalidPointerValue,	// An invalid pointer was passed
		OutOfMemory,			// We're out of memory
		InterfaceNotSupported,	// The interface requested in not supported
		UnknownFailure			// Something happened that we didn't expect
	};

	// Creator Function
	static uint32 Creator(const IID& criid, void** ppv);
	static uint32 Creator(Derived** ppDerived);

protected:
	static void InternalQueryInterface(const void* cpThis,
									   const _CAF_INTMAP_ENTRY* cpEntries,
									   const IID& criid,
									   void** ppv);
	
protected:
	TCafSubSystemObjectRoot() {;}
	virtual ~TCafSubSystemObjectRoot() {;}

private:
	TCafSubSystemObjectRoot(const TCafSubSystemObjectRoot<Derived, LifetimeManager>&) {;}
	TCafSubSystemObjectRoot& operator=(const TCafSubSystemObjectRoot<Derived, LifetimeManager>&) { return *this; }
};

// Default Creator Function (can be overridden in Derived) - returns a QI'd void*
template<typename Derived, typename LifetimeManager>
uint32 TCafSubSystemObjectRoot<Derived, LifetimeManager>::Creator(const IID& criid, void** ppv)
{
	uint32 dwRetCode = Success;
	
	try
	{
		// Make sure the outer pointer seems valid.
		if(ppv != NULL)
		{
			// Initialize the out-param
			*ppv = NULL;

			// Create a new derived object
			Derived* pObj = NULL;
			dwRetCode = Creator( &pObj );

			if( Success == dwRetCode )
			{
				// QI to the requested interface
				pObj->QueryInterface(criid, ppv);

				// if *ppv is NULL...
				if(NULL == *ppv)
				{
					// then the interface requested is not supported.
					dwRetCode = InterfaceNotSupported;
				}

				// decrement the reference count.
				pObj->Release();
			}
		}
		// Otherwise...
		else
		{
			// set the return code to indicate an invalid pointer was passed.
			dwRetCode = InvalidPointerValue;
		}
	}
	// No exceptions should be thrown, but if one is...
	catch(...)
	{
		// set the return code to unknown failure occurred.
		dwRetCode = UnknownFailure;
	}

	return dwRetCode;
}

// Creator Function (can be overridden in Derived) - returns a pDerived
template<typename Derived, typename LifetimeManager>
uint32 TCafSubSystemObjectRoot<Derived, LifetimeManager>::Creator(Derived** ppDerived)
{
	uint32 dwRetCode = Success;
	
	try
	{
		// Make sure the outer pointer seems valid.
		if(ppDerived != NULL)
		{
			// Create the out-param
			*ppDerived = new LifetimeManager;
			
			// If we were able to allocate and create...
			if(NULL != *ppDerived)
			{
				// Increment the ref count
				(*ppDerived)->AddRef();
			}
			else
			{
				// set the return code to indicate out of memory.
				dwRetCode = OutOfMemory;
			}
		}
		// Otherwise...
		else
		{
			// set the return code to indicate an invalid pointer was passed.
			dwRetCode = InvalidPointerValue;
		}
	}
	// No exceptions should be thrown, but if one is...
	catch(...)
	{
		// set the return code to unknown failure occurred.
		dwRetCode = UnknownFailure;
	}

	return dwRetCode;
}

// Table driven query interface
template<typename Derived, typename LifetimeManager>
void TCafSubSystemObjectRoot<Derived, LifetimeManager>::InternalQueryInterface(const void* cpThis,
																			   const _CAF_INTMAP_ENTRY* cpEntries,
																			   const IID& criid,
																			   void** ppv)
{
	if(NULL != cpThis)
	{
		if(cpEntries != NULL)
		{
			if(ppv != NULL)
			{
				// Initialize the out-param
				*ppv = NULL;

				// If the iid passed in is the iid of IEcmObject...
				if(::IsEqualIID(criid, CAF_IIDOF(ICafObject)))
				{
					// get the first interface in the map,
					ICafObject* pObj =
						reinterpret_cast<ICafObject*>(
							(SUBSYS_INTPTR)cpThis+cpEntries->offset);
					
					// increment the reference count,
					pObj->AddRef();
					
					// set the out-param equal to the interface pointer.
					*ppv = pObj;
				}
				// Otherwise, we are looking for something other than
				// IEcmObject, so...
				else
				{
					// get a copy of the pointer to the entries so we can
					// increment it,
					_CAF_INTMAP_ENTRY* pEntries =
						const_cast<_CAF_INTMAP_ENTRY*>(cpEntries);
					
					// loop through the interface map entries...
					while((pEntries->cpiid != NULL) &&
						(pEntries->offset != SUBSYS_INTPTR_INVALID))
					{
						// if we find a matching IID...
						if ((NULL != pEntries->cpiid) &&
							(::IsEqualIID(*(pEntries->cpiid), criid)))
						{
							// get the interface at the offset from the map,
							ICafObject* pObj =
								reinterpret_cast<ICafObject*>(
									(SUBSYS_INTPTR)cpThis+pEntries->offset);

							// increment the reference count,
							pObj->AddRef();

							// set the out-param equal to the interface
							// pointer, and
							*ppv = pObj;

							// break out of the loop.
							break;
						}

						// Move to the next entry in the interface map.
						pEntries++;
					}
				}
			}
		}
	}
}

}

#endif // _TCafSubSystemObjectRoot_H_

