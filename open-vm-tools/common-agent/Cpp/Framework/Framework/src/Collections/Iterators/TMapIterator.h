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

#ifndef TMapIterator_h_
#define TMapIterator_h_

namespace Caf {

template <typename Container, typename ValueType, typename KeyType> class TMultimapIterator;

template <typename Container, typename ValueType = typename Container::mapped_type, typename KeyType = typename Container::key_type>
class TMapIterator
{
public:
	TMapIterator ()
	{
		m_iterCurrent = m_iterEnd;
	}

	TMapIterator (Container &rContainer)
	{
		m_iterCurrent = rContainer.begin();
		m_iterEnd = rContainer.end();
	}

	TMapIterator (const TMapIterator &rhs)
	{
		m_iterCurrent = rhs.m_iterCurrent;
		m_iterEnd = rhs.m_iterEnd;
	}

	TMapIterator &operator= (const Container &rContainer)
	{
		m_iterCurrent = rContainer.begin();
		m_iterEnd = rContainer.end();
		return *this;
	}

	TMapIterator &operator= (const TMapIterator &rhs)
	{
		m_iterCurrent = rhs.m_iterCurrent;
		m_iterEnd = rhs.m_iterEnd;
		return *this;
	}

public:
	ValueType &operator* ()
	{
		return m_iterCurrent->second;
	}

	ValueType *operator-> ()
	{
		return &m_iterCurrent->second;
	}

	void operator++ ()
	{
		++m_iterCurrent;
	}

	void operator++ (int32)
	{
		m_iterCurrent++;
	}

	const KeyType &getKey (void)
	{
		return m_iterCurrent->first;
	}

	operator bool ()
	{
		return m_iterCurrent != m_iterEnd;
	}

private:
	typename Container::iterator m_iterCurrent;
	typename Container::iterator m_iterEnd;
};
}

#endif // #ifndef TMapIterator_h_
