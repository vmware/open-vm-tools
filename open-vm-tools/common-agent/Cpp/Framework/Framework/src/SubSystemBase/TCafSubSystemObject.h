/*
 *  Created: Oct 9, 2002
 *
 *	Copyright (C) 2002-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _TCafSubSystemObject_H_
#define _TCafSubSystemObject_H_

namespace Caf {

template <class Base>
class TCafSubSystemObject : public Base
{
public:  //  Constructor / Destructor
	TCafSubSystemObject() : m_lRefCnt( 0 )
	{
		// There is no module to lock if we are not in a sub-system
		// project, so conditionally define.
		#ifdef CAF_SUBSYSTEM

		// Lock the module.
		_Module.Lock();

		#endif
	}

	virtual ~TCafSubSystemObject()
	{
		// There is no module to unlock if we are not in a sub-system
		// project, so conditionally define.
		#ifdef CAF_SUBSYSTEM

		// Unlock the module.
		_Module.Unlock();
		
		#endif
	}

public: // IEcmSubSystemObject Implementations
	virtual void AddRef()
	{
		g_atomic_int_inc(&m_lRefCnt);
	}

	virtual void Release()
	{
		// If the ref-count is greater than 0...
		if (g_atomic_int_dec_and_test(&m_lRefCnt))
		{
			// delete self.
			delete this;
		}
	}

	virtual void QueryInterface(const IID& riid, void** ppv)
	{
		this->_InternalQueryInterface(riid, ppv);
	}

private:
	gint m_lRefCnt;

private:  // Deny copy and assignment
	TCafSubSystemObject(const TCafSubSystemObject&);
	TCafSubSystemObject& operator=(const TCafSubSystemObject&);
};

}

#endif // _TCafSubSystemObject_H_

