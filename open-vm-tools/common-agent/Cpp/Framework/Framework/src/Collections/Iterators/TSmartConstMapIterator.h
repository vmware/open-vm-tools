/////////////////////////////////////////////////////////////////////////////
//
//  Author:		J.P. Grossman
//
//  Created:	10/22/2003
//
//	Copyright (C) 2003-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
//	-- VMware Confidential
//
/////////////////////////////////////////////////////////////////////////////

#ifndef TSmartConstMapIterator_h_
#define TSmartConstMapIterator_h_

namespace Caf {

template <typename Container, typename SmartPtrType, typename ValueType, typename KeyType> class TSmartConstMultimapIterator;

template <typename Container, typename SmartPtrType = typename Container::mapped_type, typename ValueType = typename SmartPtrType::class_type, typename KeyType = typename Container::key_type>
class TSmartConstMapIterator
{
public:
	TSmartConstMapIterator ()
	{
		m_citerCurrent = m_citerEnd;
	}

	TSmartConstMapIterator (const Container &rcContainer)
	{
		m_citerCurrent = rcContainer.begin();
		m_citerEnd = rcContainer.end();
	}

	TSmartConstMapIterator (const TSmartConstMapIterator &rhs)
	{
		m_citerCurrent = rhs.m_citerCurrent;
		m_citerEnd = rhs.m_citerEnd;
	}

	TSmartConstMapIterator &operator= (const Container &rcContainer)
	{
		m_citerCurrent = rcContainer.begin();
		m_citerEnd = rcContainer.end();
		return *this;
	}

	TSmartConstMapIterator &operator= (const TSmartConstMapIterator &rhs)
	{
		m_citerCurrent = rhs.m_citerCurrent;
		m_citerEnd = rhs.m_citerEnd;
		return *this;
	}

public:
	const SmartPtrType &operator* ()
	{
		return m_citerCurrent->second;
	}

	const ValueType *operator-> ()
	{
		return m_citerCurrent->second.operator->();
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

#endif // #ifndef TSmartConstMapIterator_h_
