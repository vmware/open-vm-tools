/*
 *  Created: Oct 09, 2002
 *
 *	Copyright (C) 2002-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */
#ifndef _CEcmDllManager_H
#define _CEcmDllManager_H

#include <map>
#include <string>

namespace Caf {

class SUBSYSTEMBASE_LINKAGE CEcmDllManager {
public:
	static HMODULE LoadLibrary( const char * cpszLibName, bool bMustInvokeDllMain,bool bThrowExceptionUponFailure = true );
	static void UnloadLibrary( HMODULE hLibraryHandle, bool bMustInvokeDllMain );
	static void * GetFunctionAddress( HMODULE hLibraryHandle, const char * cpszFunctionName, std::string & rstrErrorMessage );
	static void GetLibraryNameFromHandle ( HMODULE hLibraryHandle, std::string & rstrLibName );

private: // a purely static class
	CEcmDllManager();
	~CEcmDllManager();
	CEcmDllManager( const CEcmDllManager& );
	CEcmDllManager& operator=( const CEcmDllManager& );

private: // used internally on Unix platforms
	static void GetLibraryNameFromAddress( const void * cpvAddressInLibrary, std::string & rstrLibName );
	static void GetMainProgramName( std::string & rstrProgName );

private:
	static GRecMutex ms_mutex;

#ifndef WIN32
	struct SModuleRefCount
	{
		HMODULE m_hModule;
		int32 m_iRefCount;
	};
	static std::map<std::string, SModuleRefCount> ms_mapLoadedModuleRefCounts;
	static std::map<HMODULE, std::string> ms_mapLoadedModules;
#endif
};
}

#endif // _CEcmDllManager_H

