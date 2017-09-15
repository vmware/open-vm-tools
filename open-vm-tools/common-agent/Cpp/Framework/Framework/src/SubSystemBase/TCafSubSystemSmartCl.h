/*
 *  Created: Oct 9, 2002
 *
 *	Copyright (C) 2002-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */
#ifndef _TCafSubSystemSmartCl_H
#define _TCafSubSystemSmartCl_H

#include "EcmSubSystemBase.h"
#include "Exception/CCafException.h"
#include "CEcmSubSystem.h"
#include <map>

namespace Caf {

struct CafSubSystemSmartClToken { ICafObject *m_pCl; CafSubSystemSmartClToken(ICafObject* p) : m_pCl(p) {} };

// this routine should really be moved to a real extern
inline BOOL CafSubSystemSmartClIsSameObject(ICafObject *pObj1, ICafObject *pObj2)
{
    BOOL bResult = TRUE;
    if( pObj1 != pObj2) 
    {
        ICafObject *p1 = 0, *p2 = 0;
        pObj1->QueryInterface(CAF_IIDOF(ICafObject), (void**)&p1);
        if( p1 ) p1->Release(); else p1 = 0;
        pObj2->QueryInterface(CAF_IIDOF(ICafObject), (void**)&p2);
        if( p2 ) p2->Release(); else p2 = 0;
        bResult = (p1 == p2); // values do not change after release
    }
    return bResult;
}

inline BOOL CafSubSystemSmartClIsSameObject(ICafObject *pObj1, const CafSubSystemSmartClToken& pObj2)
{
    return CafSubSystemSmartClIsSameObject(pObj1, pObj2.m_pCl);
}

inline BOOL CafSubSystemSmartClIsSameObject(const CafSubSystemSmartClToken& pObj1, ICafObject *pObj2)
{
    return CafSubSystemSmartClIsSameObject(pObj1.m_pCl, pObj2);
}

inline BOOL CafSubSystemSmartClIsSameObject(const CafSubSystemSmartClToken& pObj1, const CafSubSystemSmartClToken& pObj2)
{
    return CafSubSystemSmartClIsSameObject(pObj1.m_pCl, pObj2.m_pCl);
}

/**
 * @brief Template for classes exposed as subsystem objects
 */
template <class Cl>
class TCafSubSystemSmartCl
{
private:
    typedef TCafSubSystemSmartCl<Cl> SameSmartType;

public:  // constructors/destructors

	// default constructor
    TCafSubSystemSmartCl(void) :
		CAF_CM_INIT( "TCafSubSystemSmartC" ),
		m_pCl( 0 )
	{
    }

	// homogeneous raw constructor
	TCafSubSystemSmartCl(Cl* rhs) :
		CAF_CM_INIT( "TCafSubSystemSmartC" ),
		m_pCl( rhs )
    {
		if( NULL != m_pCl )
			m_pCl->AddRef();
    }

	// homogeneous smart constructor
	TCafSubSystemSmartCl(const SameSmartType& rhs) :
		CAF_CM_INIT( "TCafSubSystemSmartC" ),
		m_pCl( rhs.m_pCl )
    {
		if( NULL != m_pCl )
			m_pCl->AddRef();
    }

	// heterogeneous raw constructor
    TCafSubSystemSmartCl(ICafObject* rhs) :
		CAF_CM_INIT( "TCafSubSystemSmartC" ),
		m_pCl( 0 )
    {
		if( rhs )
		{
			// Do QueryInterface (throws exception if QI fails)
			rhs->QueryInterface( GetIID(), ( void ** ) &m_pCl );
		}
    }

	// heterogeneous smart constructor
	TCafSubSystemSmartCl(const CafSubSystemSmartClToken& rhs) :
		CAF_CM_INIT( "TCafSubSystemSmartC" ),
        m_pCl( 0 )
    {
		if( rhs.m_pCl )
		{
			// Do QueryInterface (throws exception if QI fails)
			rhs.m_pCl->QueryInterface( GetIID(), ( void ** ) &m_pCl );
		}
    }

