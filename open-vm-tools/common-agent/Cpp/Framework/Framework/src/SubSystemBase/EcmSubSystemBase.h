/*
 *  Created: Oct 09, 2002
 *
 *	Copyright (C) 2002-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _EcmSubSystemBase_H_
#define _EcmSubSystemBase_H_

namespace Caf {

//////////////////////////////////////////////////////////////////////
// Forward Declarations
//////////////////////////////////////////////////////////////////////
struct _CAF_INTMAP_ENTRY;

//////////////////////////////////////////////////////////////////////////////
// ECM Sub-System Constants
//////////////////////////////////////////////////////////////////////////////

// The value name for retrieving the module's path
extern SUBSYSTEMBASE_LINKAGE const char* MODULE_PATH_VALUE_NAME;

// The DLL exported function names
extern SUBSYSTEMBASE_LINKAGE const char* CREATE_INSTANCE;
extern SUBSYSTEMBASE_LINKAGE const char* CAN_UNLOAD_NOW;

//////////////////////////////////////////////////////////////////////////////
// ECM Object Map Support
//////////////////////////////////////////////////////////////////////////////
struct _CAF_OBJECT_ENTRY
{
	const char*					(*pfnIdentifier)();
	const UUID*					cpclsidClassId;
	uint32						(*pfnCreator)(const IID& riid, void** ppv);
	mutable ICafObject*			pCachedObject;
	bool						bIsSingleton;
	const _CAF_INTMAP_ENTRY*	(*pfnGetEntries)();
};

}

#define CAF_BEGIN_OBJECT_MAP(x) static _CAF_OBJECT_ENTRY x[] = {

#define CAF_END_OBJECT_MAP() { NULL, NULL, NULL, NULL, false, NULL }};

#define CAF_OBJECT_ENTRY(className)\
	{ className::GetObjectIdentifier,\
		NULL,\
		className::Creator,\
		NULL,\
		false,\
		className::_GetEntries },

#define CAF_OBJECT_ENTRY2(className, objectIdentifier)\
	{ objectIdentifier,\
		NULL,\
		className::Creator,\
		NULL,\
		false,\
		className::_GetEntries },

/*#define CAF_OBJECT_ENTRY_SINGLETON(className)\
	{ className::GetObjectIdentifier,\
		NULL,\
		className::Creator,\
		NULL,\
		true,\
		className::_GetEntries },*/

//////////////////////////////////////////////////////////////////////////////
// ECM Interface Map Support
//////////////////////////////////////////////////////////////////////////////
namespace Caf {
struct _CAF_INTMAP_ENTRY
{
	const IID*		cpiid;
	SUBSYS_INTPTR	offset;
};
}

#define _CAF_PACKING 8
#define CAF_offsetofclass(base, derived)\
	((SUBSYS_INTPTR)(static_cast<base*>((derived*)_CAF_PACKING))-_CAF_PACKING)

#define CAF_BEGIN_INTERFACE_MAP(x) public: \
	typedef x _CafMapClass; \
	ICafObject* _GetRawObjectInterface() \
	{ return (ICafObject*)((SUBSYS_INTPTR)this+_GetEntries()->offset); } \
	ICafObject* GetObjectInterface() { return _GetRawObjectInterface(); }\
	void _InternalQueryInterface(const IID& iid, void** ppv) \
	{ InternalQueryInterface(this, _GetEntries(), iid, ppv); } \
	static const _CAF_INTMAP_ENTRY* _GetEntries() \
	{ static const _CAF_INTMAP_ENTRY _entries[] = {

#define CAF_END_INTERFACE_MAP() {NULL, SUBSYS_INTPTR_INVALID}}; return _entries;} \
	virtual void AddRef() = 0;\
	virtual void Release() = 0;\
	virtual void QueryInterface(const IID&, void**) = 0;

#define CAF_INTERFACE_ENTRY(x)\
	{&CAF_IIDOF(x), \
	CAF_offsetofclass(x, _CafMapClass)},

#define CAF_INTERFACE_ENTRY_IID(iid, x)\
	{&iid,\
	offsetofclass(x, _CafMapClass)},

#define CAF_INTERFACE_ENTRY2(x, x2)\
	{&CSI_IIDOF(x),\
	(SUBSYS_INTPTR)((x*)(x2*)((_CafMapClass*)_CAF_PACKING))-_CAF_PACKING},

#define CAF_INTERFACE_ENTRY2_IID(iid, x, x2)\
	{&iid,\
	(SUBSYS_INTPTR)((x*)(x2*)((_CafMapClass*)_CAF_PACKING))-_CAF_PACKING},

//////////////////////////////////////////////////////////////////////////////
// ECM Helper Macros
//////////////////////////////////////////////////////////////////////////////

// Place in class to be exposed from a sub-system in order to
// specify its identifier.
#define CAF_DECLARE_OBJECT_IDENTIFIER(x)\
	public: \
	static const char* GetObjectIdentifier()\
	{\
		return x;\
	} \
	const char* GetObjectId () const \
	{ \
		return x;\
	}

//////////////////////////////////////////////////////////////////////////////
// ECM Sub-System Standard Function Exports
//////////////////////////////////////////////////////////////////////////////
// Place in module that contains DllMain
#define CAF_DECLARE_SUBSYSTEM_EXPORTS()\
	extern "C" void __declspec(dllexport) CafCreateInstance(\
		const char* rstrIdentifier, const IID & riid, void ** ppv )\
	{\
		_Module.CreateInstance(rstrIdentifier, riid, ppv);\
	}\
	extern "C" bool __declspec(dllexport) EcmDllCanUnloadNow()\
	{\
		return _Module.CanUnload();\
	}

//////////////////////////////////////////////////////////////////////////////
// ECM Sub-System Standard Includes
//////////////////////////////////////////////////////////////////////////////
#include "CEcmSubSystemModule.h"

#ifdef CAF_SUBSYSTEM
extern Caf::CEcmSubSystemModule _Module;
#endif

#include "TCafSubSystemObject.h"
#include "TCafSubSystemObjectRoot.h"
#include "CEcmSubSystem.h"


#endif // _EcmSubSystemBase_H_

