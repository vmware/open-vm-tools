/*
 *  Created: May 25, 2004
 *
 *	Copyright (C) 2004-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef TCAFQIOBJECT_H_
#define TCAFQIOBJECT_H_

template <class Base>
class TCafQIObject : public TCafObject<Base>
{
public:
	TCafQIObject ()
	{
	}

	virtual ~TCafQIObject ()
	{
	}

public:
	virtual void QueryInterface( const IID& criid, void** ppv )
	{
		Base::_InternalQueryInterface( criid, ppv );
	}
};

////////////////////////////////////////////////////////////////////////
//
// QI map
//
////////////////////////////////////////////////////////////////////////
#define CAF_BEGIN_QI() \
protected: \
	void _InternalQueryInterface (const IID &criid, void **ppv) \
	{ \
		try \
		{ \
			if (ppv) \
			{ \
				*ppv = NULL; \
				bool bUseFirstInterface = (::IsEqualGUID(criid, CAF_IIDOF(::ICafObject)) != 0); \
				if (0);

#define CAF_QI_ENTRY(Interface) \
				else if (bUseFirstInterface || ::IsEqualGUID(criid, CAF_IIDOF(Interface))) \
					*ppv = static_cast<Interface *>(this);

#define CAF_QI_ENTRY2(Interface, IntermediateInterface) \
				else if (bUseFirstInterface || ::IsEqualGUID(criid, CAF_IIDOF(Interface))) \
					*ppv = static_cast<Interface *>(static_cast<IntermediateInterface *>(this));

#define CAF_END_QI() \
				if (*ppv) \
					reinterpret_cast<ICafObject *>(this)->AddRef(); \
			} \
		} \
		catch (...) \
		{ \
		} \
	}

////////////////////////////////////////////////////////////////////////
//
// Object Id - required for ISerializableObject
//
////////////////////////////////////////////////////////////////////////
#define CAF_MAKE_OBJECT_ID(Factory, Class) \
	(std::string(Factory) + std::string(":") + std::string( #Class ))

#define CAF_DECLARE_OBJECT_ID(Class, Factory) \
	public: \
	virtual std::string GetObjectId () const \
	{ \
		return CAF_MAKE_OBJECT_ID(Factory, Class); \
	}

// Declare a smart pointer to a class that supports QI after the class has been declared
#define CAF_DECLARE_SMART_QI_POINTER(ClassName) \
	typedef TCafSmartPtr<ClassName, TCafQIObject<ClassName> > SmartPtr##ClassName; \
	typedef TCafSmartPtr<const ClassName, TCafQIObject<ClassName> > ConstPtr##ClassName

// Forward declare a smart pointer to a class that supports QI
#define CAF_DECLARE_CLASS_AND_IMPQI_POINTER(ClassName) \
	class ClassName; \
	CAF_DECLARE_SMART_QI_POINTER(ClassName)

#endif /* TCAFQIOBJECT_H_ */
