/////////////////////////////////////////////////////////////////////////////
//
//  Author:		J.P. Grossman
//
//	Purpose:	Return a lightweight iterator for an arbitrary container.
//				The iterator moves in the forward direction only; once
//				it reaches the end you need a new one.  I considered
//				adding m_citerBegin to allow for a Reset method, but
//				decided that it was better to have a "use once then
//				throw away" iterator to prevent the misuse of passing
//				the iterator around, possibly after the original
//				container has gone away.  This way use of the iterator
//				is localized, much like the use of standard iterators.
//
//  Created:	10/16/2003
//
//	Copyright (C) 2003-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
//	-- VMware Confidential
// 
/////////////////////////////////////////////////////////////////////////////

#ifndef TConstIterator_h_
#define TConstIterator_h_

namespace Caf {

template <typename Container, typename ValueType = typename Container::value_type>
class TConstIterator
{
public:
	TConstIterator ()
	{
		m_citerCurrent = m_citerEnd;
	}

	TConstIterator (const Container &rcContainer)
	{
		m_citerCurrent = rcContainer.begin();
		m_citerEnd = rcContainer.end();
	}

	TConstIterator (const TConstIterator &rhs)
	{
		m_citerCurrent = rhs.m_citerCurrent;
		m_citerEnd = rhs.m_citerEnd;
	}

	TConstIterator &operator= (const Container &rcContainer)
	{
		m_citerCurrent = rcContainer.begin();
		m_citerEnd = rcContainer.end();
		return *this;
	}

	TConstIterator &operator= (const TConstIterator &rhs)
	{
		m_citerCurrent = rhs.m_citerCurrent;
		m_citerEnd = rhs.m_citerEnd;
		return *this;
	}

public:
	const ValueType &operator* ()
	{
		return *m_citerCurrent;
	}

	const ValueType *operator-> ()
	{
		return m_citerCurrent.operator->();
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

#endif // #ifndef TConstIterator_h_