	// destructor
    ~TCafSubSystemSmartCl(void)
    {
		SafeRelease();
    }

    operator CafSubSystemSmartClToken(void) const
    {
        return CafSubSystemSmartClToken( m_pCl );
    }

public:  // Attach operations

    // homogeneous raw attachment
    void Attach(Cl* rhs)
    {
		if( rhs != m_pCl )
		{
			SafeRelease();
			if( m_pCl = rhs )
				m_pCl->AddRef();
		}
    }

	// homogeneous smart attachment
    void Attach(const SameSmartType& rhs)
    {
		Attach( rhs.m_pCl );
    }

	// heterogeneous raw attachment
    void Attach(ICafObject* rhs)
    {
		SafeRelease();
		if( rhs )
		{
			rhs->QueryInterface( GetIID(), ( void ** ) &m_pCl );
		}
    }

	// heterogeneous smart attachment
    void Attach(const CafSubSystemSmartClToken& rhs)
    {
		Attach( rhs.m_pCl );
    }

public:  // assignment operators

	// homogeneous raw assignment
    SameSmartType& operator = (Cl *rhs)
    {
		Attach( rhs );
		return *this;
    }

	// homogeneous smart assignment
    SameSmartType& operator = (const SameSmartType& rhs)
    {
		Attach( rhs );
		return *this;
    }

	// heterogeneous raw assignment
    SameSmartType& operator = (ICafObject * rhs)
    {
		Attach( rhs );
		return *this;
    }

	// heterogeneous smart assignment
    SameSmartType& operator = (const CafSubSystemSmartClToken& rhs)
    {
		Attach( rhs );
        return *this;
    }

public:  // equivalence operators (note - no identity tests performed here!)

    BOOL operator == (Cl * rhs)
    {
        return m_pCl == rhs;
    }

    BOOL operator == (const SameSmartType& rhs)
    {
        return m_pCl == rhs.m_pCl;
    }

    BOOL operator != (Cl *rhs)
    {
        return m_pCl != rhs;
    }

    BOOL operator != (const SameSmartType& rhs)
    {
        return m_pCl != rhs.m_pCl;
    }

public:  // CoCreateInstance wrappers    
    void CreateInstance(const std::string& strClassIdentifier, const bool cbIsExceptionOnFailure = true)
    {
    	CCafException* _cm_exception_ = NULL;
		SafeRelease();

		try
		{
			CEcmSubSystem oSubSystem;
			oSubSystem.Load(strClassIdentifier);
			oSubSystem.CreateInstance(strClassIdentifier, GetIID(), AsPPVArg());
		}
		catch (CCafException *e)
		{
			_cm_exception_ = e;
		}

		if( _cm_exception_ )
		{
			if( cbIsExceptionOnFailure )
			{
				_cm_exception_->throwSelf();
			}
			else
			{
				_cm_exception_->Release();
			}
		}
    }

private:
    const IID& GetIID(void) const
    {
		return CAF_IIDOF(Cl);
    }

    void** AsPPVArg(void)
    {
        SafeRelease();
        return (void **)&m_pCl;
    }

public:  // operations

// note: no Cl * operator is allowed, as it makes it very
//       easy to break the protocol of COM. Instead, these
//       four operations require the user to be explicit

    Cl* GetAddRefedInterface(void) const
    { 
        if( m_pCl)
            m_pCl->AddRef();
        return m_pCl;
    }

    Cl* GetNonAddRefedInterface(void) const
    { 
        return m_pCl;
    }

    Cl** GetReleasedInterfaceReference(void)
    { 
        SafeRelease();
        return &m_pCl;
    }

    Cl** GetNonReleasedInterfaceReference(void)
    { 
        return &m_pCl;
    }

    BOOL operator ! (void) const
    {
        return m_pCl == 0;
    }

    BOOL IsOK(void) const
    {
        return m_pCl != 0;
    }

    // instead of operator bool, we return a fake ptr type to allow if( pFoo) usage
    // but to disallow if( pFoo == pBar) which is probably wrong
    class PseudoBool {};
    operator PseudoBool * (void) const 
    {
        return (PseudoBool *)m_pCl;
    }

