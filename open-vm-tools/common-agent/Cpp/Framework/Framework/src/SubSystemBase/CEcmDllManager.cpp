/*
 *  Created: Oct 09, 2002
 *
 *	Copyright (C) 2002-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "Exception/CCafException.h"
#include "CEcmDllManager.h"

#ifndef WIN32
	#include <iostream>
	#include <dlfcn.h>
	#include <stdlib.h>
	#include <syslog.h>
#endif

#define _MAX_PATH_LARGE 32768

using namespace Caf;

GRecMutex CEcmDllManager::ms_mutex;

#if !defined( WIN32 )
std::map<std::string, CEcmDllManager::SModuleRefCount> CEcmDllManager::ms_mapLoadedModuleRefCounts;
std::map<HMODULE, std::string> CEcmDllManager::ms_mapLoadedModules;
#endif

// Here is where we work out which Unix OS style of dl functions we are 
// going to use
//   1. we have all dl functions we need dlopen, dlclose, dladdr and dlsym
//   2. we have all dl functions except dladdr (AIX)
#if defined ( __sun__ ) || defined ( __linux__ )  || defined ( WIN32 ) || defined ( __hpux__ ) || defined (__APPLE__)
// we have everything we need
#elif defined ( _AIX )
	#include <sys/types.h>
	#include <sys/ldr.h>
	#include <procinfo.h>
	extern "C"
	{
		extern int32 getargs(struct procsinfo*, int32, char*, int32);
	}
#else
	#error "Not yet ported to this platform"
#endif

typedef BOOL (*DllMainPtr) (HINSTANCE, uint32, LPVOID);
static const char *gs_cszDllMainFunctionName = "DllMain";
static const char *gs_cszMainFunctionName = "main";
#ifdef _AIX
static const char *gs_cszInitFunctionName = "_GLOBAL__DI";
#else
static const char *gs_cszInitFunctionName = "_init";
#endif

//////////////////////////////////////////////////////////////////////////////
//
//  LoadLibrary
//  Loads a shared library from any OS 
//	
//////////////////////////////////////////////////////////////////////////////
HMODULE CEcmDllManager::LoadLibrary( 
	const char * cszLibName,
	bool bMustInvokeDllMain, 
	bool bThrowExceptionUponFailure/* = true  */)
{
	CAF_CM_STATIC_FUNC( "CEcmDllManager", "LoadLibrary" );
	CAF_CM_VALIDATE_STRINGPTRA( cszLibName );

	HMODULE hRetModule = NULL;
	bool bIsLoaded = false;

#if defined (WIN32)
	const char* libPrefix = NULL;
	const char* libSuffix = ".dll";
#elif defined (__APPLE__)
    const char* libPrefix = "lib";
	const char* libSuffix = ".dylib";
#elif defined (__linux__)
	const char* libPrefix = "lib";
	const char* libSuffix = ".so";
#else
#error 'NOT IMPLEMENTED FOR THIS OPERATING SYSTEM'
#endif

	std::string libName(cszLibName);
	if (libPrefix != NULL && libName.compare(0, std::string::npos, libPrefix) != 0) {
	   libName.insert(0, libPrefix);
	}
	if (libName.find(libSuffix) == std::string::npos) {
		libName += libSuffix;
	}

	try
	{
#if defined ( WIN32 )
		hRetModule = ::LoadLibraryExA( libName.c_str(), NULL, 0 );
		uint32 rc = ::GetLastError();

		if ( NULL != hRetModule )
		{
			bIsLoaded = true;
		}
		else
		{
			std::string strSystemMessage;
			if ( ERROR_SUCCESS != rc )
			{
				strSystemMessage = BasePlatform::PlatformApi::GetApiErrorMessage(rc);
			}
			else
			{
				strSystemMessage = "GetLastError() returned ERROR_SUCCESS but hRetModule is NULL";
			}

			CAF_CM_EXCEPTION_VA2(
				rc,
				"Failed to load library: %s Error: %s",
				libName.c_str(),
				strSystemMessage.c_str());
		}
#else 
		bool bLogLoading = ( ::getenv( "SYSLOG_DLOPENS" ) != NULL );

		// this needs to be thread safe so that we dont lose the last error
		// generated if a new thread also calls dlerror - unlike windows, the
		// error message is not per thread.
		CAutoMutexLockUnlockRaw oCS( &ms_mutex );

		if ( bLogLoading )
			::syslog(LOG_DEBUG, "---- Loading %s", libName.c_str() );


		// Let's see if we've already loaded it
		std::map<std::string, SModuleRefCount>::iterator it = ms_mapLoadedModuleRefCounts.find( libName.c_str() );
		if ( it != ms_mapLoadedModuleRefCounts.end() )
		{
			it->second.m_iRefCount++;
			hRetModule = it->second.m_hModule;
			if ( bLogLoading )
#ifdef __x86_64__
				::syslog(LOG_DEBUG," ------ Already loaded as %p count is now %d", hRetModule, it->second.m_iRefCount );
#else
				::syslog(LOG_DEBUG," ------ Already loaded as %p count is now %d", hRetModule, it->second.m_iRefCount );
#endif
		}
		else
		{
			hRetModule = ::dlopen( libName.c_str(), RTLD_NOW | RTLD_LOCAL );
			if ( bLogLoading )
#ifdef __x86_64__
				::syslog(LOG_DEBUG," ------ Loaded as %p" , hRetModule);
#else
				::syslog(LOG_DEBUG," ------ Loaded as %p" , hRetModule);
#endif

			if ( NULL == hRetModule )
			{	
				std::string strSystemMessage;
				const char * pszMessage = dlerror();	
				if ( NULL != pszMessage )
				{
					strSystemMessage = pszMessage;
				}
				else
				{
					strSystemMessage = "dlerror() returned NULL";
				}

				CAF_CM_EXCEPTIONEX_VA2(
									 LibraryFailedToLoadException,
									 0,
									 "Failed to load library: %s Error: %s",
									 libName.c_str(),
									 strSystemMessage.c_str());
			}
			else
			{
				bIsLoaded = true;
				std::string strSubErrorMessage;
	
				// not all compilers will call a method on initializing
				// a shared library, so we will force ourselves to call one
				// we will mimic the Windows DllMain functionality 
				DllMainPtr pfnDllMain = (DllMainPtr)
					CEcmDllManager::GetFunctionAddress( 
						hRetModule, 
						gs_cszDllMainFunctionName,
						 strSubErrorMessage );


				if ( NULL != pfnDllMain )
				{
					BOOL result = ( *pfnDllMain )( hRetModule, DLL_PROCESS_ATTACH, NULL );
					if ( false == result )
					{
						CAF_CM_EXCEPTION_VA2(E_FAIL,
											 "%s in library %s returned false - library not loaded",
											 gs_cszDllMainFunctionName,
											 libName.c_str());
					}
				}
				else if ( bMustInvokeDllMain )
				{
					CAF_CM_EXCEPTION_VA3(E_FAIL,
										 "Unable to find %s in library %s, Error: %s - library not loaded",
										 gs_cszDllMainFunctionName,
										 libName.c_str(),
										 strSubErrorMessage.c_str());
				}
				SModuleRefCount stRefCount;
				stRefCount.m_iRefCount = 1;
				stRefCount.m_hModule = hRetModule;
				std::string strModuleName( libName.c_str() );
				ms_mapLoadedModuleRefCounts.insert( std::make_pair(strModuleName, stRefCount ) );
				ms_mapLoadedModules.insert( std::make_pair( hRetModule, strModuleName ) );
			}
		}
#endif
	}
	catch (CCafException *e)
	{
		_cm_exception_ = e;
	}

	if ( _cm_exception_ )
	{
		try
		{
			if ( bIsLoaded ) 
			{
				CEcmDllManager::UnloadLibrary( hRetModule, false );
			}
		}
		catch (CCafException *e)
		{
			e->Release();
		}


		// If we were told to eat the exception then do so
		hRetModule = NULL;
		if( !bThrowExceptionUponFailure )
		{
			_cm_exception_->Release();
		}
		else
		{
			_cm_exception_->throwSelf();
		}
	}
	return hRetModule;
}

