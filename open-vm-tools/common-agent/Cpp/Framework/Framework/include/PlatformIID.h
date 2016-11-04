/*
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef PLATFORM_IID_H
#define PLATFORM_IID_H

#ifndef WIN32

	#include <string>
	#include <memory.h>

	struct GUID
	{
		// this must match the Windows GUID structure
		uint32 Data1;  // 4 bytes
		uint16 Data2;  // 2 bytes
		uint16 Data3;  // 2 bytes
		byte Data4[8]; // 8 bytes
	};

	typedef GUID IID, UUID;

	inline bool IsEqualGUID(const GUID& rguid1, const GUID& rguid2)
	{
		return (!::memcmp(&rguid1,&rguid2,sizeof(GUID))); 
	}

	#define IsEqualIID(rguid1,rguid2) IsEqualGUID(rguid1,rguid2)
	#define IsEqualUUID(rguid1,rguid2) IsEqualGUID(rguid1,rguid2)

	extern HRESULT UuidCreate(UUID* uuid);

#endif

namespace BasePlatform {
BASEPLATFORM_LINKAGE std::string UuidToString(const UUID& uuid);
BASEPLATFORM_LINKAGE HRESULT UuidFromString(const char* strGuid, UUID& uuid);

// This mutex is used to guard the construction of the IIDs in the IIDOF macro
extern BASEPLATFORM_LINKAGE GMutex gs_BaseIIDInitMutex;
}

#define CAF_DECL_UUID(iid) \
public: \
static const IID & IIDOF() \
{ \
	static IID ms_oIID; \
	static bool m_bIsSet; \
	g_mutex_lock(&BasePlatform::gs_BaseIIDInitMutex); \
	if ( !m_bIsSet ) \
	{ \
		BasePlatform::UuidFromString( iid , ms_oIID ); \
		m_bIsSet = true; \
	} \
	g_mutex_unlock(&BasePlatform::gs_BaseIIDInitMutex); \
	return ms_oIID; \
}

#define CAF_IIDOF(type_name) type_name::IIDOF()

#endif
