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

#ifndef TSMARTMULTIMAPITERATOR_H_
#define TSMARTMULTIMAPITERATOR_H_

namespace Caf {

template <typename Container, typename SmartPtrType = typename Container::mapped_type, typename ValueType = typename SmartPtrType::class_type, typename KeyType = typename Container::key_type>
class TSmartMultimapIterator
{
public:
	TSmartMultimapIterator ()
	{
		m_iterCurrent = m_iterEnd;
	}

	TSmartMultimapIterator (Container &rContainer)
	{
		m_iterCurrent = rContainer.begin();
		m_iterEnd = rContainer.end();
	}

	TSmartMultimapIterator (const TSmartMultimapIterator &rhs)
	{
		m_iterCurrent = rhs.m_iterCurrent;
		m_iterEnd = rhs.m_iterEnd;
	}

	TSmartMultimapIterator &operator= (const TSmartMultimapIterator &rhs)
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

	SmartPtrType &operator* ()
	{
		return m_iterCurrent->second;
	}

	ValueType *operator-> ()
	{
		return m_iterCurrent->second.operator->();
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

#endif /* TSMARTMULTIMAPITERATOR_H_ */
