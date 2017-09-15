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

#ifndef TIterator_h_
#define TIterator_h_

namespace Caf {

template <typename Container, typename ValueType = typename Container::value_type>
class TIterator
{
public:
	TIterator ()
	{
		m_iterCurrent = m_iterEnd;
	}

	TIterator (Container &rContainer)
	{
		m_iterCurrent = rContainer.begin();
		m_iterEnd = rContainer.end();
	}

	TIterator (const TIterator &rhs)
	{
		m_iterCurrent = rhs.m_iterCurrent;
		m_iterEnd = rhs.m_iterEnd;
	}

	TIterator &operator= (const Container &rContainer)
	{
		m_iterCurrent = rContainer.begin();
		m_iterEnd = rContainer.end();
		return *this;
	}

	TIterator &operator= (const TIterator &rhs)
	{
		m_iterCurrent = rhs.m_iterCurrent;
		m_iterEnd = rhs.m_iterEnd;
		return *this;
	}

public:
	ValueType &operator* ()
	{
		return *m_iterCurrent;
	}

	ValueType *operator-> ()
	{
		return m_iterCurrent.operator->();
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

#endif // #ifndef TIterator_h_
