/////////////////////////////////////////////////////////////////////////////
//
//  Author:		J.P. Grossman
//
//  Created:	10/16/2003
//
//	Copyright (C) 2003-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
//	-- VMware Confidential
// 
/////////////////////////////////////////////////////////////////////////////

#ifndef TConstMapIterator_h_
#define TConstMapIterator_h_

namespace Caf {

template <typename Container, typename ValueType, typename KeyType> 
class TConstMultimapIterator;

template <typename Container, typename ValueType = typename Container::mapped_type, typename KeyType = typename Container::key_type>
class TConstMapIterator
{
public:
	TConstMapIterator ()
	{
		m_citerCurrent = m_citerEnd;
	}

	TConstMapIterator (const Container &rcContainer)
	{
		m_citerCurrent = rcContainer.begin();
		m_citerEnd = rcContainer.end();
	}

	TConstMapIterator (const TConstMapIterator &rhs)
	{
		m_citerCurrent = rhs.m_citerCurrent;
		m_citerEnd = rhs.m_citerEnd;
	}

	TConstMapIterator &operator= (const Container &rcContainer)
	{
		m_citerCurrent = rcContainer.begin();
		m_citerEnd = rcContainer.end();
		return *this;
	}

	TConstMapIterator &operator= (const TConstMapIterator &rhs)
	{
		m_citerCurrent = rhs.m_citerCurrent;
		m_citerEnd = rhs.m_citerEnd;
		return *this;
	}

public:
	const ValueType &operator* ()
	{
		return m_citerCurrent->second;
	}

	const ValueType *operator-> ()
	{
		return &m_citerCurrent->second;
	}

	void operator++ ()
	{
		++m_citerCurrent;
	}

	void operator++ (int32)
	{
		m_citerCurrent++;
	}

	const KeyType &getKey (void)
	{
		return m_citerCurrent->first;
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

#endif // #ifndef TConstMapIterator_h_
