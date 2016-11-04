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

#ifndef TSMARTCONSTMULTIMAPITERATOR_H_
#define TSMARTCONSTMULTIMAPITERATOR_H_

namespace Caf {

template <typename Container, typename SmartPtrType = typename Container::mapped_type, typename ValueType = typename SmartPtrType::class_type, typename KeyType = typename Container::key_type>
class TSmartConstMultimapIterator
{
public:
	TSmartConstMultimapIterator ()
	{
		m_citerCurrent = m_citerEnd;
	}

	TSmartConstMultimapIterator (const Container &rcContainer)
	{
		m_citerCurrent = rcContainer.begin();
		m_citerEnd = rcContainer.end();
	}

	TSmartConstMultimapIterator (const TSmartConstMultimapIterator &rhs)
	{
		m_citerCurrent = rhs.m_citerCurrent;
		m_citerEnd = rhs.m_citerEnd;
	}

	TSmartConstMultimapIterator &operator= (const TSmartConstMultimapIterator &rhs)
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

	operator bool ()
	{
		return (m_citerCurrent != m_citerEnd);
	}

private:
	typename Container::const_iterator m_citerCurrent;
	typename Container::const_iterator m_citerEnd;
};

}

#endif /* TSMARTCONSTMULTIMAPITERATOR_H_ */
