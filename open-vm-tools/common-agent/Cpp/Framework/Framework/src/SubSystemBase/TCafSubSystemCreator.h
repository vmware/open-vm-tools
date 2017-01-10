/*
 *  Created: Jan 30, 2003
 *
 *	Copyright (C) 2003-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _TCafSubSystemCreator_H_
#define _TCafSubSystemCreator_H_

#include "Exception/CCafException.h"

namespace Caf {

template< typename Derived, typename SmartPtr = TCafSmartPtr< Derived > >
class TCafSubSystemCreator
{
public:
	static SmartPtr CreateInstance();

	static void CreateInstance( uint32(*pfn)(const IID& criid, void** ppv), const IID& criid, void** ppv );

private:
	static void ValidateResultCode(const uint32 cdwRc);
};

////////////////////////////////////////////////////////
//
// CreateInstance()
//
// Creates an instance of the object and returns is as
// the supplied smart class type.
//
////////////////////////////////////////////////////////template<typename Derived>
template<typename Derived, typename SmartPtr>
SmartPtr TCafSubSystemCreator<Derived, SmartPtr>::CreateInstance()
{
	CAF_CM_STATIC_FUNC( "TCafSubSystemCreator",
						  "CreateInstance()" );

	// This local smart class object is transient.
	// If the lvalue is of the proper type then the
	// reference count will go to 2.  When the trasient
	// object dies then count will go to 1. Perfect.
	//
	// If the lvalue is not the proper type then
	// the reference count will remain at 1.  When the
	// transient object dies the count will go to 0
	// and the Derived object will be destroyed. Perfect.
	SmartPtr spcReturn;

	// Verify that the smart class template argument
	// is really a smart class object by calling
	// a method on it that must exist
	(void)spcReturn.GetNonAddRefedInterface();

	// Create a derived raw pointer
	Derived* pDerived = NULL;
	ValidateResultCode( Derived::Creator( &pDerived ) );

	// Assign it to the smart class which
	// will do an AddRef - count now = 2
	spcReturn = pDerived;

	// Release the raw pointer - count now = 1
	pDerived->Release();

	return spcReturn;
}

////////////////////////////////////////////////////////
//
// CreateInstance()
//
// Creates an instance of the object QI'd to the requested interface
// using the supplied creator function.
//
////////////////////////////////////////////////////////template<typename Derived>
template<typename Derived, typename SmartPtr>
void TCafSubSystemCreator<Derived, SmartPtr>::CreateInstance( uint32(*pfn)(const IID& criid, void** ppv), const IID& criid, void** ppv )
{
	ValidateResultCode( (*pfn)( criid, ppv ) );
}

////////////////////////////////////////////////////////
//
// ValidateResultCode()
//
// Maps the TCafSubSystemObjectRoot<>::Creator function result code
// to an exception and throw it.
//
////////////////////////////////////////////////////////template<typename Derived>
template<typename Derived, typename SmartPtr>
void TCafSubSystemCreator<Derived, SmartPtr>::ValidateResultCode(const uint32 cdwRc)
{
	CAF_CM_STATIC_FUNC( "TCafSubSystemCreator",
						  "ValidateResultCode" );

	if(TCafSubSystemObjectRoot<void, void>::Success == cdwRc)
	{
		// No point in evalulating the remaining clauses
	}
	// Otherwise, if the interface was not supported...
	else if(TCafSubSystemObjectRoot<void, void>::InterfaceNotSupported == cdwRc)
	{
		// throw an exception.
		CAF_CM_EXCEPTION_VA0(E_NOINTERFACE,
							 "The requested interface is not supported by the object requested.");
	}
	// Otherwise, if we are out of memory...
	else if(TCafSubSystemObjectRoot<void, void>::OutOfMemory == cdwRc)
	{
		// throw an exception.
		CAF_CM_EXCEPTION_EFAIL("Out of memory.");
	}
	// Otherwise, if the ppv was null...
	else if(TCafSubSystemObjectRoot<void, void>::InvalidPointerValue == cdwRc)
	{
		// throw an exception.
		CAF_CM_EXCEPTION_EFAIL("The ppv argument must not be NULL.");
	}
	// Otherwise, if something unexpected happened...
	else if(TCafSubSystemObjectRoot<void, void>::UnknownFailure == cdwRc)
	{
		// throw an exception.
		CAF_CM_EXCEPTION_EFAIL("An unexpected exception occurred while "
						   	   "trying to create requested object.");
	}
}

}

#endif // #ifndef _TCafSubSystemCreator_H_
