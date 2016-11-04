/*
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef PLATFORM_TYPES_H
#define PLATFORM_TYPES_H

#ifdef WIN32

#include <wtypes.h>

#else
//	#include <sys/timeb.h>
#include <sys/types.h>
#include <wchar.h>
#if defined (__linux__) || defined (__APPLE__)
	#include <stdint.h>
#endif

//typedef int32 RPC_STATUS;

#define __declspec(value)  
#define __stdcall
#define __cdecl
#define APIENTRY

#define FAR

typedef void * HINSTANCE;
typedef void * HMODULE;
typedef void * HANDLE;
typedef void * LPVOID;
typedef bool BOOL;
typedef uint8_t byte;
typedef int32_t HRESULT;
#endif

#endif // PLATFORM_TYPES_H
