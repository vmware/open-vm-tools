/////////////////////////////////////////////////////////////////////////////
//
//  Author:		J.P. Grossman
//
//	Purpose:	Variant of TConstIterator that deals with smart pointers
//				transparently when the arrow operator is used.  ONLY works with 
//				containers that hold smart pointers.
//
//  Created:	10/22/2003
//
//	Copyright (C) 2003-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
//	-- VMware Confidential
//
/////////////////////////////////////////////////////////////////////////////

#ifndef TSmartConstIterator_h_
#define TSmartConstIterator_h_

namespace Caf {

template <typename Container, typename SmartPtrType = typename Container::value_type, typename ValueType = typename SmartPtrType::class_type>
class TSmartConstIterator
{
public:
	TSmartConstIterator ()
	{
		m_citerCurrent = m_citerEnd;
	}

	TSmartConstIterator (const Container &rcContainer)
	{
		m_citerCurrent = rcContainer.begin();
		m_citerEnd = rcContainer.end();
	}

	TSmartConstIterator (const TSmartConstIterator &rhs)
	{
		m_citerCurrent = rhs.m_citerCurrent;
		m_citerEnd = rhs.m_citerEnd;
	}

	TSmartConstIterator &operator= (const Container &rcContainer)
	{
		m_citerCurrent = rcContainer.begin();
		m_citerEnd = rcContainer.end();
		return *this;
	}

	TSmartConstIterator &operator= (const TSmartConstIterator &rhs)
	{
		m_citerCurrent = rhs.m_citerCurrent;
		m_citerEnd = rhs.m_citerEnd;
		return *this;
	}

public:
	const SmartPtrType &operator* ()
	{
		return *m_citerCurrent;
	}

	const ValueType *operator-> ()
	{
		return m_citerCurrent->operator->();
	}

	void operator++ ()
	{
		++m_citerCurrent;
	}

	void operator++ (int32)
	{
		m_citerCurrent++;
	}

	operator bool ()
	{
		return m_citerCurrent != m_citerEnd;
	}

private:
	typename Container::const_iterator m_citerCurrent;
	typename Container::const_iterator m_citerEnd;
};
}

#endif // #ifndef TSmartConstIterator_h_
