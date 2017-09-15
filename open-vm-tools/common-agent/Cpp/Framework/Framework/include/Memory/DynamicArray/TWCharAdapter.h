//////////////////////////////////////////////////////////////////////////////
//
//	$Workfile:   TWCharAdapter.h  $
//
//	Author:		Greg Burk
//
//	Purpose:	This template provides an adapter for TDynamicArray (or
//              other types of "safe" array classes) that exposes functions
//				that are useful when working with an array of type wchar_t.
//
//				A typedef of this class is already defined and should be
//				used instead of explicitly using this class.  The typedef
//				is CEcmWCharArray.
//
//	Created:	Friday, October 18, 2002 1:59:49 PM
//
//	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
//	-- VMware Confidential
//
//	Modification History:
//
//	$Log:   //wpbuild01/PvcsData/ECM_40/archives/ECM_41/WinNT/Source/CommonAgtCol/Cpp/EcmCommonStaticMinDep/TWCharAdapter.h-arc  $
// 
//    Rev 1.2   17 Sep 2003 09:43:16   Michael.Donahue
// Implemented hooks for new library model
// 
//    Rev 1.1   16 Jan 2003 11:17:54   Greg.Burk
// Made changes necessary to accomodate new CEcmBasicString class and changes to CEcmString.
// 
//    Rev 1.0   31 Oct 2002 10:43:08   Greg.Burk
// Initial Revision
//
//////////////////////////////////////////////////////////////////////////////
#ifndef _TWCharAdapter_H_
#define _TWCharAdapter_H_

namespace Caf {

template<typename T>
class TWCharAdapter : public T
{
public:
	//////////////////////////////////////////////////////////////////////////
	// Default Constructor
	//////////////////////////////////////////////////////////////////////////
	TWCharAdapter() {}

	//////////////////////////////////////////////////////////////////////////
	// Conversion Constructor
	//////////////////////////////////////////////////////////////////////////
	TWCharAdapter(const char * rhs)
	{
		multiByteToWide(rhs);
	}

	//////////////////////////////////////////////////////////////////////////
	// Conversion Constructor
	//////////////////////////////////////////////////////////////////////////
	TWCharAdapter(const wchar_t * rhs)
	{
		(*this) = rhs;
	}

	//////////////////////////////////////////////////////////////////////////
	// Destructor
	//////////////////////////////////////////////////////////////////////////
	~TWCharAdapter() {}

	//////////////////////////////////////////////////////////////////////////
	// Assignment operator
	//////////////////////////////////////////////////////////////////////////
	TWCharAdapter& operator=(const char * rhs)
	{
		multiByteToWide(rhs);
		return *this;
	}

