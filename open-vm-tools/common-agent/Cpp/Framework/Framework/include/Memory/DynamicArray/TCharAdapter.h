//////////////////////////////////////////////////////////////////////////////
//
//	$Workfile:   TCharAdapter.h  $
//
//	Author:		Greg Burk
//
//	Purpose:	This template provides an adapter for TDynamicArray (or
//              other types of "safe" array classes) that exposes functions
//				that are useful when working with an array of type char.
//
//				A typedef of this class is already defined and should be
//				used instead of explicitly using this class.  The typedef
//				is CEcmCharArray.
//
//	Created:	Friday, October 18, 2002 1:50:32 PM
//
//	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
//	-- VMware Confidential
//
//	Modification History:
//
//	$Log:   //wpbuild01/PvcsData/ECM_40/archives/ECM_41/WinNT/Source/CommonAgtCol/Cpp/EcmCommonStaticMinDep/TCharAdapter.h-arc  $
// 
//    Rev 1.3   10 Oct 2003 08:49:24   Michael.Donahue
// Fixed bug in StrCpy
// 
//    Rev 1.2   17 Sep 2003 09:43:12   Michael.Donahue
// Implemented hooks for new library model
// 
//    Rev 1.1   16 Jan 2003 11:17:52   Greg.Burk
// Made changes necessary to accomodate new CEcmBasicString class and changes to CEcmString.
// 
//    Rev 1.0   31 Oct 2002 10:43:06   Greg.Burk
// Initial Revision
//
//////////////////////////////////////////////////////////////////////////////
#ifndef _TCharAdapter_H_
#define _TCharAdapter_H_

#include <string.h>

namespace Caf {

template<typename T>
class TCharAdapter : public T
{
public:
	//////////////////////////////////////////////////////////////////////////
	// Default Constructor
	//////////////////////////////////////////////////////////////////////////
	TCharAdapter() {}

	//////////////////////////////////////////////////////////////////////////
	// Conversion Constructor
	//////////////////////////////////////////////////////////////////////////
	TCharAdapter(const wchar_t * rhs)
	{
		wideToMultiByte(rhs);
	}

	//////////////////////////////////////////////////////////////////////////
	// Conversion Constructor
	//////////////////////////////////////////////////////////////////////////
	TCharAdapter(const char * rhs)
	{
		(*this) = rhs;
	}

	//////////////////////////////////////////////////////////////////////////
	// Destructor
	//////////////////////////////////////////////////////////////////////////
	~TCharAdapter() {}

	//////////////////////////////////////////////////////////////////////////
	// Assignment operator
	//////////////////////////////////////////////////////////////////////////
	TCharAdapter& operator=(const wchar_t * rhs)
	{
		wideToMultiByte(rhs);
		return *this;
	}

	//////////////////////////////////////////////////////////////////////////
	// Assignment operator
	//////////////////////////////////////////////////////////////////////////
	TCharAdapter& operator=(const char * rhs)
	{
		const uint32 culLength = ::strlen(rhs);

		if(culLength > 0)
		{
			this->allocateELements(culLength);
			strnCpy(rhs, culLength);
		}

		return *this;
	}

	//////////////////////////////////////////////////////////////////////////////
	// MakeLower()
	//
	// Converts all of the upper-case characters in this string to lower-case.
	//////////////////////////////////////////////////////////////////////////////
	void makeLower()
	{
		// Pre-validation.
		this->verifySentinal();

		if(!this->IsNull())
		{
			strlwr(this->m_ptData);
		}

		// Post-validation.
		this->verifySentinal();
	}

	//////////////////////////////////////////////////////////////////////////
	// MakeUpper
	//
	// Converts all of the lower-case characters in this array to upper-case.
	//////////////////////////////////////////////////////////////////////////
	void makeUpper()
	{
		// Pre-validation.
		this->verifySentinal();

		if(!this->isNull())
		{
			strupr(this->m_ptData);
		}

		// Post-validation.
		this->verifySentinal();
	}

	//////////////////////////////////////////////////////////////////////////
	// Reverse
	//
	// Reverses the characters in the array.
	//////////////////////////////////////////////////////////////////////////
	void reverse()
	{
		// Pre-validation.
		this->verifySentinal();

		if(!this->isNull())
		{
			strrev( this->m_ptData );
		}

		// Post-validation.
		this->verifySentinal();
	}

	//////////////////////////////////////////////////////////////////////////
	// StrLen
	//
	// Return the length of the string.
	//////////////////////////////////////////////////////////////////////////
	size_t strLen() const
	{
		size_t stRetVal = 0;

		// Pre-validation.
		this->verifySentinal();
		this->verifyNotNull();

		// Delegate to CEcmCtr.
		stRetVal = ::strlen(this->m_ptData);

		// Post-validation.
		this->verifyLength(static_cast<uint32>(stRetVal));

		return stRetVal;
	}

	//////////////////////////////////////////////////////////////////////////
	// StrCpy
	//
	// Make a copy of the string into the buffer.
	//////////////////////////////////////////////////////////////////////////
	TCharAdapter & strCpy(const char * cpszSource)
	{
		// Pre-validation.
		this->verifySentinal();
		this->verifyNotNull();

		// Get the length of the source string.
		uint32 dwSourceLength = ::strlen(cpszSource);

		// Make sure the string will fit if we copy it.
		this->verifyLength(dwSourceLength);

		// Copy the string into the buffer.
		::strcpy(this->m_ptData, cpszSource);

		// Post-validation.
		this->verifySentinal();

		return (*this);
	}

