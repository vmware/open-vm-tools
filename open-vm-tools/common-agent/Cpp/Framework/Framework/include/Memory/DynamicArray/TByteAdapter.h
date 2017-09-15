//////////////////////////////////////////////////////////////////////////////
//  
//  $Workfile:   TByteAdapter.h  $
//  
//  Author:     Phil Smith	
//
//  Purpose:    This template provides the ability to use a ${TDynamicArray}
//				or a TStaticArray where a byte pointer is required. It
//				provides a const byte* pointer conversion operator for use
//				when read only access is required and a function to get the 
//				pointer for write access. Use the byte adapter as follows:
//					TByteAdapter<TDynamicArray<typename T> >
//
//				Predefined character array objects CEcmCharArray and 
//				CEcmWCharArray which use the byte adapter have been provided 
//				in EcmCommonStaticMinDepInc.h. 
//
//  Created: Tuesday, August 20, 2002 10:00:00 AM
//  
//	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
//	-- VMware Confidential
//  
//  Modification History:
//  
//  $Log:   //wpbuild01/PvcsData/ECM_40/archives/ECM_41/WinNT/Source/CommonAgtCol/Cpp/EcmCommonStaticMinDep/TByteAdapter.h-arc  $
// 
//    Rev 1.4   17 Sep 2003 09:43:12   Michael.Donahue
// Implemented hooks for new library model
// 
//    Rev 1.3   31 Oct 2002 13:58:02   Greg.Burk
// Modified as per code review recomendations.
// 
//    Rev 1.2   15 Oct 2002 17:22:58   Phillip.Smith
// Documentation updates.
// 
//    Rev 1.0   09 Oct 2002 13:42:12   brian.williams
// Initial revision.
// 
//////////////////////////////////////////////////////////////////////////////
#ifndef _TByteAdapter_H_
#define _TByteAdapter_H_

namespace Caf {

template<typename T>
class TByteAdapter : public T
{
public:
	//////////////////////////////////////////////////////////////////////////
	// Default Constructor
	//////////////////////////////////////////////////////////////////////////
	TByteAdapter(const wchar_t * pwszDesc = NULL) {}

	//////////////////////////////////////////////////////////////////////////
	// Destructor
	//////////////////////////////////////////////////////////////////////////
	~TByteAdapter() {}

	//////////////////////////////////////////////////////////////////////////
	// GetNonConstBytePtr
	//
	// Get non-const pointer to internal data converted to btye *. 
	// This function should be used only when you must get a pointer that is
	// to be written to, and you should always call the ${TDynamicArray::Verify()}
	// function after modifying the data pointed to by this pointer or passing
	// the pointer to a function that modifies the data pointed to by this
	// pointer.
	//////////////////////////////////////////////////////////////////////////
	byte * getNonConstBytePtr()
	{
		this->verify();
		return reinterpret_cast<byte*>(this->getNonConstPtr());
	}

	//////////////////////////////////////////////////////////////////////////
	// GetBytePtr
	//
	// Get const pointer to internal data converted to btye *. 
	//////////////////////////////////////////////////////////////////////////
	const byte * getBytePtr() const
	{
		this->verify();
		return reinterpret_cast<const byte*>(this->getPtr());
	}

	//////////////////////////////////////////////////////////////////////////
	// const byte Conversion Operator
	//
	// Get const pointer to internal data converted to btye *. 
	//////////////////////////////////////////////////////////////////////////
	operator const byte * () const
	{
		this->verify();
		return reinterpret_cast<const byte*>(this->getPtr());
	}

private:
	TByteAdapter(const TByteAdapter & crRhs);
	TByteAdapter & operator=(const TByteAdapter & crRhs);
};

}

#endif // _TByteAdapter_H_