//////////////////////////////////////////////////////////////////////////////
//
//  UnloadLibrary
//  Unloads a shared library from any OS 
//	
//////////////////////////////////////////////////////////////////////////////
void CEcmDllManager::UnloadLibrary( HMODULE hLibraryHandle, bool bMustInvokeDllMain )
{
	CAF_CM_STATIC_FUNC( "CEcmDllManager", "UnloadLibrary" );
	CAF_CM_VALIDATE_PTR( hLibraryHandle );

#if defined ( WIN32 )
		if ( ! ::FreeLibrary( hLibraryHandle ) )
		{
			uint32 rc = ::GetLastError();
			std::string strSystemMessage;
			if ( ERROR_SUCCESS != rc )
			{
				strSystemMessage = BasePlatform::PlatformApi::GetApiErrorMessage(rc);
			}
			else
			{
				strSystemMessage = "GetLastError() returned ERROR_SUCCESS";
			}

			CAF_CM_EXCEPTION_VA1(
				rc,
				"::FreeLibrary failed: %s",
				strSystemMessage.c_str());
		}
#else
	// this needs to be thread safe so that we dont lose the last error
	// generated if a new thread also calls dlerror - unlike windows, the
	// error message is not per thread.
	bool bLogLoading = ( ::getenv( "SYSLOG_DLOPENS" ) != NULL );

	CAutoMutexLockUnlockRaw oCS( &ms_mutex );

	if ( bLogLoading )
#ifdef __x86_64__
		::syslog( LOG_DEBUG, "---- Unloading %p", hLibraryHandle );
#else
		::syslog( LOG_DEBUG, "---- Unloading %p", hLibraryHandle );
#endif
	// look for the specific library in our loaded list -
	// we'll unload if the ref count hits 0 or if it doesn't exist
	int32 iRefCount = 0;
	std::map<HMODULE, std::string>::iterator it = ms_mapLoadedModules.find( hLibraryHandle );
	if ( it != ms_mapLoadedModules.end() )
	{
		if ( bLogLoading ) {
			::syslog( LOG_DEBUG, "------ Is Library %s", it->second.c_str() );
		}

		std::map<std::string, SModuleRefCount>::iterator itCnt =
			ms_mapLoadedModuleRefCounts.find (it->second);
		if ( itCnt != ms_mapLoadedModuleRefCounts.end() )
		{
			iRefCount = --itCnt->second.m_iRefCount;
			if ( iRefCount == 0 )
				ms_mapLoadedModuleRefCounts.erase( itCnt );
		}
		if ( iRefCount == 0 )
		{
			ms_mapLoadedModules.erase( it );
		}
	}

	if ( bLogLoading )
		::syslog( LOG_DEBUG, "------ ref count is %d", iRefCount );

	if ( iRefCount == 0 )
	{
		// not all compilers will call a method on initializing
		// a shared library, so we will force ourselves to call one
		// we will mimic the Windows DllMain functionality
		std::string strSubErrorMessage;
		DllMainPtr pfnDllMain = (DllMainPtr) CEcmDllManager::GetFunctionAddress(
				hLibraryHandle,
				gs_cszDllMainFunctionName,							 strSubErrorMessage );

		if ( NULL != pfnDllMain )
		{
			BOOL result = ( *pfnDllMain )( hLibraryHandle, DLL_PROCESS_DETACH, NULL );
			if ( false == result )
			{
				CAF_CM_EXCEPTION_VA1(E_FAIL,
									 "%s returned false - library not unloaded",
									 gs_cszDllMainFunctionName);
			}
		}
		else if ( bMustInvokeDllMain )
		{
			CAF_CM_EXCEPTION_VA2(E_FAIL,
								 "Unable to find %s, Error: %s, returned false - library not unloaded",
								 gs_cszDllMainFunctionName,
								 strSubErrorMessage.c_str());
		}

		if ( 0 != ::dlclose( hLibraryHandle ) )
		{
			CAF_CM_EXCEPTION_VA1(E_FAIL,
							 	 "Unable to unload library %s",
							 	 ::dlerror());
		}
	}
#endif
}

