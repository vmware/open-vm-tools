/*
 *  Created: Oct 09, 2002
 *
 *	Copyright (C) 2002-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _CEcmSubSystemModule_H_
#define _CEcmSubSystemModule_H_

namespace Caf {

struct _CAF_OBJECT_ENTRY;

class SUBSYSTEMBASE_LINKAGE CEcmSubSystemModule  
{
public:
	CEcmSubSystemModule();
	virtual ~CEcmSubSystemModule();

	void Init(const _CAF_OBJECT_ENTRY* const pObjectEntries, const HINSTANCE hInstance);
	void Term();

	void Lock();
	void Unlock();

	void CreateInstance(const char* crstrIdentifier, const IID& criid, void** ppv);

	bool CanUnload();

	HINSTANCE GetModuleHandle();

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	const _CAF_OBJECT_ENTRY* m_pObjectEntries;
	HINSTANCE m_hInstance;
	gint m_lLockCount;
};

}

#endif // _CEcmSubSystemModule_H_