    // the arrow operator returns a pointer with AddRef and Release disabled
    class NoAddRefOrRelease : public Cl {
    private:
		virtual void AddRef() = 0;
		virtual void Release() = 0;
    };
    
	NoAddRefOrRelease *operator ->(void)
    {
        return (NoAddRefOrRelease *)m_pCl;
    }

    NoAddRefOrRelease *operator ->(void) const
    {
        return (NoAddRefOrRelease *)m_pCl;
    }

	static Cl* GetAddRefedQueryInterface(ICafObject* pICafObject)
	{
		CAF_CM_STATIC_FUNC( "TCafSubSystemSmartC", "GetAddRefedQueryInterface" );
		CAF_CM_VALIDATE_INTERFACEPTR( pICafObject );

		// QI to the interface and validate that the QI was successful.
		Cl* pCl = 0;
		pICafObject->QueryInterface( CAF_IIDOF(Cl), ( void ** ) &pCl );

		return pCl;
	}

	static Cl* GetNonAddRefedQueryInterface(ICafObject* pICafObject)
	{
		CAF_CM_STATIC_FUNC( "TCafSubSystemSmartC", "GetNonAddRefedQueryInterface" );
		CAF_CM_VALIDATE_INTERFACEPTR( pICafObject );

		// QI to the interface and validate that the QI was successful.
		Cl* pCl = 0;
		pICafObject->QueryInterface( CAF_IIDOF(Cl), ( void ** ) &pCl );

		// QueryInterface automatically AddRefs the interface, so it must be released.
		if( pCl )
			pCl->Release();

		return pCl;
	}

private:
	CAF_CM_CREATE;
    Cl* m_pCl;

private:
    void SafeRelease(void)
    {
		if( m_pCl ) 
			m_pCl->Release();
		m_pCl = 0;
    }
};

template <>
class TCafSubSystemSmartCl<ICafObject>
{
private:
    typedef ICafObject Cl;
    typedef TCafSubSystemSmartCl<Cl> SameSmartType;

public:  // constructors/destructors

	// default constructor
    TCafSubSystemSmartCl(void) :
		CAF_CM_INIT( "TCafSubSystemSmartC" ),
        m_pCl(0)
    {
    }

	// homogeneous raw constructor
	TCafSubSystemSmartCl(Cl *rhs) :
		CAF_CM_INIT( "TCafSubSystemSmartC" ),
		m_pCl(rhs)
    {
        if (NULL != m_pCl) 
            m_pCl->AddRef();
    }

	// homogeneous smart constructor
	TCafSubSystemSmartCl(const SameSmartType& rhs) :
		CAF_CM_INIT( "TCafSubSystemSmartC" ),
		m_pCl(rhs.m_pCl)
    {
        if (NULL != m_pCl) 
            m_pCl->AddRef();
    }

    operator CafSubSystemSmartClToken (void) const
    {
        return CafSubSystemSmartClToken(m_pCl);
    }

	// heterogeneous smart constructor (AddRef's instead of QI)
    TCafSubSystemSmartCl(const CafSubSystemSmartClToken& rhs) :
		CAF_CM_INIT( "TCafSubSystemSmartC" ),
        m_pCl(0)
    {
		if ( rhs.m_pCl )
		{
        	m_pCl = rhs.m_pCl;
            m_pCl->AddRef();
		}
    }

	// destructor
	~TCafSubSystemSmartCl(void)
    {
        SafeRelease();
    }

public:  // Attach operations

	// homogeneous raw attachment
    void Attach(Cl * rhs)
    {
        if (rhs != m_pCl)
        {
            SafeRelease();
			if ( rhs )
			{
            	m_pCl = rhs;
                m_pCl->AddRef();
			}
        }
    }

	// homogeneous smart attachment
    void Attach(const SameSmartType& rhs)
    {
        Attach(rhs.m_pCl);
    }