//////////////////////////////////////////////////////////////////////////////
//
//  GetFunctionAddress
//  
//  Finds the specified symbol in a library
//	
//////////////////////////////////////////////////////////////////////////////
void * CEcmDllManager::GetFunctionAddress( HMODULE hLibraryHandle, const char * cszFunctionName, std::string & rstrErrorMessage )
{
	CAF_CM_STATIC_FUNC_VALIDATE( "CEcmDllManager", "GetFunctionAddress" );
	CAF_CM_VALIDATE_STRINGPTRA( cszFunctionName );
	// It is legal for a NULL handle to be passed in - NULL is the
	// executable

	void * pvRetAddress = NULL;

	rstrErrorMessage = "";

#if defined ( WIN32 )

		pvRetAddress = ::GetProcAddress( hLibraryHandle, cszFunctionName );
		if ( NULL == pvRetAddress )
		{
			uint32 rc = ::GetLastError();
			rstrErrorMessage = "Unable to locate function ";
			rstrErrorMessage += cszFunctionName;
			rstrErrorMessage += ", Error : ";
			if ( ERROR_SUCCESS != rc )
			{
				rstrErrorMessage += BasePlatform::PlatformApi::GetApiErrorMessage(rc);
			}
			else
			{
				rstrErrorMessage += "GetLastError() returned ERROR_SUCCESS";
			}
		}
#else
	CAutoMutexLockUnlockRaw oCS( &ms_mutex );
	try
	{
		pvRetAddress = ::dlsym( hLibraryHandle, cszFunctionName );
	}
	catch (...)
	{
		// if we were passed a bad handle, this can cause a seg
		// violation - so treat it as a regular error
		// Note : Sometimes we can do nothing about it and the
		// program will crash
		pvRetAddress = NULL;
	}

	if ( NULL == pvRetAddress )
	{
		rstrErrorMessage = "Unable to locate function ";
		rstrErrorMessage += cszFunctionName;
		rstrErrorMessage += ", Error : ";
		const char * pszError = ::dlerror();
		if ( NULL != pszError )
		{
			rstrErrorMessage += pszError;
		}
		else
		{
			rstrErrorMessage += "dlerror() returned NULL";
		}
	}
#endif

	return pvRetAddress;
}

