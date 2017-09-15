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

#ifndef TSmartMapIterator_h_
#define TSmartMapIterator_h_

namespace Caf {

template <typename Container, typename SmartPtrType, typename ValueType, typename KeyType> class TSmartMultimapIterator;

template <typename Container, typename SmartPtrType = typename Container::mapped_type, typename ValueType = typename SmartPtrType::class_type, typename KeyType = typename Container::key_type>
class TSmartMapIterator
{
public:
	TSmartMapIterator ()
	{
		m_iterCurrent = m_iterEnd;
	}

	TSmartMapIterator (Container &rContainer)
	{
		m_iterCurrent = rContainer.begin();
		m_iterEnd = rContainer.end();
	}

	TSmartMapIterator (const TSmartMapIterator &rhs)
	{
		m_iterCurrent = rhs.m_iterCurrent;
		m_iterEnd = rhs.m_iterEnd;
	}

	TSmartMapIterator &operator= (const Container &rContainer)
	{
		m_iterCurrent = rContainer.begin();
		m_iterEnd = rContainer.end();
		return *this;
	}

	TSmartMapIterator &operator= (const TSmartMapIterator &rhs)
	{
		m_iterCurrent = rhs.m_iterCurrent;
		m_iterEnd = rhs.m_iterEnd;
		return *this;
	}

public:
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

#endif // #ifndef TSmartMapIterator_h_
