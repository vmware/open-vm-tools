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

#ifndef TCONSTMULTIMAPITERATOR_H_
#define TCONSTMULTIMAPITERATOR_H_

namespace Caf {

template <typename Container, typename ValueType = typename Container::mapped_type, typename KeyType = typename Container::key_type>
class TConstMultimapIterator
{
public:
	TConstMultimapIterator ()
	{
		m_citerCurrent = m_citerEnd;
	}

	TConstMultimapIterator (const Container &rcContainer)
	{
		m_citerCurrent = rcContainer.begin();
		m_citerEnd = rcContainer.end();
	}

	TConstMultimapIterator (const TConstMultimapIterator &rhs)
	{
		m_citerCurrent = rhs.m_citerCurrent;
		m_citerEnd = rhs.m_citerEnd;
	}

	TConstMultimapIterator &operator= (const TConstMultimapIterator &rhs)
	{
		m_citerCurrent = rhs.m_citerCurrent;
		m_citerEnd = rhs.m_citerEnd;
		return *this;
	}

public:
	const KeyType &getKey (void)
	{
		return m_citerCurrent->first;
	}

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

	operator bool ()
	{
		return (m_citerCurrent != m_citerEnd);
	}

private:
	typename Container::const_iterator m_citerCurrent;
	typename Container::const_iterator m_citerEnd;
};
}

#endif // #ifdef TCONSTMULTIMAPITERATOR_H_