//////////////////////////////////////////////////////////////////////////////
//
//  GetLibraryNameFromHandle
//  Determines the name of a library from its handle.
//  We are going to need to add functionality here for AIX 4.3.3 and HPUX10.20
//	
//////////////////////////////////////////////////////////////////////////////
void CEcmDllManager::GetLibraryNameFromHandle ( HMODULE hLibraryHandle, std::string & rstrLibName )
{
	CAF_CM_STATIC_FUNC( "CEcmDllManager", "GetLibraryNameFromHandle" );
	// it is legal for hLibraryModule to be NULL (it's the executable)
	// so don't validate it

	rstrLibName = "";
#if defined ( WIN32 )
	char szFilePath[ _MAX_PATH_LARGE ] = {0};
	if (0 != ::GetModuleFileNameA( hLibraryHandle, szFilePath, _MAX_PATH_LARGE - 1))
	{
		rstrLibName = szFilePath;
	}
	else
	{
		uint32 rc = ::GetLastError();
		std::string strErrorMessage( "Error Getting Module Name " );
		strErrorMessage += ", Error : ";
		if ( ERROR_SUCCESS != rc )
		{
			strErrorMessage += BasePlatform::PlatformApi::GetApiErrorMessage(rc);
		}
		else
		{
			strErrorMessage += "GetLastError() returned ERROR_SUCCESS";
		}
		CAF_CM_EXCEPTION_VA0(rc, strErrorMessage.c_str());
	}
#else
	// we know that all of our libs must have a DllMain function
	// so look it up in the specified library and then
	// use its address to find the name
	std::string strSubErrorMessage;
	void * vpAddress = NULL;

	if ( NULL == hLibraryHandle )
	{
		// if its the NULL handle, we really want to look
		// at the main program, on AIX we need to get the 1st entry from
		// loadquery, otherwise we can use dlopen on NULL
		GetMainProgramName( rstrLibName );
	}
	else
	{
		// In a dll we need to look for DllMain
		vpAddress = CEcmDllManager::GetFunctionAddress( hLibraryHandle,
				gs_cszDllMainFunctionName,
				strSubErrorMessage );


		if ( NULL == vpAddress )
		{
			// try looking for main as well
			vpAddress = CEcmDllManager::GetFunctionAddress( hLibraryHandle,
						gs_cszMainFunctionName,
						strSubErrorMessage );
		}

		if ( NULL == vpAddress )
		{
			// try looking for _init as well
			vpAddress = CEcmDllManager::GetFunctionAddress( hLibraryHandle, 
					gs_cszInitFunctionName,
					strSubErrorMessage );
		}

		if ( vpAddress )
		{
			CEcmDllManager::GetLibraryNameFromAddress( vpAddress, rstrLibName );
		}
		else
		{
			CAF_CM_EXCEPTION_VA1(E_FAIL,
								 "Cannot find symbol in library, cannot resolve library handle to file name: %s",
								 strSubErrorMessage.c_str());
		}
	}
#endif
}

