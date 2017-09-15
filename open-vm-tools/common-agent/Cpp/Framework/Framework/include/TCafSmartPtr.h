/*
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _TCafSmartPtr_H
#define _TCafSmartPtr_H

#ifdef WIN32
// Disable C4800 - performance warning forcing value to 'true' or 'false'
#pragma warning(disable: 4800)
#endif

// Forward declarations for Subsystem calls
#ifndef SUBSYSTEMBASE_LINKAGE
#ifdef WIN32
#ifdef FRAMEWORK_BUILD
#define SUBSYSTEMBASE_LINKAGE __declspec(dllexport)
#else
#define SUBSYSTEMBASE_LINKAGE __declspec(dllimport)
#endif
#else
#define SUBSYSTEMBASE_LINKAGE
#endif
#endif

namespace Caf {
extern "C" void SUBSYSTEMBASE_LINKAGE CreateObject(const char* cszObjectId, const IID& criid, void** ppv);
//extern "C" void SUBSYSTEMBASE_LINKAGE CreateQIObject(const char* cszFactoryId, const char* cszClassName, const IID& criid, void** ppv);
}

/**
 * @brief Template to wrap any interface or class and provide lifetime management.
 * <p>
 * Lifetime is managed through reference counting with counts stored in the object
 * itself. The reference methods are AddRef() and Release(). When the reference
 * count reaches zero the object will delete itself.
 * <p>
 * Template instantiation:<br>
 * <i>Cl</i> is used for accessing the class. It may or may not be derived from
 * #ICafObject.<br>
 * <i>CreateCl</i> is used for creating the class and reference counting. It must
 * support the #ICafObject interface.
 */
// CreateCl is used for creation and reference counting and must support the
// ICafObject interface.  Cl is used for accessing the class.
template<class Cl, class CreateCl = Cl>
class TCafSmartPtr {
public:
   typedef Cl class_type;
   typedef CreateCl create_type;

private:
   typedef TCafSmartPtr<Cl, CreateCl> SameSmartType;

public:
   // constructors/destructors

   // default constructor
   TCafSmartPtr(void) :
         m_pCl(0) {
   }

   // homogeneous raw constructor
   TCafSmartPtr(Cl *rhs) {
      m_pCl = rhs;
      if (m_pCreateCl)
         m_pCreateCl->AddRef();
   }

   // derived class smart constructor
   template<class Derived, class CreateDerived>
   TCafSmartPtr(const TCafSmartPtr<Derived, CreateDerived> &rhs) {
      m_pCl = rhs.GetNonAddRefedInterface();
      if (m_pCreateCl)
         m_pCreateCl->AddRef();
   }

   // homogeneous smart constructor
   TCafSmartPtr(const SameSmartType& rhs) {
      m_pCl = rhs.m_pCl;
      if (m_pCreateCl)
         m_pCreateCl->AddRef();
   }

   // don't allow construction from bool
   explicit TCafSmartPtr(const bool &rb) {
      int32 CantConstructSmartPointerFromBool[0];
   }

   // destructor
   ~TCafSmartPtr(void) {
      if (m_pCreateCl)
         m_pCreateCl->Release();
   }

public:
   // assignment operators

   // homogeneous raw assignment
   SameSmartType &operator=(Cl *rhs) {
      Attach(rhs);
      return *this;
   }

   // derived class smart assignment
   template<class Derived, class CreateDerived>
   SameSmartType &operator=(const TCafSmartPtr<Derived, CreateDerived> &rhs) {
      Attach(rhs.GetNonAddRefedInterface());
      return *this;
   }

   // homogeneous smart assignment
   SameSmartType &operator=(const SameSmartType &rhs) {
      Attach(rhs.m_pCl);
      return *this;
   }

   // don't allow assignment from bool
   void operator=(bool &rb) {
      int32 CantAssignSmartPointerFromBool[0];
   }

private:

   // This helper class is used to ensure that the old smart object gets
   // released in a safe manner.  It is not safe to touch 'this' in any way
   // whatsoever after releasing the old smart object because the release
   // could set off a chain of destruction that results in this smart pointer
   // being destroyed.  Note that this includes the exception macros which
   // reference the automatically-defined class name member variable.
   class CSafeAutoRelease {
   public:
      CSafeAutoRelease(CreateCl *pOldCreateCl) :
            m_pOldCreateCl(pOldCreateCl) {
      }
      ~CSafeAutoRelease() {
         if (m_pOldCreateCl)
            m_pOldCreateCl->Release();
      }
      CreateCl *m_pOldCreateCl;
   };

public:
   /**
    * @brief Retrieve an interface from a ICafObject pointer and assign the result to self
    * @param piObj the object to be queried
    * @param cbIsRequired if <b>true</b> then the operation must succeed else an
    * exception will be thrown. If <b>false</b> then the self value will be NULL
    * if the operation fails.
    */
   void QueryInterface(ICafObject *piObj, const bool cbIsRequired = true) {
      CSafeAutoRelease oAutoRelease(m_pCreateCl);

      m_pCreateCl = NULL;
      if (piObj)
         piObj->QueryInterface(GetIID(), reinterpret_cast<void **>(&m_pCreateCl));
      if (cbIsRequired && !m_pCreateCl)
         throw std::bad_cast();
   }