	// heterogeneous smart attachment
    void Attach(const CafSubSystemSmartClToken& rhs)
    {
        Attach(rhs.m_pCl);
    }

public:  // assignment operators
	// homogeneous raw assignment
    SameSmartType& operator = (Cl *rhs)
    {
        Attach(rhs);
        return *this;
    }

	// homogeneous smart assignment
    SameSmartType& operator = (const SameSmartType& rhs)
    {
        Attach(rhs);
        return *this;
    }

	// heterogeneous smart assignment
    SameSmartType& operator = (const CafSubSystemSmartClToken& rhs)
    {
        Attach(rhs);
        return *this;
    }

	// equivalence operators (note - no identity tests performed here!)
    BOOL operator == (Cl * rhs)
    {
        return m_pCl == rhs;
    }

    BOOL operator == (const SameSmartType& rhs)
    {
        return m_pCl == rhs.m_pCl;
    }

    BOOL operator != (Cl *rhs)
    {
        return m_pCl != rhs;
    }

    BOOL operator != (const SameSmartType& rhs)
    {
        return m_pCl != rhs.m_pCl;
    }

public:  // CoCreateInstance wrappers    
    void CreateInstance(const std::string& strClassIdentifier,
						const bool cbIsExceptionOnFailure = true)
    {
    	CCafException* _cm_exception_ = NULL;
		SafeRelease();

		try
		{
			CEcmSubSystem oSubSystem;
			oSubSystem.Load(strClassIdentifier);
			oSubSystem.CreateInstance(strClassIdentifier, GetIID(), AsPPVArg());
		}
		catch (CCafException* e)
		{
			_cm_exception_ = e;
		}

		if(_cm_exception_)
		{
			if( cbIsExceptionOnFailure )
			{
				_cm_exception_->throwSelf();
			}
			else
			{
				_cm_exception_->Release();
			}
		}
    }

public:  //  Interface pointer accessors
	// note: no If * operator is allowed, as it makes it very
	//       easy to break the protocol of COM. Instead, these
	//       four operations require the user to be explicit

    Cl * GetAddRefedInterface(void) const
    { 
        if (m_pCl)
            m_pCl->AddRef();
        return m_pCl;
    }

    Cl * GetNonAddRefedInterface(void) const
    { 
        return m_pCl;
    }

    Cl **GetReleasedInterfaceReference(void)
    { 
        SafeRelease();
        return &m_pCl;
    }

    Cl **GetNonReleasedInterfaceReference(void)
    { 
        return &m_pCl;
    }

public:  // operations
    const IID GetIID(void) const
    {
        return CAF_IIDOF(ICafObject);
    }

    void * * AsPPVArg(void)
    {
        SafeRelease();
        return (void **)&m_pCl;
    }

    BOOL operator ! (void) const
    {
        return m_pCl == 0;
    }

    BOOL IsOK(void) const
    {
        return m_pCl != 0;
    }

    // instead of operator bool, we return a fake ptr type to allow if (pFoo) usage
    // but to disallow if (pFoo == pBar) which is probably wrong
    class PseudoBool {};
    operator PseudoBool * (void) const 
    {
        return (PseudoBool *)m_pCl;
    }

    // the arrow operator returns a pointer with AddRef and Release disabled
    class NoAddRefOrRelease : public Cl {
    private:
		virtual void AddRef() = 0;
		virtual void Release() = 0;
    };

    NoAddRefOrRelease *operator ->(void)
    {
		CAF_CM_FUNCNAME_VALIDATE( "operator ->" );
		CAF_CM_VALIDATE_PTR( m_pCl );

        return (NoAddRefOrRelease *)m_pCl;
    }

    NoAddRefOrRelease *operator ->(void) const
    {
    	CAF_CM_FUNCNAME_VALIDATE( "operator ->" );
		CAF_CM_VALIDATE_PTR( m_pCl );

        return (NoAddRefOrRelease *)m_pCl;
    }
private:
	CAF_CM_CREATE;
    Cl* m_pCl;

private:
    void SafeRelease(void)
    {
        if( m_pCl ) 
            m_pCl->Release();
		m_pCl = 0;
    }
};

}

#endif