//////////////////////////////////////////////////////////////////////////////
//
//  GetLibraryNameFromAddress
//  Determines which library/exe contains the specific address (Unix only).
//  We are going to need to add functionality here for AIX 4.3.3 and HPUX10.20
//	
//////////////////////////////////////////////////////////////////////////////
void CEcmDllManager::GetLibraryNameFromAddress( const void * cpvAddressInLibrary, std::string & rstrLibName )
{
	CAF_CM_STATIC_FUNC( "CEcmDllManager", "GetLibraryNameFromAddress" );

	char * szBuffer = NULL;

	try
	{
#if defined ( WIN32 )
		CAF_CM_EXCEPTION_VA0(
			E_NOTIMPL,
			"CEcmDllManager::GetLibraryNameFromAddress not implemented");

#elif defined ( __sun__ ) || defined ( __linux__ ) || defined ( __hpux__ ) || defined (__APPLE__)

		CAutoMutexLockUnlockRaw oCS( &ms_mutex );
		Dl_info stDlInfo;

		if ( 0 != ::dladdr( const_cast<void*>( cpvAddressInLibrary ), &stDlInfo ) )
		{
			char szFilePath[ 32768 ];
			::realpath( stDlInfo.dli_fname, szFilePath );
			rstrLibName = szFilePath;
		}
		else
		{
			std::string strErrorMessage( "Unable to locate address in library " );
			strErrorMessage += ", Error : ";
			const char * pszError = ::dlerror();
			if ( NULL != pszError )
			{
				strErrorMessage += pszError;
			}
			else
			{
				strErrorMessage += "dlerror() returned NULL";
			}
			CAF_CM_EXCEPTION_EFAIL(strErrorMessage.c_str());
		}

#elif defined ( _AIX )

		szBuffer = static_cast<char*>( ::malloc(1024) );
		int32 iSize = 1024;
	
		int32 iRc = ::loadquery( L_GETINFO, static_cast<void*>( szBuffer ),iSize);

		while ( ( -1 == iRc ) && ( ENOMEM == errno ) )
		{
			::free( szBuffer );
			iSize += 1024;
			szBuffer = static_cast<char*>( ::malloc( iSize ) );
			iRc = ::loadquery( L_GETINFO, static_cast<void*>( szBuffer ),iSize);
		}
		
		if ( -1 == iRc )
		{
			::free( szBuffer );
			szBuffer = NULL;
			CAF_CM_EXCEPTION( "::loadquery failed", errno );
		}


		ld_info * pLdInfo = reinterpret_cast<ld_info*>( szBuffer );

		while ( pLdInfo )
		{
			void * pvUpper = reinterpret_cast<void*>( 
					reinterpret_cast<char*>( pLdInfo->ldinfo_dataorg) + 
						pLdInfo->ldinfo_datasize );

			if ( ( cpvAddressInLibrary >= pLdInfo->ldinfo_dataorg ) && 
				 ( cpvAddressInLibrary < pvUpper ) )
			{
				char szFilePath[ _MAX_PATH ];
				::realpath( pLdInfo->ldinfo_filename, szFilePath );
				rstrLibName = szFilePath;
				break;
			}
			else
			{
				if ( pLdInfo->ldinfo_next == 0 )
					pLdInfo = NULL;
				else
					pLdInfo = reinterpret_cast<ld_info*> (
						reinterpret_cast<char*>( pLdInfo ) + 
							pLdInfo->ldinfo_next );
			}
		}

		::free( szBuffer );
		szBuffer = NULL;

		if ( rstrLibName.length() == 0 )
		{
			CAF_CM_EXCEPTION_EFAIL("Unable to locate address in library");
		}

#else
#error "Not yet implemented on this platform";
#endif
	}
	catch(CCafException *e)
	{
		_cm_exception_ = e;
	}

	if ( szBuffer )
	{
		::free( szBuffer );
	}

	if ( _cm_exception_ )
	{
		throw _cm_exception_;
	}
}

