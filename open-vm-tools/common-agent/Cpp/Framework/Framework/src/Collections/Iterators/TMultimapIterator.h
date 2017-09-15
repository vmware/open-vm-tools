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

#ifndef TMULTIMAPITERATOR_H_
#define TMULTIMAPITERATOR_H_

namespace Caf {

template <typename Container, typename ValueType = typename Container::mapped_type, typename KeyType = typename Container::key_type>
class TMultimapIterator
{
public:
	TMultimapIterator ()
	{
		m_iterCurrent = m_iterEnd;
	}

	TMultimapIterator (Container &rContainer)
	{
		m_iterCurrent = rContainer.begin();
		m_iterEnd = rContainer.end();
	}

	TMultimapIterator (const TMultimapIterator &rhs)
	{
		m_iterCurrent = rhs.m_iterCurrent;
		m_iterEnd = rhs.m_iterEnd;
	}

	TMultimapIterator &operator= (const TMultimapIterator &rhs)
	{
		m_iterCurrent = rhs.m_iterCurrent;
		m_iterEnd = rhs.m_iterEnd;
		return *this;
	}

public:
	const KeyType &getKey (void)
	{
		return m_iterCurrent->first;
	}

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

	operator bool ()
	{
		return (m_iterCurrent != m_iterEnd);
	}

private:
	typename Container::iterator m_iterCurrent;
	typename Container::iterator m_iterEnd;
};
}

#endif /* TMULTIMAPITERATOR_H_ */