   /**
    * @brief Retrieve an interface from another smart pointer and assign the result to self
    * @param rhs the object to be queried
    * @param cbIsRequired if <b>true</b> then the operation must succeed else an
    * exception will be thrown. If <b>false</b> then the self value will be NULL
    * if the operation fails.
    */
   template<class QI, class CreateQI>
   void QueryInterface(const TCafSmartPtr<QI, CreateQI> &rhs, const bool cbIsRequired = true) {
      CSafeAutoRelease oAutoRelease(m_pCreateCl);

      m_pCreateCl = NULL;
      if (rhs)
         rhs.QueryInterface(GetIID(), reinterpret_cast<void **>(&m_pCreateCl));
      if (cbIsRequired && !m_pCreateCl)
         throw std::bad_cast();
   }

   // This function is provided so that the delegation can go through m_pCreateCl
   // rather than m_pCl.  This way, if this is a smart pointer to a class that
   // derives from multiple interfaces the compiler will not get confused about
   // which QueryInterface function to use.
   void QueryInterface(const IID &criid, void **ppv) const {
      *ppv = NULL;
      m_pCreateCl->QueryInterface(criid, ppv);
   }

   /**
    * @brief Return the UUID of the object
    * @return the UUID
    */
   static const IID& GetIID() {
      // Compiler bug workaround (see comments before COpaqueTemplate)
      // return TEcmSmartPtr_GetIID(static_cast<COpaqueTemplate<Cl> *>(NULL));
      return CAF_IIDOF(Cl);
   }

public:
   // comparison operators

   bool operator==(const Cl *rhs) const {
      return m_pCl == rhs;
   }

   template<class Derived, class CreateDerived>
   bool operator==(const TCafSmartPtr<Derived, CreateDerived> &rhs) const {
      return m_pCl == rhs.GetNonAddRefedInterface();
   }

   bool operator!=(const Cl *rhs) const {
      return m_pCl != rhs;
   }

   template<class Derived, class CreateDerived>
   bool operator!=(const TCafSmartPtr<Derived, CreateDerived> &rhs) const {
      return m_pCl != rhs.GetNonAddRefedInterface();
   }

   bool operator<(const SameSmartType &rhs) const {
      return (m_pCl < rhs.m_pCl);
   }

public:
   // conversion

   // This takes the place of operator bool.
   // It turns out that the presence of operator bool
   // causes the compiler to "get lost" when compiling
   // comparison operations such as if( spcPtr1 == spcPtr2)...
   //
   // This conversion operator will satisfy the compiler when
   // compiling comparison operations.
   class PseudoBool {
   };
   operator PseudoBool *() const {
      return (PseudoBool *) m_pCl;
   }

public:
   // instance creation
   /**
    * @brief Create an instance of the CreateCl object
    * <p>
    * The object will have an initial reference count of 1.
    */
   void CreateInstance() {
      CSafeAutoRelease oAutoRelease(m_pCreateCl);

      ////////////////////////////////////////////////////////////////////////
      //
      // This code is used to verify that it is safe to use a union of Cl
      // and CreateCl.  An error indicates that it is *not* safe in which
      // case the smart pointer cannot be used as defined.  To solve this
      // problem, eliminate the second template parameter from the smart
      // pointer definition which will force Cl and CreateCl to be the
      // same, e.g.:
      //
      //		typedef TCafSmartPtr<TCafObject<Class> > SmartPtrClass;
      //
      ////////////////////////////////////////////////////////////////////////
      Cl *pCl = static_cast<Cl *>(reinterpret_cast<CreateCl *>(0x4));
      if (pCl != reinterpret_cast<Cl *>(0x4))
         throw std::logic_error("Illegal use of TCafSmartPtr<> (See comments in TCafSmartPtr.h)");
      m_pCreateCl = new CreateCl;
      if (!m_pCreateCl)
         throw std::bad_alloc();
      m_pCreateCl->AddRef();
   }