//////////////////////////////////////////////////////////////////////////////
//
//  GetMainProgramName
//  Determines the name of the current program.
//  We are going to need to add functionality here for AIX 4.3.3 and HPUX10.20
//	
//////////////////////////////////////////////////////////////////////////////
void CEcmDllManager::GetMainProgramName ( std::string & rstrProgName )
{
	CAF_CM_STATIC_FUNC( "CEcmDllManager", "GetMainProgramName" );

	char * szBuffer = NULL;
	HMODULE hSearchModule = NULL;
	std::string strSubErrorMessage;
	try
	{
		rstrProgName = "";
#if defined ( WIN32 )
		
		char szFilePath[ _MAX_PATH_LARGE] = {0};
		if ( 0 != ::GetModuleFileNameA( NULL, szFilePath, _MAX_PATH_LARGE-1) )
		{
			rstrProgName = szFilePath;
		}
		else
		{
			uint32 rc = ::GetLastError();
			std::string strErrorMessage( "Error Getting Module Name " );
			strErrorMessage += ", Error : ";
			if ( ERROR_SUCCESS != rc )
			{
				strErrorMessage += BasePlatform::PlatformApi::GetApiErrorMessage(rc);
			}
			else
			{
				strErrorMessage += "GetLastError() returned ERROR_SUCCESS";
			}
			CAF_CM_EXCEPTION_VA0(rc, strErrorMessage.c_str());
		}
#elif defined ( _AIX )

		// on AIX we need to get the 1st entry from
		// loadquery, otherwise we can use dlopen on NULL
		szBuffer = static_cast<char*>( ::malloc(1024) );
		int32 iSize = 1024;
	
		int32 iRc = ::loadquery( L_GETINFO, 
		static_cast<void*>( szBuffer ),iSize);

		while ( ( -1 == iRc ) && ( ENOMEM == errno ) )
		{
			::free( szBuffer );
			iSize += 1024;
			szBuffer = static_cast<char*>( ::malloc( iSize ) );
			iRc = ::loadquery( L_GETINFO, 
					static_cast<void*>( szBuffer ),iSize);
		}
		
		if ( -1 == iRc )
		{
			::free( szBuffer );
			szBuffer = NULL;
			CAF_CM_EXCEPTION( "::loadquery failed", errno );
		}


		ld_info * pLdInfo = reinterpret_cast<ld_info*>( szBuffer );

		char szFilePath[ _MAX_PATH ];
		if ( NULL == ::realpath( pLdInfo->ldinfo_filename, szFilePath ) )
		{
			std::string strError( "::realpath failed for " );
			strError += pLdInfo->ldinfo_filename;
			strError += " attempting ::getargs";
			CAF_CM_WARNING( static_cast<const wchar_t*>( strError ) );

			// use getargs to get arg 0
			struct procsinfo stProcInfo;
			stProcInfo.pi_pid = ::getpid();
			char aszArgs[_MAX_PATH * 2];
			if ( 0 == ::getargs( &stProcInfo, sizeof(procsinfo),
					aszArgs, _MAX_PATH * 2 ) )
			{
				if ( NULL == ::realpath( aszArgs, szFilePath ) )
				{
					std::string strError( "::realpath failed - using result of getargs ");
					strError += pLdInfo->ldinfo_filename ;
					CAF_CM_ERROR( static_cast<const wchar_t*>( strError ),
							errno );
				}
				else
				{
					rstrProgName = szFilePath;
				}
			}
			else
			{
				std::string strError( "::getargs failed - using result of loadquery " );
				strError += pLdInfo->ldinfo_filename;
				CAF_CM_ERROR( static_cast<const wchar_t*>( strError ),
						errno );
			}
		}
		else
		{
			// set from the realpath command
			rstrProgName = szFilePath;
		}

		::free( szBuffer );
		szBuffer = NULL;

#elif defined ( __linux__ ) || defined ( __sun__ )
		// we know that all of our libs must have a DllMain function
		// so look it up in the specified library and then
		// use its address to find the name
		void * vpAddress = NULL;
		hSearchModule = ::dlopen( NULL, RTLD_LAZY | RTLD_LOCAL );

		if ( ! hSearchModule )
		{
			CAF_CM_EXCEPTION_EFAIL(::dlerror());
		}

		// In a dll we need to look for DllMain
		vpAddress = CEcmDllManager::GetFunctionAddress( hSearchModule, 
					gs_cszDllMainFunctionName,
					strSubErrorMessage );
		

		if ( NULL == vpAddress )
		{
			// try looking for main as well
			vpAddress = CEcmDllManager::GetFunctionAddress( hSearchModule, 
					gs_cszMainFunctionName,
					strSubErrorMessage );
		}

		if ( NULL == vpAddress )
		{
			// try looking for _init as well
			vpAddress = CEcmDllManager::GetFunctionAddress( hSearchModule,
					gs_cszInitFunctionName,
					strSubErrorMessage );
		}

		if ( vpAddress )
		{
			CEcmDllManager::GetLibraryNameFromAddress( vpAddress, rstrProgName );
		}
		else
		{
			std::string strErrorMessage( "Cannot find symbol in library, cannot resolve library handle to file name" );
			strErrorMessage += strSubErrorMessage;
			CAF_CM_EXCEPTION_EFAIL(strErrorMessage.c_str());
		}
#else
		CAF_CM_EXCEPTION_VA0(E_NOTIMPL, "Not Yet Ported to this platform");
#endif
	}
	catch (CCafException* e)
	{
		_cm_exception_ = e;
	}

#ifndef WIN32
	if ( hSearchModule )
	{
		::dlclose(hSearchModule);
	}
#endif

	if ( szBuffer )
	{
		::free( szBuffer );
	}

	if ( _cm_exception_ )
	{
		throw _cm_exception_;
	}
}

