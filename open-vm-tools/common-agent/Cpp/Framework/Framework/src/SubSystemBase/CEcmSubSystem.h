/*
 *  Created: Oct 09, 2002
 *
 *	Copyright (C) 2002-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _CEcmSubSystem_H_
#define _CEcmSubSystem_H_

#include <map>

namespace Caf {

// Typedef the function pointers.
typedef	void ( * CreateInstanceFunc )( const char*, const IID & riid, void ** ppv ) ;

typedef	bool( * CanUnloadNowFunc )();

// Class declaration
class SUBSYSTEMBASE_LINKAGE CEcmSubSystem
{
public:
	CEcmSubSystem( bool bIsUnloadable = false );
	virtual ~ CEcmSubSystem();
	
	bool IsRegistered( const std::string & rstrSubSystemIdentifier );
	void Load( const std::string & rstrSubSystemIdentifier );
	bool IsUnloadable() const; 
	std::string GetSubSystemID() const;
	bool Unload( const bool cbMustUnloadNow = true );
	void GetVersionInfo();
	void CreateInstance( const std::string & rstrIdentifier, const IID & riid,
		void ** ppv );

public: // copy constructor and assignment operator 
	CEcmSubSystem( const CEcmSubSystem& crCEcmSubSystem);
	CEcmSubSystem& operator= ( const CEcmSubSystem& crCEcmSubSystem );

public: // less than, allows use in associative containers	
	bool operator< ( const CEcmSubSystem& ) const;

private:
	// Standard ECM macro for a standard object.
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	
	// Flag indicating whether a sub-system is able to be unloaded.
	bool m_bIsUnloadable;

	// Sub-system identifier.
	std::string m_strSubSystemIdentifier;
	
	// Sub-system DLL module handle.
	HMODULE m_hModule;

	// A map to hold a module cache.
	static std::map<std::string, HMODULE> m_mapModuleCache;

	// A critical section to protect the module cache.
	static GRecMutex m_oModuleCacheMutex;

	// A pointer to the create instance of the function of the sub-system.
	CreateInstanceFunc m_pfnCreateInstance;

	// A pointer to the can unload function of the sub-system.
	CanUnloadNowFunc m_pfnCanUnloadNow;
};
}

#endif // _CEcmSubSystem_H_