   /**
    * @brief Create an instance of a subsystem object
    * <p>
    * Objects exposed as subsystems (#Caf::TCafSubSystemSmartCl) are identified
    * by a string.  The Cl and CreateCl template arguments would both be
    * set to an interface on the subsystem object of interest.
    * <p>
    * The object will have an initial reference count of 1.
    */
   void CreateInstance(const char* cszObjectId) {
      CSafeAutoRelease oAutoRelease(m_pCreateCl);
      Caf::CreateObject(cszObjectId, GetIID(), reinterpret_cast<void **>(&m_pCreateCl));
   }

//	void CreateInstance (const char *cszFactoryId, const char *cszClassName)
//	{
//		CSafeAutoRelease oAutoRelease(m_pCreateCl);
//		Caf::CreateQIObject(cszFactoryId, cszClassName, GetIID(), reinterpret_cast<void **>(&m_pCreateCl));
//	}

public:
   // operations
   Cl *GetAddRefedInterface() const {
      if (!m_pCl)
         throw std::runtime_error("TCafSmartPtr: m_pCl is NULL");
      if (m_pCreateCl)
         m_pCreateCl->AddRef();
      return m_pCl;
   }

   Cl *GetNonAddRefedInterface() const {
      return m_pCl;
   }

   Cl **GetReleasedInterfaceReference(void) {
      CSafeAutoRelease oAutoRelease(m_pCreateCl);
      m_pCreateCl = NULL;
      return &m_pCreateCl;
   }

   Cl **GetNonReleasedInterfaceReference(void) {
      return &m_pCreateCl;
   }

   void **GetAsPPVArg(void) {
      return (void**) GetReleasedInterfaceReference();
   }

   bool IsNull() const {
      return m_pCl == 0;
   }

   // the arrow operator simply returns the pointer
   Cl *operator->() const {
      if (!m_pCl)
         throw std::runtime_error("TCafSmartPtr: m_pCl is NULL");
      return m_pCl;
   }

   Cl &operator*() const {
      if (!m_pCl)
         throw std::runtime_error("TCafSmartPtr: m_pCl is NULL");
      return *m_pCl;
   }

private:
   // m_pCreateCl is used for reference counting; m_pCl is used for object access.
   union {
      Cl *m_pCl;
      CreateCl *m_pCreateCl;
   };

private:
   // homogeneous raw attachment
   void Attach(Cl* rhs) {
      CSafeAutoRelease oAutoRelease(m_pCreateCl);
      m_pCl = rhs;
      if (m_pCreateCl)
         m_pCreateCl->AddRef();
   }
};

// These template functions will give you a reference to the
// underlying object wraped in a smart class
template<class Cl, class CreateCl>
const Cl& ToObj(const TCafSmartPtr<Cl, CreateCl>& spcT) {
   return *(spcT.GetNonAddRefedInterface());
}

template<class Cl, class CreateCl>
Cl& ToNonConstObj(const TCafSmartPtr<Cl, CreateCl>& spcT) {
   return *(spcT.GetNonAddRefedInterface());
}

////////////////////////////////////////////////////////////////////////
//
// Declaration Macros
//
////////////////////////////////////////////////////////////////////////

// Declare a smart pointer after the class has been declared
#define CAF_DECLARE_SMART_POINTER(ClassName) \
	typedef TCafSmartPtr<ClassName, TCafObject<ClassName> > SmartPtr##ClassName; \
//	typedef TCafSmartPtr<const ClassName, TCafObject<ClassName> > ConstPtr##ClassName

// Forward declare a class smart pointer
#define CAF_DECLARE_CLASS_AND_SMART_POINTER(ClassName) \
	class ClassName; \
	CAF_DECLARE_SMART_POINTER(ClassName)

// Forward declare a struct smart pointer
#define CAF_DECLARE_STRUCT_AND_SMART_POINTER(StructName) \
	struct StructName; \
	CAF_DECLARE_SMART_POINTER(StructName)

// Helper macro - do not use directly
#define CAF_DECLARE_SMART_INTERFACE_HELPER(InterfaceName) \
	typedef TCafSmartPtr<InterfaceName> SmartPtr##InterfaceName; \
//	typedef TCafSmartPtr<const InterfaceName, InterfaceName> ConstPtr##InterfaceName

// Declare a smart pointer to an interface in the interface header file
#define CAF_DECLARE_SMART_INTERFACE_POINTER(InterfaceName) \
	CAF_DECLARE_SMART_INTERFACE_HELPER(InterfaceName)

// Forward declare a smart interface pointer
#define CAF_FORWARD_DECLARE_SMART_INTERFACE(InterfaceName) \
	struct InterfaceName; \
	CAF_DECLARE_SMART_INTERFACE_HELPER(InterfaceName)

#endif // #ifndef _TCafSmartPtr_H