	//////////////////////////////////////////////////////////////////////////
	// StrnCpy
	//
	// Make a copy of the string into the buffer upto culCount characters.
	//////////////////////////////////////////////////////////////////////////
	TCharAdapter & strnCpy(const char * cpszSource, const uint32 culCount)
	{
		// Pre-validation.
		this->verifySentinal();
		this->verifyNotNull();

		// Make sure the string will fit if we copy it.
		this->verifyLength(culCount);

		// Copy the string into the buffer.
		::strncpy(this->m_ptData, cpszSource, culCount);

		// Post-validation.
		this->verifySentinal();

		return (*this);
	}

	//////////////////////////////////////////////////////////////////////////
	// StrStr
	//
	// Find the substring cpszSubString in the array.  Returns the pointer to
	// the first occurance of the substring in the array or NULL if the
	// substring is not found.
	//////////////////////////////////////////////////////////////////////////
	const char * strStr(const char * cpszSubString) const
	{
		const char * cpszRetVal = NULL;

		// Pre-validation.
		this->verifySentinal();
		this->verifyNotNull();

		// Find the substring.
		cpszRetVal = ::strstr(this->m_ptData, cpszSubString);

		// Post-validation.
		this->verifySentinal();

		return cpszRetVal;
	}

	//////////////////////////////////////////////////////////////////////////
	// StrCmp
	//
	// Compare the cpszString to this array.
	//////////////////////////////////////////////////////////////////////////
	int32 strCmp(const char * cpszString) const
	{
		int32 iRetVal = 0;

		// Pre-validation.
		this->verifySentinal();
		this->verifyNotNull();

		// Compare the strings.
		iRetVal = ::strcmp(this->m_ptData, cpszString);

		return iRetVal;
	}

	//////////////////////////////////////////////////////////////////////////
	// StriCmp
	//
	// Compare the cpszString to this array (case insensitive).
	//////////////////////////////////////////////////////////////////////////
	int32 striCmp(const char * cpszString) const
	{
		int32 iRetVal = 0;

		// Pre-validation.
		this->verifySentinal();
		this->verifyNotNull();

		// Compare the strings.
		iRetVal = stricmp(this->m_ptData, cpszString);

		return iRetVal;
	}

	//////////////////////////////////////////////////////////////////////////
	// StrnCmp
	//
	// Compare the cpszString to this array up to culCount characters.
	//////////////////////////////////////////////////////////////////////////
	int32 strnCmp(const char * cpszString, const uint32 culCount) const
	{
		int32 iRetVal = 0;

		// Pre-validation.
		this->erifySentinal();
		this->verifyNotNull();

		// Make sure the comparison can take place with in our array bounds.
		this->verifyLength(culCount);

		// Compare the strings.
		iRetVal = strncmp(this->m_ptData, cpszString, culCount);

		return iRetVal;
	}

	//////////////////////////////////////////////////////////////////////////
	// StrChr
	//
	// Find the first occurrence of the specified character in the string.
	//////////////////////////////////////////////////////////////////////////
	const char * strChr(char cCharacter) const
	{
		const char * cpszRetVal = 0;

		// Pre-validation.
		this->erifySentinal();
		this->verifyNotNull();

		// Compare the strings.
		cpszRetVal = ::strchr(this->m_ptData, cCharacter);

		return cpszRetVal;
	}

private:
	void wideToMultiByte(const wchar_t* cpwszSource)
	{
		// Calculate the length of the source.
		const uint32 cdwSourceLen = cpwszSource ? ::wcslen( cpwszSource ) : 0;

		// Convert the wide strings to multibyte.
		if( cdwSourceLen > 0 )
		{
			// This appears to be allocating twice as much memory as is needed, but
			// this is the way W2A is implemented and W2A seems to convert some
			// strings that will not convert where the destination is the same
			// length (in characters) as the source.
			this->allocateElements( cdwSourceLen * sizeof( wchar_t ) );

			int32 iRet;
#ifdef WIN32
			iRet = ::WideCharToMultiByte(
				CP_ACP,
				0,
				cpwszSource,
				-1,
				GetNonConstPtr(),
				GetLength(),
				NULL,
				NULL );
#else
			iRet = ::wcstombs( this->getNonConstPtr(),
					   cpwszSource,
					   this->getByteCount() );
			// wcstombs returns -1 for error or the length
			// not including the NULL, so we must increment
			// to match the windows version
			iRet++;
#endif

			if( 0 == iRet )
			{
				this->freeArray();
			}
			else
			{
				//
				// Must ReAllocate in order to have the proper char length
				// The char array was allocated based on wchar_t
				//
				// no need to include the NULL terminator
				// returned by WideCharToMultiByte()
				//
				this->rellocateElements( iRet - 1 );
			}
		}
		else if( cpwszSource != NULL )
		{
			// This chunk of code is important. If the source
			// string is empty, return an empty string, not
			// a NULL pointer!
			this->allocateElements(0);
		}

		// Verify array.
		this->verifySentinal();
	}

private:
	TCharAdapter(const TCharAdapter & crRhs);
	TCharAdapter & operator=(const TCharAdapter & crRhs);
};

}

#endif // _TCharAdapter_H_