	//////////////////////////////////////////////////////////////////////////
	// Assignment operator
	//////////////////////////////////////////////////////////////////////////
	TWCharAdapter& operator=(const wchar_t * rhs)
	{
		const uint32 culLength = ::wcslen(rhs);

		if(culLength > 0)
		{
			this->allocateElements(culLength);
			wcsnCpy(rhs, culLength);
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

		if(!this->isNull())
		{
			wcslwr(this->m_ptData);
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
			wcsupr(this->m_ptData);
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
			wcsrev(this->m_ptData);
		}

		// Post-validation.
		this->verifySentinal();
	}

	//////////////////////////////////////////////////////////////////////////
	// WcsLen
	//
	// Return the length of the string.
	//////////////////////////////////////////////////////////////////////////
	size_t wcsLen() const
	{
		size_t stRetVal = 0;
		this->verifySentinal();
		this->verifyNotNull();
		stRetVal = ::wcslen(this->m_ptData, this->m_strDesc);
		this->verifyLength(static_cast<uint32>(stRetVal));
		return stRetVal;
	}

	//////////////////////////////////////////////////////////////////////////
	// WcsCpy
	//
	// Make a copy of the string into the buffer.
	//////////////////////////////////////////////////////////////////////////
	TWCharAdapter & wcsCpy(const wchar_t * cpwszSource)
	{
		// Pre-validation.
		this->verifySentinal();
		this->verifyNotNull();

		// Get the length of the source string.
		uint32 dwSourceLength = ::wcslen(cpwszSource);

		// Make sure the string will fit if we copy it.
		this->verifyLength(dwSourceLength);

		// Copy the string into the buffer.
		::wcscpy(this->m_ptData, cpwszSource);

		// Post-validation.
		this->verifySentinal();

		return (*this);
	}

	//////////////////////////////////////////////////////////////////////////
	// WcsnCpy
	//
	// Make a copy of the string into the buffer upto culCount characters.
	//////////////////////////////////////////////////////////////////////////
	TWCharAdapter & wcsnCpy(const wchar_t * cpwszSource, const uint32 culCount)
	{
		// Pre-validation.
		this->verifySentinal();
		this->verifyNotNull();

		// Make sure the string will fit if we copy it.
		this->verifyLength(culCount);

		// Copy the string into the buffer.
		::wcsncpy(this->m_ptData, cpwszSource, culCount);

		// Post-validation.
		this->verifySentinal();

		return (*this);
	}

	//////////////////////////////////////////////////////////////////////////
	// WcsStr
	//
	// Find the substring cpwszSubString in the array.  Returns the pointer to
	// the first occurance of the substring in the array or NULL if the
	// substring is not found.
	//////////////////////////////////////////////////////////////////////////
	const wchar_t * wcsStr(const wchar_t * cpwszSubString) const
	{
		wchar_t * pwszRetVal = NULL;

		// Pre-validation.
		this->verifySentinal();
		this->verifyNotNull();

		// Find the substring.
		pwszRetVal = ::wcsstr(this->m_ptData, cpwszSubString);

		// Post-validation.
		this->verifySentinal();

		return pwszRetVal;
	}

	//////////////////////////////////////////////////////////////////////////
	// WcsCmp
	//
	// Compare the cpwszString to this array.
	//////////////////////////////////////////////////////////////////////////
	int32 wcsCmp(const wchar_t * cpwszString) const
	{
		int32 iRetVal = 0;

		// Pre-validation.
		this->verifySentinal();
		this->verifyNotNull();

		// Compare the strings.
		iRetVal = ::wcscmp(this->m_ptData, cpwszString);

		return iRetVal;
	}

	//////////////////////////////////////////////////////////////////////////
	// WcsiCmp
	//
	// Compare the cpwszString to this array (case insensitive).
	//////////////////////////////////////////////////////////////////////////
	int32 wcsiCmp(const wchar_t * cpwszString) const
	{
		int32 iRetVal = 0;

		// Pre-validation.
		this->verifySentinal();
		this->verifyNotNull();

		// Compare the strings.
		iRetVal = wcsicmp(this->m_ptData, cpwszString);

		return iRetVal;
	}

	//////////////////////////////////////////////////////////////////////////
	// WcsnCmp
	//
	// Compare the cpwszString to this array up to culCount characters.
	//////////////////////////////////////////////////////////////////////////
	int32 wcsnCmp(const wchar_t * cpwszString, const uint32 culCount) const
	{
		int32 iRetVal = 0;

		// Pre-validation.
		this->verifySentinal();
		this->verifyNotNull();

		// Make sure the comparison can take place with in our array bounds.
		this->verifyLength(culCount);
			
		// Compare the strings.
		iRetVal = wcsncmp(this->m_ptData, cpwszString, culCount);

		return iRetVal;
	}

	//////////////////////////////////////////////////////////////////////////
	// WcsChr
	//
	// Find the first occurrence of the specified character in the string.
	//////////////////////////////////////////////////////////////////////////
	wchar_t * wcsChr(wchar_t wcCharacter) const
	{
		wchar_t * pwszRetVal = 0;

		// Pre-validation.
		this->verifySentinal();
		this->verifyNotNull();

		// Compare the strings.
		pwszRetVal = ::wcschr(this->m_ptData, wcCharacter);

		return pwszRetVal;
	}

private:
	void multiByteToWide(const char* cpszSource)
	{
		// Calculate the length of the source.
		const uint32 cdwSourceLen = cpszSource ? ::strlen( cpszSource ) : 0;

		// Convert the multibyte strings to wide.
		if( cdwSourceLen > 0 )
		{
			this->allocateElements(cdwSourceLen);
			int32 iRet;
#ifdef WIN32
			iRet = ::MultiByteToWideChar(
				CP_ACP,
				0,
				cpszSource,
				cdwSourceLen,
				GetNonConstPtr(),
				GetLength() );
#else
			iRet = ::mbstowcs( this->getNonConstPtr(), cpszSource, cdwSourceLen );
			// mbstowcs returns -1 on error or the number of
			// characters without the null terminator, so
			// we can increase it by 1 for the null or to 0 for
			// the error condition below
			if ( 0 != iRet )
				iRet++;
#endif
				
			if( 0 == iRet )
			{
				this->freeArray();
			}
		}
		else if( cpszSource != NULL )
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
	TWCharAdapter(const TWCharAdapter & crRhs);
	TWCharAdapter & operator=(const TWCharAdapter & crRhs);
};

}

#endif // _TWCharAdapter_H_
