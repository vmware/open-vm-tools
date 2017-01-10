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

#ifndef TSmartIterator_h_
#define TSmartIterator_h_

namespace Caf {

template <typename Container, typename SmartPtrType = typename Container::value_type, typename ValueType = typename SmartPtrType::class_type>
class TSmartIterator
{
public:
	TSmartIterator ()
	{
		m_iterCurrent = m_iterEnd;
	}

	TSmartIterator (Container &rContainer)
	{
		m_iterCurrent = rContainer.begin();
		m_iterEnd = rContainer.end();
	}

	TSmartIterator (const TSmartIterator &rhs)
	{
		m_iterCurrent = rhs.m_iterCurrent;
		m_iterEnd = rhs.m_iterEnd;
	}

	TSmartIterator &operator= (const Container &rContainer)
	{
		m_iterCurrent = rContainer.begin();
		m_iterEnd = rContainer.end();
		return *this;
	}

	TSmartIterator &operator= (const TSmartIterator &rhs)
	{
		m_iterCurrent = rhs.m_iterCurrent;
		m_iterEnd = rhs.m_iterEnd;
		return *this;
	}

public:
	SmartPtrType &operator* ()
	{
		return *m_iterCurrent;
	}

	ValueType *operator-> ()
	{
		return m_iterCurrent->operator->();
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
		return m_iterCurrent != m_iterEnd;
	}

private:
	typename Container::iterator m_iterCurrent;
	typename Container::iterator m_iterEnd;
};
}

#endif // #ifndef TSmartIterator_h_
